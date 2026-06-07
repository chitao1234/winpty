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

inline bool isCjkCodePage(UINT codePage)
{
    switch (codePage) {
        case 932:   // Shift-JIS, Japanese
        case 936:   // GBK, Simplified Chinese
        case 949:   // Unified Hangul, Korean
        case 950:   // Big5, Traditional Chinese
        case 1361:  // Johab, Korean
        case 54936: // GB18030, Simplified Chinese
            return true;
        default:
            return false;
    }
}

#endif // CONSOLE_CODE_PAGE_H
