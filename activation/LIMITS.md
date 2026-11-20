# Owner recovery prototype limits

This directory is a host prototype under the Apache-2.0 license. Nothing here
has locked, wiped, or located a phone. Cuttlefish has not been booted for
these checks. The Treble GSI product is not in this tree.

## Owner key

The phone's owner holds an Ed25519 key. `GenerateOwnerKey` returns the seed
to the caller. The record stores the public key, not the seed. There is no
account, no project escrow, and no staff override.

Losing the key and then wiping the phone blocks setup only when the owner
opted into that strict record. `IssueRecord` writes nothing unless that
opt-in is passed. The warning is `StrictRecoveryWarning`.

The blob is versioned and signed by the owner key inside it. The signature
rejects a partial edit. It does not stop someone who can replace the whole
file, because the public key travels in the file. A bootloader or another
hardware-authenticated store has to bind the blob. This prototype does not
have that store.

A missing blob with no platform marker means "not enrolled," and fresh setup
is allowed. A missing blob when the marker is present, or any corrupt blob,
fails closed. That result is not a new phone. `FreshSetupAllowed` is false
for both enrolled and fail-closed.

Transfer (`ELE-OS/transfer/v1`) replaces the public key when the current
owner authorizes the new one. There is no function that clears ownership.
This record code does not remember transfer nonces. The caller has to reject
a replay before it rewrites the blob.

## Reset-surviving lock

A reset-surviving lock is not a brand feature and not a feature of the
shared targets.

`AssessResetSurvivingLock` returns not available for Cuttlefish and for a
Treble GSI even if every observation bit is set. Cuttlefish is a virtual
device. A GSI keeps the phone's bootloader and vendor image. No phone maker
has agreed to preserve an ELE-OS owner record or to refuse erase and reflash
while the bootloader is locked.

On a specific board the function still does not verify hardware. The result
stays not available unless the caller reports all four conditions below.
When all four are reported, the statement says this build has not verified
them:

1. The owner record is still present after a normal factory reset of userdata.
2. Every setup path on a user build checks it. A userdebug image with root
   does not count.
3. The bootloader is locked to a root of trust that verifies this OS.
4. While locked, the bootloader refuses to erase the owner record and refuses
   to flash a different OS.

No board has been shown to meet that list. Do not tell an owner that a wipe
leaves the phone locked to their key.

## What the host fixture is

`tests/owner_record.cpp` writes a blob outside a temporary userdata directory
and deletes that directory. The blob is still readable. That is a filesystem
fixture. It is not a factory reset, not Cuttlefish, and not evidence for the
four conditions above.

This change does not send a location, lock a screen, or erase a device.
