"""Offline installer contract tests: no GitHub network access required."""
import io
from pathlib import Path
import tempfile
import unittest
from zipfile import ZipFile
from tools import install_rustext as installer


def package(files):
    buf = io.BytesIO()
    with ZipFile(buf, "w") as z:
        for name, contents in files.items():
            z.writestr(name, contents)
    return buf.getvalue()


class RustextInstallerTests(unittest.TestCase):
    def test_pin_is_openmp_nomemhack_release(self):
        self.assertEqual(installer.VERSION, "v2.0.11")
        self.assertTrue(installer.URL.endswith("rustext-2.0.11-nomemhack.zip"))
        self.assertEqual(
            installer.SHA256,
            "b51e5d55492ff5e249665270333342131070cd0d171f43ad9893a8d563b2638b",
        )

    def test_adds_only_rustext_and_is_idempotent(self):
        original = {"name": "TARKENSO", "pawn": {
            "main_scripts": ["signal0317"], "legacy_plugins": ["mysql", "streamer"]}}
        updated = installer.updated_config(original)
        self.assertEqual(updated["pawn"]["legacy_plugins"], ["mysql", "streamer", "rustext"])
        self.assertEqual(updated["pawn"]["main_scripts"], ["signal0317"])
        self.assertEqual(original["pawn"]["legacy_plugins"], ["mysql", "streamer"])
        self.assertEqual(updated, installer.updated_config(updated))

    def test_legacy_plugins_can_be_string(self):
        value = installer.updated_config({"pawn": {"legacy_plugins": "mysql streamer"}})
        self.assertEqual(value["pawn"]["legacy_plugins"], ["mysql", "streamer", "rustext"])
        with self.assertRaises(ValueError):
            installer.updated_config({"pawn": {"legacy_plugins": [0]}})

    def test_platform_selection_does_not_extract_other_archive_entries(self):
        data = package({"folder/rustext.so": b"linux", "other/rustext.dll": b"win",
                        "../untrusted.exe": b"not copied"})
        self.assertEqual(installer.extract_library(data, "linux"), b"linux")
        self.assertEqual(installer.extract_library(data, "windows"), b"win")

    def test_missing_or_duplicate_plugin_is_rejected(self):
        with self.assertRaises(ValueError):
            installer.extract_library(package({"readme.txt": b"text"}), "linux")
        with self.assertRaises(ValueError):
            installer.extract_library(package({"first/rustext.so": b"a",
                                               "second/rustext.so": b"b"}), "linux")

    def test_install_cannot_start_without_config(self):
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaisesRegex(ValueError, "config.json"):
                installer.install(Path(root), "linux")


if __name__ == "__main__":
    unittest.main()
