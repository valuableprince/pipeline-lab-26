from pathlib import Path
import unittest
s=(Path(__file__).resolve().parents[1]/"gamemodes/includes/commands.inc").read_text("utf-8")
class FounderCommandRoute(unittest.TestCase):
    def test_exact_slash(self):
        self.assertIn('return Command_ExecuteAdmin(playerid, "founder", params);',s)
        self.assertNotIn('return Command_ExecuteAdmin(playerid, "/founder", params);',s)
        self.assertIn('format(input, sizeof(input), "/%s", command);',s)
if __name__=="__main__": unittest.main()
