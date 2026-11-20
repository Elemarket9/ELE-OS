# Engineering continuation

The owner authorized all 350 capabilities and verified merges. Start from
the registry and original specification, not a newly invented ID list.
The registry uses conservative states: existing upstream functionality and
unmerged PRs are not automatically ELE-OS completion evidence. Update entries
with exact source, dependencies, integration and verification as work lands.

The resource sampling command is read-only and bounded; it is installed in
the Cuttlefish product source configuration. Run `/product/bin/ele-resource`
after a genuine build. Unknown thermal authority always denies background
authorization. It does not change scheduling, device power or memory policy.
Next: privileged supported thermal-policy adapter, then a permitted system job
controller; never weaken SELinux or Android's scheduler/security controls.

Existing open PRs for product classes, OTA, Faith and recovery should be
reviewed in dependency order against latest main. Some are deliberately
draft or prototype-only. Do not merge a UI as a firewall or a host-only
cryptographic component as device recovery. Record CI and hardware evidence
separately. The baseline still requires a full pinned AOSP checkout and KVM.

The ARM64 Treble GSI and low-RAM product configurations are registered in source. The low-RAM class uses upstream lmkd mode and ART build flags, not a measured memory or battery improvement. Both low-RAM products inherit shared settings once. GSI deployment must establish which product-partition artifacts can actually be installed on the specific device. Do not flash these test-key targets on an owner's production device.

OTA: the test-key local verifier checks matching package versions, integrity and signatures; it deliberately does not install. It must not be connected to update_engine or trusted as a release client. The next integration requires signed AOSP target files, release trust anchors, and a genuine A/B update/recovery environment. Preserve descriptor-relative reads and resource caps.

External devices: the debug app uses Android public USB, audio and display inventories and Camera2 external preview, with English fallback/RTL resources. Descriptor classification is not compatibility certification. No USB device permission, raw device access, or audio recording is requested by inventory. Next: device-specific UVC and PTP/MTP transfer validation, explicit access grant and removal handling, and actual supported satellite link APIs.

Owner recovery: the existing Ed25519 challenge path now expires nonces after 120 seconds, rejects canceled or rotated proofs and enforces a 60-second cooldown after five failed signatures even across challenge rotation. The clock is monotonic and tests cover the boundary without sleeping. This is a host-verified in-memory mechanism, not device unlock enforcement; the trusted setup service must preserve a single authority, cancel sessions on suspend, enforce caller identity and implement durable hardware-backed retry limits. Do not merge the unsigned enrollment or handwritten curve arithmetic prototypes without replacing their unsafe assumptions and reviewing the cryptography.

Bluetooth: the debug External devices app now has a foreground BLE controller. User-approved nearby-device permission, 10-second local scan, explicit system pairing, and separately maintained GATT connections to bonded BLE/dual-mode accessories use public Android APIs. One handshake/service discovery at a time, up to eight requested links, 25-second timeout per opening and manual per-device disconnect/retry. Stop closes tool-owned links and clears scan results. System audio/HID profiles and radio coexistence are not replaced; no synchronized multi-speaker output or physical controller capacity is claimed. Next: physical two/three-device BLE and mixed audio/HID tests, reconnect policies, controller scheduling and supported LE Audio routing.

Networking: a foreground diagnostic screen reads public ConnectivityManager, LinkProperties and TrafficStats counters, labels default network transport/validation/captive portal/private DNS/MTU and calculates observed interface byte rates every two seconds. Resets baseline on interface or network change, counter reversal, long interval and unavailable data. No probing, no IP or DNS address logging, no network binding or firewall. TrafficStats is partial and cannot establish billing, maximum bandwidth or seamless handover. Next: authorized per-transport integration, policy-level routing, real mixed-link field testing and controller/backend recovery.

BLE recovery now offers a default-off owner opt-in for each new connection request: up to three retries at 1/2/4 second backoff plus bounded peer jitter. Retry timers are tied to request identity, use the same eight-link budget, rejoin at the end of FIFO and are canceled on close/permission or adapter loss/screen exit. No unlimited reconnect or background service. Physical transport reliability and mixed-profile arbitration still require supported hardware.

Storage: ele-store is a native debug record utility with bounded 16 MiB payloads, descriptor-relative private-file access, SHA-256 corruption detection, file fsync/atomic rename/directory fsync and explicit uncertain-durability status. Host tests exercise concurrent replacement, tampered records, oversize, symlinks and special files. Next: durable service integration, authorized stale temporary cleanup, power-cut failure injection on supported filesystems, and authenticated/encrypted backup manifests. It is not filesystem journaling or a vault.
