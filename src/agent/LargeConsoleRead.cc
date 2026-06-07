// Copyright (c) 2015 Ryan Prichard
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

#include "LargeConsoleRead.h"

#include <algorithm>
#include <stdlib.h>

#include "../shared/WindowsVersion.h"
#include "ConsoleCodePage.h"
#include "Scraper.h"
#include "Win32ConsoleBuffer.h"

namespace {

const WORD kDefaultBlankAttributes =
    FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;

static WORD blankAttributesForNulCell(
        const CHAR_INFO *line,
        int width,
        int col)
{
    for (int i = col - 1; i >= 0; --i) {
        if (line[i].Attributes != 0) {
            return line[i].Attributes;
        }
    }
    for (int i = col + 1; i < width; ++i) {
        if (line[i].Attributes != 0) {
            return line[i].Attributes;
        }
    }
    return kDefaultBlankAttributes;
}

} // anonymous namespace

LargeConsoleReadBuffer::LargeConsoleReadBuffer() :
    m_rect(0, 0, 0, 0), m_rectWidth(0)
{
}

void largeConsoleRead(LargeConsoleReadBuffer &out,
                      Win32ConsoleBuffer &buffer,
                      const SmallRect &readArea,
                      WORD attributesMask) {
    ASSERT(readArea.Left >= 0 &&
           readArea.Top >= 0 &&
           readArea.Right >= readArea.Left &&
           readArea.Bottom >= readArea.Top &&
           readArea.width() <= MAX_CONSOLE_WIDTH);
    const size_t count = readArea.width() * readArea.height();
    if (out.m_data.size() < count) {
        out.m_data.resize(count);
    }
    out.m_rect = readArea;
    out.m_rectWidth = readArea.width();

    const bool useLargeReads = isAtLeastWindows8();
    /*
     * On XP CJK raster-font consoles, ReadConsoleOutputW collapses full-width
     * characters when reading a multi-row rectangle, shifting data from later
     * rows into earlier rows.  Reading one row at a time keeps the rows
     * intact.  Normalize the trailing NUL fill cells to blanks below.
     */
    const int maxReadLines =
        !useLargeReads && isCjkCodePage(GetConsoleOutputCP())
            ? 1
            : std::max(1, MAX_CONSOLE_WIDTH / readArea.width());
    if (useLargeReads) {
        buffer.read(readArea, out.m_data.data());
    } else {
        int curLine = readArea.Top;
        while (curLine <= readArea.Bottom) {
            const SmallRect subReadArea(
                readArea.Left,
                curLine,
                readArea.width(),
                std::min(maxReadLines, readArea.Bottom + 1 - curLine));
            buffer.read(subReadArea, out.lineDataMut(curLine));
            curLine = subReadArea.Bottom + 1;
        }
    }
    for (int line = readArea.Top; line <= readArea.Bottom; ++line) {
        CHAR_INFO *const lineData = out.lineDataMut(line);
        for (int col = 0; col < readArea.width(); ++col) {
            CHAR_INFO &cell = lineData[col];
            if (cell.Char.UnicodeChar == L'\0') {
                cell.Char.UnicodeChar = L' ';
                if (cell.Attributes == 0) {
                    cell.Attributes =
                        blankAttributesForNulCell(lineData, readArea.width(), col);
                }
            }
        }
    }
    if (attributesMask != static_cast<WORD>(~0)) {
        for (size_t i = 0; i < count; ++i) {
            out.m_data[i].Attributes &= attributesMask;
        }
    }
}
