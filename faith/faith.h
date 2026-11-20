// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Host prototype of the Faith assistant. Image targets are the Cuttlefish
// product and a Treble GSI. This library does not read a brand, a
// manufacturer, or a device codename, and it does not fire Android intents.

namespace ele::faith {

enum class ToolCategory { Call, Message, Alarm, Setting, AppLaunch };

struct Permissions {
    bool call = false;
    bool message = false;
    bool alarm = false;
    bool setting = false;
    bool app_launch = false;

    bool Allows(ToolCategory category) const;
    void Set(ToolCategory category, bool enabled);
};

struct Intent {
    ToolCategory category = ToolCategory::Call;
    std::string action;
    std::string data_uri;
    std::string package_name;
    std::vector<std::string> categories;
    std::vector<std::pair<std::string, std::string>> extras;
};

enum class ToolOutcome { NotATool, Emitted, RefusedDisabled, RefusedInvalid };

struct ToolDecision {
    ToolOutcome outcome = ToolOutcome::NotATool;
    bool has_category = false;
    ToolCategory category = ToolCategory::Call;
    std::string tool;
    std::string reason;
    Intent intent;
};

// A tool call is one JSON object and nothing else. Any other text is not a tool.
ToolDecision DecideTool(const std::string& model_text, const Permissions& permissions);

const char* CategoryName(ToolCategory category);
bool CategoryFromName(const std::string& name, ToolCategory& category);

enum class WeightCheck { BudgetUnset, SizeUnknown, FileFitsInBudget, FileExceedsBudget };

struct WeightBudget {
    uint64_t memory_budget_bytes = 0;
    uint64_t weight_file_bytes = 0;
};

// Compares the weight file's size with a budget. This is not a resident-set measurement.
WeightCheck CheckWeightBudget(const WeightBudget& budget);

enum class CloudBuild {
    Ok,
    Disabled,
    MissingEndpoint,
    MissingModel,
    BadUrl,
    RefusedHost,
    BadKey,
};

struct CloudConfig {
    bool enabled = false;
    std::string endpoint;
    std::string model;
};

struct CloudRequest {
    std::string url;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
};

// Builds a text-only request. No provider is named. Google inference hosts are refused.
// The API key, when set, is placed only in an Authorization header.
CloudBuild BuildCloudRequest(const CloudConfig& config, const std::string& prompt,
                             const std::string& api_key, CloudRequest& request);

class Runtime {
public:
    virtual ~Runtime() = default;
    virtual bool available() const = 0;
    virtual bool complete(const std::string& prompt, std::string& out, std::string& error) = 0;
};

class CloudTransport {
public:
    virtual ~CloudTransport() = default;
    virtual bool send(const CloudRequest& request, std::string& response, std::string& error) = 0;
};

enum class ReplyKind { Text, ToolEmitted, ToolRefused, Unavailable, Error };

struct Reply {
    ReplyKind kind = ReplyKind::Unavailable;
    std::string text;
    ToolDecision tool;
};

class Assistant {
public:
    Assistant(Runtime* runtime, CloudTransport* cloud);

    void set_permissions(Permissions permissions);
    void set_cloud(CloudConfig config, std::string api_key);
    void set_weight_budget(WeightBudget budget);
    Reply Handle(const std::string& prompt) const;

private:
    Runtime* runtime_;
    CloudTransport* cloud_;
    Permissions permissions_{};
    CloudConfig cloud_config_{};
    std::string api_key_;
    WeightBudget budget_{};
    bool budget_set_ = false;
};

// Runs an executable. The prompt is the entire stdin. The reply is stdout.
// The executable is not a shell command. The caller keeps the path alive.
class ProcessRuntime : public Runtime {
public:
    ProcessRuntime(std::string executable, int timeout_ms);
    bool available() const override;
    bool complete(const std::string& prompt, std::string& out, std::string& error) override;

private:
    std::string executable_;
    int timeout_ms_;
};

}  // namespace ele::faith
