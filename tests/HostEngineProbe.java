// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

import org.eleos.faith.host.HostEngines;

import java.nio.file.Path;

/**
 * One host run of Pico and whisper.cpp. Prints what happened.
 * A transcript that is not the spoken sentence is still a successful run of the tools.
 */
public final class HostEngineProbe {
    private static final String SENTENCE = "Hey Faith. What time is it?";

    public static void main(String[] args) {
        if (args.length != 3) {
            System.err.println("usage: HostEngineProbe pico2wave whisper-cli model");
            System.exit(2);
        }
        HostEngines.PicoCommand pico = new HostEngines.PicoCommand(Path.of(args[0]));
        byte[] pcm = pico.synthesize(SENTENCE);
        if (pcm == null) {
            System.err.println("PICO failed: " + pico.lastError());
            System.exit(1);
        }
        int rms = HostEngines.Wav.rms(pcm);
        System.out.println("PICO bytes=" + pcm.length + " samples=" + (pcm.length / 2) + " rms=" + rms
                + " rate=" + HostEngines.PicoCommand.SAMPLE_RATE_HZ
                + " language=" + HostEngines.PicoCommand.LANGUAGE
                + " lingware=" + HostEngines.PicoCommand.LINGWARE);
        if (rms < 1000 || pcm.length < 32000) {
            System.err.println("PICO audio was silent or too short");
            System.exit(1);
        }

        HostEngines.WhisperCommand whisper = new HostEngines.WhisperCommand(Path.of(args[1]), Path.of(args[2]));
        String transcript = whisper.transcribe(pcm, HostEngines.PicoCommand.SAMPLE_RATE_HZ);
        System.out.println("WHISPER raw=" + singleLine(whisper.lastRaw()));
        if (transcript == null) {
            System.err.println("WHISPER failed: " + whisper.lastError());
            System.exit(1);
        }
        System.out.println("WHISPER transcript=" + transcript);
        boolean heard = Phrase.contains(transcript, VoiceSession.DEFAULT_WAKE_PHRASE);
        System.out.println("PHRASE_GATE match=" + heard + " command=" + Phrase.commandAfter(transcript, VoiceSession.DEFAULT_WAKE_PHRASE));
        System.out.println("Host engine probe finished");
    }

    private static String singleLine(String raw) {
        if (raw == null) {
            return "";
        }
        return raw.replace('\n', ' ').replace('\r', ' ').trim();
    }
}
