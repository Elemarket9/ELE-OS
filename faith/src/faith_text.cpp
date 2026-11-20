// SPDX-License-Identifier: Apache-2.0

#include "faith.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {

std::string Escape(const std::string& text) {
    std::string out;
    for (unsigned char c : text) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out.push_back(static_cast<char>(c));
    }
    return out;
}

bool ParseU64(const std::string& text, uint64_t& value) {
    if (text.empty()) return false;
    uint64_t out = 0;
    for (unsigned char c : text) {
        if (c < '0' || c > '9') return false;
        uint64_t digit = c - '0';
        if (out > (UINT64_MAX - digit) / 10) return false;
        out = out * 10 + digit;
    }
    value = out;
    return true;
}

void EnableList(const std::string& list, ele::faith::Permissions& permissions, bool& ok) {
    size_t start = 0;
    while (start <= list.size()) {
        size_t comma = list.find(',', start);
        std::string name = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        ele::faith::ToolCategory category;
        if (!ele::faith::CategoryFromName(name, category)) {
            ok = false;
            return;
        }
        permissions.Set(category, true);
        if (comma == std::string::npos) return;
        start = comma + 1;
    }
}

void PrintIntent(const ele::faith::Intent& intent) {
    std::printf("action=%s\n", intent.action.c_str());
    std::printf("data=%s\n", Escape(intent.data_uri).c_str());
    if (!intent.package_name.empty()) {
        std::printf("package=%s\n", Escape(intent.package_name).c_str());
    }
    for (const std::string& category : intent.categories) {
        std::printf("intent_category=%s\n", category.c_str());
    }
    for (const auto& extra : intent.extras) {
        std::printf("extra.%s=%s\n", extra.first.c_str(), Escape(extra.second).c_str());
    }
}

void Print(const ele::faith::Reply& reply) {
    using ele::faith::ReplyKind;
    switch (reply.kind) {
        case ReplyKind::Text:
            std::printf("kind=text\ntext=%s\n", Escape(reply.text).c_str());
            return;
        case ReplyKind::Unavailable:
            std::printf("kind=unavailable\ntext=%s\n", Escape(reply.text).c_str());
            return;
        case ReplyKind::Error:
            std::printf("kind=error\ntext=%s\n", Escape(reply.text).c_str());
            return;
        case ReplyKind::ToolEmitted:
        case ReplyKind::ToolRefused:
            break;
    }
    const bool emitted = reply.kind == ReplyKind::ToolEmitted;
    std::printf("kind=%s\n", emitted ? "emitted" : "refused");
    const char* category = reply.tool.has_category ? ele::faith::CategoryName(reply.tool.category) : "";
    std::printf("category=%s\n", category);
    std::printf("tool=%s\n", Escape(reply.tool.tool).c_str());
    if (!emitted) {
        std::printf("reason=%s\n", Escape(reply.tool.reason).c_str());
        return;
    }
    PrintIntent(reply.tool.intent);
}

}  // namespace

int main(int argc, char** argv) {
    std::fprintf(stderr,
                 "faith-text: prototype. Targets are the Cuttlefish product and a Treble GSI. "
                 "This process is neither. No speech and no wake phrase. "
                 "Cloud stays off unless enabled, and this binary does not send cloud requests.\n");

    ele::faith::Permissions permissions;
    ele::faith::CloudConfig cloud;
    std::string runtime_path;
    ele::faith::WeightBudget budget;
    bool saw_budget = false;
    bool saw_weight = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto need = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", flag);
                return nullptr;
            }
            return argv[++i];
        };
        if (arg == "--help") {
            std::fprintf(stderr,
                         "usage: faith-text [--runtime PATH] [--enable call,message,alarm,setting,app]\n"
                         "                  [--cloud-enable --cloud-endpoint URL --cloud-model NAME]\n"
                         "                  [--memory-budget-bytes N --weight-bytes N]\n");
            return 0;
        }
        if (arg == "--runtime") {
            const char* value = need("--runtime");
            if (!value) return 2;
            runtime_path = value;
            continue;
        }
        if (arg == "--enable") {
            const char* value = need("--enable");
            if (!value) return 2;
            bool ok = true;
            EnableList(value, permissions, ok);
            if (!ok) {
                std::fprintf(stderr, "unknown permission category\n");
                return 2;
            }
            continue;
        }
        if (arg == "--cloud-enable") {
            cloud.enabled = true;
            continue;
        }
        if (arg == "--cloud-endpoint") {
            const char* value = need("--cloud-endpoint");
            if (!value) return 2;
            cloud.endpoint = value;
            continue;
        }
        if (arg == "--cloud-model") {
            const char* value = need("--cloud-model");
            if (!value) return 2;
            cloud.model = value;
            continue;
        }
        if (arg == "--memory-budget-bytes") {
            const char* value = need("--memory-budget-bytes");
            if (!value || !ParseU64(value, budget.memory_budget_bytes)) return 2;
            saw_budget = true;
            continue;
        }
        if (arg == "--weight-bytes") {
            const char* value = need("--weight-bytes");
            if (!value || !ParseU64(value, budget.weight_file_bytes)) return 2;
            saw_weight = true;
            continue;
        }
        std::fprintf(stderr, "unknown argument\n");
        return 2;
    }

    std::string prompt;
    char buffer[4096];
    while (true) {
        std::size_t n = std::fread(buffer, 1, sizeof buffer, stdin);
        if (n > 0) prompt.append(buffer, n);
        if (prompt.size() > 16384) {
            std::printf("kind=error\ntext=prompt too large\n");
            return 1;
        }
        if (n < sizeof buffer) break;
    }

    ele::faith::ProcessRuntime process(runtime_path, 180000);
    ele::faith::Runtime* runtime = runtime_path.empty() ? nullptr : &process;
    ele::faith::Assistant assistant(runtime, nullptr);
    assistant.set_permissions(permissions);
    assistant.set_cloud(cloud, "");
    if (saw_budget || saw_weight) assistant.set_weight_budget(budget);
    ele::faith::Reply reply = assistant.Handle(prompt);
    Print(reply);
    return reply.kind == ele::faith::ReplyKind::Error ? 1 : 0;
}
