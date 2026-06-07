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

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

#include "../include/winpty.h"

namespace {

const wchar_t kOutput[] =
    L"\x4e2d A\r\n"  // CJK UNIFIED IDEOGRAPH-4E2D
    L"\x6587 B\r\n"  // CJK UNIFIED IDEOGRAPH-6587
    L"\x6c49 C\r\n"  // CJK UNIFIED IDEOGRAPH-6C49
    L"\x5b57 D\r\n"  // CJK UNIFIED IDEOGRAPH-5B57
    L"\x6d4b E\r\n"  // CJK UNIFIED IDEOGRAPH-6D4B
    L"\x8bd5 F\r\n"  // CJK UNIFIED IDEOGRAPH-8BD5
    L"\x884c G\r\n"  // CJK UNIFIED IDEOGRAPH-884C
    L"\x7ec8 H\r\n"; // CJK UNIFIED IDEOGRAPH-7EC8

const wchar_t kExpected[] =
    L"\x4e2d A\n"
    L"\x6587 B\n"
    L"\x6c49 C\n"
    L"\x5b57 D\n"
    L"\x6d4b E\n"
    L"\x8bd5 F\n"
    L"\x884c G\n"
    L"\x7ec8 H\n";

std::vector<unsigned char> utf8FromWide(const wchar_t *text)
{
    const int wideLen = static_cast<int>(std::wcslen(text));
    const int len = WideCharToMultiByte(
        CP_UTF8, 0, text, wideLen, nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        std::fprintf(stderr, "WideCharToMultiByte sizing failed: %lu\n",
                     GetLastError());
        std::exit(1);
    }
    std::vector<unsigned char> result(len);
    if (!WideCharToMultiByte(
            CP_UTF8, 0, text, wideLen, reinterpret_cast<char *>(result.data()),
            len, nullptr, nullptr)) {
        std::fprintf(stderr, "WideCharToMultiByte conversion failed: %lu\n",
                     GetLastError());
        std::exit(1);
    }
    return result;
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

void dumpBytes(const char *label, const std::vector<unsigned char> &content)
{
    std::fprintf(stderr, "%s (%u bytes):", label,
                 static_cast<unsigned int>(content.size()));
    for (const unsigned char ch : content) {
        std::fprintf(stderr, " %02x", ch);
    }
    std::fprintf(stderr, "\n");
}

void check(bool ok, const char *message)
{
    if (!ok) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

void childTest()
{
    SetConsoleCP(936);
    SetConsoleOutputCP(936);

    DWORD actual = 0;
    const BOOL ret = WriteConsoleW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        kOutput,
        static_cast<DWORD>(std::wcslen(kOutput)),
        &actual,
        nullptr);
    check(ret, "WriteConsoleW failed");
    check(actual == std::wcslen(kOutput), "WriteConsoleW wrote partial output");
    std::exit(42);
}

void parentTest()
{
    wchar_t program[MAX_PATH];
    wchar_t cmdline[MAX_PATH + 16];
    check(GetModuleFileNameW(nullptr, program, MAX_PATH) != 0,
          "GetModuleFileNameW failed");

    cmdline[0] = L'\0';
    std::wcscat(cmdline, L"\"");
    std::wcscat(cmdline, program);
    std::wcscat(cmdline, L"\" CHILD");

    winpty_error_ptr_t err = nullptr;
    auto agentCfg = winpty_config_new(WINPTY_FLAG_PLAIN_OUTPUT, &err);
    check(agentCfg != nullptr, "winpty_config_new failed");
    winpty_config_set_initial_size(agentCfg, 80, 12);
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
        program, cmdline, nullptr, nullptr, &err);
    check(spawnCfg != nullptr, "winpty_spawn_config_new failed");
    HANDLE process = nullptr;
    const BOOL spawnSuccess = winpty_spawn(
        pty, spawnCfg, &process, nullptr, nullptr, &err);
    winpty_spawn_config_free(spawnCfg);
    check(spawnSuccess && process != nullptr, "winpty_spawn failed");

    auto content = filterContent(readAll(conout));

    DWORD exitCode = 0;
    check(GetExitCodeProcess(process, &exitCode) && exitCode == 42,
          "child process did not exit cleanly");
    CloseHandle(process);
    CloseHandle(conin);
    CloseHandle(conout);
    winpty_free(pty);

    const auto expected = utf8FromWide(kExpected);
    if (content != expected) {
        dumpBytes("actual", content);
        dumpBytes("expected", expected);
        std::exit(1);
    }
}

} // anonymous namespace

int main(int argc, char *argv[])
{
    if (argc == 1) {
        parentTest();
    } else {
        childTest();
    }
    return 0;
}
