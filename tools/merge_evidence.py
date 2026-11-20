# SPDX-License-Identifier: Apache-2.0
"""Merge per-workstream evidence files into capabilities/registry.json.

Each workstream owns capabilities/evidence/<domain>.json (a JSON array).
The original specification fields (title, requirements, verification) are
never touched. Only evidence fields are replaced. A workstream cannot
promote a capability above "Source implemented": higher states need an AOSP
build, a Cuttlefish boot or hardware evidence that host tests do not give.

Usage: python3 tools/merge_evidence.py [--commit SHA]
"""
import json
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parents[1]
registry_path = root / "capabilities/registry.json"
registry = json.loads(registry_path.read_text())
by_id = {c["id"]: c for c in registry["capabilities"]}
allowed = {"Not started", "In development", "Source implemented"}
commit = None
if len(sys.argv) == 3 and sys.argv[1] == "--commit":
    commit = sys.argv[2]

seen = {}
for path in sorted((root / "capabilities/evidence").glob("*.json")):
    for e in json.loads(path.read_text()):
        ident = e["id"]
        if ident not in by_id:
            sys.exit(f"{path.name}: unknown id {ident}")
        if ident in seen:
            sys.exit(f"{ident} claimed by {seen[ident]} and {path.name}")
        seen[ident] = path.name
        if e["status"] not in allowed:
            sys.exit(f"{path.name}: {ident} status {e['status']} exceeds host evidence")
        for p in e["implementation_paths"]:
            if not (root / p).is_file():
                sys.exit(f"{path.name}: {ident} missing path {p}")
        if e["status"] == "Source implemented":
            if not e["implementation_paths"] or not e["outstanding_blockers"]:
                sys.exit(f"{path.name}: {ident} needs paths and remaining blockers")
        cap = by_id[ident]
        cap["status"] = e["status"]
        cap["implementation_paths"] = e["implementation_paths"]
        cap["integration_interfaces"] = e["integration_interfaces"]
        cap["security_considerations"] = e["security_considerations"]
        cap["outstanding_blockers"] = e["outstanding_blockers"]
        if commit and e["status"] != "Not started" and commit not in cap["commit_references"]:
            cap["commit_references"].append(commit)

registry_path.write_text(json.dumps(registry, indent=2, ensure_ascii=False) + "\n")
counts = {}
for c in registry["capabilities"]:
    counts[c["status"]] = counts.get(c["status"], 0) + 1
print(json.dumps(counts, sort_keys=True))
