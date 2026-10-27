"""Prevent collisions between native Ship Studio TextDraw click/hover regions."""
from pathlib import Path
import re
import unittest

EDITOR = (Path(__file__).resolve().parents[1] / "gamemodes/includes/ship_editor.inc").read_text(encoding="utf-8")
BUTTON = re.compile(
    r'ShipEditor_AddButton\(playerid,\s*(SE_[A-Z_]+),\s*'
    r'([\d.]+),\s*([\d.]+),\s*"([^"]+)",\s*([\d.]+)\);'
)

class ShipStudioHitboxes(unittest.TestCase):
    def test_all_buttons_are_nonoverlapping_and_onscreen(self):
        buttons = BUTTON.findall(EDITOR)
        self.assertEqual(len(buttons), 28)
        self.assertEqual(len({b[0] for b in buttons}), 28)
        rects = []
        for slot, x, y, label, w in buttons:
            x, y, w = float(x), float(y), float(w)
            # open.mp font-1 box: origin starts 5 units left / 10 units up.
            left, top, right, bottom = x - 5, y - 10, x + w, y + 19
            self.assertGreaterEqual(left, 0, slot)
            self.assertGreaterEqual(top, 0, slot)
            self.assertLessEqual(right, 640, slot)
            self.assertLessEqual(bottom, 480, slot)
            rects.append((slot, label, left, top, right, bottom))
        for i, (sa, a, ax, ay, ar, ab) in enumerate(rects):
            for sb, b, bx, by, br, bb in rects[i+1:]:
                dx = min(ar, br) - max(ax, bx)
                dy = min(ab, bb) - max(ay, by)
                self.assertFalse(dx > 0 and dy > 0,
                    f"{sa} ({a}) overlaps {sb} ({b}): {dx}x{dy}")

    def test_click_dispatch_and_hitbox_configuration(self):
        self.assertIn('TEXT_DRAW_ALIGN_LEFT', EDITOR)
        self.assertIn('PlayerTextDrawTextSize(playerid, td, x+w, y+19.0)', EDITOR)
        self.assertIn('PlayerTextDrawSetSelectable(playerid, td, true)', EDITOR)
        self.assertIn('return ShipEditor_Action(playerid, i);', EDITOR)
        self.assertIn('case SE_LS_BERTH: format(op, sizeof(op), "ls berth");', EDITOR)
        self.assertIn('case SE_ROUTE_PREVIEW: return ShipManeuver_EditorPreview(playerid);', EDITOR)

if __name__ == "__main__":
    unittest.main()
