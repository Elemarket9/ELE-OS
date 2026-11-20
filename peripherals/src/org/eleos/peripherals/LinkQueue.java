// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Main-thread FIFO admission: one handshake at a time, independent ready links.
 * The cap is an app resource budget, not a controller's hardware link limit.
 */
final class LinkQueue {
    enum State { QUEUED, OPENING, READY, BACKOFF }
    static final int LIMIT = 8;
    private final Map<String, State> links = new LinkedHashMap<>();

    boolean enqueue(String id) {
        if (id == null || id.isEmpty() || links.size() >= LIMIT || links.containsKey(id)) return false;
        links.put(id, State.QUEUED);
        return true;
    }
    String next() {
        if (links.containsValue(State.OPENING)) return null;
        for (Map.Entry<String, State> entry : links.entrySet()) {
            if (entry.getValue() == State.QUEUED) {
                entry.setValue(State.OPENING);
                return entry.getKey();
            }
        }
        return null;
    }
    boolean ready(String id) {
        if (links.get(id) != State.OPENING) return false;
        links.put(id, State.READY);
        return true;
    }
    boolean defer(String id) {
        State current = links.get(id);
        if (current != State.OPENING && current != State.READY) return false;
        links.put(id, State.BACKOFF);
        return true;
    }
    boolean requeue(String id) {
        if (links.get(id) != State.BACKOFF) return false;
        // Retry goes to the tail, so it cannot monopolize the handshake slot.
        links.remove(id);
        links.put(id, State.QUEUED);
        return true;
    }
    State state(String id) { return links.get(id); }
    boolean remove(String id) { return links.remove(id) != null; }
    List<String> ids() { return new ArrayList<>(links.keySet()); }
    void clear() { links.clear(); }
    int count(State state) {
        int n = 0;
        for (State value : links.values()) if (value == state) n++;
        return n;
    }
}
