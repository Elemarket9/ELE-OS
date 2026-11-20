# SPDX-License-Identifier: Apache-2.0
import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
registry = json.loads((root / "capabilities/registry.json").read_text())
spec = (root / registry["specification"]).read_text()
pattern = r"^### (EOS-\d{3}) — (.+)\n- Build: (.+)\n- Acceptance: (.+)\n- Status: PLANNED"
original = re.findall(pattern, spec, re.M)
entries = registry["capabilities"]
assert len(original) == len(entries) == 350
assert [e["id"] for e in entries] == [f"EOS-{i:03d}" for i in range(1, 351)]
for e, (ident, title, build, acceptance) in zip(entries, original):
    assert (e["id"], e["title"], e["functional_requirements"], e["verification_requirements"]) == (ident, title, build, acceptance)
    assert e["status"] in registry["status_values"]
    for path in e["implementation_paths"]:
        assert (root / path).is_file(), path
    if e["status"] == "Complete":
        assert e["commit_references"] and e["implementation_paths"] and not e["outstanding_blockers"]
print("350 capability IDs and original requirements verified")
