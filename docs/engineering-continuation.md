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
