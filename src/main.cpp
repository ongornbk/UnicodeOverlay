#include "SharedHeader.h"

#include <shellscalingapi.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shcore.lib")

#include "D3D12OverlayRenderer.h"

namespace {
constexpr UINT WMU_ACTIVATE = WM_APP + 1;
constexpr UINT WMU_KEY = WM_APP + 2;
constexpr UINT WMU_DEACTIVATE = WM_APP + 3;
constexpr UINT WMU_RENDER = WM_APP + 4;

constexpr wchar_t kClassName[] = L"UnicodeOverlayWindow";


class UnicodeInjector {
public:
    // VK_PACKET + KEYEVENTF_UNICODE sends UTF-16 code units. This is not
    // the legacy numeric Alt-code mechanism and supports non-ASCII Unicode.
    static bool Insert(const std::wstring& text) {
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

class App;

class KeyboardManager {
public:
    explicit KeyboardManager(App& app) : m_app(app) {}
    bool Start();
    void Stop();
    bool IsActive() const noexcept { return m_active.load(std::memory_order_acquire); }
    void SetActivationVk(UINT vk) noexcept { m_activationVk = vk; }

    // Called by the UI thread when the overlay ends so the hook stops swallowing keys.
    void SetOverlayActive(bool active) noexcept {
        m_active.store(active, std::memory_order_release);
        if (!active) m_activationDown = false;
    }

private:
    static LRESULT CALLBACK HookProc(int, WPARAM, LPARAM);
    void ThreadMain();
    void HandleHook(const KBDLLHOOKSTRUCT& k, bool down);

    App& m_app;
    std::thread m_thread;
    std::atomic<bool> m_active{false};
    std::atomic<bool> m_stop{false};
    DWORD m_threadId{};
    HHOOK m_hook{};
    // Changed default activation from left Alt to VK_RETURN (num-pad Enter).
    // The num-pad Enter reports VK_RETURN with the extended flag set; the hook
    // checks for that to distinguish it from the main Enter key.
    UINT m_activationVk{VK_RETURN};
    bool m_activationDown{};
};

class App {
public:
    bool Initialize(HINSTANCE hinst) {
        m_hinst = hinst;
        m_db.Seed();

        WNDCLASSEXW wc{sizeof(wc)};
        wc.hInstance = hinst;
        wc.lpfnWndProc = &App::WndProc;
        wc.lpszClassName = kClassName;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        if (!RegisterClassExW(&wc)) return false;

        m_hwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            kClassName, L"Unicode Overlay",
            WS_POPUP,
            0,0,640,430, nullptr, nullptr, hinst, this);
        if (!m_hwnd) return false;

        if (!m_renderer.Initialize(m_hwnd)) return false;

        m_keyboard = std::make_unique<KeyboardManager>(*this);
        if (!m_keyboard->Start()) return false;

        return true;
    }

    int Run() {
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

    void Shutdown() {
        if (m_keyboard) m_keyboard->Stop();
        m_renderer.Shutdown();
        if (m_hwnd) DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    void BeginActivation() {
        if (m_active) return;
        m_active = true;
        m_selected = 0;
        m_previousForeground = GetForegroundWindow();

        POINT pt{};
        GetCursorPos(&pt);
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(mon, &mi);

        UINT dpi = GetDpiForWindow(m_hwnd);
        LONG w = MulDiv(640, dpi, 96);
        LONG h = MulDiv(430, dpi, 96);
        LONG x = pt.x - w / 2;
        LONG y = pt.y - h / 2;

        x = std::max(mi.rcWork.left,
            std::min(x, mi.rcWork.right - w));

        y = std::max(mi.rcWork.top,
            std::min(y, mi.rcWork.bottom - h));

        SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        Render();
    }

    void EndActivation(bool commit) {
        if (!m_active) return;
        m_active = false;

        HWND target = m_previousForeground;
        m_previousForeground = nullptr;

        bool ok = true;
        if (commit && m_selected < std::min<size_t>(9, m_db.Sorted().size())) {
            const auto text = m_db.Sorted()[m_selected].text;
            ok = UnicodeInjector::Insert(text);
            if (ok) m_db.Use(m_selected);
        }

        ShowWindow(m_hwnd, SW_HIDE);
        if (target && IsWindow(target) && target != m_hwnd) {
            SetForegroundWindow(target);
        }

        // Notify KeyboardManager so the hook stops swallowing input.
        if (m_keyboard) m_keyboard->SetOverlayActive(false);

        // If injection failed, briefly expose a diagnostic in the window title.
        // It does not create a message box or steal focus.
        if (!ok) SetWindowTextW(m_hwnd, L"Unicode Overlay - SendInput blocked");
        else SetWindowTextW(m_hwnd, L"Unicode Overlay");
    }

    void Key(UINT vk, bool down, bool repeat) {
        if (!m_active || !down || repeat) return;
        const auto count = std::min<size_t>(9, m_db.Sorted().size());
        if (vk >= '1' && vk <= '9') {
            const size_t i = vk - '1';
            if (i < count) { m_selected = i; Render(); }
            return;
        }
        if (vk == VK_UP) {
            if (count) m_selected = (m_selected + count - 1) % count;
            Render(); return;
        }
        if (vk == VK_DOWN) {
            if (count) m_selected = (m_selected + 1) % count;
            Render(); return;
        }
        if (vk == VK_RETURN) {
            PostMessageW(m_hwnd, WMU_DEACTIVATE, 1, 0);
            return;
        }
        if (vk == VK_ESCAPE) {
            PostMessageW(m_hwnd, WMU_DEACTIVATE, 0, 0);
            return;
        }
    }

    void Render() {
        if (!m_active) return;
        m_renderer.Render(m_db, m_selected, GetDpiForWindow(m_hwnd));
    }

    HWND Window() const noexcept { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<App*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd,msg,wp,lp);

        switch (msg) {
        case WM_DPICHANGED: {
            auto* r = reinterpret_cast<RECT*>(lp);
            SetWindowPos(hwnd, nullptr, r->left,r->top,
                         r->right-r->left,r->bottom-r->top,
                         SWP_NOACTIVATE | SWP_NOZORDER);
            self->m_renderer.Resize();
            if (self->m_active) self->Render();
            return 0;
        }
        case WMU_ACTIVATE:
            self->BeginActivation(); return 0;
        case WMU_DEACTIVATE:
            self->EndActivation(wp != 0); return 0;
        case WMU_KEY:
            self->Key(static_cast<UINT>(wp), (lp & 1) != 0, (lp & 2) != 0); return 0;
        case WM_CLOSE:
            PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(hwnd,msg,wp,lp);
    }

    HINSTANCE m_hinst{};
    HWND m_hwnd{};
    HWND m_previousForeground{};
    bool m_active{};
    size_t m_selected{};
    CharacterDatabase m_db;
    D3D12OverlayRenderer m_renderer;
    std::unique_ptr<KeyboardManager> m_keyboard;
};

thread_local KeyboardManager* g_keyboardManager = nullptr;

LRESULT CALLBACK KeyboardManager::HookProc(int code, WPARAM wp, LPARAM lp) {
    if (code < 0) return CallNextHookEx(nullptr, code, wp, lp);
    auto* self = g_keyboardManager;
    if (!self) return CallNextHookEx(nullptr, code, wp, lp);

    const auto* k = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
    const bool injected = (k->flags & LLKHF_INJECTED) != 0;
    if (injected) return CallNextHookEx(nullptr, code, wp, lp);

    const bool down = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);
    const bool up   = (wp == WM_KEYUP   || wp == WM_SYSKEYUP);
    if (!down && !up) return CallNextHookEx(nullptr, code, wp, lp);

    const UINT vk = static_cast<UINT>(k->vkCode);

    // Detect activation key == num-pad Enter:
    bool isActivation = (vk == self->m_activationVk);
    if (isActivation && vk == VK_RETURN) {
        // num-pad Enter reports VK_RETURN with the extended flag set (0x01).
        // Require the extended flag so main Enter doesn't act as the special key.
        isActivation = (k->flags & LLKHF_EXTENDED) != 0;
    }

    if (isActivation) {
        if (down && !self->m_activationDown) {
            // If overlay is not active, open it.
            if (!self->m_active.load(std::memory_order_acquire)) {
                self->m_activationDown = true;
                self->m_active.store(true, std::memory_order_release);
                PostMessageW(self->m_app.Window(), WMU_ACTIVATE, 0, 0);
            } else {
                // Overlay already open: close without committing the symbol
                // and DO NOT forward an Enter to the application (avoid creating
                // a newline). Simply cancel the overlay and restore input.
                PostMessageW(self->m_app.Window(), WMU_DEACTIVATE, 0, 0);
            }
            return 1; // swallow activation key-down
        }
        if (up) {
            // Always clear the activation-down latch on key-up.
            self->m_activationDown = false;
            return 1; // swallow activation key-up
        }
        return 1; // swallow auto-repeat of activation
    }

    if (self->m_active.load(std::memory_order_acquire)) {
        if (down) {
            const bool repeat = (k->flags & LLKHF_ALTDOWN) != 0 &&
                                (vk == VK_LMENU || vk == VK_RMENU);
            PostMessageW(self->m_app.Window(),
                               WMU_KEY, vk, (repeat ? 3 : 1));
        }
        return 1; // all keyboard input is private to the overlay while held
    }

    return CallNextHookEx(nullptr, code, wp, lp);
}

bool KeyboardManager::Start()
{
    m_stop.store(false);
    m_thread = std::thread(&KeyboardManager::ThreadMain, this);

    // Wait briefly for the hook thread to publish its ID.
    for (int i=0; i<100 && m_threadId==0; ++i) Sleep(1);
    return m_threadId != 0;
}

void KeyboardManager::Stop()
{
    m_stop.store(true, std::memory_order_release);
    if (m_threadId) PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
    if (m_thread.joinable()) m_thread.join();
    m_threadId = 0;
}

void KeyboardManager::ThreadMain()
{
	g_keyboardManager = this;
	m_threadId = GetCurrentThreadId();

	m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, HookProc, nullptr, 0);
	if (!m_hook)
    {
		g_keyboardManager = nullptr;
		return;
	}

	MSG msg{};
	while (!m_stop.load(std::memory_order_acquire)) {
		const BOOL r = GetMessageW(&msg, nullptr, 0, 0);
		if (r <= 0) break;
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	UnhookWindowsHookEx(m_hook);
	m_hook = nullptr;
	g_keyboardManager = nullptr;
}

}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    App app;
    if (app.Initialize(hInstance) == false)
    {
        MessageBoxW(nullptr, L"UnicodeOverlay initialization failed.", L"UnicodeOverlay", MB_ICONERROR | MB_OK);
        return 1;
    }

    const int rc = app.Run();
    app.Shutdown();
    return rc;
}
