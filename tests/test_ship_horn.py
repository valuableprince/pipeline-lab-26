"""SHIP-HORN-01: selected sound, playback paths, radio safety and menu."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
HORN=(ROOT/"gamemodes/includes/ship_horn.inc").read_text("utf-8")
PORT=(ROOT/"gamemodes/includes/ship_port.inc").read_text("utf-8")
ROUTE=(ROOT/"gamemodes/includes/ship_port_route.inc").read_text("utf-8")
PANEL=(ROOT/"gamemodes/includes/ship_panel.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
SHIP=(ROOT/"gamemodes/includes/ship_studio.inc").read_text("utf-8")

class ShipHornContracts(unittest.TestCase):
    def test_selected_sound_without_unverified_hotlink(self):
        self.assertIn('Pixabay "ship horn very close" (sound ID 14642)', HORN)
        self.assertIn('horn_url.txt', HORN)
        self.assertIn('SHIP_HORN_DEFAULT_URL "https://tarkenso.ru/assets/audio/ship-horn.mp3"', HORN)
        self.assertIn('Using project-hosted ship horn MP3.', HORN)
        self.assertIn('GetObjectPos(gShipRoot, x, y, z);', HORN)
        self.assertIn('stock ShipHorn_Load()', HORN)
        self.assertIn('ShipHorn_URLValid', HORN)
        self.assertIn('https://', HORN)
        self.assertIn('.mp3', HORN)
        self.assertNotIn('cdn.pixabay.com/audio', HORN)
        self.assertIn('#include "ship_horn.inc"', MAIN)
        self.assertIn('ShipHorn_Init();', MAIN)

    def test_stream_only_to_nearby_audience(self):
        for flag in ('IsPlayerConnected(playerid)', 'gAuthState[playerid] != AUTH_READY',
                     'GetPlayerVirtualWorld(playerid) != 0',
                     'GetPlayerInterior(playerid) != 0',
                     'gRadioStation[playerid] != -1',
                     'Race_IsParticipant(playerid)', 'GetPlayerPos(playerid',
                     'SHIP_HORN_RADIUS', 'SHIP_HORN_COOLDOWN'):
            self.assertIn(flag, HORN)
        self.assertIn('PlayAudioStreamForPlayer(playerid, gShipHornURL,', HORN)
        self.assertNotIn('StopAudioStreamForPlayer(', HORN)
        self.assertNotIn('SetTimer(', HORN)
        self.assertNotIn('PlayerPlaySound(', HORN)
        self.assertNotIn('SetObjectPos(', HORN)

    def test_automatic_departure_and_arrival(self):
        self.assertEqual(PORT.count('ShipHorn_Play();'), 2)
        self.assertIn('ShipHorn_Play(); // berth reached', ROUTE)
        self.assertIn('forward ShipHorn_Play();', SHIP)

    def test_manual_menu_no_extra_command(self):
        self.assertIn('Подать корабельный гудок', PANEL)
        self.assertIn('case 4: ShipHorn_Manual(playerid);', PANEL)
        self.assertIn('if (listitem < 0 || listitem > 4) return 1;', PANEL)
        self.assertIn('Studio_Require(playerid)', HORN)
        self.assertIn('stock ShipHorn_Manual(playerid)', HORN)
        self.assertIn('if (!gShipHornURL[0]) return -1;', HORN)

if __name__ == '__main__':
    unittest.main()
