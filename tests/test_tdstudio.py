"""Host integration contracts; behavioral assertions execute in the Pawn fixture."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / "gamemodes/includes"
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")


class TextDrawStudioIntegration(unittest.TestCase):
    def test_dispatchers_route_editor_before_sorting(self):
        for callback, editor, sorting in (
            ("OnPlayerClickPlayerTextDraw", "TDS_OnClick", "SortUI_OnClick"),
            ("OnPlayerClickTextDraw", "TDS_OnCancel", "SortUI_OnCancel"),
        ):
            body = GAME.split(f"public {callback}(", 1)[1].split("\n}", 1)[0]
            self.assertLess(body.index(editor), body.index(sorting))
        self.assertIn("TDS_OnDialog(playerid, dialogid, response, listitem, inputtext)", GAME)

    def test_cleanup_hooks_cover_all_lifecycles(self):
        for callback, cleanup in (
            ("OnGameModeExit", "TDS_Shutdown()"),
            ("OnPlayerDisconnect", "TDS_Reset(playerid, false)"),
            ("OnPlayerConnect", "TDS_Reset(playerid)"),
            ("OnPlayerDeath", "TDS_Close(playerid)"),
        ):
            body = GAME.split(f"public {callback}(", 1)[1].split("\n}", 1)[0]
            self.assertIn(cleanup, body)

    def test_reserved_dialog_range_does_not_overlap_host(self):
        values = set(range(7100, 7118))
        for path in [ROOT / "gamemodes/signal0317.pwn", *INC.glob("*.inc")]:
            if path.name.startswith("tdstudio"):
                continue
            for name, value in re.findall(r"^#define\s+(\w+)\s+\((\d+)\)", path.read_text(encoding="utf-8"), re.M):
                if any(word in name for word in ("DIALOG", "UI_", "MENU")):
                    self.assertNotIn(int(value), values, (path.name, name))

    def test_sorting_and_foreign_commands_respect_modal_session(self):
        source = (INC / "commands.inc").read_text(encoding="utf-8")
        self.assertIn("if (!TDS_CommandAllowed(playerid, cmd)) return 0;", source)
        for command in ("tdstudio", "tdgrant"):
            self.assertEqual(source.count(f"cmd:{command}("), 1)
        source = (INC / "waste_sort.inc").read_text(encoding="utf-8")
        body = source.split("stock SortUI_Open(playerid)", 1)[1].split("\n}", 1)[0]
        self.assertLess(body.index("TDS_Active(playerid)"), body.index("CreatePlayerTextDraw("))

    def test_persisted_scalar_contract_covers_entire_model(self):
        model = (INC / "tdstudio_model.inc").read_text(encoding="utf-8")
        codec = (INC / "tdstudio_codec.inc").read_text(encoding="utf-8")
        schema = (ROOT / "db/migrations/012_tdstudio_projects.sql").read_text(encoding="utf-8")
        enum = model.split("enum E_TDS_ELEMENT", 1)[1].split("};", 1)[0]
        fields = re.findall(r"\b(td\w+)\b", enum)
        wire = codec.split("gTDSFields[] = {", 1)[1].split("};", 1)[0]
        persisted = re.findall(r"\b(td\w+)\b", wire)
        self.assertEqual(fields[:-1], persisted)
        self.assertEqual(fields[-1], "tdText")
        self.assertIn(f"<> {len(persisted)+1} THEN", schema)
        self.assertIn("ON DELETE CASCADE", schema)
        self.assertIn("START TRANSACTION;", schema)
        self.assertIn("FOR UPDATE", schema)

    def test_editor_db_errors_are_intercepted_before_host_failure(self):
        source = (INC / "db_primary.inc").read_text(encoding="utf-8")
        body = source.split("public OnQueryError(", 1)[1]
        self.assertLess(body.index("TDSDB_OnError(callback, query)"), body.index("gDBPrimaryReady = false"))
        db = (INC / "tdstudio_db.inc").read_text(encoding="utf-8")
        self.assertNotIn("mysql_connect", db)
        self.assertNotIn("mysql_query(", db)

    def test_runtime_harness_is_in_ci(self):
        source = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
        self.assertIn("python3 tools/test_tdstudio.py", source)
        self.assertIn("test_tdstudio.py", source)


if __name__ == "__main__":
    unittest.main()
