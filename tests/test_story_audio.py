"""STORY-AUDIO-01: optional cassette voiceover with text fallback."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
STORY = (ROOT / "gamemodes/includes/story_prologue.inc").read_text("utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
SCRIPT = (ROOT / "docs/room17_cassette.md").read_text("utf-8")
CONFIG = (ROOT / "scriptfiles/story_tape.cfg.example").read_text("utf-8")


class StoryAudioContract(unittest.TestCase):
    def test_optional_private_audio_stream(self):
        self.assertIn('#define STORY_TAPE_CONFIG      "story_tape.cfg"', STORY)
        self.assertIn('fopen(STORY_TAPE_CONFIG, io_read)', STORY)
        self.assertIn("Story_TapeInit();", MAIN)
        self.assertIn("https://audio.tarkenso.ru/", STORY)
        self.assertIn("https://tarkenso.ru/", STORY)
        self.assertIn('strfind(url, ".mp3"', STORY)
        self.assertIn('strfind(url, ".ogg"', STORY)
        self.assertIn("PlayAudioStreamForPlayer(playerid, gStoryTapeUrl,", STORY)
        self.assertIn("gStoryLocations[1][0]", STORY)
        self.assertIn("18.0, true", STORY)
        self.assertIn("Story_TapeStop(playerid);", STORY)
        self.assertIn("StopAudioStreamForPlayer(playerid)", STORY)

    def test_story_not_blocked_without_acted_mp3(self):
        self.assertIn("if (gStoryTapeUrl[0])", STORY)
        self.assertIn("else\n                ShowPlayerDialog(playerid, STORY_DIALOG_ROOM", STORY)
        self.assertIn("Плёнка (текстовая версия)", STORY)
        self.assertIn("STORY_DIALOG_TAPE", STORY)
        self.assertIn("if (Story_TapePlay(playerid)) return 1;", STORY)
        self.assertIn("Story_Advance(playerid, expected)", STORY)
        self.assertIn("if (!Story_TapePlay(playerid))", STORY)
        self.assertIn("return 1;", STORY)
        self.assertIn("Story_TapeStop(playerid);\n    Story_ClearMarker", STORY)
        self.assertIn("Story_TapeStop(playerid);\n    if (gAuthState", STORY)

    def test_script_and_config_are_not_fake_asset(self):
        self.assertIn("НОРА", SCRIPT)
        self.assertIn("Контур", SCRIPT)
        self.assertIn("MP3", SCRIPT)
        self.assertIn("файл ещё не опубликован", SCRIPT)
        self.assertIn("https://audio.tarkenso.ru/story/room17-cassette.mp3", CONFIG)
        self.assertFalse((ROOT / "scriptfiles/story_tape.cfg").exists())


if __name__ == "__main__":
    unittest.main()
