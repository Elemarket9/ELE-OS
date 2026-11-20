// SPDX-License-Identifier: Apache-2.0

#include "faith.h"

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <limits>
#include <map>

namespace ele::faith {
namespace {

std::string Trim(const std::string& text) {
    size_t begin = 0;
    while (begin < text.size() &&
           (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\n' || text[begin] == '\r')) {
        ++begin;
    }
    size_t end = text.size();
    while (end > begin &&
           (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\n' || text[end - 1] == '\r')) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool IsDigit(char c) { return c >= '0' && c <= '9'; }
bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

bool ValidNumber(const std::string& number) {
    if (number.size() < 3 || number.size() > 20) return false;
    int digits = 0;
    for (unsigned char c : number) {
        if (IsDigit(static_cast<char>(c))) {
            ++digits;
            continue;
        }
        if (c != '+' && c != '*' && c != '#' && c != '-') return false;
    }
    return digits >= 3;
}

bool ValidText(const std::string& text, size_t max_bytes) {
    if (text.empty() || text.size() > max_bytes) return false;
    for (unsigned char c : text) {
        if (c < 0x20 || c == 0x7f) return false;
    }
    return true;
}

bool ValidPackage(const std::string& name) {
    if (name.size() < 3 || name.size() > 200) return false;
    int dots = 0;
    size_t segment = 0;
    for (unsigned char c : name) {
        if (c == '.') {
            if (segment == 0) return false;
            ++dots;
            segment = 0;
            continue;
        }
        if (segment == 0 && !IsAlpha(static_cast<char>(c))) return false;
        if (!IsAlpha(static_cast<char>(c)) && !IsDigit(static_cast<char>(c)) && c != '_') return false;
        ++segment;
    }
    return dots >= 1 && segment > 0;
}

struct Object {
    std::map<std::string, std::string> strs;
    std::map<std::string, int64_t> ints;
};

class Parser {
public:
    explicit Parser(const std::string& in) : in_(in) {}

    bool Parse(Object& out) {
        Skip();
        if (!Consume('{')) return false;
        Skip();
        if (Peek('}')) {
            ++i_;
            Skip();
            return i_ == in_.size();
        }
        int keys = 0;
        while (true) {
            if (++keys > 16) return false;
            std::string key;
            if (!ParseString(key)) return false;
            if (out.strs.count(key) || out.ints.count(key)) return false;
            if (!Consume(':')) return false;
            if (!ParseValue(out, key)) return false;
            Skip();
            if (Peek(',')) {
                ++i_;
                Skip();
                continue;
            }
            if (Peek('}')) {
                ++i_;
                Skip();
                return i_ == in_.size();
            }
            return false;
        }
    }

private:
    void Skip() {
        while (i_ < in_.size() &&
               (in_[i_] == ' ' || in_[i_] == '\t' || in_[i_] == '\n' || in_[i_] == '\r')) {
            ++i_;
        }
    }

    bool Peek(char c) const { return i_ < in_.size() && in_[i_] == c; }

    bool Consume(char c) {
        Skip();
        if (!Peek(c)) return false;
        ++i_;
        return true;
    }

    bool ParseString(std::string& out) {
        Skip();
        if (!Peek('"')) return false;
        ++i_;
        out.clear();
        while (i_ < in_.size()) {
            unsigned char c = static_cast<unsigned char>(in_[i_++]);
            if (c == '"') return out.size() <= 2000;
            if (c == '\\') {
                if (i_ >= in_.size()) return false;
                char decoded = 0;
                if (!DecodeEscape(decoded)) return false;
                out.push_back(decoded);
            } else if (c < 0x20 || c == 0x7f) {
                return false;
            } else {
                out.push_back(static_cast<char>(c));
            }
            if (out.size() > 2000) return false;
        }
        return false;
    }

    bool DecodeEscape(char& decoded) {
        char e = in_[i_++];
        switch (e) {
            case '"':
            case '\\':
            case '/':
                decoded = e;
                return true;
            case 'n':
                decoded = '\n';
                return true;
            case 't':
                decoded = '\t';
                return true;
            case 'r':
                decoded = '\r';
                return true;
            case 'u':
                return DecodeUnicode(decoded);
            default:
                return false;
        }
    }

    bool DecodeUnicode(char& decoded) {
        if (i_ + 4 > in_.size()) return false;
        int code = 0;
        for (int n = 0; n < 4; ++n) {
            unsigned char h = static_cast<unsigned char>(in_[i_++]);
            code <<= 4;
            if (h >= '0' && h <= '9') code += h - '0';
            else if (h >= 'a' && h <= 'f') code += h - 'a' + 10;
            else if (h >= 'A' && h <= 'F') code += h - 'A' + 10;
            else return false;
        }
        if (code < 0x20 || code >= 0x7f) return false;
        decoded = static_cast<char>(code);
        return true;
    }

    bool ParseInt(int64_t& out) {
        Skip();
        if (i_ >= in_.size()) return false;
        bool neg = false;
        if (in_[i_] == '-') {
            neg = true;
            ++i_;
        }
        if (i_ >= in_.size() || !IsDigit(in_[i_])) return false;
        if (in_[i_] == '0' && i_ + 1 < in_.size() && IsDigit(in_[i_ + 1])) return false;
        int64_t value = 0;
        while (i_ < in_.size() && IsDigit(in_[i_])) {
            int digit = in_[i_] - '0';
            if (value > (std::numeric_limits<int64_t>::max() - digit) / 10) return false;
            value = value * 10 + digit;
            ++i_;
        }
        out = neg ? -value : value;
        return true;
    }

    bool ParseValue(Object& obj, const std::string& key) {
        Skip();
        if (i_ >= in_.size()) return false;
        if (in_[i_] == '"') {
            std::string value;
            if (!ParseString(value)) return false;
            obj.strs.emplace(key, std::move(value));
            return true;
        }
        if (in_[i_] == '-' || IsDigit(in_[i_])) {
            int64_t value = 0;
            if (!ParseInt(value)) return false;
            obj.ints.emplace(key, value);
            return true;
        }
        return false;
    }

    const std::string& in_;
    size_t i_ = 0;
};

bool OnlyKeys(const Object& object, std::initializer_list<const char*> allowed) {
    auto known = [&](const std::string& key) {
        for (const char* name : allowed) {
            if (key == name) return true;
        }
        return false;
    };
    for (const auto& entry : object.strs) {
        if (!known(entry.first)) return false;
    }
    for (const auto& entry : object.ints) {
        if (!known(entry.first)) return false;
    }
    return true;
}

const std::string* FindStr(const Object& object, const char* key) {
    auto it = object.strs.find(key);
    return it == object.strs.end() ? nullptr : &it->second;
}

const int64_t* FindInt(const Object& object, const char* key) {
    auto it = object.ints.find(key);
    return it == object.ints.end() ? nullptr : &it->second;
}

void AddExtra(Intent& intent, const char* key, const std::string& value) {
    intent.extras.emplace_back(key, value);
}

bool FillCall(const Object& object, Intent& intent) {
    if (!OnlyKeys(object, {"tool", "number"})) return false;
    const std::string* number = FindStr(object, "number");
    if (!number || !ValidNumber(*number)) return false;
    intent.category = ToolCategory::Call;
    intent.action = "android.intent.action.CALL";
    intent.data_uri = "tel:" + *number;
    return true;
}

bool FillMessage(const Object& object, Intent& intent) {
    if (!OnlyKeys(object, {"tool", "number", "body"})) return false;
    const std::string* number = FindStr(object, "number");
    const std::string* body = FindStr(object, "body");
    if (!number || !body || !ValidNumber(*number) || !ValidText(*body, 500)) return false;
    intent.category = ToolCategory::Message;
    intent.action = "android.intent.action.SENDTO";
    intent.data_uri = "sms:" + *number;
    AddExtra(intent, "sms_body", *body);
    return true;
}

bool FillAlarmSet(const Object& object, Intent& intent) {
    if (!OnlyKeys(object, {"tool", "hour", "minute", "label"})) return false;
    const int64_t* hour = FindInt(object, "hour");
    const int64_t* minute = FindInt(object, "minute");
    const std::string* label = FindStr(object, "label");
    if (!hour || !minute || *hour < 0 || *hour > 23 || *minute < 0 || *minute > 59) return false;
    if (label && !ValidText(*label, 80)) return false;
    intent.category = ToolCategory::Alarm;
    intent.action = "android.intent.action.SET_ALARM";
    AddExtra(intent, "android.intent.extra.alarm.HOUR", std::to_string(*hour));
    AddExtra(intent, "android.intent.extra.alarm.MINUTES", std::to_string(*minute));
    if (label) AddExtra(intent, "android.intent.extra.alarm.MESSAGE", *label);
    AddExtra(intent, "android.intent.extra.alarm.SKIP_UI", "false");
    return true;
}

bool FillAlarmCancel(const Object& object, Intent& intent) {
    if (!OnlyKeys(object, {"tool", "hour", "minute", "label"})) return false;
    const int64_t* hour = FindInt(object, "hour");
    const int64_t* minute = FindInt(object, "minute");
    const std::string* label = FindStr(object, "label");
    if ((hour == nullptr) != (minute == nullptr)) return false;
    if ((label == nullptr) == (hour == nullptr)) return false;
    if (label && !ValidText(*label, 80)) return false;
    if (hour && (!minute || *hour < 0 || *hour > 23 || *minute < 0 || *minute > 59)) return false;
    intent.category = ToolCategory::Alarm;
    intent.action = "android.intent.action.DISMISS_ALARM";
    if (label) {
        AddExtra(intent, "android.intent.extra.alarm.SEARCH_MODE", "android.label");
        AddExtra(intent, "android.intent.extra.alarm.MESSAGE", *label);
    } else {
        AddExtra(intent, "android.intent.extra.alarm.SEARCH_MODE", "android.time");
        AddExtra(intent, "android.intent.extra.alarm.HOUR", std::to_string(*hour));
        AddExtra(intent, "android.intent.extra.alarm.MINUTES", std::to_string(*minute));
    }
    return true;
}

const char* SettingAction(const std::string& screen) {
    if (screen == "wifi") return "android.settings.WIFI_SETTINGS";
    if (screen == "bluetooth") return "android.settings.BLUETOOTH_SETTINGS";
    if (screen == "sound") return "android.settings.SOUND_SETTINGS";
    if (screen == "display") return "android.settings.DISPLAY_SETTINGS";
    if (screen == "airplane") return "android.settings.AIRPLANE_MODE_SETTINGS";
    return nullptr;
}

bool FillSetting(const Object& object, Intent& intent) {
    if (!OnlyKeys(object, {"tool", "screen"})) return false;
    const std::string* screen = FindStr(object, "screen");
    const char* action = screen ? SettingAction(*screen) : nullptr;
    if (!action) return false;
    intent.category = ToolCategory::Setting;
    intent.action = action;
    return true;
}

bool FillLaunch(const Object& object, Intent& intent) {
    if (!OnlyKeys(object, {"tool", "package"})) return false;
    const std::string* package = FindStr(object, "package");
    if (!package || !ValidPackage(*package)) return false;
    intent.category = ToolCategory::AppLaunch;
    intent.action = "android.intent.action.MAIN";
    intent.package_name = *package;
    intent.categories.push_back("android.intent.category.LAUNCHER");
    return true;
}

bool KnownTool(const std::string& tool, ToolCategory& category) {
    if (tool == "call.place") {
        category = ToolCategory::Call;
        return true;
    }
    if (tool == "message.send") {
        category = ToolCategory::Message;
        return true;
    }
    if (tool == "alarm.set" || tool == "alarm.cancel") {
        category = ToolCategory::Alarm;
        return true;
    }
    if (tool == "setting.open") {
        category = ToolCategory::Setting;
        return true;
    }
    if (tool == "app.launch") {
        category = ToolCategory::AppLaunch;
        return true;
    }
    return false;
}

bool FillIntent(const Object& object, const std::string& tool, Intent& intent) {
    if (tool == "call.place") return FillCall(object, intent);
    if (tool == "message.send") return FillMessage(object, intent);
    if (tool == "alarm.set") return FillAlarmSet(object, intent);
    if (tool == "alarm.cancel") return FillAlarmCancel(object, intent);
    if (tool == "setting.open") return FillSetting(object, intent);
    if (tool == "app.launch") return FillLaunch(object, intent);
    return false;
}

std::string JsonString(const std::string& text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[c >> 4]);
                    out.push_back(hex[c & 0xf]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
    return out;
}

bool HostEndsWith(const std::string& host, const char* suffix) {
    const size_t n = std::strlen(suffix);
    if (host.size() < n) return false;
    if (host.compare(host.size() - n, n, suffix) != 0) return false;
    return host.size() == n || host[host.size() - n - 1] == '.';
}

bool RefusedHost(const std::string& host) {
    return HostEndsWith(host, "google.com") || HostEndsWith(host, "googleapis.com") ||
           HostEndsWith(host, "gstatic.com") || HostEndsWith(host, "google.dev");
}

bool ValidHost(const std::string& host) {
    if (host.empty() || host.size() > 253 || host.front() == '.' || host.back() == '.') return false;
    bool label = false;
    for (size_t i = 0; i < host.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(host[i]);
        if (c == '.') {
            if (!label) return false;
            label = false;
            continue;
        }
        if (!IsAlpha(static_cast<char>(c)) && !IsDigit(static_cast<char>(c)) && c != '-') return false;
        label = true;
    }
    return label;
}

bool ParseHttpsHost(const std::string& url, std::string& host) {
    const std::string prefix = "https://";
    if (url.compare(0, prefix.size(), prefix) != 0) return false;
    if (url.find('@') != std::string::npos) return false;
    if (url.find(' ') != std::string::npos || url.find('\n') != std::string::npos) return false;
    std::string rest = url.substr(prefix.size());
    if (rest.empty()) return false;
    auto cut = rest.find_first_of("/?#");
    std::string authority = cut == std::string::npos ? rest : rest.substr(0, cut);
    auto colon = authority.find(':');
    if (colon != std::string::npos) {
        host = authority.substr(0, colon);
        std::string port = authority.substr(colon + 1);
        if (port.empty() || port.find_first_not_of("0123456789") != std::string::npos) return false;
    } else {
        host = authority;
    }
    for (char& c : host) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return ValidHost(host);
}

bool ValidModelName(const std::string& model) {
    if (model.empty() || model.size() > 128) return false;
    for (unsigned char c : model) {
        if (c < 0x20 || c == 0x7f) return false;
    }
    return true;
}

bool ValidApiKey(const std::string& key) {
    for (unsigned char c : key) {
        if (c <= 0x20 || c == 0x7f) return false;
    }
    return true;
}

const char* CloudMessage(CloudBuild build) {
    switch (build) {
        case CloudBuild::Disabled:
            return "cloud inference is off";
        case CloudBuild::MissingEndpoint:
            return "cloud endpoint is empty";
        case CloudBuild::MissingModel:
            return "cloud model name is empty or unusable";
        case CloudBuild::BadUrl:
            return "cloud endpoint must be an https URL with a host";
        case CloudBuild::RefusedHost:
            return "cloud endpoint host is refused";
        case CloudBuild::BadKey:
            return "cloud API key is not usable";
        case CloudBuild::Ok:
            return "";
    }
    return "cloud request was not built";
}

int64_t NowMs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

}  // namespace

bool Permissions::Allows(ToolCategory category) const {
    switch (category) {
        case ToolCategory::Call:
            return call;
        case ToolCategory::Message:
            return message;
        case ToolCategory::Alarm:
            return alarm;
        case ToolCategory::Setting:
            return setting;
        case ToolCategory::AppLaunch:
            return app_launch;
    }
    return false;
}

void Permissions::Set(ToolCategory category, bool enabled) {
    switch (category) {
        case ToolCategory::Call:
            call = enabled;
            break;
        case ToolCategory::Message:
            message = enabled;
            break;
        case ToolCategory::Alarm:
            alarm = enabled;
            break;
        case ToolCategory::Setting:
            setting = enabled;
            break;
        case ToolCategory::AppLaunch:
            app_launch = enabled;
            break;
    }
}

const char* CategoryName(ToolCategory category) {
    switch (category) {
        case ToolCategory::Call:
            return "call";
        case ToolCategory::Message:
            return "message";
        case ToolCategory::Alarm:
            return "alarm";
        case ToolCategory::Setting:
            return "setting";
        case ToolCategory::AppLaunch:
            return "app";
    }
    return "";
}

bool CategoryFromName(const std::string& name, ToolCategory& category) {
    if (name == "call") {
        category = ToolCategory::Call;
        return true;
    }
    if (name == "message") {
        category = ToolCategory::Message;
        return true;
    }
    if (name == "alarm") {
        category = ToolCategory::Alarm;
        return true;
    }
    if (name == "setting") {
        category = ToolCategory::Setting;
        return true;
    }
    if (name == "app") {
        category = ToolCategory::AppLaunch;
        return true;
    }
    return false;
}

ToolDecision DecideTool(const std::string& model_text, const Permissions& permissions) {
    ToolDecision decision;
    const std::string text = Trim(model_text);
    if (text.empty() || text.front() != '{') {
        decision.outcome = ToolOutcome::NotATool;
        return decision;
    }
    Object object;
    Parser parser(text);
    if (!parser.Parse(object)) {
        decision.outcome = ToolOutcome::RefusedInvalid;
        decision.reason = "unreadable tool call";
        return decision;
    }
    const std::string* tool = FindStr(object, "tool");
    if (!tool) {
        decision.outcome = ToolOutcome::RefusedInvalid;
        decision.reason = "invalid tool";
        return decision;
    }
    decision.tool = *tool;
    ToolCategory category = ToolCategory::Call;
    if (!KnownTool(*tool, category)) {
        decision.outcome = ToolOutcome::RefusedInvalid;
        decision.reason = "invalid tool";
        return decision;
    }
    decision.has_category = true;
    decision.category = category;
    Intent intent;
    if (!FillIntent(object, *tool, intent)) {
        decision.outcome = ToolOutcome::RefusedInvalid;
        decision.reason = "invalid arguments";
        return decision;
    }
    if (!permissions.Allows(category)) {
        decision.outcome = ToolOutcome::RefusedDisabled;
        decision.reason = "disabled";
        return decision;
    }
    decision.outcome = ToolOutcome::Emitted;
    decision.intent = std::move(intent);
    return decision;
}

WeightCheck CheckWeightBudget(const WeightBudget& budget) {
    if (budget.memory_budget_bytes == 0) return WeightCheck::BudgetUnset;
    if (budget.weight_file_bytes == 0) return WeightCheck::SizeUnknown;
    if (budget.weight_file_bytes > budget.memory_budget_bytes) return WeightCheck::FileExceedsBudget;
    return WeightCheck::FileFitsInBudget;
}

CloudBuild BuildCloudRequest(const CloudConfig& config, const std::string& prompt,
                             const std::string& api_key, CloudRequest& request) {
    request = CloudRequest{};
    if (!config.enabled) return CloudBuild::Disabled;
    if (config.endpoint.empty()) return CloudBuild::MissingEndpoint;
    if (!ValidModelName(config.model)) return CloudBuild::MissingModel;
    std::string host;
    if (!ParseHttpsHost(config.endpoint, host)) return CloudBuild::BadUrl;
    if (RefusedHost(host)) return CloudBuild::RefusedHost;
    if (!ValidApiKey(api_key)) return CloudBuild::BadKey;
    request.url = config.endpoint;
    request.body = "{\"prompt\":" + JsonString(prompt) + ",\"model\":" + JsonString(config.model) + "}";
    request.headers.emplace_back("Content-Type", "application/json");
    if (!api_key.empty()) request.headers.emplace_back("Authorization", "Bearer " + api_key);
    return CloudBuild::Ok;
}

Assistant::Assistant(Runtime* runtime, CloudTransport* cloud) : runtime_(runtime), cloud_(cloud) {}

void Assistant::set_permissions(Permissions permissions) { permissions_ = permissions; }

void Assistant::set_cloud(CloudConfig config, std::string api_key) {
    cloud_config_ = std::move(config);
    api_key_ = std::move(api_key);
}

void Assistant::set_weight_budget(WeightBudget budget) {
    budget_ = budget;
    budget_set_ = true;
}

Reply Assistant::Handle(const std::string& prompt) const {
    Reply reply;
    if (Trim(prompt).empty()) {
        reply.kind = ReplyKind::Error;
        reply.text = "empty prompt";
        return reply;
    }
    if (prompt.size() > 16384) {
        reply.kind = ReplyKind::Error;
        reply.text = "prompt too large";
        return reply;
    }

    const bool local = runtime_ && runtime_->available();
    if (local && budget_set_ && CheckWeightBudget(budget_) == WeightCheck::FileExceedsBudget) {
        reply.kind = ReplyKind::Unavailable;
        reply.text = "local weight file exceeds the configured memory budget; runtime was not started";
        return reply;
    }

    std::string generated;
    if (local) {
        std::string error;
        if (!runtime_->complete(prompt, generated, error)) {
            reply.kind = ReplyKind::Error;
            reply.text = error.empty() ? "runtime failed" : error;
            return reply;
        }
    } else if (cloud_config_.enabled) {
        CloudRequest request;
        CloudBuild built = BuildCloudRequest(cloud_config_, prompt, api_key_, request);
        if (built != CloudBuild::Ok) {
            reply.kind = ReplyKind::Error;
            reply.text = CloudMessage(built);
            return reply;
        }
        if (!cloud_) {
            reply.kind = ReplyKind::Error;
            reply.text = "cloud transport is not included; no request was sent";
            return reply;
        }
        std::string error;
        if (!cloud_->send(request, generated, error)) {
            reply.kind = ReplyKind::Error;
            reply.text = error.empty() ? "cloud transport failed" : error;
            return reply;
        }
    } else {
        reply.kind = ReplyKind::Unavailable;
        reply.text = "No local runtime is configured, and cloud inference is off.";
        return reply;
    }

    if (Trim(generated).empty()) {
        reply.kind = ReplyKind::Error;
        reply.text = "empty model reply";
        return reply;
    }
    ToolDecision decision = DecideTool(generated, permissions_);
    if (decision.outcome == ToolOutcome::NotATool) {
        reply.kind = ReplyKind::Text;
        reply.text = Trim(generated);
        return reply;
    }
    reply.tool = std::move(decision);
    reply.kind = reply.tool.outcome == ToolOutcome::Emitted ? ReplyKind::ToolEmitted : ReplyKind::ToolRefused;
    reply.text = reply.tool.reason;
    return reply;
}

ProcessRuntime::ProcessRuntime(std::string executable, int timeout_ms)
    : executable_(std::move(executable)), timeout_ms_(timeout_ms) {}

bool ProcessRuntime::available() const {
    if (executable_.empty()) return false;
    struct stat st {};
    return stat(executable_.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
           access(executable_.c_str(), X_OK) == 0;
}

bool ProcessRuntime::complete(const std::string& prompt, std::string& out, std::string& error) {
    out.clear();
    error.clear();
    if (!available()) {
        error = "runtime is not available";
        return false;
    }
    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};
    if (pipe(in_pipe) != 0) {
        error = "failed to start runtime";
        return false;
    }
    if (pipe(out_pipe) != 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        error = "failed to start runtime";
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        error = "failed to start runtime";
        return false;
    }
    if (pid == 0) {
        if (dup2(in_pipe[0], STDIN_FILENO) < 0 || dup2(out_pipe[1], STDOUT_FILENO) < 0) _exit(127);
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        execl(executable_.c_str(), executable_.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);
    sigset_t block, previous;
    sigemptyset(&block);
    sigaddset(&block, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &block, &previous);
    size_t written = 0;
    while (written < prompt.size()) {
        ssize_t n = write(in_pipe[1], prompt.data() + written, prompt.size() - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        written += static_cast<size_t>(n);
    }
    close(in_pipe[1]);
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);

    fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    std::string collected;
    bool kill_child = false;
    const int64_t deadline = NowMs() + timeout_ms_;
    while (collected.size() <= 65536) {
        int wait_ms = static_cast<int>(deadline - NowMs());
        if (wait_ms <= 0) {
            kill_child = true;
            error = "runtime timed out";
            break;
        }
        pollfd pfd {};
        pfd.fd = out_pipe[0];
        pfd.events = POLLIN;
        int polled = poll(&pfd, 1, wait_ms);
        if (polled < 0 && errno == EINTR) continue;
        if (polled == 0) {
            kill_child = true;
            error = "runtime timed out";
            break;
        }
        char buffer[4096];
        ssize_t n = read(out_pipe[0], buffer, sizeof buffer);
        if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (n <= 0) break;
        collected.append(buffer, static_cast<size_t>(n));
        if (collected.size() > 65536) {
            kill_child = true;
            error = "runtime output exceeded the limit";
            break;
        }
    }
    close(out_pipe[0]);
    if (kill_child) kill(pid, SIGKILL);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            error = "failed to wait for runtime";
            return false;
        }
    }
    if (!error.empty()) return false;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        error = "runtime exited with status " + std::to_string(code);
        return false;
    }
    out = std::move(collected);
    return true;
}

}  // namespace ele::faith
