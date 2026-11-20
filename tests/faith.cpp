// SPDX-License-Identifier: Apache-2.0

#include "faith.h"

#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <string>

namespace {

int failures = 0;

void Expect(bool cond, const char* expr, int line) {
    if (!cond) {
        std::fprintf(stderr, "FAIL tests/faith.cpp:%d %s\n", line, expr);
        ++failures;
    }
}

#define EXPECT(cond) Expect((cond), #cond, __LINE__)

class ScriptedRuntime : public ele::faith::Runtime {
public:
    bool up = true;
    bool ok = true;
    int calls = 0;
    std::string reply = "hello";
    std::string seen;

    bool available() const override { return up; }
    bool complete(const std::string& prompt, std::string& out, std::string& error) override {
        ++calls;
        seen = prompt;
        if (!ok) {
            error = "scripted failure";
            return false;
        }
        out = reply;
        return true;
    }
};

class FakeCloud : public ele::faith::CloudTransport {
public:
    int calls = 0;
    bool ok = true;
    std::string reply = "from cloud";
    ele::faith::CloudRequest last;

    bool send(const ele::faith::CloudRequest& request, std::string& response, std::string& error) override {
        ++calls;
        last = request;
        if (!ok) {
            error = "transport failed";
            return false;
        }
        response = reply;
        return true;
    }
};

struct ScopedDir {
    std::string path;
    ScopedDir() {
        char buf[] = "/tmp/faithXXXXXX";
        if (char* made = mkdtemp(buf)) path = made;
    }
    ~ScopedDir() {
        if (path.empty()) return;
        DIR* dir = opendir(path.c_str());
        if (dir) {
            while (dirent* ent = readdir(dir)) {
                if (ent->d_name[0] == '.') continue;
                unlink((path + "/" + ent->d_name).c_str());
            }
            closedir(dir);
        }
        rmdir(path.c_str());
    }
    std::string WriteExe(const char* name, const std::string& body) const {
        std::string file = path + "/" + name;
        std::ofstream out(file);
        out << body;
        out.close();
        chmod(file.c_str(), 0755);
        return file;
    }
};

std::string Extra(const ele::faith::Intent& intent, const char* key) {
    std::string found;
    int count = 0;
    for (const auto& extra : intent.extras) {
        if (extra.first == key) {
            found = extra.second;
            ++count;
        }
    }
    EXPECT(count == 1);
    return found;
}

ele::faith::Permissions AllOn() {
    ele::faith::Permissions permissions;
    permissions.Set(ele::faith::ToolCategory::Call, true);
    permissions.Set(ele::faith::ToolCategory::Message, true);
    permissions.Set(ele::faith::ToolCategory::Alarm, true);
    permissions.Set(ele::faith::ToolCategory::Setting, true);
    permissions.Set(ele::faith::ToolCategory::AppLaunch, true);
    return permissions;
}

void ExpectRefused(const char* json, ele::faith::ToolCategory category, const char* reason) {
    ele::faith::ToolDecision decision = ele::faith::DecideTool(json, ele::faith::Permissions{});
    EXPECT(decision.outcome == ele::faith::ToolOutcome::RefusedDisabled ||
           decision.outcome == ele::faith::ToolOutcome::RefusedInvalid);
    EXPECT(decision.outcome == (std::string(reason) == "disabled" ? ele::faith::ToolOutcome::RefusedDisabled
                                                                  : ele::faith::ToolOutcome::RefusedInvalid));
    EXPECT(decision.has_category);
    EXPECT(decision.category == category);
    EXPECT(decision.reason == reason);
    EXPECT(decision.intent.action.empty());
    EXPECT(decision.intent.extras.empty());
}

void TestGate() {
    ele::faith::Permissions off;
    EXPECT(!off.Allows(ele::faith::ToolCategory::Call));
    EXPECT(!off.Allows(ele::faith::ToolCategory::Message));
    EXPECT(!off.Allows(ele::faith::ToolCategory::Alarm));
    EXPECT(!off.Allows(ele::faith::ToolCategory::Setting));
    EXPECT(!off.Allows(ele::faith::ToolCategory::AppLaunch));

    const char* call = "{\"tool\":\"call.place\",\"number\":\"+15555550100\"}";
    ExpectRefused(call, ele::faith::ToolCategory::Call, "disabled");
    ele::faith::ToolDecision allowed = ele::faith::DecideTool(call, AllOn());
    EXPECT(allowed.outcome == ele::faith::ToolOutcome::Emitted);
    EXPECT(allowed.intent.action == "android.intent.action.CALL");
    EXPECT(allowed.intent.data_uri == "tel:+15555550100");
    EXPECT(allowed.intent.extras.empty());

    const char* message =
        "{\"tool\":\"message.send\",\"number\":\"+15555550100\",\"body\":\"fixture message\"}";
    ExpectRefused(message, ele::faith::ToolCategory::Message, "disabled");
    allowed = ele::faith::DecideTool(message, AllOn());
    EXPECT(allowed.outcome == ele::faith::ToolOutcome::Emitted);
    EXPECT(allowed.intent.action == "android.intent.action.SENDTO");
    EXPECT(allowed.intent.data_uri == "sms:+15555550100");
    EXPECT(Extra(allowed.intent, "sms_body") == "fixture message");

    const char* alarm = "{\"tool\":\"alarm.set\",\"hour\":7,\"minute\":30,\"label\":\"fixture\"}";
    ExpectRefused(alarm, ele::faith::ToolCategory::Alarm, "disabled");
    allowed = ele::faith::DecideTool(alarm, AllOn());
    EXPECT(allowed.outcome == ele::faith::ToolOutcome::Emitted);
    EXPECT(allowed.intent.action == "android.intent.action.SET_ALARM");
    EXPECT(Extra(allowed.intent, "android.intent.extra.alarm.HOUR") == "7");
    EXPECT(Extra(allowed.intent, "android.intent.extra.alarm.MINUTES") == "30");
    EXPECT(Extra(allowed.intent, "android.intent.extra.alarm.MESSAGE") == "fixture");
    EXPECT(Extra(allowed.intent, "android.intent.extra.alarm.SKIP_UI") == "false");

    const char* cancel = "{\"tool\":\"alarm.cancel\",\"label\":\"fixture\"}";
    ExpectRefused(cancel, ele::faith::ToolCategory::Alarm, "disabled");
    allowed = ele::faith::DecideTool(cancel, AllOn());
    EXPECT(allowed.intent.action == "android.intent.action.DISMISS_ALARM");
    EXPECT(Extra(allowed.intent, "android.intent.extra.alarm.SEARCH_MODE") == "android.label");
    EXPECT(Extra(allowed.intent, "android.intent.extra.alarm.MESSAGE") == "fixture");

    allowed = ele::faith::DecideTool("{\"tool\":\"alarm.cancel\",\"hour\":7,\"minute\":0}", AllOn());
    EXPECT(allowed.outcome == ele::faith::ToolOutcome::Emitted);
    EXPECT(Extra(allowed.intent, "android.intent.extra.alarm.SEARCH_MODE") == "android.time");

    const char* setting = "{\"tool\":\"setting.open\",\"screen\":\"wifi\"}";
    ExpectRefused(setting, ele::faith::ToolCategory::Setting, "disabled");
    allowed = ele::faith::DecideTool(setting, AllOn());
    EXPECT(allowed.intent.action == "android.settings.WIFI_SETTINGS");
    EXPECT(ele::faith::DecideTool("{\"tool\":\"setting.open\",\"screen\":\"bluetooth\"}", AllOn())
               .intent.action == "android.settings.BLUETOOTH_SETTINGS");
    EXPECT(ele::faith::DecideTool("{\"tool\":\"setting.open\",\"screen\":\"sound\"}", AllOn()).intent.action ==
           "android.settings.SOUND_SETTINGS");
    EXPECT(ele::faith::DecideTool("{\"tool\":\"setting.open\",\"screen\":\"display\"}", AllOn()).intent.action ==
           "android.settings.DISPLAY_SETTINGS");
    EXPECT(ele::faith::DecideTool("{\"tool\":\"setting.open\",\"screen\":\"airplane\"}", AllOn()).intent.action ==
           "android.settings.AIRPLANE_MODE_SETTINGS");

    const char* launch = "{\"tool\":\"app.launch\",\"package\":\"com.example.fixture\"}";
    ExpectRefused(launch, ele::faith::ToolCategory::AppLaunch, "disabled");
    allowed = ele::faith::DecideTool(launch, AllOn());
    EXPECT(allowed.intent.action == "android.intent.action.MAIN");
    EXPECT(allowed.intent.package_name == "com.example.fixture");
    EXPECT(allowed.intent.categories.size() == 1);
    EXPECT(allowed.intent.categories[0] == "android.intent.category.LAUNCHER");

    EXPECT(ele::faith::DecideTool("{\"tool\":\"call.place\",\"number\":\"12\"}", AllOn()).reason ==
           "invalid arguments");
    EXPECT(ele::faith::DecideTool("{\"tool\":\"setting.open\",\"screen\":\"shell\"}", AllOn()).reason ==
           "invalid arguments");
    EXPECT(ele::faith::DecideTool("{\"tool\":\"app.launch\",\"package\":\"not a package\"}", AllOn()).reason ==
           "invalid arguments");
    EXPECT(ele::faith::DecideTool("{\"tool\":\"alarm.set\",\"hour\":24,\"minute\":0}", AllOn()).reason ==
           "invalid arguments");
    EXPECT(ele::faith::DecideTool("{\"tool\":\"shell.run\"}", AllOn()).reason == "invalid tool");
    EXPECT(!ele::faith::DecideTool("{\"tool\":\"shell.run\"}", AllOn()).has_category);
    EXPECT(ele::faith::DecideTool("{\"tool\":\"call.place\",\"number\":\"+15555550100\",\"cmd\":\"rm\"}", AllOn())
               .reason == "invalid arguments");
    EXPECT(ele::faith::DecideTool("hello {\"tool\":\"call.place\"}", AllOn()).outcome ==
           ele::faith::ToolOutcome::NotATool);
    EXPECT(ele::faith::DecideTool("{", AllOn()).reason == "unreadable tool call");
    EXPECT(ele::faith::DecideTool("{\"tool\":\"alarm.cancel\"}", AllOn()).reason == "invalid arguments");
    EXPECT(ele::faith::DecideTool(
               "{\"tool\":\"alarm.cancel\",\"label\":\"fixture\",\"hour\":7,\"minute\":0}", AllOn())
               .reason == "invalid arguments");
}

void TestBudget() {
    using ele::faith::CheckWeightBudget;
    using ele::faith::WeightBudget;
    using ele::faith::WeightCheck;
    EXPECT(CheckWeightBudget(WeightBudget{}) == WeightCheck::BudgetUnset);
    WeightBudget unknown;
    unknown.memory_budget_bytes = 100;
    EXPECT(CheckWeightBudget(unknown) == WeightCheck::SizeUnknown);
    WeightBudget fits;
    fits.memory_budget_bytes = 200;
    fits.weight_file_bytes = 200;
    EXPECT(CheckWeightBudget(fits) == WeightCheck::FileFitsInBudget);
    WeightBudget over;
    over.memory_budget_bytes = 10;
    over.weight_file_bytes = 11;
    EXPECT(CheckWeightBudget(over) == WeightCheck::FileExceedsBudget);
}

void TestCloud() {
    ele::faith::CloudConfig config;
    ele::faith::CloudRequest request;
    request.url = "stale";
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "", request) == ele::faith::CloudBuild::Disabled);
    EXPECT(request.url.empty());
    config.enabled = true;
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "", request) == ele::faith::CloudBuild::MissingEndpoint);
    config.endpoint = "https://example.com/v1";
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "", request) == ele::faith::CloudBuild::MissingModel);
    config.model = "demo";
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "super-secret-key", request) == ele::faith::CloudBuild::Ok);
    EXPECT(request.url == "https://example.com/v1");
    EXPECT(request.body == "{\"prompt\":\"hello\",\"model\":\"demo\"}");
    EXPECT(request.url.find("super-secret-key") == std::string::npos);
    EXPECT(request.body.find("super-secret-key") == std::string::npos);
    EXPECT(request.body.find("audio") == std::string::npos);
    bool saw_auth = false;
    for (const auto& header : request.headers) {
        if (header.first == "Authorization") {
            saw_auth = header.second == "Bearer super-secret-key";
        }
    }
    EXPECT(saw_auth);
    config.endpoint = "https://example.com/v1";
    EXPECT(ele::faith::BuildCloudRequest(config, "a\"b\\c\n", "", request) == ele::faith::CloudBuild::Ok);
    EXPECT(request.body.find("a\\\"b\\\\c\\n") != std::string::npos);

    config.endpoint = "http://example.com/v1";
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "", request) == ele::faith::CloudBuild::BadUrl);
    config.endpoint = "https://user:pass@example.com/v1";
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "", request) == ele::faith::CloudBuild::BadUrl);
    config.endpoint = "https://generativelanguage.googleapis.com/v1";
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "", request) == ele::faith::CloudBuild::RefusedHost);
    config.endpoint = "https://gemini.google.com/app";
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "", request) == ele::faith::CloudBuild::RefusedHost);
    config.endpoint = "https://notgoogle.com/v1";
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "", request) == ele::faith::CloudBuild::Ok);
    config.endpoint = "https://google.com.evil.example/v1";
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "", request) == ele::faith::CloudBuild::Ok);
    config.endpoint = "https://example.com:8443/v1";
    EXPECT(ele::faith::BuildCloudRequest(config, "hello", "bad key", request) == ele::faith::CloudBuild::BadKey);
}

void TestAssistant() {
    ScriptedRuntime runtime;
    FakeCloud cloud;
    ele::faith::Assistant bare(nullptr, nullptr);
    ele::faith::Reply reply = bare.Handle("hello");
    EXPECT(reply.kind == ele::faith::ReplyKind::Unavailable);
    EXPECT(reply.text.find("cloud inference is off") != std::string::npos);

    ele::faith::Assistant assistant(&runtime, &cloud);
    reply = assistant.Handle("hello");
    EXPECT(reply.kind == ele::faith::ReplyKind::Text);
    EXPECT(reply.text == "hello");
    EXPECT(runtime.calls == 1);
    EXPECT(cloud.calls == 0);
    EXPECT(runtime.seen == "hello");

    runtime.reply = "{\"tool\":\"alarm.set\",\"hour\":7,\"minute\":30,\"label\":\"fixture\"}";
    reply = assistant.Handle("set the fixture alarm");
    EXPECT(reply.kind == ele::faith::ReplyKind::ToolRefused);
    EXPECT(reply.tool.reason == "disabled");
    EXPECT(reply.tool.intent.action.empty());

    ele::faith::Permissions permissions;
    permissions.Set(ele::faith::ToolCategory::Alarm, true);
    assistant.set_permissions(permissions);
    reply = assistant.Handle("set the fixture alarm");
    EXPECT(reply.kind == ele::faith::ReplyKind::ToolEmitted);
    EXPECT(reply.tool.intent.action == "android.intent.action.SET_ALARM");

    ele::faith::CloudConfig config;
    config.enabled = true;
    config.endpoint = "https://example.com/v1";
    config.model = "demo";
    assistant.set_cloud(config, "super-secret-key");
    runtime.ok = false;
    int cloud_before = cloud.calls;
    reply = assistant.Handle("hello");
    EXPECT(reply.kind == ele::faith::ReplyKind::Error);
    EXPECT(cloud.calls == cloud_before);

    runtime.up = false;
    reply = assistant.Handle("hello");
    EXPECT(reply.kind == ele::faith::ReplyKind::Text);
    EXPECT(reply.text == "from cloud");
    EXPECT(cloud.calls == cloud_before + 1);
    EXPECT(cloud.last.body.find("hello") != std::string::npos);
    EXPECT(reply.text.find("super-secret-key") == std::string::npos);

    runtime.up = true;
    runtime.ok = true;
    ele::faith::WeightBudget budget;
    budget.memory_budget_bytes = 10;
    budget.weight_file_bytes = 11;
    assistant.set_weight_budget(budget);
    int calls = runtime.calls;
    reply = assistant.Handle("hello");
    EXPECT(reply.kind == ele::faith::ReplyKind::Unavailable);
    EXPECT(runtime.calls == calls);
    EXPECT(reply.text.find("was not started") != std::string::npos);

    ele::faith::Assistant cloud_only(nullptr, nullptr);
    cloud_only.set_cloud(config, "");
    reply = cloud_only.Handle("hello");
    EXPECT(reply.kind == ele::faith::ReplyKind::Error);
    EXPECT(reply.text.find("no request was sent") != std::string::npos);

    config.endpoint = "https://generativelanguage.googleapis.com/v1";
    ele::faith::Assistant refused(nullptr, &cloud);
    refused.set_cloud(config, "");
    int sent = cloud.calls;
    reply = refused.Handle("hello");
    EXPECT(reply.kind == ele::faith::ReplyKind::Error);
    EXPECT(cloud.calls == sent);
}

void TestProcess() {
    ScopedDir dir;
    EXPECT(!dir.path.empty());
    std::string seen = dir.path + "/seen";
    std::string script = dir.WriteExe(
        "reply.sh", "#!/bin/sh\ncat > '" + seen + "'\nprintf '%s' 'plain reply'\n");
    ele::faith::ProcessRuntime runtime(script, 5000);
    EXPECT(runtime.available());
    std::string out, error;
    EXPECT(runtime.complete("prompt-text", out, error));
    EXPECT(out == "plain reply");
    std::ifstream in(seen);
    std::string got;
    std::getline(in, got);
    EXPECT(got == "prompt-text");

    std::string fail = dir.WriteExe("fail.sh", "#!/bin/sh\ncat >/dev/null\nexit 3\n");
    ele::faith::ProcessRuntime bad(fail, 5000);
    EXPECT(!bad.complete("x", out, error));
    EXPECT(error.find("status 3") != std::string::npos);

    std::string slow = dir.WriteExe("slow.sh", "#!/bin/sh\ncat >/dev/null\nsleep 5\n");
    ele::faith::ProcessRuntime timed(slow, 400);
    EXPECT(!timed.complete("x", out, error));
    EXPECT(error == "runtime timed out");

    ele::faith::ProcessRuntime missing("/tmp/faith-does-not-exist", 1000);
    EXPECT(!missing.available());
}

}  // namespace

int main() {
    TestGate();
    TestBudget();
    TestCloud();
    TestAssistant();
    TestProcess();
    if (failures) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
