// Copyright (c) 2026 Xuntao Chi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef CONSOLE_CODE_PAGE_H
#define CONSOLE_CODE_PAGE_H

#include <windows.h>

/*
 * These code pages have classic East Asian DBCS lead-byte ranges and can use
 * the old console's double-cell storage for wide glyphs.  On pre-Windows-8
 * consoles, that storage can make multi-row ReadConsoleOutputW calls collapse
 * rows.  Do not include HZ/GB18030/UTF-8-style transform pages such as 52936,
 * 54936, or 65001 merely because they can encode CJK text; probes on XP/Win7
 * showed those pages storing byte-like cells or no CJK cells instead of the
 * corrupting double-cell layout.
 */
inline bool isEastAsianDbcsCodePage(UINT codePage)
{
    switch (codePage) {
        case 932:   // Shift-JIS, Japanese.
        case 936:   // GBK, Simplified Chinese.
        case 949:   // Unified Hangul, Korean.
        case 950:   // Big5, Traditional Chinese.
        case 1361:  // Johab, Korean.
        case 10001: // Mac Japanese.
        case 10002: // Mac Traditional Chinese Big5.
        case 10003: // Mac Korean.
        case 10008: // Mac Simplified Chinese GB2312.
        case 20000: // CNS, Traditional Chinese.
        case 20001: // TCA, Traditional Chinese.
        case 20002: // Eten, Traditional Chinese.
        case 20003: // IBM5550, Traditional Chinese.
        case 20004: // TeleText, Traditional Chinese.
        case 20005: // Wang, Traditional Chinese.
        case 20932: // JIS X 0208/0212, Japanese.
        case 20936: // GB2312, Simplified Chinese.
        case 51949: // EUC Korean.
            return true;
        default:
            return false;
    }
}

inline bool isJapaneseDbcsCodePage(UINT codePage)
{
    return codePage == 932 ||
        codePage == 10001 ||
        codePage == 20932;
}

inline bool isSimplifiedChineseDbcsCodePage(UINT codePage)
{
    return codePage == 936 ||
        codePage == 10008 ||
        codePage == 20936;
}

inline bool isTraditionalChineseDbcsCodePage(UINT codePage)
{
    return codePage == 950 ||
        (codePage >= 20000 && codePage <= 20005) ||
        codePage == 10002;
}

inline bool isKoreanDbcsCodePage(UINT codePage)
{
    return codePage == 949 ||
        codePage == 1361 ||
        codePage == 10003 ||
        codePage == 51949;
}

#endif // CONSOLE_CODE_PAGE_H
