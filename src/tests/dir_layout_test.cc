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
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

#include "../include/winpty.h"

namespace {

const wchar_t kProbeRoot[] = L"C:\\chi";

const wchar_t *const kProbeFiles[] = {
    L"__winpty_layout_A_\x4e2d.txt",
    L"__winpty_layout_B_\x6587.txt",
    L"__winpty_layout_C_\x6c49.txt",
    L"__winpty_layout_D_\x5b57.txt",
    L"__winpty_layout_E_\x6d4b.txt",
    L"__winpty_layout_F_\x8bd5.txt",
};

void fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

void check(bool ok, const char *message)
{
    if (!ok) {
        fail(message);
    }
}

std::wstring joinPath(const wchar_t *dir, const wchar_t *leaf)
{
    std::wstring ret(dir);
    ret += L"\\";
    ret += leaf;
    return ret;
}

std::vector<unsigned char> utf8FromWide(const wchar_t *text)
{
    const int wideLen = static_cast<int>(std::wcslen(text));
    const int len = WideCharToMultiByte(
        CP_UTF8, 0, text, wideLen, nullptr, 0, nullptr, nullptr);
    check(len > 0, "WideCharToMultiByte sizing failed");
    std::vector<unsigned char> result(len);
    check(WideCharToMultiByte(
              CP_UTF8, 0, text, wideLen,
              reinterpret_cast<char *>(result.data()), len, nullptr, nullptr),
          "WideCharToMultiByte conversion failed");
    return result;
}

std::string toString(const std::vector<unsigned char> &bytes)
{
    return std::string(bytes.begin(), bytes.end());
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

std::vector<unsigned char> filterContent(
        const std::vector<unsigned char> &content)
{
    std::vector<unsigned char> result;
    auto it = content.begin();
    const auto itEnd = content.end();
    while (it < itEnd) {
        if (*it == '\r') {
            it++;
        } else if (*it == '\x1b' && (it + 1) < itEnd && *(it + 1) == '[') {
            it += 2;
            while (it < itEnd && !std::isalpha(*it)) {
                it++;
            }
            if (it < itEnd) {
                it++;
            }
        } else {
            result.push_back(*it);
            it++;
        }
    }
    return result;
}

std::vector<unsigned char> readAll(HANDLE handle)
{
    unsigned char buf[1024];
    std::vector<unsigned char> result;
    while (true) {
        DWORD amount = 0;
        const BOOL ret = ReadFile(handle, buf, sizeof(buf), &amount, nullptr);
        if (!ret || amount == 0) {
            break;
        }
        result.insert(result.end(), buf, buf + amount);
    }
    return result;
}

std::vector<std::string> splitLines(const std::vector<unsigned char> &content)
{
    std::vector<std::string> lines;
    std::string cur;
    for (const unsigned char ch : content) {
        if (ch == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(static_cast<char>(ch));
        }
    }
    if (!cur.empty()) {
        lines.push_back(cur);
    }
    return lines;
}

void dumpOutput(const std::vector<unsigned char> &content)
{
    const auto lines = splitLines(content);
    std::fprintf(stderr, "captured dir output:\n");
    for (size_t i = 0; i < lines.size(); ++i) {
        std::fprintf(stderr, "%02u: %s\n",
                     static_cast<unsigned int>(i), lines[i].c_str());
    }
}

std::vector<unsigned char> runDirThroughWinpty(UINT64 agentFlags)
{
    wchar_t systemDir[MAX_PATH];
    check(GetSystemDirectoryW(systemDir, MAX_PATH) != 0,
          "GetSystemDirectoryW failed");
    std::wstring cmd(systemDir);
    cmd += L"\\cmd.exe";

    wchar_t cmdline[1024];
    cmdline[0] = L'\0';
    std::wcscat(cmdline, L"\"");
    std::wcscat(cmdline, cmd.c_str());
    std::wcscat(cmdline, L"\" /d /c chcp 936>nul & dir /-c /a:-d C:\\chi");

    winpty_error_ptr_t err = nullptr;
    auto agentCfg = winpty_config_new(agentFlags, &err);
    check(agentCfg != nullptr, "winpty_config_new failed");
    winpty_config_set_initial_size(agentCfg, 80, 25);
    auto pty = winpty_open(agentCfg, &err);
    winpty_config_free(agentCfg);
    check(pty != nullptr, "winpty_open failed");

    HANDLE conin = CreateFileW(
        winpty_conin_name(pty),
        GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    HANDLE conout = CreateFileW(
        winpty_conout_name(pty),
        GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    check(conin != INVALID_HANDLE_VALUE, "opening CONIN pipe failed");
    check(conout != INVALID_HANDLE_VALUE, "opening CONOUT pipe failed");

    auto spawnCfg = winpty_spawn_config_new(
        WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN | WINPTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN,
        cmd.c_str(), cmdline, kProbeRoot, nullptr, &err);
    check(spawnCfg != nullptr, "winpty_spawn_config_new failed");
    HANDLE process = nullptr;
    const BOOL spawnSuccess = winpty_spawn(
        pty, spawnCfg, &process, nullptr, nullptr, &err);
    winpty_spawn_config_free(spawnCfg);
    check(spawnSuccess && process != nullptr, "winpty_spawn failed");

    auto content = filterContent(readAll(conout));

    DWORD exitCode = 0;
    check(GetExitCodeProcess(process, &exitCode) && exitCode == 0,
          "cmd.exe dir did not exit cleanly");
    CloseHandle(process);
    CloseHandle(conin);
    CloseHandle(conout);
    winpty_free(pty);
    return content;
}

void verifyDirLayout(const std::vector<unsigned char> &content)
{
    check(std::find(content.begin(), content.end(), '\0') == content.end(),
          "dir output contains NUL bytes");

    const auto lines = splitLines(content);
    std::vector<std::string> names;
    for (const wchar_t *const leaf : kProbeFiles) {
        names.push_back(toString(utf8FromWide(leaf)));
    }

    size_t fileNameColumn = static_cast<size_t>(-1);
    for (const std::string &name : names) {
        int matchCount = 0;
        std::string matchedLine;
        size_t matchedColumn = 0;
        for (const std::string &line : lines) {
            const size_t pos = line.find(name);
            if (pos == std::string::npos) {
                continue;
            }
            matchCount++;
            matchedLine = line;
            matchedColumn = pos;
        }
        if (matchCount != 1) {
            dumpOutput(content);
            fail("expected each probe filename to appear exactly once");
        }
        for (const std::string &otherName : names) {
            if (otherName == name) {
                continue;
            }
            if (matchedLine.find(otherName) != std::string::npos) {
                dumpOutput(content);
                fail("one dir output line contains multiple probe filenames");
            }
        }
        if (fileNameColumn == static_cast<size_t>(-1)) {
            fileNameColumn = matchedColumn;
            if (fileNameColumn < 30 || fileNameColumn > 55) {
                dumpOutput(content);
                fail("probe filename column is outside the normal dir layout");
            }
        } else if (matchedColumn != fileNameColumn) {
            dumpOutput(content);
            fail("probe filenames are not aligned to the same dir column");
        }
        const size_t expectedLineLength = matchedColumn + name.size();
        if (matchedLine.size() != expectedLineLength) {
            dumpOutput(content);
            fail("dir output line has trailing spill after the filename");
        }
        if (matchedLine.find(' ', expectedLineLength) != std::string::npos) {
            dumpOutput(content);
            fail("dir output line has trailing blanks after the filename");
        }
        if (matchedColumn == 0 || matchedLine[matchedColumn - 1] != ' ') {
            dumpOutput(content);
            fail("probe filename is not preceded by the dir column gap");
        }
    }
}

} // anonymous namespace

int main()
{
    if (!IsValidCodePage(936)) {
        std::fprintf(stderr, "skipping: code page 936 is not installed\n");
        return 0;
    }
    createProbeFiles();
    verifyDirLayout(runDirThroughWinpty(WINPTY_FLAG_PLAIN_OUTPUT));
    verifyDirLayout(runDirThroughWinpty(0));
    deleteProbeFiles();
    return 0;
}
