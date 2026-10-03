#pragma once
#include "SharedHeader.h"

class UnicodeInjector
{
public:
    static bool Insert(const std::wstring& text)
    {
        if (text.empty()) return true;

        std::vector<INPUT> inputs;
        inputs.reserve(text.size() * 2);
        for (wchar_t ch : text) {
            INPUT down{};
            down.type = INPUT_KEYBOARD;
            down.ki.wVk = 0;
            down.ki.wScan = static_cast<WORD>(ch);
            down.ki.dwFlags = KEYEVENTF_UNICODE;
            inputs.push_back(down);

            INPUT up = down;
            up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            inputs.push_back(up);
        }
        const UINT sent = SendInput(static_cast<UINT>(inputs.size()),
            inputs.data(), sizeof(INPUT));
        return sent == inputs.size();
    }
};