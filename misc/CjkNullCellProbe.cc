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

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

namespace {

const int kWidth = 16;
const int kHeight = 6;

HANDLE g_report = INVALID_HANDLE_VALUE;

void emit(const std::string &text)
{
    DWORD actual = 0;
    WriteFile(g_report, text.data(), static_cast<DWORD>(text.size()),
              &actual, nullptr);
}

void emitf(const char *fmt, ...)
{
    char buffer[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    emit(buffer);
}

void check(bool ok, const char *message)
{
    if (!ok) {
        emitf("fatal: %s last_error=%lu\n", message, GetLastError());
        std::exit(1);
    }
}

void clearConsole(HANDLE conout)
{
    CHAR_INFO blank = {};
    blank.Char.UnicodeChar = L' ';
    blank.Attributes = 7;
    std::vector<CHAR_INFO> blanks(kWidth * kHeight, blank);
    SMALL_RECT rect = {0, 0, kWidth - 1, kHeight - 1};
    COORD size = {kWidth, kHeight};
    COORD pos = {0, 0};
    check(WriteConsoleOutputW(conout, blanks.data(), size, pos, &rect),
          "WriteConsoleOutputW clear failed");
    SetConsoleCursorPosition(conout, pos);
}

void writeAt(HANDLE conout, int x, int y, const wchar_t *text)
{
    COORD pos = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
    check(SetConsoleCursorPosition(conout, pos),
          "SetConsoleCursorPosition failed");
    DWORD actual = 0;
    check(WriteConsoleW(conout, text, static_cast<DWORD>(wcslen(text)),
                        &actual, nullptr),
          "WriteConsoleW failed");
}

std::vector<CHAR_INFO> readRect(HANDLE conout, int rows)
{
    std::vector<CHAR_INFO> data(kWidth * rows);
    SMALL_RECT rect = {0, 0, kWidth - 1, static_cast<SHORT>(rows - 1)};
    COORD size = {kWidth, static_cast<SHORT>(rows)};
    COORD pos = {0, 0};
    check(ReadConsoleOutputW(conout, data.data(), size, pos, &rect),
          "ReadConsoleOutputW rect failed");
    return data;
}

std::vector<CHAR_INFO> readRows(HANDLE conout, int rows)
{
    std::vector<CHAR_INFO> data(kWidth * rows);
    for (int y = 0; y < rows; ++y) {
        SMALL_RECT rect = {0, static_cast<SHORT>(y),
                           kWidth - 1, static_cast<SHORT>(y)};
        COORD size = {kWidth, 1};
        COORD pos = {0, 0};
        check(ReadConsoleOutputW(conout, &data[y * kWidth], size, pos, &rect),
              "ReadConsoleOutputW row failed");
    }
    return data;
}

void dumpCells(const char *label, const std::vector<CHAR_INFO> &data, int rows)
{
    emitf("%s\n", label);
    for (int y = 0; y < rows; ++y) {
        emitf("row %d:", y);
        for (int x = 0; x < kWidth; ++x) {
            const CHAR_INFO &cell = data[y * kWidth + x];
            emitf(" %02d=U+%04x/%04x",
                  x,
                  static_cast<unsigned int>(cell.Char.UnicodeChar),
                  static_cast<unsigned int>(cell.Attributes));
        }
        emit("\n");
    }
}

} // anonymous namespace

int main()
{
    g_report = GetStdHandle(STD_OUTPUT_HANDLE);
    check(g_report != INVALID_HANDLE_VALUE, "stdout unavailable");

    FreeConsole();
    check(AllocConsole(), "AllocConsole failed");
    check(SetConsoleOutputCP(936), "SetConsoleOutputCP(936) failed");
    check(SetConsoleCP(936), "SetConsoleCP(936) failed");

    HANDLE conout = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    check(conout != INVALID_HANDLE_VALUE, "opening CONOUT$ failed");

    COORD size = {kWidth, kHeight};
    SetConsoleScreenBufferSize(conout, size);
    clearConsole(conout);

    writeAt(conout, 0, 0, L"A\x4e2d" L"B123456789");     // one CJK char
    writeAt(conout, 0, 1, L"A\x4e2d\x6587" L"B1234567"); // two CJK chars
    writeAt(conout, 0, 2, L"ASCII123456789");

    const int rows = 3;
    const std::vector<CHAR_INFO> multi = readRect(conout, rows);
    const std::vector<CHAR_INFO> byRow = readRows(conout, rows);

    emitf("cp=%u width=%d rows=%d\n", GetConsoleOutputCP(), kWidth, rows);
    dumpCells("multi-row read:", multi, rows);
    dumpCells("row-by-row read:", byRow, rows);

    CloseHandle(conout);
    FreeConsole();
    return 0;
}
