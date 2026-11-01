"""Coverage map for every ship-related player command and editor action.

These source/dispatch regression tests run in CI together with the Pawn compiler.
They are NOT a substitute for manual click and persistence tests on a live client.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

SHIP = read("gamemodes/includes/ship_studio.inc")
PORT = read("gamemodes/includes/ship_port.inc")
EDITOR = read("gamemodes/includes/ship_editor.inc")
PANEL = read("gamemodes/includes/ship_panel.inc")
SURF = read("gamemodes/includes/ship_passenger_sync.inc")
CAM = read("gamemodes/includes/ship_camera.inc")
COMMANDS = read("gamemodes/includes/commands.inc")

ACTIONS = {
    "help", "status", "edit", "port", "load", "release", "cargo",
    "create", "clear", "preset", "stop", "save", "export", "add",
    "move", "turn", "set", "mark", "undo", "run", "oceanprobe",
}
EDITOR_ACTIONS = {
    "SE_NORTH", "SE_SOUTH", "SE_WEST", "SE_EAST", "SE_ROTATE_L",
    "SE_ROTATE_R", "SE_ZUP", "SE_ZDOWN", "SE_FINE", "SE_COARSE",
    "SE_ZOOM_IN", "SE_ZOOM_OUT", "SE_SAVE", "SE_CANCEL",
    "SE_LS_BERTH", "SE_LS_EXIT", "SE_SF_BERTH", "SE_SF_EXIT",
    "SE_ROUTE_LS_OUT", "SE_ROUTE_LS_IN", "SE_ROUTE_SF_OUT",
    "SE_ROUTE_SF_IN", "SE_ROUTE_ADD", "SE_ROUTE_UNDO",
    "SE_ROUTE_PREVIEW", "SE_ROUTE_STOP", "SE_ROUTE_SAVE", "SE_ROUTE_LOAD",
}


def action_body(action, next_action):
    first = SHIP.index('if (!strcmp(action, "' + action + '", true))')
    tail = SHIP[first:]
    if next_action is None:
        return tail
    last = tail.index('if (!strcmp(action, "' + next_action + '", true))')
    return tail[:last]


class ShipCommandAudit(unittest.TestCase):
    def test_entrypoints_and_entire_ship_dispatch_table(self):
        for command, handler in (
            ("ship", "Ship_Command(playerid, params)"),
            ("shipcam", "ShipCam_Command(playerid)"),
            ("shipsurf", "ShipSurf_Command(playerid, params)"),
        ):
            self.assertIn("cmd:" + command + "(playerid, params[])", COMMANDS)
            self.assertIn("return " + handler + ";", COMMANDS)
        found = set(re.findall(r'if \(!strcmp\(action, "([^"]+)", true\)\)', SHIP))
        self.assertEqual(ACTIONS, found & ACTIONS)
        self.assertEqual(ACTIONS, found, "Audit new /ship verbs before releasing them")

    def test_all_argument_formats_use_argument_tail_not_the_whole_command(self):
        self.assertIn("new action[24], arguments[160];", SHIP)
        self.assertIn("while (start < length", SHIP)
        self.assertIn("strmid(action, params, start, delimiter, sizeof(action));", SHIP)
        self.assertIn("strmid(arguments, params, start, length, sizeof(arguments));", SHIP)
        for signature in ('"i", model', '"s[16]", direction', '"iffffff", model, dx',
                          '"f", meters', '"f", degreesValue', '"ffff", x, y, z, angle'):
            self.assertIn("sscanf(arguments, " + signature, SHIP)
        self.assertNotIn('sscanf(params, "s[24]', SHIP)
        self.assertIn("ShipEditor_Command(playerid, editParams);", SHIP)
        self.assertIn("ShipPort_Command(playerid, portParams);", SHIP)
        self.assertIn('sscanf(portParams, "s[12]s[16]"', SHIP)
        self.assertIn('sscanf(params, "s[12]s[16]"', PORT)

    def test_status_help_and_persistence(self):
        self.assertIn('if (isnull(params)) return ShipPanel_Open(playerid);', SHIP)
        self.assertIn('GetObjectRot(gShipRoot, rx, ry, yaw);', SHIP)
        self.assertIn('angle %.2f', SHIP)
        for signature in ("Ship_Save()", "Ship_Load()", "Ship_Export()",
                          "Ship_Mark()", "Ship_Reset()", "Ship_Preset(playerid"):
            self.assertIn(signature, SHIP)
        self.assertIn('SHIP_DATA          SHIP_DIR "/cargo_01.dat"', SHIP)
        self.assertIn('SHIP_PORT_FILE SHIP_DIR "/ports_01.dat"', PORT)
        self.assertIn('ShipPort_Save()', PORT)
        self.assertIn('ShipPort_Status(playerid)', PORT)

    def test_port_and_editor_operations(self):
        for operation in ('"berth"', '"exit"', '"depart"'):
            self.assertIn('!strcmp(operation, ' + operation, PORT)
        for place in ('"ls"', '"sf"'):
            self.assertIn('!strcmp(place, ' + place, PORT)
        self.assertIn('if (gShipEditOwner != playerid ||', SHIP)
        self.assertIn('strcmp(operation, "berth", true)', SHIP)
        self.assertIn('strcmp(operation, "exit", true)', SHIP)
        self.assertIn("ShipPort_Command(playerid, op);", EDITOR)
        self.assertIn('!strcmp(params, "save", true)', EDITOR)
        self.assertIn('!strcmp(params, "cancel", true)', EDITOR)
        self.assertIn('!strcmp(params, "start", true)', EDITOR)
        self.assertIn("ShipEditor_End(playerid, true);", EDITOR)
        self.assertIn("ShipEditor_End(playerid, false);", EDITOR)

    def test_every_textdraw_button_has_one_action_slot(self):
        definitions = re.findall(r'#define (SE_[A-Z_]+) \((\d+)\)', EDITOR)
        self.assertEqual(28, len(definitions))
        self.assertEqual(EDITOR_ACTIONS, {name for name, _ in definitions})
        self.assertEqual(set(range(28)), {int(n) for _, n in definitions})
        for action in EDITOR_ACTIONS:
            self.assertIn("ShipEditor_AddButton(playerid, " + action + ",", EDITOR)
        self.assertIn("return ShipEditor_Action(playerid, i);", EDITOR)
        self.assertIn("PlayerTextDrawTextSize(playerid, td, x+w, y+10.0);", EDITOR)
        for action, handler in (("SE_ROUTE_ADD", "ShipManeuver_EditorAdd"),
                                ("SE_ROUTE_UNDO", "ShipManeuver_EditorUndo"),
                                ("SE_ROUTE_PREVIEW", "ShipManeuver_EditorPreview"),
                                ("SE_ROUTE_STOP", "ShipManeuver_EditorStop"),
                                ("SE_ROUTE_SAVE", "ShipManeuver_EditorSave"),
                                ("SE_ROUTE_LOAD", "ShipManeuver_EditorLoad")):
            self.assertIn("case " + action + ": return " + handler, EDITOR)

    def test_panel_routes_follow_legacy_command_handlers(self):
        self.assertIn('return Ship_Command(playerid, command);', PANEL)
        for command in ('port ls depart', 'port sf depart',
                        'preset lssf', 'preset sfls', 'cargo',
                        'save', 'load', 'export', 'release', 'help', 'clear'):
            self.assertIn('ShipPanel_Dispatch(playerid, "' + command + '")', PANEL)
        self.assertIn("ShipPanel_OpenEditor(playerid)", PANEL)
        self.assertIn("ShipCam_Command(playerid)", PANEL)

    def test_unsafe_teleports_and_reset_require_unoccupied_hull(self):
        for action, next_action in (("clear", "preset"), ("set", "mark"), ("run", None)):
            self.assertIn("ShipEditor_Occupied()", action_body(action, next_action),
                          action + " must reject occupied hull")
        self.assertIn("Ship_Stop();", action_body("set", "mark"))
        self.assertIn("Ship_Stop();", action_body("run", None))
        self.assertIn("Ship_Reset();", action_body("clear", "preset"))

    def test_live_routes_cannot_be_edited_in_motion(self):
        for action, next_action in (("add", "move"), ("move", "turn"),
                                    ("turn", "set"), ("mark", "undo"), ("undo", "run")):
            body = action_body(action, next_action)
            self.assertIn("gShipPortDeparture >= 0", body, action)
            self.assertIn("gShipManeuverActive", body, action)
            self.assertIn("gShipRunning", body, action)
        preset = SHIP.split("stock Ship_Preset(playerid, bool:reverse)", 1)[1].split(
            "stock Ship_Help(playerid)", 1)[0]
        self.assertIn("gShipPortDeparture >= 0", preset)
        self.assertIn("gShipManeuverActive", preset)

    def test_camera_and_surf_diagnostics_are_separate(self):
        self.assertIn("if (!Admin_Require(playerid, PERM_MODERATE)) return 1;", CAM)
        self.assertIn("if (!Studio_Require(playerid)) return 1;", SURF)
        for action in ("watch", "off", "status"):
            self.assertIn('!strcmp(action, "' + action + '", true)', SURF)
        self.assertIn("ShipSurf_Status(playerid, targetid)", SURF)
        self.assertIn("ShipSurf_Reset(targetid)", SURF)


if __name__ == "__main__":
    unittest.main()
