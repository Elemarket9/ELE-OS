// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;
public final class LinkQueueTest {
    static void check(boolean value) { if (!value) throw new AssertionError("link queue invariant"); }
    public static void main(String[] args) {
        LinkQueue q = new LinkQueue();
        check(!q.enqueue(null) && !q.enqueue(""));
        for (int i = 0; i < LinkQueue.LIMIT; i++) check(q.enqueue("peer" + i));
        check(!q.enqueue("peer0") && !q.enqueue("extra"));
        check(!q.ready("peer1"));
        check("peer0".equals(q.next()) && q.next() == null);
        check(q.ready("peer0") && !q.ready("peer0"));
        check("peer1".equals(q.next()));
        check(q.remove("peer1") && !q.ready("peer1"));
        check(q.enqueue("extra"));
        for (int i = 2; i < LinkQueue.LIMIT; i++) {
            check(("peer" + i).equals(q.next()));
            check(q.ready("peer" + i));
        }
        check("extra".equals(q.next()) && q.ready("extra"));
        check(q.count(LinkQueue.State.READY) == LinkQueue.LIMIT);
        check(q.next() == null && !q.remove("missing"));
        q.clear();
        check(q.ids().isEmpty() && q.next() == null);
        check(q.enqueue("a") && q.enqueue("b"));
        check("a".equals(q.next()) && q.defer("a"));
        check("b".equals(q.next()) && q.ready("b"));
        check(q.count(LinkQueue.State.BACKOFF)==1 && q.requeue("a"));
        check("a".equals(q.next()) && q.ready("a") && q.defer("a"));
        check(q.remove("a") && !q.requeue("a"));
        RetryBudget retry = new RetryBudget();
        check(retry.nextDelay(0)==1000);
        check(retry.nextDelay(-1)==2250);
        check(retry.nextDelay(0)==4000);
        check(retry.nextDelay(0)==-1 && retry.nextDelay(0)==-1);
        System.out.println("BLE admission and recovery queue checks passed");
    }
}
