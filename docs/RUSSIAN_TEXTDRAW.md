# TARKENSO: Russian TextDraw and rustext

## Choice and compatibility

Plugin: **ziggi/rustext v2.0.11 nomemhack**, recommended in the [open.mp installation guide](https://open.mp/docs/server/Installation).
Use the *nomemhack* release, not the old memory-hook build or CRP.

Verified upstream release: https://github.com/ziggi/rustext/releases/tag/v2.0.11

Archive: https://github.com/ziggi/rustext/releases/download/v2.0.11/rustext-2.0.11-nomemhack.zip

SHA-256: b51e5d55492ff5e249665270333342131070cd0d171f43ad9893a8d563b2638b

The plugin converts textdraw/game text CP1251 characters at the outgoing RPC layer. A Pawn include is **not required** for default mode (SanLtd mapping). Other client russifier variants may require selecting a per-player mapping later. Server-side conversion cannot install missing glyph textures/fonts on an unmodified game client. The visual result is not guaranteed merely by successful Pawn compilation.

## Installation on an existing server

1. Stop the test server and back up its config.json and plugins directory.
2. From a fresh checkout with Python 3 and Internet access:

   Windows:
   ```powershell
   py tools/install_rustext.py --server "D:\TARKENSO\server" --platform windows
   ```

   Linux:
   ```bash
   python3 tools/install_rustext.py --server /path/to/openmp --platform linux
   ```

3. The installer fetches only the pinned official archive, verifies its SHA-256, extracts rustext.dll (Windows) or rustext.so (Linux) to the server's plugins directory, then appends rustext to the existing pawn.legacy_plugins list in config.json, retaining preexisting entries such as mysql.
4. Restart the server and check its startup log for rustext v2.0.11 and the absence of load/init errors. Never place this legacy plugin into components.
5. At the Fort Carson recycling station use /sort or ALT. Check **СОРТИРОВКА ОТХОДОВ** and the Russian category, hint and status text (correct, error, selection) with a regular game client. Category labels are now Russian: БУМАГА / ПЛАСТИК / МЕТАЛЛ.
6. Completing the sixth item must immediately hide the panel and restore ordinary mouse controls. Success and error count should remain visible in the chat. Verify a second client/installation. If text appears as incorrect symbols, the rustext default SanLtd mapping may not match that client's font resources. Do not declare Cyrillic fixed until on-client UAT.

The GitHub build converts UTF-8 source .pwn/.inc files to CP1251 before Pawn compilation. Do not remove this step. No database changes are necessary.

## Rollback

Stop open.mp. Restore config.json.before-rustext.bak (or remove only rustext from pawn.legacy_plugins if config subsequently changed). Remove the rustext library, or restore the .before-rustext.bak version if the library was replaced. To revert the Russian TextDraw content, deploy the pre-change AMX.

## Boundaries

The installer does not access your hosting account or deploy automatically. If your host has no terminal/Python, use the verified upstream package manually and edit its config.json. The Github Actions compile only checks Pawn and test contracts; it cannot validate actual GTA font rendering.
