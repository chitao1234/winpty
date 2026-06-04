// Copyright (c) 2026 Ryan Prichard
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

#include "ConsoleWindow.h"

namespace {

typedef HWND WINAPI GetConsoleWindow_t();

GetConsoleWindow_t *getConsoleWindowProc()
{
    static GetConsoleWindow_t *pGetConsoleWindow = nullptr;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (kernel32 != nullptr) {
            pGetConsoleWindow =
                reinterpret_cast<GetConsoleWindow_t*>(
                    GetProcAddress(kernel32, "GetConsoleWindow"));
        }
    }
    return pGetConsoleWindow;
}

HWND getConsoleWindowByTitle()
{
    wchar_t oldTitle[1024];
    oldTitle[0] = L'\0';
    GetConsoleTitleW(oldTitle, sizeof(oldTitle) / sizeof(oldTitle[0]));

    wchar_t uniqueTitle[128];
    wsprintfW(uniqueTitle,
              L"winpty-console-%lu-%lu",
              static_cast<unsigned long>(GetCurrentProcessId()),
              static_cast<unsigned long>(GetTickCount()));

    if (!SetConsoleTitleW(uniqueTitle)) {
        return nullptr;
    }

    Sleep(100);
    HWND hwnd = FindWindowW(nullptr, uniqueTitle);
    SetConsoleTitleW(oldTitle);
    return hwnd;
}

} // anonymous namespace

HWND getConsoleWindowCompat()
{
    GetConsoleWindow_t *pGetConsoleWindow = getConsoleWindowProc();
    if (pGetConsoleWindow != nullptr) {
        HWND hwnd = pGetConsoleWindow();
        if (hwnd != nullptr) {
            return hwnd;
        }
    }
    return getConsoleWindowByTitle();
}
