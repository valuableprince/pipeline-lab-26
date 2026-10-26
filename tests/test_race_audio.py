"""RACE-AUDIO-01: интеграционные регрессии и лицензированный MP3."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
AUDIO = (ROOT / "gamemodes/includes/race_audio.inc").read_text(encoding="utf-8")
CORE = (ROOT / "gamemodes/includes/race_core.inc").read_text(encoding="utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/RADIO_PLAYER.md").read_text(encoding="utf-8")


class RaceAudioTests(unittest.TestCase):
    def test_supported_direct_audio_and_license(self):
        self.assertIn("Roger_That,_Elbucky_(Remastered).mp3", AUDIO)
        self.assertIn("Stab_\'em_with_Your_Fork.mp3", AUDIO)
        self.assertNotIn("youtube.com", AUDIO)
        self.assertNotIn("soundcloud.com", AUDIO)
        self.assertNotIn("music.apple.com", AUDIO)
        self.assertIn("BarkTunes", DOC)
        self.assertIn("creativecommons.org/licenses/by/4.0/", DOC)
        self.assertIn("Реальный UAT необходим", DOC)

    def test_playback_lifecycle(self):
        self.assertIn("RaceAudio_Begin(playerid);", CORE)
        self.assertIn("RaceAudio_Stop(playerid);", CORE)
        self.assertIn("RaceAudio_Tick(playerid, gettime() - gRaceStartAt);", CORE)
        self.assertIn("RaceAudio_PreviewTick();", CORE)
        self.assertIn("StopAudioStreamForPlayer(playerid);", AUDIO)
        self.assertIn("PlayAudioStreamForPlayer(playerid, RACE_AUDIO_TRACK_A)", AUDIO)
        self.assertIn("PlayAudioStreamForPlayer(playerid, RACE_AUDIO_TRACK_B)", AUDIO)
        self.assertIn("elapsed >= RACE_AUDIO_TRACK_A_SEC", AUDIO)
        self.assertIn("#define RACE_AUDIO_TRACK_A_SEC   (150)", AUDIO)

    def test_state_guards_and_credits(self):
        self.assertIn("gRaceAudioPlaying[playerid] && IsPlayerConnected(playerid)", AUDIO)
        self.assertIn("gRaceAudioEnabled[playerid] = true;", AUDIO)
        self.assertIn("gRaceAudioEnabled[playerid] = false;", AUDIO)
        self.assertIn("gRaceAudioPreviewUntil[playerid] = gettime() + RACE_AUDIO_PREVIEW_SEC", AUDIO)
        self.assertIn("now >= gRaceAudioPreviewUntil[playerid]", AUDIO)
        self.assertIn("RaceAudio_OnConnect(playerid);", CORE)
        self.assertIn("RaceAudio_OnDisconnect(playerid);", CORE)
        self.assertIn("if (inRace)", AUDIO)
        for term in ('"on"', '"off"', '"test"', '"status"', '"credits"'):
            self.assertIn(term, AUDIO)

    def test_command_and_build(self):
        self.assertIn('#include "race_audio.inc"\n#include "race_progress.inc"\n#include "race_core.inc"', MAIN)
        self.assertIn("cmd:raceaudio(playerid, params[])", CORE)
        self.assertIn('strcmp(cmd, "raceaudio", true)', COMMANDS)
        for path in ("tools/build_windows.py", ".github/workflows/build.yml"):
            c = (ROOT / path).read_text(encoding="utf-8")
            self.assertIn('"race_audio.inc"', c)
        AUDIO.encode("cp1251")


if __name__ == "__main__":
    unittest.main()
