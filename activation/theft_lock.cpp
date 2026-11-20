// SPDX-License-Identifier: Apache-2.0
#include "theft_lock.h"

namespace ele {
namespace {
const char* kCuttlefish =
    "Cuttlefish is a virtual device. A file kept beside a wiped directory is a "
    "host fixture, not a reset-surviving lock.";
const char* kGsi =
    "A Treble GSI uses the phone's own bootloader and vendor image. No phone "
    "maker has agreed to keep an ELE-OS owner record or to block erase and "
    "reflash while locked. The GSI does not provide a reset-surviving lock.";
const char* kReported =
    "Four hardware conditions were reported for one board. This build has not "
    "verified them. Do not tell the owner the lock survives a reset until an "
    "evidence note records the wipe, the user-build setup gate, the locked "
    "bootloader, and the refused erase and reflash.";
const char* kWipe =
    "The owner record has not been shown to survive a factory reset of userdata.";
const char* kUser =
    "A user build that checks every setup path has not been shown. A userdebug "
    "build with root does not qualify.";
const char* kBoot =
    "The bootloader has not been shown locked to a root of trust that verifies this OS.";
const char* kFastboot =
    "While locked, the bootloader has not been shown to refuse erasing the owner "
    "record and flashing another OS.";
}

LockAssessment AssessResetSurvivingLock(BoardObservations observed, BoardClass board) {
    if (board == BoardClass::Cuttlefish) return {ResetSurvivingLock::NotAvailable, kCuttlefish};
    if (board == BoardClass::TrebleGsi) return {ResetSurvivingLock::NotAvailable, kGsi};
    if (!observed.record_survives_userdata_wipe) return {ResetSurvivingLock::NotAvailable, kWipe};
    if (!observed.user_build_checks_every_setup_path) return {ResetSurvivingLock::NotAvailable, kUser};
    if (!observed.bootloader_locked_to_this_os) return {ResetSurvivingLock::NotAvailable, kBoot};
    if (!observed.locked_bootloader_rejects_erase_and_reflash) {
        return {ResetSurvivingLock::NotAvailable, kFastboot};
    }
    return {ResetSurvivingLock::ReportedByCaller, kReported};
}

BoardObservations CuttlefishObservations() { return {}; }
BoardObservations TrebleGsiObservations() { return {}; }
}
