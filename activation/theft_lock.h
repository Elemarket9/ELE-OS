// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace ele {

// Observations the caller claims for one board. This library does not read
// a bootloader, a partition, or a phone. Setting a bit without an evidence
// note is a false claim by the caller.
struct BoardObservations {
    bool record_survives_userdata_wipe = false;
    bool user_build_checks_every_setup_path = false;
    bool bootloader_locked_to_this_os = false;
    bool locked_bootloader_rejects_erase_and_reflash = false;
};

enum class BoardClass { Specific, Cuttlefish, TrebleGsi };
enum class ResetSurvivingLock { NotAvailable, ReportedByCaller };

struct LockAssessment {
    ResetSurvivingLock result;
    // Static text for a device page. It does not name a phone brand.
    const char* statement;
};

// Cuttlefish and a Treble GSI are not available, even if every bit is set.
// No phone maker has agreed to provide the four conditions.
LockAssessment AssessResetSurvivingLock(BoardObservations observed, BoardClass board);

BoardObservations CuttlefishObservations();
BoardObservations TrebleGsiObservations();
}
