/* SPDX-License-Identifier: Apache-2.0 */
package org.eleos.share;

import android.os.Build;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.math.BigInteger;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.security.interfaces.ECKey;
import java.security.spec.ECFieldFp;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.ECParameterSpec;
import java.security.spec.EllipticCurve;
import java.util.Arrays;
import java.util.Date;
import java.util.Locale;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLPeerUnverifiedException;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import javax.security.auth.x500.X500Principal;

/**
 * One-session, TLS 1.3 pairing over an already-connected TCP socket.
 *
 * <p>Both peers create an ephemeral AndroidKeyStore P-256 key and a
 * provider-generated self-signed certificate. The trust manager accepts only
 * a single, valid, self-signed P-256 certificate and records its SHA-256
 * fingerprint. The six-digit SAS is derived symmetrically from the sorted
 * local and peer fingerprints. The application must show the SAS and obtain
 * the user's out-of-band comparison before calling {@link #confirmPeer}.
 * Transfer code is invoked only by {@link #runTransferProtocol} after that
 * confirmation. No socket or stream is exposed before confirmation.
 *
 * <p>Requires Android API 29 or newer for the public TLS 1.3 socket API and
 * API 23 or newer for AndroidKeyStore EC key generation. This class uses
 * public platform APIs and no custom cryptography, custom certificate
 * signing, hidden APIs, or trust-all manager.
 *
 * <p>Lifecycle: call {@link #establish} once for each connected socket, show
 * {@link #getSasCode} and {@link #getPeerFingerprintSha256}, then call
 * {@link #confirmPeer(String)} only after the user compares the SAS through
 * an independent channel. Run one synchronous transfer callback and close
 * the session. If pairing is canceled or fails, close it; cleanup removes
 * the per-session AndroidKeyStore alias and closes the layered socket.
 */
public final class AndroidTlsPairing implements AutoCloseable {
    public enum Role {
        CLIENT,
        SERVER
    }

    /** Synchronous transfer callback; it is never called before confirmation. */
    public interface TransferProtocol {
        void run(InputStream input, OutputStream output) throws IOException;
    }

    private static final String KEYSTORE = "AndroidKeyStore";
    private static final String KEY_ALIAS_PREFIX = "ele-share-session-";
    private static final String SAS_DOMAIN = "ELE-SHARE-TLS13-SAS-v1|";
    private static final String TLS_13 = "TLSv1.3";
    private static final int HANDSHAKE_TIMEOUT_MS = 15_000;
    private static final long CERT_LIFETIME_MS = 24L * 60L * 60L * 1000L;
    private static final long CLOCK_SKEW_MS = 5L * 60L * 1000L;

    private final Object lock = new Object();
    private final SSLSocket tlsSocket;
    private final String keyAlias;
    private final byte[] localFingerprint;
    private final byte[] peerFingerprint;
    private final String peerFingerprintText;
    private final String sasCode;
    private boolean peerConfirmed;
    private boolean transferStarted;
    private boolean closed;

    private AndroidTlsPairing(SSLSocket socket, String alias,
                              byte[] localFingerprint, byte[] peerFingerprint) {
        this.tlsSocket = socket;
        this.keyAlias = alias;
        this.localFingerprint = localFingerprint.clone();
        this.peerFingerprint = peerFingerprint.clone();
        this.peerFingerprintText = toHex(peerFingerprint, true);
        this.sasCode = deriveSas(this.localFingerprint, this.peerFingerprint);
    }

    /**
     * Performs mutual TLS 1.3 on an already-connected socket. The passed
     * socket becomes owned by this session and is closed on failure or close.
     */
    public static AndroidTlsPairing establish(Socket connectedSocket, Role role)
            throws IOException, GeneralSecurityException {
        if (Build.VERSION.SDK_INT < 29) {
            throw new GeneralSecurityException("TLS 1.3 pairing requires Android API 29 or newer");
        }
        if (connectedSocket == null || role == null) {
            throw new NullPointerException("socket and role are required");
        }
        if (!connectedSocket.isConnected() || connectedSocket.isClosed() ||
                connectedSocket.isInputShutdown() || connectedSocket.isOutputShutdown()) {
            throw new IOException("pairing requires an open, connected socket");
        }

        SessionIdentity identity = createIdentity();
        SSLSocket tls = null;
        boolean success = false;
        try {
            KeyStore keyStore = KeyStore.getInstance(KEYSTORE);
            keyStore.load(null);
            if (!keyStore.isKeyEntry(identity.alias)) {
                throw new GeneralSecurityException("AndroidKeyStore session key is unavailable");
            }

            KeyManagerFactory keyManagers = KeyManagerFactory.getInstance(
                    KeyManagerFactory.getDefaultAlgorithm());
            keyManagers.init(keyStore, null);

            PeerTrustManager trustManager = new PeerTrustManager(identity.certificate);
            SSLContext context = SSLContext.getInstance(TLS_13);
            context.init(keyManagers.getKeyManagers(), new TrustManager[]{trustManager},
                    new SecureRandom());

            SocketAddressParts peer = SocketAddressParts.from(connectedSocket);
            SSLSocketFactory factory = context.getSocketFactory();
            tls = (SSLSocket) factory.createSocket(connectedSocket, peer.host,
                    peer.port, true);
            tls.setUseClientMode(role == Role.CLIENT);
            tls.setEnabledProtocols(new String[]{TLS_13});
            if (role == Role.SERVER) {
                tls.setNeedClientAuth(true);
            }

            int previousTimeout = connectedSocket.getSoTimeout();
            tls.setSoTimeout(HANDSHAKE_TIMEOUT_MS);
            tls.startHandshake();
            tls.setSoTimeout(previousTimeout);

            SSLSession session = tls.getSession();
            if (!TLS_13.equals(session.getProtocol())) {
                throw new GeneralSecurityException("TLS 1.3 was not negotiated");
            }
            Certificate[] presented = session.getPeerCertificates();
            if (presented.length != 1 || !(presented[0] instanceof X509Certificate)) {
                throw new SSLPeerUnverifiedException("expected one peer self-signed certificate");
            }
            byte[] sessionFingerprint = sha256(presented[0].getEncoded());
            byte[] capturedFingerprint = trustManager.getCapturedFingerprint();
            if (capturedFingerprint == null ||
                    !MessageDigest.isEqual(sessionFingerprint, capturedFingerprint)) {
                throw new SSLPeerUnverifiedException("peer certificate pin capture mismatch");
            }
            Certificate[] localCertificates = session.getLocalCertificates();
            if (localCertificates == null || localCertificates.length != 1 ||
                    !(localCertificates[0] instanceof X509Certificate) ||
                    !MessageDigest.isEqual(sha256(localCertificates[0].getEncoded()),
                            sha256(identity.certificate.getEncoded()))) {
                throw new SSLPeerUnverifiedException(
                        "TLS peer did not authenticate this session certificate");
            }

            AndroidTlsPairing pairing = new AndroidTlsPairing(tls, identity.alias,
                    sha256(identity.certificate.getEncoded()), capturedFingerprint);
            success = true;
            return pairing;
        } finally {
            if (!success) {
                if (tls != null) {
                    try {
                        tls.close();
                    } catch (IOException ignored) {
                        // Preserve the handshake or validation failure.
                    }
                } else {
                    try {
                        connectedSocket.close();
                    } catch (IOException ignored) {
                        // Preserve the setup failure.
                    }
                }
                deleteKey(identity.alias);
            }
        }
    }

    /** Returns the peer's SHA-256 X.509 certificate fingerprint, uppercase hex. */
    public String getPeerFingerprintSha256() {
        return peerFingerprintText;
    }

    /** Returns the identical six-digit SAS at both peers for this TLS session. */
    public String getSasCode() {
        return sasCode;
    }

    /**
     * Records the user's out-of-band SAS comparison. A mismatch never opens
     * the transfer gate. Callers should invoke this only after the displayed
     * code was compared over a separate trusted channel.
     */
    public void confirmPeer(String comparedSas) throws GeneralSecurityException {
        if (comparedSas == null) {
            throw new NullPointerException("comparedSas");
        }
        byte[] provided = comparedSas.getBytes(StandardCharsets.US_ASCII);
        byte[] expected = sasCode.getBytes(StandardCharsets.US_ASCII);
        synchronized (lock) {
            ensureOpen();
            if (transferStarted) {
                throw new IllegalStateException("transfer protocol has already started");
            }
            if (!MessageDigest.isEqual(provided, expected)) {
                peerConfirmed = false;
                throw new GeneralSecurityException("peer SAS did not match");
            }
            peerConfirmed = true;
        }
    }

    /** True only after the caller supplied the matching SAS. */
    public boolean isPeerConfirmed() {
        synchronized (lock) {
            return peerConfirmed && !closed;
        }
    }

    /**
     * Invokes the transfer protocol at most once, and only after confirmPeer.
     * Streams are session-owned and become invalid when this method returns.
     */
    public void runTransferProtocol(TransferProtocol protocol) throws IOException {
        if (protocol == null) {
            throw new NullPointerException("protocol");
        }
        synchronized (lock) {
            ensureOpen();
            if (!peerConfirmed) {
                throw new SecurityException("peer confirmation is required before transfer");
            }
            if (transferStarted) {
                throw new IllegalStateException("transfer protocol may run only once");
            }
            transferStarted = true;
        }
        try {
            protocol.run(tlsSocket.getInputStream(), tlsSocket.getOutputStream());
        } finally {
            close();
        }
    }

    @Override
    public void close() {
        synchronized (lock) {
            if (closed) {
                return;
            }
            closed = true;
            peerConfirmed = false;
            Arrays.fill(localFingerprint, (byte) 0);
            Arrays.fill(peerFingerprint, (byte) 0);
        }
        try {
            tlsSocket.close();
        } catch (IOException ignored) {
            // The session is closed regardless of transport cleanup errors.
        } finally {
            deleteKey(keyAlias);
        }
    }

    private void ensureOpen() {
        if (closed || tlsSocket.isClosed()) {
            throw new IllegalStateException("pairing session is closed");
        }
    }

    private static SessionIdentity createIdentity() throws GeneralSecurityException {
        SecureRandom random = new SecureRandom();
        byte[] sessionId = new byte[16];
        random.nextBytes(sessionId);
        String alias = KEY_ALIAS_PREFIX + toHex(sessionId, false).toLowerCase(Locale.ROOT);
        Arrays.fill(sessionId, (byte) 0);

        long now = System.currentTimeMillis();
        Date notBefore = new Date(now - CLOCK_SKEW_MS);
        Date notAfter = new Date(now + CERT_LIFETIME_MS);
        byte[] serialBytes = new byte[16];
        random.nextBytes(serialBytes);
        BigInteger serial = new BigInteger(1, serialBytes);
        Arrays.fill(serialBytes, (byte) 0);

        try {
            KeyPairGenerator generator = KeyPairGenerator.getInstance(
                    KeyProperties.KEY_ALGORITHM_EC, KEYSTORE);
            KeyGenParameterSpec spec = new KeyGenParameterSpec.Builder(alias,
                    KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setAlgorithmParameterSpec(new ECGenParameterSpec("secp256r1"))
                    .setKeySize(256)
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .setCertificateSubject(new X500Principal("CN=EleShare-" + alias.substring(
                            KEY_ALIAS_PREFIX.length())))
                    .setCertificateSerialNumber(serial)
                    .setCertificateNotBefore(notBefore)
                    .setCertificateNotAfter(notAfter)
                    .setKeyValidityStart(notBefore)
                    .setKeyValidityEnd(notAfter)
                    .build();
            generator.initialize(spec);
            KeyPair pair = generator.generateKeyPair();

            KeyStore keyStore = KeyStore.getInstance(KEYSTORE);
            keyStore.load(null);
            Certificate[] chain = keyStore.getCertificateChain(alias);
            if (pair.getPrivate() == null || chain == null || chain.length != 1 ||
                    !(chain[0] instanceof X509Certificate)) {
                throw new GeneralSecurityException("AndroidKeyStore did not create a session certificate");
            }
            X509Certificate certificate = (X509Certificate) chain[0];
            validatePeerCertificate(certificate, certificate.getPublicKey());
            if (!MessageDigest.isEqual(pair.getPublic().getEncoded(),
                    certificate.getPublicKey().getEncoded())) {
                throw new GeneralSecurityException("session certificate key does not match its alias");
            }
            return new SessionIdentity(alias, certificate);
        } catch (GeneralSecurityException | IOException | RuntimeException e) {
            deleteKey(alias);
            if (e instanceof GeneralSecurityException) {
                throw (GeneralSecurityException) e;
            }
            throw new GeneralSecurityException("could not create AndroidKeyStore TLS identity", e);
        }
    }

    private static void deleteKey(String alias) {
        try {
            KeyStore keyStore = KeyStore.getInstance(KEYSTORE);
            keyStore.load(null);
            if (keyStore.containsAlias(alias)) {
                keyStore.deleteEntry(alias);
            }
        } catch (GeneralSecurityException | IOException ignored) {
            // Best effort: never replace the pairing or transfer failure.
        }
    }

    private static void validatePeerCertificate(X509Certificate certificate,
                                                java.security.PublicKey expectedCurve)
            throws GeneralSecurityException, CertificateException {
        certificate.checkValidity();
        if (!certificate.getSubjectX500Principal().equals(certificate.getIssuerX500Principal()) ||
                certificate.getBasicConstraints() >= 0 ||
                !"SHA256withECDSA".equalsIgnoreCase(certificate.getSigAlgName())) {
            throw new CertificateException("peer certificate is not a self-signed ECDSA leaf");
        }
        if (!(certificate.getPublicKey() instanceof ECKey) || !(expectedCurve instanceof ECKey) ||
                !sameCurve(((ECKey) expectedCurve).getParams(),
                        ((ECKey) certificate.getPublicKey()).getParams())) {
            throw new CertificateException("peer key is not on the negotiated P-256 curve");
        }
        certificate.verify(certificate.getPublicKey());
    }

    private static boolean sameCurve(ECParameterSpec expected, ECParameterSpec actual) {
        if (expected == null || actual == null ||
                !(expected.getCurve().getField() instanceof ECFieldFp) ||
                !(actual.getCurve().getField() instanceof ECFieldFp)) {
            return false;
        }
        EllipticCurve a = expected.getCurve();
        EllipticCurve b = actual.getCurve();
        ECFieldFp fieldA = (ECFieldFp) a.getField();
        ECFieldFp fieldB = (ECFieldFp) b.getField();
        return fieldA.getP().equals(fieldB.getP()) &&
                a.getA().equals(b.getA()) && a.getB().equals(b.getB()) &&
                expected.getGenerator().getAffineX().equals(actual.getGenerator().getAffineX()) &&
                expected.getGenerator().getAffineY().equals(actual.getGenerator().getAffineY()) &&
                expected.getOrder().equals(actual.getOrder()) &&
                expected.getCofactor() == actual.getCofactor() &&
                fieldA.getP().bitLength() == 256;
    }

    private static String deriveSas(byte[] local, byte[] peer) {
        byte[] first = compareUnsigned(local, peer) <= 0 ? local : peer;
        byte[] second = first == local ? peer : local;
        byte[] domain = SAS_DOMAIN.getBytes(StandardCharsets.US_ASCII);
        byte[] input = new byte[domain.length + first.length + second.length];
        System.arraycopy(domain, 0, input, 0, domain.length);
        System.arraycopy(first, 0, input, domain.length, first.length);
        System.arraycopy(second, 0, input, domain.length + first.length, second.length);
        try {
            byte[] digest = sha256(input);
            long value = ((digest[0] & 0xffL) << 24) |
                    ((digest[1] & 0xffL) << 16) |
                    ((digest[2] & 0xffL) << 8) |
                    (digest[3] & 0xffL);
            return String.format(Locale.US, "%06d", value % 1_000_000L);
        } catch (GeneralSecurityException e) {
            throw new IllegalStateException("SHA-256 is unavailable", e);
        } finally {
            Arrays.fill(input, (byte) 0);
        }
    }

    private static int compareUnsigned(byte[] a, byte[] b) {
        for (int i = 0; i < Math.min(a.length, b.length); i++) {
            int x = a[i] & 0xff;
            int y = b[i] & 0xff;
            if (x != y) {
                return x < y ? -1 : 1;
            }
        }
        return Integer.compare(a.length, b.length);
    }

    private static byte[] sha256(byte[] input) throws GeneralSecurityException {
        return MessageDigest.getInstance("SHA-256").digest(input);
    }

    private static String toHex(byte[] bytes, boolean colonSeparated) {
        StringBuilder out = new StringBuilder(bytes.length * (colonSeparated ? 3 : 2));
        for (int i = 0; i < bytes.length; i++) {
            if (colonSeparated && i != 0) {
                out.append(':');
            }
            int value = bytes[i] & 0xff;
            if (value < 16) {
                out.append('0');
            }
            out.append(Integer.toHexString(value).toUpperCase(Locale.US));
        }
        return out.toString();
    }

    private static final class SessionIdentity {
        final String alias;
        final X509Certificate certificate;

        SessionIdentity(String alias, X509Certificate certificate) {
            this.alias = alias;
            this.certificate = certificate;
        }
    }

    private static final class PeerTrustManager implements X509TrustManager {
        private final java.security.PublicKey expectedCurve;
        private volatile byte[] capturedFingerprint;

        PeerTrustManager(X509Certificate localCertificate) {
            this.expectedCurve = localCertificate.getPublicKey();
        }

        @Override
        public void checkClientTrusted(X509Certificate[] chain, String authType)
                throws CertificateException {
            captureAndValidate(chain);
        }

        @Override
        public void checkServerTrusted(X509Certificate[] chain, String authType)
                throws CertificateException {
            captureAndValidate(chain);
        }

        @Override
        public X509Certificate[] getAcceptedIssuers() {
            // Empty issuer hints request a client certificate without falsely
            // claiming a CA trust anchor for this ephemeral self-signed peer.
            return new X509Certificate[0];
        }

        byte[] getCapturedFingerprint() {
            byte[] fingerprint = capturedFingerprint;
            return fingerprint == null ? null : fingerprint.clone();
        }

        private void captureAndValidate(X509Certificate[] chain) throws CertificateException {
            if (chain == null || chain.length != 1 || chain[0] == null) {
                throw new CertificateException("expected exactly one peer certificate");
            }
            X509Certificate certificate = chain[0];
            try {
                validatePeerCertificate(certificate, expectedCurve);
            } catch (GeneralSecurityException e) {
                throw new CertificateException("peer certificate validation failed", e);
            }
            try {
                byte[] fingerprint = sha256(certificate.getEncoded());
                byte[] previous = capturedFingerprint;
                if (previous != null && !MessageDigest.isEqual(previous, fingerprint)) {
                    throw new CertificateException("peer certificate changed during handshake");
                }
                capturedFingerprint = fingerprint;
            } catch (GeneralSecurityException e) {
                throw new CertificateException("could not fingerprint peer certificate", e);
            }
        }
    }

    private static final class SocketAddressParts {
        final String host;
        final int port;

        private SocketAddressParts(String host, int port) {
            this.host = host;
            this.port = port;
        }

        static SocketAddressParts from(Socket socket) throws IOException {
            if (socket.getInetAddress() == null || socket.getPort() <= 0) {
                throw new IOException("connected socket has no peer address");
            }
            return new SocketAddressParts(socket.getInetAddress().getHostAddress(), socket.getPort());
        }
    }
}
