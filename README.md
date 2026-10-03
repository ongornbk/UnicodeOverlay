# UnicodeOverlay

A small C++20 / Win32 / Direct3D 12 prototype for keyboard-driven Unicode/symbol insertion.

## Build

Requirements:
- Visual Studio 2022 with Desktop development with C++
- Windows 10/11 SDK
- x64
- No third-party libraries

Open `UnicodeOverlay.sln`, select `Release | x64`, and Build.

## Prototype controls

Default activation key: **Left Alt**.

While holding Left Alt:
- `1`..`9`: select one of the nine displayed entries.
- Up/Down: move selection.
- Enter: commit immediately.
- Escape: cancel.
- Other keyboard input is intercepted and not passed to the foreground application.

On activation, the current foreground HWND is saved. The overlay is `WS_EX_NOACTIVATE`, so it does not take focus. On commit, Unicode is injected with `SendInput`; then the overlay is hidden and the saved foreground window is restored when possible.

The database is initialized with Stop → Θ, Long pause → —, TM → ™. Edit `CharacterDatabase::Seed()` in `src/main.cpp` to add entries.

## Important security/UIPI limitation

`SendInput` is subject to Windows User Interface Privilege Isolation (UIPI). A non-elevated process cannot inject input into a higher-integrity target. The application deliberately remains `asInvoker` and does not request administrator rights. If the target is elevated, injection can fail; the overlay reports this briefly instead of pretending the insertion succeeded.

A global `WH_KEYBOARD_LL` hook is used. The hook callback does only constant-time state checks and posts small messages to the UI thread. It does not perform rendering, allocation, or expensive work. Windows requires the hook thread to pump a message loop.

## Rendering

The overlay uses a minimal D3D12 swap chain and a single shader-based quad. Rendering is event driven: activation, selection, database updates, and DPI changes invalidate the overlay. There is no render loop and no polling timer. The swap chain is presented only after an explicit render request.

For maximum simplicity, the prototype draws table rows as geometry with a solid background and uses a tiny GDI text layer for glyphs. This keeps the D3D12 requirement explicit while avoiding a full text shaping stack. For production-grade complex-script typography, DirectWrite would be the next Windows-only extension.

## Files

- `src/main.cpp` — complete prototype
- `UnicodeOverlay.vcxproj` — VS project
- `UnicodeOverlay.sln` — solution
- `app.manifest` — PerMonitorV2 DPI
- `app.rc` — manifest resource
