#pragma once
#include "SharedHeader.h"
#include <algorithm>
#include <cwctype>

struct Entry
{
    std::wstring code; // friendly name / "code"
    std::wstring text; // actual glyph
    uint64_t uses{};
};

class CharacterDatabase
{
    std::vector<Entry> m_entries;

    static std::wstring ToLower(std::wstring s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](wchar_t c) { return std::towlower(c); });
        return s;
    }

public:
    void Seed()
    {
        // Expanded list including mathematical symbols.
        m_entries = {
            {L"Theta",           L"Θ", 42},
            {L"Long dash",       L"—", 31},
            {L"Trademark",       L"™", 17},
            {L"Copyright",       L"©", 12},
            {L"Degree",          L"°", 10},
            {L"Right arrow",     L"→", 8},
            {L"Check",           L"✓", 7},
            {L"Not equal",       L"≠", 4},
            {L"Euro",            L"€", 3},

            // Math symbols
            {L"Plus-minus",      L"±", 20},
            {L"Multiplication",  L"×", 15},
            {L"Division",        L"÷", 14},
            {L"Approximately",   L"≈", 9},
            {L"Less-equal",      L"≤", 9},
            {L"Greater-equal",   L"≥", 9},
            {L"Infinity",        L"∞", 11},
            {L"Integral",        L"∫", 5},
            {L"Summation",       L"∑", 6},
            {L"Product",         L"∏", 4},
            {L"Partial",         L"∂", 3},
            {L"Nabla",           L"∇", 2},
        };
        Sort();
    }

    const std::vector<Entry>& Sorted() const noexcept { return m_entries; }

    // Return a filtered copy of entries matching the query (case-insensitive)
    std::vector<Entry> Filtered(const std::wstring& query) const
    {
        if (query.empty()) return m_entries;
        const auto q = ToLower(query);
        std::vector<Entry> out;
        out.reserve(m_entries.size());
        for (const auto& e : m_entries) {
            const std::wstring codeLower = ToLower(e.code);
            const std::wstring textLower = ToLower(e.text);
            if (codeLower.find(q) != std::wstring::npos ||
                textLower.find(q) != std::wstring::npos) {
                out.push_back(e);
            }
        }
        return out;
    }

    // Increment usage by friendly code name, then re-sort.
    void UseByCode(const std::wstring& code)
    {
        auto it = std::find_if(m_entries.begin(), m_entries.end(),
            [&](const Entry& e) { return e.code == code; });
        if (it == m_entries.end()) return;
        ++it->uses;
        Sort();
    }

    // legacy: increment by index in the underlying storage
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
