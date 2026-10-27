#if defined _tarkenso_console_ru_included
    #endinput
#endif
#define _tarkenso_console_ru_included

// CONSOLE-RU-01: Pawn sources compile as CP1251 for game clients, but Linux
// console/log panels normally consume UTF-8 bytes. Convert ONLY console text;
// do not alter client chat, dialogs, TextDraw, database values or passwords.
// A separate UTF-8 file remains readable even if the hosting panel is broken.
#define CONSOLE_RU_FILE "console_ru_utf8.log"

// Unicode for CP1251 bytes 0x80..0xBF; 0x98 is unassigned -> '?'.
new const gConsoleRU_CP1251[64] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
    0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x003F, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
    0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
    0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457
};

// Output is unpacked Pawn string of UTF-8 bytes. Never write a partial codepoint.
// Input is the existing CP1251 Pawn string (not UTF-8 source file bytes).
stock ConsoleRU_ToUTF8(const source[], destination[], capacity)
{
    if (capacity < 2) return 0;
    new length = 0;
    for (new i = 0; source[i] != EOS; i++)
    {
        new byte = source[i] & 0xFF;
        new codepoint;
        if (byte < 0x80) codepoint = byte;
        else if (byte >= 0xC0) codepoint = 0x0410 + byte - 0xC0;
        else codepoint = gConsoleRU_CP1251[byte - 0x80];
        // Keep each log entry on a single line (no log-injection via newlines).
        if (codepoint < 0x20 || codepoint == 0x7F) codepoint = 0x20;
        new needed = 1;
        if (codepoint >= 0x800) needed = 3;
        else if (codepoint >= 0x80) needed = 2;
        if (length + needed >= capacity) break;
        if (needed == 1) destination[length++] = codepoint;
        else if (needed == 2)
        {
            destination[length++] = 0xC0 | (codepoint >> 6);
            destination[length++] = 0x80 | (codepoint & 0x3F);
        }
        else
        {
            destination[length++] = 0xE0 | (codepoint >> 12);
            destination[length++] = 0x80 | ((codepoint >> 6) & 0x3F);
            destination[length++] = 0x80 | (codepoint & 0x3F);
        }
    }
    destination[length] = EOS;
    return length;
}

stock ConsoleRU_Log(const message[])
{
    // A fixed bound prevents pathological log sizes. print() receives valid
    // UTF-8 bytes; the open.mp logger sends these bytes to stdout and log.txt.
    new text[480];
    new length = ConsoleRU_ToUTF8(message, text, sizeof(text) - 1);
    if (!length) return 0;
    print(text);
    text[length++] = '\n';
    text[length] = EOS;
    new File:out = fopen(CONSOLE_RU_FILE, io_append);
    if (!out) return 0; // Console output remains available.
    new result = fwrite(out, text);
    fclose(out);
    return result > 0;
}

stock ConsoleRU_Init()
{
    // Explicit startup probe: inspect terminal + scriptfiles/console_ru_utf8.log.
    ConsoleRU_Log("[TARKENSO] Проверка кодировки: Привет, мир! Ёж, №1, —.");
    return 1;
}
