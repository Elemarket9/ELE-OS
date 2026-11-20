#include "note.h"

#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <string>

namespace {

void Usage() {
    std::cerr << "usage: ele-ota-note check FILE\n"
              << "       ele-ota-note write --target cuttlefish|treble-gsi --month YYYY-MM "
                 "--branch-phase maintained|extended --platform-source VALUE [--aosp-tag TAG] "
                 "[--fixes IDS] --out FILE\n";
}

bool Take(int argc, char** argv, int& index, std::string& dest) {
    if (index + 1 >= argc) return false;
    dest = argv[++index];
    return !dest.empty();
}

void Report(const ele::ota::Note& note) {
    std::cout << "note=valid\n"
              << "built=" << note.built << "\n"
              << "bulletin-is-platform-update=" << note.bulletin_is_platform_update << "\n"
              << "firmware=" << note.firmware << "\n"
              << "server=none\n"
              << "target=" << note.target << "\n"
              << "product=" << note.product << "\n"
              << "branch-phase=" << note.branch_phase << "\n"
              << "platform-source=" << note.platform_source << "\n"
              << "month=" << note.month << "\n";
}

std::string ReadFile(const std::string& path, std::string& error) {
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        error = "cannot read note";
        return "";
    }
    std::string data;
    char buffer[1024];
    while (data.size() <= ele::ota::kMaxMetadataBytes) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            close(fd);
            error = "cannot read note";
            return "";
        }
        if (n == 0) break;
        data.append(buffer, static_cast<size_t>(n));
    }
    close(fd);
    if (data.size() > ele::ota::kMaxMetadataBytes) {
        error = "note exceeds prototype limit";
        return "";
    }
    return data;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        Usage();
        return 2;
    }
    std::string command = argv[1];
    std::string error;
    if (command == "check") {
        if (argc != 3) {
            Usage();
            return 2;
        }
        std::string raw = ReadFile(argv[2], error);
        ele::ota::Note note;
        if (!error.empty() || !ele::ota::ParseNote(raw, note, error)) {
            std::cerr << error << "\n";
            return 1;
        }
        Report(note);
        return 0;
    }
    if (command != "write") {
        Usage();
        return 2;
    }
    std::string target, month, phase, source, tag, fixes, out;
    bool saw_tag = false;
    bool saw_fixes = false;
    for (int i = 2; i < argc; ++i) {
        std::string flag = argv[i];
        bool ok = true;
        if (flag == "--target") ok = Take(argc, argv, i, target);
        else if (flag == "--month") ok = Take(argc, argv, i, month);
        else if (flag == "--branch-phase") ok = Take(argc, argv, i, phase);
        else if (flag == "--platform-source") ok = Take(argc, argv, i, source);
        else if (flag == "--aosp-tag") {
            ok = Take(argc, argv, i, tag);
            saw_tag = true;
        } else if (flag == "--fixes") {
            ok = Take(argc, argv, i, fixes);
            saw_fixes = true;
        } else if (flag == "--out") ok = Take(argc, argv, i, out);
        else ok = false;
        if (!ok) {
            Usage();
            return 2;
        }
    }
    if (target.empty() || month.empty() || phase.empty() || source.empty() || out.empty()) {
        Usage();
        return 2;
    }
    if (saw_tag) {
        if (tag.rfind("aosp-tag:", 0) != 0) tag = "aosp-tag:" + tag;
    } else {
        tag.clear();
    }
    if (!saw_fixes) fixes.clear();
    ele::ota::Note note;
    if (!ele::ota::BuildNote(target, month, phase, source, tag, fixes, note, error)) {
        std::cerr << error << "\n";
        return 1;
    }
    std::string canonical = ele::ota::NoteCanonical(note);
    int fd = open(out.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0 || write(fd, canonical.data(), canonical.size()) != static_cast<ssize_t>(canonical.size())) {
        if (fd >= 0) close(fd);
        std::cerr << "cannot write note\n";
        return 1;
    }
    close(fd);
    Report(note);
    std::cout << "status=note-written\n";
    return 0;
}
