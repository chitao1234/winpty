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

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <sstream>
#include <string>
#include <vector>

namespace {

const int kWidth = 80;
const int kHeight = 80;
const wchar_t kProbeRoot[] = L"C:\\chi";
const wchar_t *const kProbeFiles[] = {
    L"__winpty_layout_A_\x4e2d.txt",
    L"__winpty_layout_B_\x6587.txt",
    L"__winpty_layout_C_\x6c49.txt",
    L"__winpty_layout_D_\x5b57.txt",
    L"__winpty_layout_E_\x6d4b.txt",
    L"__winpty_layout_F_\x8bd5.txt",
};

HANDLE g_report = INVALID_HANDLE_VALUE;

std::wstring joinPath(const wchar_t *dir, const wchar_t *leaf)
{
    std::wstring ret(dir);
    ret += L"\\";
    ret += leaf;
    return ret;
}

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

void deleteProbeFiles()
{
    for (const wchar_t *const leaf : kProbeFiles) {
        DeleteFileW(joinPath(kProbeRoot, leaf).c_str());
    }
}

void createProbeFiles()
{
    if (!CreateDirectoryW(kProbeRoot, nullptr)) {
        check(GetLastError() == ERROR_ALREADY_EXISTS,
              "failed to create C:\\chi");
    }
    deleteProbeFiles();
    for (const wchar_t *const leaf : kProbeFiles) {
        const std::wstring path = joinPath(kProbeRoot, leaf);
        HANDLE file = CreateFileW(
            path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        check(file != INVALID_HANDLE_VALUE, "failed to create probe file");
        CloseHandle(file);
    }
}

std::string cellText(const CHAR_INFO &cell)
{
    const wchar_t ch = cell.Char.UnicodeChar;
    if (ch == L'\0') {
        return "<0000>";
    }
    if (ch >= 0x20 && ch <= 0x7e) {
        return std::string(1, static_cast<char>(ch));
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "<%04x>", static_cast<unsigned int>(ch));
    return buf;
}

bool isBlankCell(const CHAR_INFO &cell)
{
    return cell.Char.UnicodeChar == L'\0' || cell.Char.UnicodeChar == L' ';
}

std::string summarizeRow(const CHAR_INFO *row)
{
    int last = kWidth - 1;
    while (last >= 0 && isBlankCell(row[last])) {
        last--;
    }
    if (last < 0) {
        return "";
    }
    std::string ret;
    for (int i = 0; i <= last; ++i) {
        ret += cellText(row[i]);
    }
    return ret;
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

void runDir()
{
    wchar_t systemDir[MAX_PATH];
    check(GetSystemDirectoryW(systemDir, MAX_PATH) != 0,
          "GetSystemDirectoryW failed");
    std::wstring cmd(systemDir);
    cmd += L"\\cmd.exe";

    wchar_t cmdline[1024];
    cmdline[0] = L'\0';
    wcscat(cmdline, L"\"");
    wcscat(cmdline, cmd.c_str());
    wcscat(cmdline, L"\" /d /c chcp 936>nul & dir /-c /a:-d C:\\chi");

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    check(CreateProcessW(cmd.c_str(), cmdline, nullptr, nullptr, TRUE, 0,
                         nullptr, kProbeRoot, &si, &pi),
          "CreateProcessW cmd.exe failed");
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    check(exitCode == 0, "cmd.exe dir failed");
}

std::vector<CHAR_INFO> readMulti(HANDLE conout, int rows)
{
    std::vector<CHAR_INFO> data(kWidth * rows);
    SMALL_RECT rect = {0, 0, kWidth - 1, static_cast<SHORT>(rows - 1)};
    COORD size = {kWidth, static_cast<SHORT>(rows)};
    COORD pos = {0, 0};
    check(ReadConsoleOutputW(conout, data.data(), size, pos, &rect),
          "multi-row ReadConsoleOutputW failed");
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
              "single-row ReadConsoleOutputW failed");
    }
    return data;
}

bool rowHasProbeFile(const CHAR_INFO *row)
{
    const std::string summary = summarizeRow(row);
    return summary.find("__winpty_layout_") != std::string::npos;
}

int findProbePrefix(const CHAR_INFO *row)
{
    const char prefix[] = "__winpty_layout_";
    const int prefixLen = sizeof(prefix) - 1;
    for (int x = 0; x <= kWidth - prefixLen; ++x) {
        bool match = true;
        for (int i = 0; i < prefixLen; ++i) {
            if (row[x + i].Char.UnicodeChar != prefix[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            return x;
        }
    }
    return -1;
}

void dumpCells(const char *label, int y, const CHAR_INFO *row, int first, int last)
{
    emitf("cells row %02d %s:", y, label);
    for (int x = first; x <= last; ++x) {
        emitf(" [%02d]=U+%04x/%04x",
              x,
              static_cast<unsigned int>(row[x].Char.UnicodeChar),
              static_cast<unsigned int>(row[x].Attributes));
    }
    emit("\n");
}

} // anonymous namespace

int main()
{
    g_report = GetStdHandle(STD_OUTPUT_HANDLE);
    check(g_report != INVALID_HANDLE_VALUE, "stdout unavailable");

    createProbeFiles();

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
    runDir();

    CONSOLE_SCREEN_BUFFER_INFO info = {};
    check(GetConsoleScreenBufferInfo(conout, &info),
          "GetConsoleScreenBufferInfo failed");
    const int rows = std::min<int>(kHeight, info.dwCursorPosition.Y + 1);
    const std::vector<CHAR_INFO> multi = readMulti(conout, rows);
    const std::vector<CHAR_INFO> byRow = readRows(conout, rows);

    int diffCount = 0;
    for (int i = 0; i < kWidth * rows; ++i) {
        if (multi[i].Char.UnicodeChar != byRow[i].Char.UnicodeChar ||
                multi[i].Attributes != byRow[i].Attributes) {
            diffCount++;
        }
    }

    emitf("cp=%u rows=%d cursor=(%d,%d) multi_vs_row_diffs=%d\n",
          GetConsoleOutputCP(), rows, info.dwCursorPosition.X,
          info.dwCursorPosition.Y, diffCount);

    int printedDiffs = 0;
    for (int y = 0; y < rows && printedDiffs < 40; ++y) {
        for (int x = 0; x < kWidth && printedDiffs < 40; ++x) {
            const CHAR_INFO &m = multi[y * kWidth + x];
            const CHAR_INFO &r = byRow[y * kWidth + x];
            if (m.Char.UnicodeChar != r.Char.UnicodeChar ||
                    m.Attributes != r.Attributes) {
                emitf("diff row=%d col=%d multi=U+%04x/%04x row=U+%04x/%04x\n",
                      y, x, static_cast<unsigned int>(m.Char.UnicodeChar),
                      static_cast<unsigned int>(m.Attributes),
                      static_cast<unsigned int>(r.Char.UnicodeChar),
                      static_cast<unsigned int>(r.Attributes));
                printedDiffs++;
            }
        }
    }

    for (int y = 0; y < rows; ++y) {
        const CHAR_INFO *m = &multi[y * kWidth];
        const CHAR_INFO *r = &byRow[y * kWidth];
        if (rowHasProbeFile(m) || rowHasProbeFile(r)) {
            emitf("row %02d multi: %s\n", y, summarizeRow(m).c_str());
            emitf("row %02d row:   %s\n", y, summarizeRow(r).c_str());
            const int prefixCol = findProbePrefix(r);
            if (prefixCol >= 0) {
                const int cjkCol = prefixCol + 17;
                const int first = std::max(0, cjkCol - 3);
                const int last = std::min(kWidth - 1, cjkCol + 4);
                dumpCells("multi", y, m, first, last);
                dumpCells("row", y, r, first, last);
            }
        }
    }

    CloseHandle(conout);
    FreeConsole();
    deleteProbeFiles();
    return 0;
}
