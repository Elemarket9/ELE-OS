"""Host unit tests: configuration, source integration and initramfs format."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

NATIVE = Path(__file__).resolve().parents[1] / "kernel/native"


def load(name):
    spec = importlib.util.spec_from_file_location(name, NATIVE / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


b, s = load("build"), load("smoke")


class NativeTests(unittest.TestCase):
    def test_config(self):
        for arch, flags in b.ARCH_FLAGS.items():
            good = "\n".join("CONFIG_" + x + "=y" for x in b.COMMON + flags)
            good += '\nCONFIG_LOCALVERSION="-eleos"\n# CONFIG_LOCALVERSION_AUTO is not set\n# CONFIG_PSI_DEFAULT_DISABLED is not set'
            b.verify_config(good, arch)
            for x in b.COMMON + flags:
                with self.assertRaises(ValueError):
                    b.verify_config(good.replace("CONFIG_" + x + "=y", ""), arch)
            with self.assertRaises(ValueError):
                b.verify_config(good + "\nCONFIG_ELE_PRESSURE=m", arch)

    def test_integrate(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            drivers = root / "drivers"
            drivers.mkdir()
            (root / "include/linux").mkdir(parents=True)
            (root / "security").mkdir()
            (root / "security/security.c").write_text(
                "#include <linux/mm.h>\n"
                "static unsigned long mmap_prot(struct file *file, unsigned long prot) { return prot; }\n"
                "int security_mmap_file(struct file *file, unsigned long prot,\n unsigned long flags)\n{ return 0; }\n"
                "int security_file_mprotect(struct vm_area_struct *vma, unsigned long reqprot,\n unsigned long prot)\n{ return 0; }\n")
            (drivers / "Kconfig").write_text('menu "Device Drivers"\nendmenu\n')
            (drivers / "Makefile").write_text("# original\n")
            b.integrate(root)
            sec = (root / "security/security.c").read_text()
            self.assertIn("ele_memguard_denies(mmap_prot(file, prot))", sec)
            self.assertIn("ele_memguard_denies(prot)", sec)
            self.assertEqual(sec.count("return -EACCES;"), 2)
            self.assertTrue((root / "include/linux/ele_memguard.h").is_file())
            self.assertIn('source "drivers/ele/Kconfig"\nendmenu', (drivers / "Kconfig").read_text())
            self.assertIn("obj-$(CONFIG_ELE_PRESSURE) += ele/", (drivers / "Makefile").read_text())
            self.assertEqual((drivers / "ele/ele_pressure.c").read_bytes(),
                             (NATIVE / "pressure/ele_pressure.c").read_bytes())
            with self.assertRaises(ValueError):
                b.integrate(root)

    def test_guard_refuses_changed_api(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "include/linux").mkdir(parents=True)
            (root / "security").mkdir()
            source = root / "security/security.c"
            original = "#include <linux/mm.h>\nint mmap_prot(void) { return 0; }\n"
            source.write_text(original)
            with self.assertRaises(ValueError):
                b.integrate_guard(root)
            self.assertEqual(source.read_text(), original)
            self.assertFalse((root / "include/linux/ele_memguard.h").exists())

    def test_newc(self):
        data = s.initramfs(b"test")
        pos, files = 0, {}
        while pos < len(data):
            self.assertEqual(data[pos:pos+6], b"070701")
            fields = [int(data[pos+6+i*8:pos+14+i*8], 16) for i in range(13)]
            size, namesize = fields[6], fields[11]
            name = data[pos+110:pos+110+namesize-1].decode()
            start = (pos+110+namesize+3) & ~3
            files[name] = (fields[1], fields[9:11], data[start:start+size])
            pos = (start+size+3) & ~3
        self.assertEqual(files["init"], (0o100755, [0, 0], b"test"))
        self.assertEqual(files["dev/console"][:2], (0o20600, [5, 1]))
        self.assertIn("TRAILER!!!", files)

    def test_refuse_existing_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with self.assertRaises(ValueError):
                b.build(root, root, "x86_64", 1, "")


if __name__ == "__main__":
    unittest.main()
