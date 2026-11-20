// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/** Host checks for the voice policy. They do not hear or speak. */
public final class FaithVoiceTest {
    private static int failed;

    public static void main(String[] args) {
        phrase();
        endpoint();
        defaults();
        listeningGate();
        transcriptGate();
        typedText();
        audioPaths();
        cloudGate();
        panel();
        if (failed != 0) {
            System.err.println(failed + " faith voice checks failed");
            System.exit(1);
        }
        System.out.println("Faith voice policy checks passed");
    }

    private static void phrase() {
        check(Phrase.contains("Hey Faith", "Hey Faith"), "default phrase matches itself");
        check(Phrase.contains("HEY, FAITH! What time?", "Hey Faith"), "punctuation and case");
        check("What time?".equals(Phrase.commandAfter("HEY, FAITH! What time?", "Hey Faith")),
                "command keeps the remainder");
        check("What time is it?".equals(Phrase.commandAfter("Hey Faith. What time is it?", "Hey Faith")),
                "sentence after the phrase");
        check("".equals(Phrase.commandAfter("Hey Faith", "Hey Faith")), "phrase alone has no command");
        check(!Phrase.contains("hey", "Hey Faith"), "one word is not the phrase");
        check(!Phrase.contains("faith", "Hey Faith"), "the name alone is not the phrase");
        check(!Phrase.contains("hey faithful", "Hey Faith"), "faithful is a different word");
        check(!Phrase.contains("heyfaith", "Hey Faith"), "joined words do not match");
        check("set a timer".equals(Phrase.commandAfter("um Hey Faith set a timer", "Hey Faith")),
                "phrase may follow a short lead-in");
        check(Phrase.acceptable("Okay computer"), "owner phrase is accepted");
        check("open maps".equals(Phrase.commandAfter("okay computer, open maps", "Okay computer")),
                "owner phrase is the gate");
        check(Phrase.contains("Hé Faith", "Hé Faith"), "accented phrase");
        check(!Phrase.acceptable("   "), "blank phrase is rejected");
        check(!Phrase.acceptable("!!!"), "punctuation is not a phrase");
        check(Phrase.acceptable("a".repeat(64)), "64 letters fit");
        check(!Phrase.acceptable("a".repeat(65)), "65 letters do not");
        check("Hey Faith".equals(Phrase.collapse("  Hey   Faith  ")), "display whitespace collapses");
    }

    private static void endpoint() {
        check(CloudEndpoint.decide(null) == CloudEndpoint.Decision.CLEARED, "null endpoint clears");
        check(CloudEndpoint.decide("  ") == CloudEndpoint.Decision.CLEARED, "blank endpoint clears");
        check(CloudEndpoint.decide("http://127.0.0.1:9/faith") == CloudEndpoint.Decision.ACCEPTED,
                "loopback endpoint is the owner's");
        check(CloudEndpoint.decide("https://example.com/faith") == CloudEndpoint.Decision.ACCEPTED,
                "unnamed https endpoint is accepted");
        check(CloudEndpoint.decide("https://notgoogleapis.com/x") == CloudEndpoint.Decision.ACCEPTED,
                "a name that merely contains the letters is not Google");
        check(CloudEndpoint.decide("https://speech.googleapis.com.evil.test/x") == CloudEndpoint.Decision.ACCEPTED,
                "a suffix that is not the Google domain is not Google");
        check(CloudEndpoint.decide("https://speech.googleapis.com/v1/speech:recognize") == CloudEndpoint.Decision.REFUSED,
                "Google speech host is refused");
        check(CloudEndpoint.decide("https://Speech.GoogleAPIs.com/v1") == CloudEndpoint.Decision.REFUSED,
                "Google host match ignores case");
        check(CloudEndpoint.decide("https://texttospeech.googleapis.com/") == CloudEndpoint.Decision.REFUSED,
                "Google text-to-speech host is refused");
        check(CloudEndpoint.decide("https://generativelanguage.googleapis.com/") == CloudEndpoint.Decision.REFUSED,
                "Google generative host is refused");
        check(CloudEndpoint.decide("https://speech.google.com/") == CloudEndpoint.Decision.REFUSED,
                "speech.google.com is refused");
        check(CloudEndpoint.decide("https://user:pass@speech.googleapis.com/v1") == CloudEndpoint.Decision.REFUSED,
                "credentials do not hide a Google host");
        check(CloudEndpoint.decide("ftp://example.com/faith") == CloudEndpoint.Decision.INVALID, "ftp is rejected");
        check(CloudEndpoint.decide("not a url") == CloudEndpoint.Decision.INVALID, "plain text is rejected");
    }

    private static void defaults() {
        VoiceSession session = VoiceSession.prototype(new MissingAssistantClient());
        check(VoiceSession.DEFAULT_WAKE_PHRASE.equals("Hey Faith"), "default phrase text");
        check("Hey Faith".equals(session.wakePhrase()), "session starts on Hey Faith");
        check(!session.listening(), "listening starts off");
        check(!session.captureEnabled(), "capture starts off");
        check(!session.cloudEnabled(), "cloud starts off");
        check(session.cloudEndpoint().isEmpty(), "endpoint starts empty");
        check(session.cloudGate() == VoiceSession.CloudGate.OFF, "cloud gate starts off");
        check(!session.cloudTransmits(), "prototype provider does not transmit");
        UnavailableWakeListener wake = new UnavailableWakeListener();
        check(!wake.accept(new byte[] {1, 0}, 16000), "unavailable wake never hits");
        check(!new NoNetworkCloudSpeech().transmits(), "no-network provider flag");
        check(new NoNetworkCloudSpeech().transcribe(new byte[] {1, 0}, 16000, "http://127.0.0.1/") == null,
                "no-network provider returns no transcript");
    }

    private static void listeningGate() {
        FakeWake wake = new FakeWake();
        FakeStt stt = new FakeStt();
        FakeTts tts = new FakeTts();
        FakeCloud cloud = new FakeCloud();
        FakeAssistant assistant = new FakeAssistant();
        VoiceSession session = new VoiceSession(assistant, stt, tts, wake, cloud);
        check(wake.releases == 1, "constructor releases the detector");
        check(wake.accepts == 0, "constructor does not feed audio");

        VoiceSession.Result ignored = session.onAudio(new byte[] {1, 0}, 16000);
        check(ignored.kind() == VoiceSession.Result.Kind.IGNORED, "audio while idle is ignored");
        check(wake.accepts == 0 && stt.calls == 0 && cloud.calls == 0 && assistant.calls == 0,
                "idle audio reaches no engine");

        VoiceSession.Result typedGate = session.onTranscript("Hey Faith what time");
        check(typedGate.kind() == VoiceSession.Result.Kind.IGNORED, "transcript while idle is ignored");
        check(assistant.calls == 0, "idle transcript does not ask the assistant");

        session.setListening(true);
        check(session.captureEnabled(), "listening allows capture");
        check(wake.arms == 1 && "Hey Faith".equals(wake.phrase), "listening arms the default phrase");
        session.setListening(false);
        check(!session.captureEnabled() && wake.releases == 2, "turning listening off releases the detector");
        session.onAudio(new byte[] {1, 0}, 16000);
        check(wake.accepts == 0, "released detector is not fed");
    }

    private static void transcriptGate() {
        Harness harness = Harness.create();
        harness.session.setListening(true);
        VoiceSession.Result awake = harness.session.onTranscript("Hey Faith");
        check(awake.kind() == VoiceSession.Result.Kind.AWAKE, "the phrase alone opens a session");
        check(harness.assistant.calls == 0, "a bare wake does not ask yet");

        VoiceSession.Result asked = harness.session.onTranscript("Hey Faith, what time is it?");
        check(asked.kind() == VoiceSession.Result.Kind.REPLIED, "a command after the phrase is answered");
        check("what time is it?".equals(harness.assistant.lastText), "the phrase is not part of the question");
        check(harness.assistant.lastFromVoice, "transcript questions are marked as voice");
        check(asked.spoken(), "a complete reply is spoken when synthesis returns audio");
        check(harness.stt.calls == 0 && harness.cloud.calls == 0, "the text gate does not call speech engines");

        check(harness.session.setWakePhrase("Okay computer"), "phrase change");
        check("Okay computer".equals(harness.wake.phrase), "a listening change re-arms the detector");
        VoiceSession.Result custom = harness.session.onTranscript("okay computer open maps");
        check("open maps".equals(harness.assistant.lastText), "the owner's phrase starts the command");
        VoiceSession.Result missed = harness.session.onTranscript("Hey Faith open maps");
        check(missed.kind() == VoiceSession.Result.Kind.NO_WAKE, "the old phrase does not wake");

        check(!harness.session.setWakePhrase("   "), "blank replacement is rejected");
        check("Okay computer".equals(harness.session.wakePhrase()), "rejected text keeps the saved phrase");
    }

    private static void typedText() {
        Harness harness = Harness.create();
        VoiceSession.Result empty = harness.session.onTypedText("  ");
        check(empty.kind() == VoiceSession.Result.Kind.EMPTY, "blank typing is empty");
        check(harness.assistant.calls == 0, "blank typing does not ask");

        VoiceSession.Result reply = harness.session.onTypedText("Hello");
        check(reply.kind() == VoiceSession.Result.Kind.REPLIED, "typing works while listening is off");
        check("Hello".equals(harness.assistant.lastText) && !harness.assistant.lastFromVoice, "typed text is not voice");
        check(harness.stt.calls == 0 && harness.cloud.calls == 0 && harness.wake.accepts == 0,
                "typing does not touch speech or the cloud");

        VoiceSession missing = VoiceSession.prototype(new MissingAssistantClient());
        VoiceSession.Result unanswered = missing.onTypedText("Hello");
        check(unanswered.kind() == VoiceSession.Result.Kind.UNANSWERED, "a missing service does not pretend to answer");
        check(MissingAssistantClient.MESSAGE.equals(unanswered.text()), "the missing-service sentence is fixed");
        check(!unanswered.spoken(), "an incomplete reply is not spoken");
    }

    private static void audioPaths() {
        Harness harness = Harness.create();
        harness.session.setListening(true);
        byte[] pcm = new byte[] {1, 0, 2, 0};
        VoiceSession.Result quiet = harness.session.onAudio(pcm, 16000);
        check(quiet.kind() == VoiceSession.Result.Kind.LISTENING, "audio without a hit keeps listening");
        check(harness.wake.accepts == 1 && harness.stt.calls == 0, "a miss does not transcribe");

        harness.wake.hit = true;
        harness.stt.transcript = "Hey Faith";
        VoiceSession.Result awake = harness.session.onAudio(pcm, 16000);
        check(awake.kind() == VoiceSession.Result.Kind.AWAKE, "audio of only the phrase stays awake");
        check(harness.assistant.calls == 0 && harness.cloud.calls == 0, "awake audio stays on device and waits");

        harness.stt.transcript = "Hey Faith set a timer";
        VoiceSession.Result command = harness.session.onAudio(pcm, 16000);
        check("set a timer".equals(harness.assistant.lastText), "on-device transcript strips the phrase");
        check(command.spoken() && harness.tts.calls == 1, "the reply is synthesized on device");
        check(harness.cloud.calls == 0, "the on-device path does not call cloud");

        harness.stt.transcript = "hello";
        VoiceSession.Result remainder = harness.session.onAudio(pcm, 16000);
        check("hello".equals(harness.assistant.lastText), "text after an acoustic hit is the command");
        check(remainder.kind() == VoiceSession.Result.Kind.REPLIED, "that command is answered");

        harness.stt.transcript = null;
        int cloudBefore = harness.cloud.calls;
        int assistantBefore = harness.assistant.calls;
        VoiceSession.Result missing = harness.session.onAudio(pcm, 16000);
        check(missing.kind() == VoiceSession.Result.Kind.UNAVAILABLE, "a recognizer miss is unavailable");
        check(harness.cloud.calls == cloudBefore && harness.assistant.calls == assistantBefore,
                "a recognizer miss does not fall through to cloud or the assistant");

        VoiceSession.Result badRate = harness.session.onAudio(pcm, 0);
        check(badRate.kind() == VoiceSession.Result.Kind.LISTENING, "a bad rate is not a hit");
    }

    private static void cloudGate() {
        Harness harness = Harness.create();
        harness.session.setListening(true);
        harness.wake.hit = true;
        harness.stt.transcript = "on device";
        byte[] pcm = new byte[] {3, 0};

        harness.session.setCloudEnabled(true);
        check(harness.session.cloudGate() == VoiceSession.CloudGate.NO_ENDPOINT, "opt-in without an endpoint stays closed");
        harness.session.onAudio(pcm, 16000);
        check(harness.stt.calls == 1 && harness.cloud.calls == 0, "a missing endpoint stays on device");

        check(harness.session.setCloudEndpoint("https://speech.googleapis.com/v1") == CloudEndpoint.Decision.REFUSED,
                "Google endpoint is not stored");
        check(harness.session.cloudEndpoint().isEmpty(), "a refusal leaves the endpoint empty");
        harness.session.onAudio(pcm, 16000);
        check(harness.cloud.calls == 0, "a refused endpoint is not called");

        check(harness.session.setCloudEndpoint("https://example.com/speech") == CloudEndpoint.Decision.ACCEPTED,
                "owner endpoint is stored");
        harness.session.setCloudEnabled(false);
        harness.session.onAudio(pcm, 16000);
        check(harness.cloud.calls == 0 && harness.stt.calls == 3, "a saved endpoint is idle while cloud is off");

        harness.session.setCloudEnabled(true);
        harness.cloud.transcript = "from cloud";
        int sttBefore = harness.stt.calls;
        VoiceSession.Result cloud = harness.session.onAudio(pcm, 16000);
        check(harness.cloud.calls == 1, "opt-in with an endpoint calls the provider");
        check("https://example.com/speech".equals(harness.cloud.lastEndpoint), "the call uses the owner endpoint");
        check(harness.stt.calls == sttBefore, "cloud opt-in does not also run on-device recognition");
        check("from cloud".equals(harness.assistant.lastText), "the cloud transcript is the question");
        check(cloud.kind() == VoiceSession.Result.Kind.REPLIED, "a cloud transcript can be answered");

        harness.cloud.transcript = null;
        sttBefore = harness.stt.calls;
        VoiceSession.Result failed = harness.session.onAudio(pcm, 16000);
        check(failed.kind() == VoiceSession.Result.Kind.UNAVAILABLE, "cloud failure is unavailable");
        check(harness.stt.calls == sttBefore, "cloud failure does not fall back to on-device recognition");

        harness.session.onTypedText("still local");
        check(harness.cloud.calls == 2, "typing does not add a cloud call");
        check("still local".equals(harness.assistant.lastText), "typing still reaches the assistant");
    }

    private static void panel() {
        VoiceSession session = VoiceSession.prototype(new MissingAssistantClient());
        check(VoicePanel.mic(session) == VoicePanel.Mic.IDLE, "idle panel");
        check(VoicePanel.cloud(session) == VoicePanel.CloudLine.OFF, "cloud panel starts off");
        session.setListening(true);
        check(VoicePanel.mic(session) == VoicePanel.Mic.LISTENING_MIC_CLOSED,
                "listening in this build does not claim an open microphone");
        session.setCloudEnabled(true);
        check(VoicePanel.cloud(session) == VoicePanel.CloudLine.NEEDS_ENDPOINT, "cloud without an endpoint");
        session.setCloudEndpoint("http://127.0.0.1:9/faith");
        check(VoicePanel.cloud(session) == VoicePanel.CloudLine.OPTED_IN_NOTHING_SENT,
                "the shipped provider still sends nothing");
    }

    private static void check(boolean condition, String message) {
        if (!condition) {
            failed++;
            System.err.println("FAIL " + message);
        }
    }

    private static final class Harness {
        final VoiceSession session;
        final FakeWake wake;
        final FakeStt stt;
        final FakeTts tts;
        final FakeCloud cloud;
        final FakeAssistant assistant;

        private Harness(VoiceSession session, FakeWake wake, FakeStt stt, FakeTts tts, FakeCloud cloud,
                FakeAssistant assistant) {
            this.session = session;
            this.wake = wake;
            this.stt = stt;
            this.tts = tts;
            this.cloud = cloud;
            this.assistant = assistant;
        }

        static Harness create() {
            FakeWake wake = new FakeWake();
            FakeStt stt = new FakeStt();
            FakeTts tts = new FakeTts();
            FakeCloud cloud = new FakeCloud();
            FakeAssistant assistant = new FakeAssistant();
            return new Harness(new VoiceSession(assistant, stt, tts, wake, cloud), wake, stt, tts, cloud, assistant);
        }
    }

    private static final class FakeWake implements WakeListener {
        int arms;
        int releases;
        int accepts;
        String phrase = "";
        boolean hit;

        @Override
        public void arm(String phrase) {
            arms++;
            this.phrase = phrase;
        }

        @Override
        public void release() {
            releases++;
        }

        @Override
        public boolean accept(byte[] pcmLe16, int sampleRateHz) {
            accepts++;
            return hit;
        }
    }

    private static final class FakeStt implements SpeechToText {
        int calls;
        String transcript = "";

        @Override
        public String transcribe(byte[] pcmLe16, int sampleRateHz) {
            calls++;
            return transcript;
        }
    }

    private static final class FakeTts implements TextToSpeech {
        int calls;

        @Override
        public byte[] synthesize(String text) {
            calls++;
            return new byte[] {1, 0};
        }
    }

    private static final class FakeCloud implements CloudSpeechProvider {
        int calls;
        String lastEndpoint = "";
        String transcript;

        @Override
        public boolean transmits() {
            return true;
        }

        @Override
        public String transcribe(byte[] pcmLe16, int sampleRateHz, String endpoint) {
            calls++;
            lastEndpoint = endpoint;
            return transcript;
        }
    }

    private static final class FakeAssistant implements AssistantClient {
        int calls;
        String lastText = "";
        boolean lastFromVoice;

        @Override
        public AssistantReply ask(AssistantTurn turn) {
            calls++;
            lastText = turn.text();
            lastFromVoice = turn.fromVoice();
            return new AssistantReply("echo " + turn.text(), true);
        }
    }
}
