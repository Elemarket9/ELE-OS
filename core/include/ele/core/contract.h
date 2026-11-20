// SPDX-License-Identifier: Apache-2.0
// ELE-OS shared architectural contract, version 1.
//
// Every workstream library (kernel/, memory/, storage/, security/, power/,
// net/, graphics/, appcompat/, faith/, hal/, satellite/, distributed/,
// recovery/, media/, virt/, optimizer/) depends only on this header plus the
// C++17 standard library and, where cryptography is needed, OpenSSL/BoringSSL
// libcrypto. No workstream includes another workstream's private headers;
// cross-domain cooperation goes through the optimizer's Decision contract
// below.
#pragma once

#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ele::core {

inline constexpr unsigned kContractVersion = 1;

// ---------------------------------------------------------------- Status
enum class Code {
    kOk = 0,
    kInvalidArgument,
    kOutOfRange,
    kResourceExhausted,
    kPermissionDenied,
    kNotFound,
    kUnavailable,      // source missing / hardware absent; never silently zero
    kFailedPrecondition,
    kDataLoss,         // integrity failure
    kAborted,          // conflict, retry may succeed
    kInternal,
};

struct Status {
    Code code = Code::kOk;
    std::string message;
    bool ok() const { return code == Code::kOk; }
    static Status Ok() { return {}; }
    static Status Error(Code c, std::string m) { return {c, std::move(m)}; }
};

template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)) {}            // NOLINT
    Result(Status status) : status_(std::move(status)) {}    // NOLINT
    bool ok() const { return value_.has_value(); }
    const Status& status() const { return status_; }
    const T& value() const { return *value_; }
    T& value() { return *value_; }
    const T* operator->() const { return &*value_; }
    T* operator->() { return &*value_; }
private:
    std::optional<T> value_;
    Status status_{Code::kInternal, "no value"};
};

// ------------------------------------------------------- Checked arithmetic
// All counters, budgets and sizes go through these helpers so that overflow
// is reported instead of wrapping.
template <typename T>
inline bool CheckedAdd(T a, T b, T* out) {
    static_assert(std::numeric_limits<T>::is_integer, "integer only");
    return !__builtin_add_overflow(a, b, out);
}
template <typename T>
inline bool CheckedMul(T a, T b, T* out) {
    static_assert(std::numeric_limits<T>::is_integer, "integer only");
    return !__builtin_mul_overflow(a, b, out);
}
template <typename T>
inline T SaturatingAdd(T a, T b) {
    T out;
    return CheckedAdd(a, b, &out) ? out : std::numeric_limits<T>::max();
}
template <typename T>
inline T SaturatingSub(T a, T b) { return a > b ? T(a - b) : T(0); }

// ------------------------------------------------------------------ Clock
// Injectable monotonic clock so that algorithms are deterministic in tests
// and use CLOCK_MONOTONIC on devices.
class Clock {
public:
    virtual ~Clock() = default;
    virtual std::uint64_t NowUs() const = 0;
};
class SteadyClock final : public Clock {
public:
    std::uint64_t NowUs() const override {
        return std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
};
class ManualClock final : public Clock {
public:
    std::uint64_t NowUs() const override { return now_; }
    void Advance(std::uint64_t us) { now_ = SaturatingAdd(now_, us); }
    void Set(std::uint64_t us) { now_ = us; }
private:
    std::uint64_t now_ = 0;
};

// -------------------------------------------------------- System files
// Read/write kernel interfaces (/proc, /sys, cgroupfs) relative to a root so
// host tests run against fixture trees and devices use "/". Reads are bounded.
class SysFs {
public:
    explicit SysFs(std::string root = "") : root_(std::move(root)) {}
    std::string Path(const std::string& p) const { return root_ + p; }
    std::optional<std::string> Read(const std::string& p,
                                    std::size_t limit = 1 << 20) const {
        std::ifstream in(Path(p), std::ios::binary);
        if (!in) return std::nullopt;
        std::string out(limit, '\0');
        in.read(&out[0], std::streamsize(limit));
        out.resize(std::size_t(in.gcount()));
        return out;
    }
    std::optional<std::int64_t> ReadInt(const std::string& p) const {
        auto s = Read(p, 64);
        if (!s) return std::nullopt;
        std::istringstream is(*s);
        std::int64_t v;
        if (!(is >> v)) return std::nullopt;
        return v;
    }
    // Writes are explicit actuations. Callers must have passed policy checks.
    Status Write(const std::string& p, const std::string& value) const {
        std::ofstream out(Path(p), std::ios::binary | std::ios::trunc);
        if (!out) return Status::Error(Code::kUnavailable, "cannot open " + p);
        out << value;
        out.flush();
        return out ? Status::Ok() : Status::Error(Code::kInternal, "write " + p);
    }
private:
    std::string root_;
};

// ----------------------------------------------- Unified policy precedence
// Independently developed algorithms submit proposals; the optimizer
// arbitrates them in this fixed order. A lower-precedence proposal can never
// override a constraint from a higher one (e.g. performance cannot exceed a
// thermal limit, AI cannot relax a security rule).
enum class Authority : std::uint8_t {
    kHardwareSafety = 0,  // thermal trip, battery protection, brown-out
    kSecurity = 1,        // sandbox, permission, integrity enforcement
    kMemorySafety = 2,    // OOM avoidance, corruption prevention
    kOwner = 3,           // explicit owner preference
    kReliability = 4,     // recovery, health
    kEnergy = 5,          // battery budget
    kPerformance = 6,     // latency / throughput
    kIntelligence = 7,    // AI suggestions (advisory only)
};

// A bounded numeric knob (e.g. "cpu.max_freq_khz") with min/max constraints.
struct Proposal {
    std::string knob;
    Authority authority;
    std::optional<std::int64_t> min;  // inclusive lower bound demanded
    std::optional<std::int64_t> max;  // inclusive upper bound demanded
    std::optional<std::int64_t> preferred;
    std::string source;               // component name for audit
};

}  // namespace ele::core
