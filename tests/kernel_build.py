"""Host-level tests only; no kernel compile or boot is performed."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("kernel_build", Path(__file__).resolve().parents[1] / "kernel/build.py")
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
GOOD = "\n".join(f"{key}=y" for key in k.REQUIRED)


class KernelBuildTests(unittest.TestCase):
    def test_config_valid(self):
        k.check_config(GOOD)

    def test_config_missing_or_disabled(self):
        for key in k.REQUIRED:
            for replacement in ("", f"# {key} is not set", f"{key}=m"):
                with self.subTest(key=key, replacement=replacement), self.assertRaises(ValueError):
                    k.check_config(GOOD.replace(f"{key}=y", replacement))

    def test_config_duplicate(self):
        with self.assertRaises(ValueError):
            k.check_config(GOOD + "\nCONFIG_BPF_SYSCALL=y")

    def test_manifest(self):
        k.check_manifest(f'<manifest><project path="common" revision="{"a" * 40}"/></manifest>'.encode())
        for bad in (b'<manifest/>', b'<manifest><project path="common" revision="main"/></manifest>', b'<other/>'):
            with self.assertRaises(ValueError):
                k.check_manifest(bad)

    def test_image_headers(self):
        with tempfile.TemporaryDirectory() as tmp:
            for arch, offset, magic in (("x86_64", 0x202, b"HdrS"), ("aarch64", 0x38, b"ARM\x64")):
                path = Path(tmp) / k.TARGETS[arch]
                path.write_bytes(bytes(offset) + magic)
                k.verify_image(path, arch)
                path.write_bytes(b"not a kernel")
                with self.assertRaises(ValueError):
                    k.verify_image(path, arch)

    def test_refuse_existing_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / ".repo").mkdir()
            with self.assertRaisesRegex(ValueError, "already exist"):
                k.build(root, root, "x86_64")

    def test_refuse_output_in_workspace(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / ".repo").mkdir()
            with self.assertRaisesRegex(ValueError, "outside"):
                k.build(root, root / "out", "x86_64")


if __name__ == "__main__":
    unittest.main()
