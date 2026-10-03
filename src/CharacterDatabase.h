#pragma once
#include "SharedHeader.h"

struct Entry
{
    std::wstring code;
    std::wstring text;
    uint64_t uses{};
};

class CharacterDatabase
{
    std::vector<Entry> m_entries;
public:
    void Seed()
    {
        m_entries = {
            {L"Stop",       L"Θ", 42},
            {L"Long pause", L"—", 31},
            {L"TM",         L"™", 17},
            {L"Copyright",  L"©", 12},
            {L"Degree",     L"°", 10},
            {L"Arrow",      L"→", 8},
            {L"Check",      L"✓", 7},
            {L"Not equal",  L"≠", 4},
            {L"Euro",       L"€", 3},
        };
        Sort();
    }

    const std::vector<Entry>& Sorted() const noexcept { return m_entries; }

    void Use(size_t i)
    {
        if (i >= m_entries.size()) return;
        ++m_entries[i].uses;
        Sort();
    }

private:
    void Sort()
    {
        std::stable_sort(m_entries.begin(), m_entries.end(),
            [](const Entry& a, const Entry& b) {
                return a.uses > b.uses;
            });
    }
};
