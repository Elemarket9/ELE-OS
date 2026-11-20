// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/**
 * Faith voice policy. Not thread-safe. The microphone is not opened here.
 *
 * Listening and cloud speech both start off. Cloud audio is offered to the
 * provider only after the owner opts in and the endpoint is accepted, and
 * only for a buffer the wake listener accepted. A cloud failure does not
 * fall through to the on-device recognizer, and an on-device failure does
 * not fall through to the cloud. Typed text never goes to either speech path.
 */
public final class VoiceSession {
    public static final String DEFAULT_WAKE_PHRASE = "Hey Faith";

    public enum CloudGate {
        OFF, NO_ENDPOINT, OPTED_IN
    }

    private final AssistantClient assistant;
    private final SpeechToText speechToText;
    private final TextToSpeech textToSpeech;
    private final WakeListener wake;
    private final CloudSpeechProvider cloud;

    private String wakePhrase = DEFAULT_WAKE_PHRASE;
    private boolean listening;
    private boolean cloudEnabled;
    private String cloudEndpoint = "";

    public VoiceSession(AssistantClient assistant, SpeechToText speechToText, TextToSpeech textToSpeech,
            WakeListener wake, CloudSpeechProvider cloud) {
        if (assistant == null || speechToText == null || textToSpeech == null || wake == null || cloud == null) {
            throw new IllegalArgumentException("engine");
        }
        this.assistant = assistant;
        this.speechToText = speechToText;
        this.textToSpeech = textToSpeech;
        this.wake = wake;
        this.cloud = cloud;
        wake.release();
    }

    /** Prototype wiring: no model, no recognizer, no speaker, no network. */
    public static VoiceSession prototype(AssistantClient assistant) {
        return new VoiceSession(assistant, new UnavailableSpeechToText(), new UnavailableTextToSpeech(),
                new UnavailableWakeListener(), new NoNetworkCloudSpeech());
    }

    public String wakePhrase() {
        return wakePhrase;
    }

    public boolean setWakePhrase(String phrase) {
        if (!Phrase.acceptable(phrase)) {
            return false;
        }
        wakePhrase = Phrase.collapse(phrase);
        if (listening) {
            wake.arm(wakePhrase);
        }
        return true;
    }

    public boolean listening() {
        return listening;
    }

    public void setListening(boolean on) {
        listening = on;
        if (on) {
            wake.arm(wakePhrase);
        } else {
            wake.release();
        }
    }

    /** True only while the owner has turned listening on. The capture itself is the caller's job. */
    public boolean captureEnabled() {
        return listening;
    }

    public boolean cloudEnabled() {
        return cloudEnabled;
    }

    public void setCloudEnabled(boolean on) {
        cloudEnabled = on;
    }

    public String cloudEndpoint() {
        return cloudEndpoint;
    }

    public CloudEndpoint.Decision setCloudEndpoint(String url) {
        CloudEndpoint.Decision decision = CloudEndpoint.decide(url);
        if (decision == CloudEndpoint.Decision.ACCEPTED) {
            cloudEndpoint = url.trim();
        } else if (decision == CloudEndpoint.Decision.CLEARED) {
            cloudEndpoint = "";
        }
        return decision;
    }

    public CloudGate cloudGate() {
        if (!cloudEnabled) {
            return CloudGate.OFF;
        }
        if (cloudEndpoint.isEmpty()) {
            return CloudGate.NO_ENDPOINT;
        }
        return CloudGate.OPTED_IN;
    }

    public boolean cloudTransmits() {
        return cloud.transmits();
    }

    public Result onAudio(byte[] pcm, int sampleRateHz) {
        if (!listening) {
            return Result.of(Result.Kind.IGNORED);
        }
        if (pcm == null || pcm.length == 0 || sampleRateHz <= 0) {
            return Result.of(Result.Kind.LISTENING);
        }
        if (!wake.accept(pcm, sampleRateHz)) {
            return Result.of(Result.Kind.LISTENING);
        }
        String transcript;
        if (cloudGate() == CloudGate.OPTED_IN) {
            transcript = cloud.transcribe(pcm, sampleRateHz, cloudEndpoint);
        } else {
            transcript = speechToText.transcribe(pcm, sampleRateHz);
        }
        if (transcript == null || transcript.isBlank()) {
            return Result.of(Result.Kind.UNAVAILABLE);
        }
        if (Phrase.contains(transcript, wakePhrase)) {
            String command = Phrase.commandAfter(transcript, wakePhrase);
            if (command.isEmpty()) {
                return Result.of(Result.Kind.AWAKE);
            }
            return ask(command, true);
        }
        return ask(transcript.trim(), true);
    }

    /** Text wake gate. Used when a transcript already exists. It does not call speech engines. */
    public Result onTranscript(String transcript) {
        if (!listening) {
            return Result.of(Result.Kind.IGNORED);
        }
        if (transcript == null || !Phrase.contains(transcript, wakePhrase)) {
            return Result.of(Result.Kind.NO_WAKE);
        }
        String command = Phrase.commandAfter(transcript, wakePhrase);
        if (command.isEmpty()) {
            return Result.of(Result.Kind.AWAKE);
        }
        return ask(command, true);
    }

    public Result onTypedText(String text) {
        if (text == null || text.isBlank()) {
            return Result.of(Result.Kind.EMPTY);
        }
        return ask(text.trim(), false);
    }

    private Result ask(String text, boolean fromVoice) {
        AssistantReply reply = assistant.ask(new AssistantTurn(text, fromVoice));
        if (reply == null || !reply.complete()) {
            return new Result(Result.Kind.UNANSWERED, reply == null ? "" : reply.text(), false);
        }
        byte[] spoken = textToSpeech.synthesize(reply.text());
        return new Result(Result.Kind.REPLIED, reply.text(), spoken != null && spoken.length > 0);
    }

    public static final class Result {
        public enum Kind {
            IGNORED, LISTENING, NO_WAKE, AWAKE, EMPTY, UNAVAILABLE, UNANSWERED, REPLIED
        }

        private final Kind kind;
        private final String text;
        private final boolean spoken;

        private Result(Kind kind, String text, boolean spoken) {
            this.kind = kind;
            this.text = text == null ? "" : text;
            this.spoken = spoken;
        }

        private static Result of(Kind kind) {
            return new Result(kind, "", false);
        }

        public Kind kind() {
            return kind;
        }

        public String text() {
            return text;
        }

        public boolean spoken() {
            return spoken;
        }
    }
}
