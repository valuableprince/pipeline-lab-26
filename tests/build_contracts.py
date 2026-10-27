"""Exercise both real source-staging paths; no compiler or GTA execution here.

The Windows compiler is a test double only for inspecting its temporary input.
The CI staging Python is executed verbatim in an isolated copy of gamemodes.
Actual Pawn compilation remains a separate build/runtime check.
"""
from contextlib import redirect_stdout
from functools import lru_cache
from io import StringIO
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
from unittest.mock import patch

from tools import build_windows

ROOT = Path(__file__).resolve().parents[1]


@lru_cache(maxsize=1)
def staged_sources():
    results = {}
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        server = temp / "server"
        (server / "qawno/include").mkdir(parents=True)
        (server / "qawno/pawncc.exe").touch()
        (server / "qawno/include/open.mp.inc").touch()

        def inspect_compiler(command, **kwargs):
            source = Path(command[-1])
            results["windows"] = {p.name: p.read_bytes() for p in source.parent.iterdir()}
            output = Path(next(arg[2:] for arg in command if arg.startswith("-o")))
            output.with_suffix(".amx").write_bytes(b"test-double: not compiled Pawn")
            return subprocess.CompletedProcess(command, 0, "Pawn compiler 3.10.11\n")

        with patch.object(build_windows.subprocess, "run", side_effect=inspect_compiler), redirect_stdout(StringIO()):
            if build_windows.build(server, temp / "windows") != 0:
                raise AssertionError("Windows source staging failed")

        workflow = (ROOT / ".github/workflows/build.yml").read_text("utf-8")
        match = re.search(
            r"(?ms)^      - name: Stage source\n.*?python3 - <<'PY'\n(.*?)^          PY$",
            workflow,
        )
        if not match:
            raise AssertionError("Cannot locate the current CI source-staging step")
        shutil.copytree(ROOT / "gamemodes", temp / "gamemodes")
        subprocess.run([sys.executable, "-c", textwrap.dedent(match.group(1))], cwd=temp, check=True)
        results["ci"] = {p.name: p.read_bytes() for p in (temp / "build/stage").iterdir()}
    return results


def assert_staged(case, *modules):
    """Assert module contents, not an obsolete explicit list in build scripts."""
    for name in modules:
        expected = (ROOT / "gamemodes/includes" / name).read_text("utf-8").encode("cp1251")
        for backend, files in staged_sources().items():
            with case.subTest(backend=backend, module=name):
                case.assertIn(name, files)
                case.assertEqual(files[name], expected)


def assert_include_order(case, source, *modules):
    """Require unique ordered includes while permitting unrelated includes between."""
    includes = re.findall(r'^#include\s+"([^"]+)"', source, re.M)
    for name in modules:
        case.assertEqual(includes.count(name), 1, name)
    positions = [includes.index(name) for name in modules]
    case.assertEqual(positions, sorted(positions))
