"""RACE-PLAYLIST-02: базовые регрессии лицензируемого набора."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
PLAYLIST = (ROOT / "gamemodes/includes/race_playlist.inc").read_text(encoding="utf-8")
AUDIO = (ROOT / "gamemodes/includes/race_audio.inc").read_text(encoding="utf-8")
CORE = (ROOT / "gamemodes/includes/race_core.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/RADIO_PLAYER.md").read_text(encoding="utf-8")
CONFIG = (ROOT / "config/race_playlist.example.cfg").read_text(encoding="utf-8")


class RacePlaylistTest(unittest.TestCase):
    def test_requested_tracks(self):
        for term in ("Vielleicht Vielleicht", "Holy Priest & elMefti Remix",
                     "Vois sur ton chemin", "Derniere danse", "Heute Nacht",
                     "Insomnia", "Rave Never Ends", "MilleniumKid & JBs",
                     "BENNETT", "Indila & BENNETT", "Maddix",
                     "Restricted & NIKSTER", "Novah"):
            self.assertIn(term, PLAYLIST)
        self.assertIn("#define RACE_PLAYLIST_COUNT       (6)", PLAYLIST)
        PLAYLIST.encode("cp1251")

    def test_safe_fallback_when_not_configured(self):
        self.assertIn('fopen(RACE_PLAYLIST_FILE, io_read)', PLAYLIST)
        self.assertIn("RacePlaylist_Reset();", PLAYLIST)
        self.assertIn("if (!file)", PLAYLIST)
        self.assertIn("if (!valid)", PLAYLIST)
        self.assertIn("gRacePlaylistReady = true;", PLAYLIST)
        self.assertIn("if (valid && fread(file, line, sizeof(line))) valid = false;", PLAYLIST)
        self.assertIn('https://tarkenso.ru/', PLAYLIST)
        self.assertIn('https://audio.tarkenso.ru/', PLAYLIST)
        self.assertIn("gRacePlaylistSeconds[slot] = duration;", PLAYLIST)
        self.assertEqual(len(CONFIG.strip().splitlines()), 6)
        self.assertTrue(all(line.startswith("0|REPLACE") for line in CONFIG.splitlines()))
        self.assertIn("Roger_That,_Elbucky_(Remastered).mp3", AUDIO)
        self.assertIn("Stab_\'em_with_Your_Fork.mp3", AUDIO)

    def test_lifecycle_and_world(self):
        self.assertIn('#include "race_playlist.inc"\n#include "race_audio.inc"', MAIN)
        self.assertIn("RacePlaylist_Load();", CORE)
        self.assertIn("RacePlaylist_Reset();", CORE)
        self.assertIn("RaceAudio_OnRaceStart();", CORE)
        self.assertIn("RacePlaylist_StartingStage(gRaceAudioRound)", AUDIO)
        self.assertIn("RacePlaylist_NextStage(gRaceAudioStage[playerid])", AUDIO)
        self.assertIn("RacePlaylist_StageDuration(next)", AUDIO)
        self.assertIn("RaceAudio_Stop(playerid);", CORE)
        self.assertIn("#define RACE_WORLD_ID            (1800)", (ROOT / "gamemodes/includes/race_map.inc").read_text(encoding="utf-8"))
        self.assertIn("#define RACE_MIN_PLAYERS       (1)", CORE)

    def test_controls_and_rights_not_assumed(self):
        self.assertIn('cmd:raceaudio(playerid, params[])', CORE)
        self.assertIn('if (!strcmp(params, "playlist", true))', AUDIO)
        self.assertIn("PlayAudioStreamForPlayer(playerid, gRacePlaylistUrls[stage - 1])", AUDIO)
        self.assertNotIn("music.apple.com", PLAYLIST)
        self.assertIn("не означает права", DOC)
        for file in ("tools/build_windows.py", ".github/workflows/build.yml"):
            self.assertIn('"race_playlist.inc"', (ROOT / file).read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
