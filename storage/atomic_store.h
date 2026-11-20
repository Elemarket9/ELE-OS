// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace eleos::storage {
constexpr std::size_t kMaxBytes = 16 * 1024 * 1024;
enum class Result { Ok, Rejected, DurabilityUncertain };
using Bytes = std::vector<std::uint8_t>;
// Directory must be a private directory owned by the calling effective UID.
// A name is a simple component. Writes are atomic, last successful rename wins.
// SHA-256 detects corruption; it is not authenticity against the directory owner.
Result Put(const std::string& directory, const std::string& name, const Bytes& data);
Result Get(const std::string& directory, const std::string& name, Bytes& data);
bool ReadBounded(int fd, Bytes& data);
bool WriteAll(int fd, const std::uint8_t* data, std::size_t len);
}
