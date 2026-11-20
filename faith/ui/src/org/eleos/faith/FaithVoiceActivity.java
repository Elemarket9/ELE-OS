// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.Switch;
import android.widget.TextView;

/**
 * Prototype voice screen for Cuttlefish and a Treble GSI.
 * It does not request the microphone or the network, and it does not target a phone maker.
 */
public final class FaithVoiceActivity extends Activity {
    private static final String PREFS = "faith_voice";

    private VoiceSession session;
    private SharedPreferences prefs;
    private EditText phraseField;
    private EditText messageField;
    private EditText endpointField;
    private Switch listenSwitch;
    private Switch cloudSwitch;
    private TextView phraseStatus;
    private TextView activePhrase;
    private TextView micStatus;
    private TextView reply;
    private TextView cloudStatus;
    private Integer cloudNote;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        setContentView(R.layout.activity_faith);
        prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        session = VoiceSession.prototype(new MissingAssistantClient());
        phraseField = findViewById(R.id.phrase);
        messageField = findViewById(R.id.message);
        endpointField = findViewById(R.id.endpoint);
        listenSwitch = findViewById(R.id.listen);
        cloudSwitch = findViewById(R.id.cloud);
        phraseStatus = findViewById(R.id.phrase_status);
        activePhrase = findViewById(R.id.active_phrase);
        micStatus = findViewById(R.id.mic_status);
        reply = findViewById(R.id.reply);
        cloudStatus = findViewById(R.id.cloud_status);

        if (!session.setWakePhrase(prefs.getString("phrase", VoiceSession.DEFAULT_WAKE_PHRASE))) {
            session.setWakePhrase(VoiceSession.DEFAULT_WAKE_PHRASE);
        }
        String storedEndpoint = prefs.getString("endpoint", "");
        if (storedEndpoint != null && !storedEndpoint.isEmpty()) {
            if (session.setCloudEndpoint(storedEndpoint) != CloudEndpoint.Decision.ACCEPTED) {
                prefs.edit().remove("endpoint").apply();
            }
        }
        listenSwitch.setChecked(prefs.getBoolean("listening", false));
        cloudSwitch.setChecked(prefs.getBoolean("cloud", false));
        session.setListening(listenSwitch.isChecked());
        session.setCloudEnabled(cloudSwitch.isChecked());
        phraseField.setText(session.wakePhrase());
        endpointField.setText(session.cloudEndpoint());

        findViewById(R.id.save_phrase).setOnClickListener(v -> savePhrase());
        findViewById(R.id.send).setOnClickListener(v -> send());
        findViewById(R.id.save_endpoint).setOnClickListener(v -> saveEndpoint());
        listenSwitch.setOnCheckedChangeListener(this::onListen);
        cloudSwitch.setOnCheckedChangeListener(this::onCloud);
        render();
    }

    private void savePhrase() {
        if (!session.setWakePhrase(phraseField.getText().toString())) {
            phraseStatus.setText(R.string.phrase_rejected);
            return;
        }
        prefs.edit().putString("phrase", session.wakePhrase()).apply();
        phraseStatus.setText(R.string.phrase_saved);
        render();
    }

    private void onListen(CompoundButton button, boolean checked) {
        session.setListening(checked);
        prefs.edit().putBoolean("listening", checked).apply();
        render();
    }

    private void send() {
        VoiceSession.Result result = session.onTypedText(messageField.getText().toString());
        if (result.kind() == VoiceSession.Result.Kind.EMPTY) {
            reply.setText(R.string.reply_empty_input);
            return;
        }
        reply.setText(result.text());
    }

    private void onCloud(CompoundButton button, boolean checked) {
        cloudNote = null;
        session.setCloudEnabled(checked);
        prefs.edit().putBoolean("cloud", checked).apply();
        render();
    }

    private void saveEndpoint() {
        CloudEndpoint.Decision decision = session.setCloudEndpoint(endpointField.getText().toString());
        if (decision == CloudEndpoint.Decision.REFUSED) {
            cloudNote = R.string.cloud_refused;
            render();
            return;
        }
        if (decision == CloudEndpoint.Decision.INVALID) {
            cloudNote = R.string.cloud_invalid;
            render();
            return;
        }
        cloudNote = null;
        prefs.edit().putString("endpoint", session.cloudEndpoint()).apply();
        render();
    }

    private void render() {
        activePhrase.setText(getString(R.string.active_phrase, session.wakePhrase()));
        if (VoicePanel.mic(session) == VoicePanel.Mic.IDLE) {
            micStatus.setText(R.string.mic_off);
        } else {
            micStatus.setText(R.string.mic_on_closed);
        }
        if (cloudNote != null) {
            cloudStatus.setText(cloudNote.intValue());
            return;
        }
        switch (VoicePanel.cloud(session)) {
            case NEEDS_ENDPOINT:
                cloudStatus.setText(R.string.cloud_needs_endpoint);
                break;
            case OPTED_IN_CAN_SEND:
                cloudStatus.setText(R.string.cloud_can_send);
                break;
            case OPTED_IN_NOTHING_SENT:
                cloudStatus.setText(R.string.cloud_opted_in_no_send);
                break;
            case OFF:
            default:
                cloudStatus.setText(R.string.cloud_off);
                break;
        }
    }
}
