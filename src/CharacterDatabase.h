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
            // General
            {L"Theta",              L"Θ", 42},
            {L"Long dash",          L"—", 31},
            {L"Trademark",          L"™", 17},
            {L"Copyright",          L"©", 12},
            {L"Degree",             L"°", 10},
            {L"Right arrow",        L"→", 8},
            {L"Check",              L"✓", 7},
            {L"Not equal",          L"≠", 4},
            {L"Euro",               L"€", 3},

            // Math - basic
            {L"Plus",               L"+", 30},
            {L"Minus",              L"−", 30},
            {L"Plus-minus",         L"±", 20},
            {L"Minus-plus",         L"∓", 10},
            {L"Multiplication",     L"×", 15},
            {L"Division",           L"÷", 14},
            {L"Dot operator",       L"⋅", 12},
            {L"Bullet operator",    L"∙", 8},
            {L"Approximately",      L"≈", 9},
            {L"Almost equal",       L"≃", 7},
            {L"Equivalent",         L"≡", 8},
            {L"Proportional",       L"∝", 8},
            {L"Less than",          L"<", 12},
            {L"Greater than",       L">", 12},
            {L"Less-equal",         L"≤", 9},
            {L"Greater-equal",      L"≥", 9},
            {L"Much less",          L"≪", 6},
            {L"Much greater",       L"≫", 6},
            {L"Not less",           L"≮", 4},
            {L"Not greater",        L"≯", 4},
            {L"Not approximately",  L"≉", 3},
            {L"Not equivalent",     L"≢", 3},

            // Math - common symbols
            {L"Infinity",           L"∞", 11},
            {L"Integral",           L"∫", 5},
            {L"Double integral",    L"∬", 4},
            {L"Triple integral",    L"∭", 3},
            {L"Contour integral",   L"∮", 4},
            {L"Summation",          L"∑", 6},
            {L"Product",            L"∏", 4},
            {L"Coproduct",          L"∐", 3},
            {L"Partial",            L"∂", 3},
            {L"Nabla",              L"∇", 2},
            {L"Square root",        L"√", 8},
            {L"Cube root",          L"∛", 5},
            {L"Fourth root",        L"∜", 4},
            {L"Therefore",          L"∴", 4},
            {L"Because",            L"∵", 4},
            {L"Proportional to",    L"∝", 5},
            {L"Angle",              L"∠", 6},
            {L"Measured angle",     L"∡", 3},
            {L"Spherical angle",    L"∢", 3},
            {L"Perpendicular",      L"⊥", 5},
            {L"Parallel",           L"∥", 5},
            {L"Congruent",          L"≅", 5},
            {L"Similar",            L"∼", 5},

            // Set theory
            {L"Element of",         L"∈", 10},
            {L"Not element of",     L"∉", 7},
            {L"Contains",           L"∋", 6},
            {L"Does not contain",   L"∌", 5},
            {L"Subset",             L"⊂", 9},
            {L"Subset or equal",    L"⊆", 9},
            {L"Superset",            L"⊃", 8},
            {L"Superset or equal",  L"⊇", 8},
            {L"Not subset",         L"⊄", 4},
            {L"Not subset or equal",L"⊈", 4},
            {L"Not superset",       L"⊅", 4},
            {L"Not superset or equal", L"⊉", 4},
            {L"Empty set",           L"∅", 8},
            {L"Union",               L"∪", 9},
            {L"Intersection",        L"∩", 9},
            {L"Disjoint union",      L"⊎", 5},
            {L"Set difference",      L"∖", 7},
            {L"Symmetric difference",L"△", 5},
            {L"Complement",          L"∁", 4},
            {L"Universal set",       L"𝕌", 3},
            {L"Power set",           L"𝒫", 3},

            // Logic
            {L"Logical not",         L"¬", 8},
            {L"Logical and",         L"∧", 8},
            {L"Logical or",          L"∨", 8},
            {L"Exclusive or",        L"⊕", 6},
            {L"Logical implication", L"⇒", 8},
            {L"Logical equivalence", L"⇔", 8},
            {L"Left implication",    L"⇐", 5},
            {L"Not implication",     L"⇏", 4},
            {L"For all",             L"∀", 9},
            {L"There exists",        L"∃", 9},
            {L"There does not exist",L"∄", 5},
            {L"Truth",               L"⊤", 4},
            {L"False",               L"⊥", 4},

            // Functions / mappings
            {L"Function",            L"ƒ", 7},
            {L"Function f",          L"f", 5},
            {L"Composition",         L"∘", 6},
            {L"Maps to",             L"↦", 8},
            {L"Injection",           L"↪", 5},
            {L"Surjection",           L"↠", 5},
            {L"Embedding",            L"↬", 3},
            {L"Function arrow",      L"→", 8},
            {L"Bidirectional map",   L"↔", 7},
            {L"Long right arrow",    L"⟶", 5},
            {L"Long left arrow",     L"⟵", 5},
            {L"Long double arrow",   L"⟷", 4},

            // Greek lowercase
            {L"Alpha",               L"α", 10},
            {L"Beta",                L"β", 10},
            {L"Gamma",               L"γ", 10},
            {L"Delta",               L"δ", 10},
            {L"Epsilon",             L"ε", 9},
            {L"Varepsilon",           L"ϵ", 7},
            {L"Zeta",                L"ζ", 7},
            {L"Eta",                 L"η", 7},
            {L"Theta",               L"θ", 12},
            {L"Vartheta",             L"ϑ", 7},
            {L"Iota",                L"ι", 7},
            {L"Kappa",               L"κ", 7},
            {L"Lambda",              L"λ", 10},
            {L"Mu",                  L"μ", 8},
            {L"Nu",                  L"ν", 7},
            {L"Xi",                  L"ξ", 8},
            {L"Omicron",             L"ο", 5},
            {L"Pi",                  L"π", 12},
            {L"Rho",                 L"ρ", 7},
            {L"Sigma",               L"σ", 10},
            {L"Final sigma",          L"ς", 5},
            {L"Tau",                 L"τ", 7},
            {L"Upsilon",             L"υ", 7},
            {L"Phi",                 L"φ", 10},
            {L"Varphi",               L"ϕ", 7},
            {L"Chi",                 L"χ", 7},
            {L"Psi",                 L"ψ", 8},
            {L"Omega",               L"ω", 10},

            // Greek uppercase
            {L"Alpha",               L"Α", 7},
            {L"Beta",                L"Β", 7},
            {L"Gamma",               L"Γ", 8},
            {L"Delta",               L"Δ", 10},
            {L"Epsilon",             L"Ε", 5},
            {L"Zeta",                L"Ζ", 5},
            {L"Eta",                 L"Η", 5},
            {L"Theta",               L"Θ", 12},
            {L"Iota",                L"Ι", 5},
            {L"Kappa",               L"Κ", 5},
            {L"Lambda",              L"Λ", 8},
            {L"Mu",                  L"Μ", 5},
            {L"Nu",                  L"Ν", 5},
            {L"Xi",                  L"Ξ", 6},
            {L"Omicron",             L"Ο", 5},
            {L"Pi",                  L"Π", 8},
            {L"Rho",                 L"Ρ", 5},
            {L"Sigma",               L"Σ", 8},
            {L"Tau",                 L"Τ", 5},
            {L"Upsilon",             L"Υ", 5},
            {L"Phi",                 L"Φ", 8},
            {L"Chi",                 L"Χ", 5},
            {L"Psi",                 L"Ψ", 7},
            {L"Omega",               L"Ω", 10},

            // Superscripts
            {L"Superscript zero",    L"⁰", 4},
            {L"Superscript one",     L"¹", 5},
            {L"Superscript two",     L"²", 8},
            {L"Superscript three",   L"³", 8},
            {L"Superscript four",    L"⁴", 4},
            {L"Superscript five",    L"⁵", 4},
            {L"Superscript six",     L"⁶", 4},
            {L"Superscript seven",   L"⁷", 4},
            {L"Superscript eight",   L"⁸", 4},
            {L"Superscript nine",    L"⁹", 4},
            {L"Superscript plus",    L"⁺", 4},
            {L"Superscript minus",   L"⁻", 4},
            {L"Superscript equals",  L"⁼", 3},
            {L"Superscript n",       L"ⁿ", 5},

            // Subscripts
            {L"Subscript zero",      L"₀", 4},
            {L"Subscript one",       L"₁", 4},
            {L"Subscript two",       L"₂", 5},
            {L"Subscript three",     L"₃", 5},
            {L"Subscript four",      L"₄", 4},
            {L"Subscript five",      L"₅", 4},
            {L"Subscript six",       L"₆", 4},
            {L"Subscript seven",     L"₇", 4},
            {L"Subscript eight",     L"₈", 4},
            {L"Subscript nine",      L"₉", 4},
            {L"Subscript plus",      L"₊", 3},
            {L"Subscript minus",     L"₋", 3},
            {L"Subscript equals",    L"₌", 3},
            {L"Subscript n",         L"ₙ", 4},

            // Fractions
            {L"One half",             L"½", 8},
            {L"One third",            L"⅓", 6},
            {L"Two thirds",           L"⅔", 6},
            {L"One quarter",          L"¼", 6},
            {L"Three quarters",       L"¾", 6},
            {L"One fifth",            L"⅕", 5},
            {L"Two fifths",           L"⅖", 4},
            {L"Three fifths",         L"⅗", 4},
            {L"Four fifths",          L"⅘", 4},
            {L"One eighth",            L"⅛", 4},
            {L"Three eighths",        L"⅜", 4},
            {L"Five eighths",         L"⅝", 4},
            {L"Seven eighths",        L"⅞", 4},

            // Arrows
            {L"Left arrow",            L"←", 8},
            {L"Right arrow",           L"→", 8},
            {L"Up arrow",              L"↑", 7},
            {L"Down arrow",            L"↓", 7},
            {L"Left-right arrow",      L"↔", 7},
            {L"Up-down arrow",         L"↕", 6},
            {L"Double left arrow",     L"⇐", 5},
            {L"Double right arrow",    L"⇒", 6},
            {L"Double left-right",     L"⇔", 7},
            {L"Double up arrow",       L"⇑", 4},
            {L"Double down arrow",     L"⇓", 4},
            {L"Left hook arrow",       L"↩", 5},
            {L"Right hook arrow",      L"↪", 5},
            {L"Clockwise arrow",       L"↻", 4},
            {L"Counterclockwise arrow",L"↺", 4},
            {L"North-east arrow",      L"↗", 4},
            {L"North-west arrow",      L"↖", 4},
            {L"South-east arrow",      L"↘", 4},
            {L"South-west arrow",      L"↙", 4},

            // Brackets
            {L"Left parenthesis",      L"(", 8},
            {L"Right parenthesis",     L")", 8},
            {L"Left square bracket",   L"[", 7},
            {L"Right square bracket",  L"]", 7},
            {L"Left curly bracket",    L"{", 7},
            {L"Right curly bracket",   L"}", 7},
            {L"Left angle bracket",    L"⟨", 6},
            {L"Right angle bracket",   L"⟩", 6},
            {L"Left double bracket",   L"⟦", 5},
            {L"Right double bracket",  L"⟧", 5},
            {L"Left floor",            L"⌊", 5},
            {L"Right floor",           L"⌋", 5},
            {L"Left ceiling",          L"⌈", 5},
            {L"Right ceiling",         L"⌉", 5},

            // Japanese / CJK brackets
            {L"Japanese corner bracket left",  L"「", 7},
            {L"Japanese corner bracket right", L"」", 7},
            {L"Japanese double corner left",   L"『", 7},
            {L"Japanese double corner right",  L"』", 7},
            {L"Japanese lenticular left",      L"【", 6},
            {L"Japanese lenticular right",     L"】", 6},
            {L"Japanese white lenticular left", L"〖", 4},
            {L"Japanese white lenticular right",L"〗", 4},
            {L"Japanese tortoise shell left",  L"〔", 5},
            {L"Japanese tortoise shell right", L"〕", 5},
            {L"Japanese double parentheses left",  L"〝", 4},
            {L"Japanese double parentheses right", L"〟", 4},
            {L"Fullwidth left parenthesis",      L"（", 5},
            {L"Fullwidth right parenthesis",     L"）", 5},
            {L"Fullwidth left square bracket",   L"［", 5},
            {L"Fullwidth right square bracket",  L"］", 5},
            {L"Fullwidth left curly bracket",    L"｛", 5},
            {L"Fullwidth right curly bracket",   L"｝", 5},

            // Currency
            {L"Dollar",              L"$", 15},
            {L"Euro",                L"€", 12},
            {L"Pound",               L"£", 10},
            {L"Yen",                 L"¥", 10},
            {L"Yuan",                L"元", 7},
            {L"WON",                 L"₩", 7},
            {L"Rupee",               L"₹", 8},
            {L"Ruble",               L"₽", 7},
            {L"Lira",                L"₺", 6},
            {L"Hryvnia",             L"₴", 5},
            {L"Baht",                L"฿", 5},
            {L"Bitcoin",              L"₿", 7},
            {L"Cent",                L"¢", 7},
            {L"Franc",               L"₣", 3},
            {L"Peso",                L"₱", 6},
            {L"Naira",               L"₦", 5},
            {L"Won",                 L"₩", 6},
            {L"Mill",                L"₥", 2},

            // Miscellaneous mathematical / technical
            {L"Prime",               L"′", 6},
            {L"Double prime",        L"″", 5},
            {L"Triple prime",         L"‴", 4},
            {L"Per mille",            L"‰", 7},
            {L"Per ten thousand",     L"‱", 3},
            {L"Micro",                L"µ", 5},
            {L"Section",              L"§", 7},
            {L"Paragraph",            L"¶", 7},
            {L"Number sign",          L"№", 5},
            {L"Temperature",          L"℃", 5},
            {L"Fahrenheit",           L"℉", 5},
            {L"Angstrom",             L"Å", 5},
            {L"Planck constant",      L"ℏ", 4},
            {L"Euler number",         L"ℯ", 4},
            {L"Imaginary unit",       L"ℑ", 4},
            {L"Real part",             L"ℜ", 4},
            {L"Blackboard R",         L"ℝ", 5},
            {L"Blackboard N",         L"ℕ", 5},
            {L"Blackboard Z",         L"ℤ", 5},
            {L"Blackboard Q",         L"ℚ", 5},
            {L"Blackboard C",         L"ℂ", 5},

            // Geometric symbols
            { L"Triangle",             L"△", 6 },
            {L"White triangle",       L"▷", 4},
            {L"Black triangle",       L"▶", 4},
            {L"White circle",         L"○", 5},
            {L"Black circle",         L"●", 5},
            {L"White square",         L"□", 5},
            {L"Black square",         L"■", 5},
            {L"Diamond",              L"◇", 5},
            {L"Black diamond",        L"◆", 5},
            {L"Star",                 L"★", 5},
            {L"White star",           L"☆", 4},
            {L"Middle dot",           L"·", 7},
            {L"Multiplication cross", L"✕", 5},
            {L"Heavy check",          L"✔", 5},
            {L"Heavy cross",          L"✖", 5},

            // Punctuation / typography
            {L"Ellipsis",             L"…", 10},
            {L"Horizontal ellipsis",  L"⋯", 5},
            {L"Vertical ellipsis",    L"⋮", 4},
            {L"Midline ellipsis",     L"⋰", 3},
            {L"Em dash",              L"—", 10},
            {L"En dash",              L"–", 9},
            {L"Figure dash",          L"‒", 4},
            {L"Minus sign",           L"−", 10},
            {L"Non-breaking hyphen",  L"-", 4},
            {L"Bullet",               L"•", 8},
            {L"Quotation mark left",  L"“", 6},
            {L"Quotation mark right", L"”", 6},
            {L"Single quote left",    L"‘", 5},
            {L"Single quote right",   L"’", 5},
            {L"Guillemets left",      L"«", 5},
            {L"Guillemets right",     L"»", 5},
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
