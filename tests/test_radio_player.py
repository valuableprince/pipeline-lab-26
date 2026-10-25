"""RADIO-01: контракты каталога /play и владения персональным аудиопотоком."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
RADIO = (ROOT / "gamemodes/includes/radio_player.inc").read_text(encoding="utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
RACE = (ROOT / "gamemodes/includes/race_core.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/RADIO_PLAYER.md").read_text(encoding="utf-8")


class RadioPlayerContractTests(unittest.TestCase):
    def test_ten_direct_streams_and_requested_stations(self):
        urls = re.findall(r'^\s*"(https?://[^"]+)"\s*,?\s*$', RADIO, re.MULTILINE)
        self.assertEqual(len(urls), 10)
        self.assertEqual(len(set(urls)), len(urls))
        self.assertTrue(all(not re.search(r"\.(?:m3u8?|pls|html?)(?:\?|$)", url, re.I)
                            for url in urls))
        for station in ("Европа Плюс", "Ретро FM", "Радио Рекорд",
                        "Радио Орфей", "Radio Swiss Classic", "MDR Klassik",
                        "ANTENNE BAYERN", "Heart 80s", "NPO Radio 2",
                        "Radio Swiss Pop"):
            self.assertIn(station, RADIO)
        self.assertIn("#define RADIO_STATION_COUNT   (10)", RADIO)
        self.assertIn("rr_main96.aacp", RADIO)

    def test_dialog_lifecycle(self):
        self.assertIn("cmd:play(playerid, params[])", COMMANDS)
        self.assertIn("return Radio_Open(playerid);", COMMANDS)
        self.assertIn("DIALOG_STYLE_TABLIST_HEADERS", RADIO)
        self.assertNotIn("DIALOG_STYLE_LIST,", RADIO)
        self.assertNotIn("gRadioCategories", RADIO)
        self.assertIn("gRadioCountries", RADIO)
        self.assertIn('"№\\tРадиостанция\\tСтрана\\n"', RADIO)
        self.assertIn('"%d\\t%s\\t%s\\n"', RADIO)
        self.assertIn('i + 1, gRadioNames[i], gRadioCountries[i]', RADIO)
        self.assertIn('"—\\tВыключить радио\\t—"', RADIO)
        self.assertIn("if (listitem == RADIO_STATION_COUNT)", RADIO)
        countries = re.search(r"new const gRadioCountries\\[\\]\\[\\] =\\s*\\{(.*?)\\};", RADIO, re.DOTALL)
        self.assertIsNotNone(countries)
        self.assertEqual(len(re.findall(r'^\\s*"[^"]+"\\s*,?\\s*
        self.assertIn("if (!response) return 1;", RADIO)
        self.assertIn("if (listitem == RADIO_STATION_COUNT)", RADIO)
        self.assertIn("return Radio_Play(playerid, listitem);", RADIO)

    def test_integration_and_audio_ownership(self):
        self.assertIn('#include "radio_player.inc"', MAIN)
        self.assertIn("Radio_OnConnect(playerid);", MAIN)
        self.assertIn("Radio_OnDisconnect(playerid);", MAIN)
        self.assertIn("Radio_OnDialog(playerid, dialogid, response, listitem)", MAIN)
        self.assertIn("Radio_Stop(playerid);", RACE)
        self.assertIn("GetPlayerVirtualWorld(playerid) == RACE_WORLD_ID", RADIO)
        self.assertIn('strcmp(cmd, "raceaudio", true)', COMMANDS)
        self.assertIn("if (gRadioStation[playerid] != -1 && IsPlayerConnected(playerid))", RADIO)
        self.assertIn("StopAudioStreamForPlayer(playerid);", RADIO)
        self.assertIn("PlayAudioStreamForPlayer(playerid, gRadioUrls[station])", RADIO)
        self.assertIn("gRadioStation[playerid] = -1;", RADIO)

    def test_encoding_and_manual_validation_documented(self):
        RADIO.encode("cp1251")
        self.assertIn("Ручная проверка", DOC)
        self.assertIn("AAC", DOC)
        self.assertIn("не подтверждает", DOC)


if __name__ == "__main__":
    unittest.main()
, countries.group(1), re.MULTILINE)), 10)
        self.assertIn("Швейцария", countries.group(1))
        self.assertIn("Германия", countries.group(1))
        self.assertIn("Великобритания", countries.group(1))
        self.assertIn("Нидерланды", countries.group(1))
        self.assertIn("Выключить радио", RADIO)
        self.assertIn("if (!response) return 1;", RADIO)
        self.assertIn("if (listitem == RADIO_STATION_COUNT)", RADIO)
        self.assertIn("return Radio_Play(playerid, listitem);", RADIO)

    def test_integration_and_audio_ownership(self):
        self.assertIn('#include "radio_player.inc"', MAIN)
        self.assertIn("Radio_OnConnect(playerid);", MAIN)
        self.assertIn("Radio_OnDisconnect(playerid);", MAIN)
        self.assertIn("Radio_OnDialog(playerid, dialogid, response, listitem)", MAIN)
        self.assertIn("Radio_Stop(playerid);", RACE)
        self.assertIn("GetPlayerVirtualWorld(playerid) == RACE_WORLD_ID", RADIO)
        self.assertIn('strcmp(cmd, "raceaudio", true)', COMMANDS)
        self.assertIn("if (gRadioStation[playerid] != -1 && IsPlayerConnected(playerid))", RADIO)
        self.assertIn("StopAudioStreamForPlayer(playerid);", RADIO)
        self.assertIn("PlayAudioStreamForPlayer(playerid, gRadioUrls[station])", RADIO)
        self.assertIn("gRadioStation[playerid] = -1;", RADIO)

    def test_encoding_and_manual_validation_documented(self):
        RADIO.encode("cp1251")
        self.assertIn("Ручная проверка", DOC)
        self.assertIn("AAC", DOC)
        self.assertIn("не подтверждает", DOC)


if __name__ == "__main__":
    unittest.main()
