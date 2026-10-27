"""Regression coverage for dynamic include staging in CI and Windows builds."""
import unittest

from build_contracts import ROOT, staged_sources


class BuildStagingTests(unittest.TestCase):
    def test_both_builds_stage_every_include_and_primary_source(self):
        # This includes transitive modules such as tdstudio_history.inc, not
        # only the includes named directly by the gamemode entrypoint.
        source = ROOT / "gamemodes/signal0317.pwn"
        expected = {
            p.name: p.read_text("utf-8").encode("cp1251", errors="strict")
            for p in (ROOT / "gamemodes/includes").glob("*.inc")
        }
        expected[source.name] = b"#define TARKENSO_DB_PRIMARY\n" + source.read_text("utf-8").encode("cp1251")
        builds = staged_sources()
        self.assertEqual(set(builds), {"windows", "ci"})
        for backend, files in builds.items():
            with self.subTest(backend=backend):
                self.assertEqual(set(files), set(expected))
                for name, content in expected.items():
                    with self.subTest(module=name):
                        self.assertEqual(files[name], content)


if __name__ == "__main__":
    unittest.main()
