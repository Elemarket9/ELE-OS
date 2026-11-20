# SPDX-License-Identifier: Apache-2.0
"""Structural check of every Android.bp in this overlay.

Soong parses every Android.bp in the tree, so one bad file breaks the whole
AOSP build. This is not Soong: it parses the Blueprint subset this overlay
uses (module blocks, strings, lists, booleans, integers, nested maps,
comments), and checks that module names are unique, srcs exist, every native
module that includes ele/core headers names libele_core_headers, and module
types are ones this overlay is allowed to use. Passing it does not mean the
platform compiles.
"""
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
ALLOWED = {"cc_library_static", "cc_library_headers", "cc_library_shared",
           "cc_binary", "cc_test", "cc_test_host", "cc_library", "cc_binary_host", "cc_library_host_static",
           "android_app", "sh_binary", "java_library", "prebuilt_etc"}
TOKEN = re.compile(r'\s*(?:(//[^\n]*|/\*.*?\*/)|("(?:[^"\\]|\\.)*")|([A-Za-z_][A-Za-z0-9_]*)|(-?\d+)|([{}\[\]:,=+]))', re.S)


def tokens(text, name):
    pos, out = 0, []
    while pos < len(text):
        if text[pos:].strip() == "":
            break
        m = TOKEN.match(text, pos)
        if not m:
            sys.exit(f"{name}: cannot tokenize at offset {pos}: {text[pos:pos+30]!r}")
        pos = m.end()
        if m.group(1):
            continue
        out.append(m.group(0).strip())
    return out


class Parser:
    def __init__(self, toks, name):
        self.t, self.i, self.name = toks, 0, name

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else None

    def take(self, want=None):
        tok = self.peek()
        if tok is None or (want and tok != want):
            sys.exit(f"{self.name}: expected {want or 'token'}, got {tok}")
        self.i += 1
        return tok

    def value(self):
        tok = self.peek()
        if tok == "{":
            return self.map()
        if tok == "[":
            self.take("[")
            items = []
            while self.peek() != "]":
                items.append(self.value())
                if self.peek() == ",":
                    self.take(",")
                elif self.peek() != "]":
                    sys.exit(f"{self.name}: expected , or ] got {self.peek()}")
            self.take("]")
            return items
        tok = self.take()
        if tok.startswith('"'):
            v = tok[1:-1]
            while self.peek() == "+":
                self.take("+")
                v += self.value()
            return v
        if tok in ("true", "false"):
            return tok == "true"
        if re.fullmatch(r"-?\d+", tok):
            return int(tok)
        sys.exit(f"{self.name}: unsupported value {tok}")

    def map(self):
        self.take("{")
        out = {}
        while self.peek() != "}":
            key = self.take()
            if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key):
                sys.exit(f"{self.name}: bad property {key}")
            self.take(":")
            if key in out:
                sys.exit(f"{self.name}: duplicate property {key}")
            out[key] = self.value()
            if self.peek() == ",":
                self.take(",")
            elif self.peek() != "}":
                sys.exit(f"{self.name}: expected , or }} got {self.peek()}")
        self.take("}")
        return out

    def modules(self):
        mods = []
        while self.peek() is not None:
            kind = self.take()
            if kind not in ALLOWED:
                sys.exit(f"{self.name}: module type {kind} not allowed")
            mods.append((kind, self.map()))
        return mods


names = {}
count = 0
for bp in sorted(root.rglob("Android.bp")):
    if ".git" in bp.parts:
        continue
    rel = bp.relative_to(root)
    for kind, props in Parser(tokens(bp.read_text(), str(rel)), str(rel)).modules():
        name = props.get("name")
        if not name:
            sys.exit(f"{rel}: {kind} without name")
        if name in names:
            sys.exit(f"{rel}: module {name} also defined in {names[name]}")
        names[name] = rel
        count += 1
        srcs = props.get("srcs", [])
        uses_core = False
        for s in srcs:
            if any(c in s for c in "*?:") :
                if not list(bp.parent.glob(s)):
                    sys.exit(f"{rel}: {name} glob {s} matches nothing")
                files = list(bp.parent.glob(s))
            else:
                f = bp.parent / s
                if not f.is_file():
                    sys.exit(f"{rel}: {name} missing src {s}")
                files = [f]
            for f in files:
                if f.suffix in (".cpp", ".cc", ".h") and "ele/core/" in f.read_text(errors="ignore"):
                    uses_core = True
        if uses_core and kind.startswith("cc_"):
            libs = props.get("header_libs", []) + props.get("static_libs", [])
            if "libele_core_headers" not in props.get("header_libs", []) and not libs:
                sys.exit(f"{rel}: {name} includes ele/core but lacks libele_core_headers")
print(f"{count} Blueprint modules structurally checked (Soong was not run)")
