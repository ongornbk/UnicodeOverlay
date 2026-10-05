# UnicodeOverlay

A lightweight character selection overlay backed by a small SQLite database. The project provides a searchable, multikey character database and a D3D12 overlay for quick insertion of unicode characters.

Features
- Persistent SQLite-backed character store (`characters.db`).
- Multikey lookup via a `keys` table (both friendly `code` and the `text` glyph are usable keys).
- Atomic lookups that increment usage via `UPDATE ... RETURNING`.
- Seedable built-in character list (no duplicates on re-seed).
- Uses UTF-16 SQLite API (`sqlite3_open16`) so `std::wstring` is used end-to-end.

Quick usage
- Open the overlay and:
  - Press Numpad Enter to confirm / close with the numpad.
  - Use arrow keys to navigate the list.
  - Type to filter (search).
  - Press Esc to close.
  - Press standard Enter to insert the highlighted character into the target.
- Searching matches `code`, `text`, and additional keys (case-insensitive).

Installation (Windows 10 x64, Visual Studio)
1. Install `sqlite3` with vcpkg (recommended):
   - `vcpkg install sqlite3:x64-windows`
   - Integrate with Visual Studio: run __vcpkg integrate install__ so projects pick up vcpkg libraries automatically.
2. Open the solution in Visual Studio 2022.
3. Ensure the project platform is `x64` and the C++ standard is set to C++20:
   - __Project Properties > Configuration Properties > C/C++ > Language > C++ Language Standard__
4. Build. Alternatively, manual linkage of `sqlite3.lib` is possible if you prefer not to use vcpkg.

Database details
- DB file: `characters.db` (created in the working directory by the app).
- Tables: `entries (id, code, text, uses)` and `keys (entry_id, key)`.
- `keys.key` is UNIQUE; seeding uses `INSERT OR IGNORE` to avoid duplicate key errors.
- `CharacterDatabase::Seed()` is available to populate the DB programmatically; it checks for existing `code+text` pairs before inserting.

Editing the database
- Edit `characters.db` with your preferred SQLite client. I personally use HeidiSQL. DB Browser for SQLite is also a good lightweight alternative.

Development notes
- See `src\CharacterDatabase.h` for the database API and behavior.
- The project uses UTF-16 SQLite functions and prepared statements with RAII finalization.

Contributing
- Fork, make changes, open a PR. Keep changes small and focused.

License
- Check the repository root for licensing information.
