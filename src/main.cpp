#include <windows.h>
#include <shellscalingapi.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <thread>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shcore.lib")

using Microsoft::WRL::ComPtr;

namespace {

constexpr UINT kFrameCount = 2;
constexpr UINT WMU_ACTIVATE = WM_APP + 1;
constexpr UINT WMU_KEY = WM_APP + 2;
constexpr UINT WMU_DEACTIVATE = WM_APP + 3;
constexpr UINT WMU_RENDER = WM_APP + 4;

constexpr wchar_t kClassName[] = L"UnicodeOverlayWindow";

struct Entry {
    std::wstring code;
    std::wstring text;
    uint64_t uses{};
};

class CharacterDatabase {
public:
    void Seed() {
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

    void Use(size_t i) {
        if (i >= m_entries.size()) return;
        ++m_entries[i].uses;
        Sort();
    }

private:
    void Sort() {
        std::stable_sort(m_entries.begin(), m_entries.end(),
            [](const Entry& a, const Entry& b) {
                return a.uses > b.uses;
            });
    }
    std::vector<Entry> m_entries;
};

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

class D3D12OverlayRenderer {
public:
    ~D3D12OverlayRenderer() { Shutdown(); }

    bool Initialize(HWND hwnd) {
        m_hwnd = hwnd;

        UINT factoryFlags = 0;
#if defined(_DEBUG)
        {
            ComPtr<ID3D12Debug> debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
                debug->EnableDebugLayer();
                factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif
        if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory))))
            return false;

        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 d{};
            adapter->GetDesc1(&d);
            if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { adapter.Reset(); continue; }
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                             IID_PPV_ARGS(&m_device)))) break;
            adapter.Reset();
        }
        if (!m_device) {
            if (FAILED(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))) return false;
            if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                         IID_PPV_ARGS(&m_device)))) return false;
        }

        D3D12_COMMAND_QUEUE_DESC q{};
        q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(m_device->CreateCommandQueue(&q, IID_PPV_ARGS(&m_queue)))) return false;

        DXGI_SWAP_CHAIN_DESC1 sc{};
        sc.Width = 0; sc.Height = 0;
        sc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sc.BufferCount = kFrameCount;
        sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sc.SampleDesc.Count = 1;
        sc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        ComPtr<IDXGISwapChain1> swap1;
        if (FAILED(m_factory->CreateSwapChainForHwnd(m_queue.Get(), hwnd, &sc,
                                                     nullptr, nullptr, &swap1)))
            return false;
        if (FAILED(swap1.As(&m_swapChain))) return false;
        m_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                         IID_PPV_ARGS(&m_fence)))) return false;
        m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_fenceEvent) return false;

        // D3D11On12 lets Direct2D/DirectWrite render text using the D3D12
        // backbuffers. All APIs are Windows SDK APIs; no third-party renderer.
        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
        if (FAILED(D3D11On12CreateDevice(
                m_device.Get(), D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                levels, ARRAYSIZE(levels), nullptr, 0, 0,
                &m_d3d11, &m_d3d11Context, nullptr)))
            return false;
        if (FAILED(m_d3d11.As(&m_on12))) return false;

        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                     IID_PPV_ARGS(&m_d2dFactory)))) return false;
        ComPtr<IDXGIDevice> dxgiDevice;
        if (FAILED(m_d3d11.As(&dxgiDevice))) return false;

        if (FAILED(m_d2dFactory->CreateDevice(
            dxgiDevice.Get(), &m_d2dDevice))) return false;
        if (FAILED(m_d2dDevice->CreateDeviceContext(
                D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext))) return false;

        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                       __uuidof(IDWriteFactory),
                                       reinterpret_cast<IUnknown**>(m_writeFactory.GetAddressOf()))))
            return false;

        if (FAILED(m_writeFactory->CreateTextFormat(
                L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                18.0f, L"", &m_textFormat)))
            return false;
        m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        for (UINT i = 0; i < kFrameCount; ++i) {
            ComPtr<ID3D12Resource> buffer;
            if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&buffer)))) return false;
            D3D11_RESOURCE_FLAGS flags{};
            // Ensure the wrapped resource advertises render-target bind support
            // so D2D/DirectWrite can create a bitmap target from the DXGI surface.
            flags.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (FAILED(m_on12->CreateWrappedResource(
                    buffer.Get(), &flags,
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PRESENT,
                    IID_PPV_ARGS(&m_wrapped[i]))))
                return false;
        }

        m_ready = true;
        return true;// Resize();
    }

    bool Resize() {
        if (!m_ready) return false;
        WaitGpu();
        m_d2dTarget.Reset();
        for (auto& r : m_wrapped) r.Reset();

        if (FAILED(m_swapChain->ResizeBuffers(kFrameCount, 0, 0,
                                              DXGI_FORMAT_B8G8R8A8_UNORM, 0)))
            return false;

        for (UINT i = 0; i < kFrameCount; ++i) {
            ComPtr<ID3D12Resource> buffer;
            if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&buffer)))) return false;
            D3D11_RESOURCE_FLAGS flags{};
            // Same as Initialize: tell the wrapped resource it can be used as a render target.
            flags.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (FAILED(m_on12->CreateWrappedResource(
                    buffer.Get(), &flags,
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PRESENT,
                    IID_PPV_ARGS(&m_wrapped[i]))))
                return false;
        }
        return true;
    }

    void Render(const CharacterDatabase& db, size_t selected, UINT dpi) {
        if (!m_ready) return;
        WaitGpu();

        const UINT index = m_swapChain->GetCurrentBackBufferIndex();
        ID3D11Resource* resources[] = { m_wrapped[index].Get() };
        m_on12->AcquireWrappedResources(resources, 1);

        m_d2dContext->SetTarget(nullptr);
        ComPtr<IDXGISurface> surface;
        if (FAILED(m_wrapped[index].As(&surface))) {
            m_on12->ReleaseWrappedResources(resources, 1);
            m_d3d11Context->Flush();
            return;
        }

        // Use properties that match a typical backbuffer used with D2D:
        // - target + cannot-draw (we render via D2D into it)
        // - premultiplied alpha for DXGI_FORMAT_B8G8R8A8_UNORM backbuffers
        // - dpi 0.0f to avoid DPI mismatch errors
        D2D1_BITMAP_PROPERTIES1 props =
            D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                0.0f,
                0.0f);
        ComPtr<ID2D1Bitmap1> target;
        HRESULT hr = m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), &props, &target);
        if (FAILED(hr)) {
            wchar_t msg[256];
            _snwprintf_s(msg, _countof(msg), L"CreateBitmapFromDxgiSurface failed: 0x%08X\n", static_cast<unsigned int>(hr));
            OutputDebugStringW(msg);

            // Try a conservative fallback with the same alpha mode and zero DPI.
            D2D1_BITMAP_PROPERTIES1 fallbackProps = props;
            ComPtr<ID2D1Bitmap1> fallbackTarget;
            HRESULT hr2 = m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), &fallbackProps, &fallbackTarget);
            if (SUCCEEDED(hr2)) {
                target = fallbackTarget;
            } else {
                _snwprintf_s(msg, _countof(msg), L"Fallback CreateBitmapFromDxgiSurface failed: 0x%08X\n", static_cast<unsigned int>(hr2));
                OutputDebugStringW(msg);
                m_on12->ReleaseWrappedResources(resources, 1);
                m_d3d11Context->Flush();
                return;
            }
        }
        m_d2dContext->SetTarget(target.Get());

        const auto size = m_d2dContext->GetSize();
        ComPtr<ID2D1SolidColorBrush> bg, fg, accent, grid;
        m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.035f,0.04f,0.05f,0.96f), &bg);
        m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.94f,0.95f,0.97f,1.0f), &fg);
        m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.30f,0.65f,1.0f,1.0f), &accent);
        m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.16f,0.18f,0.22f,1.0f), &grid);

        m_d2dContext->BeginDraw();
        m_d2dContext->Clear(D2D1::ColorF(0.0f,0.0f,0.0f,0.0f));

        const float pad = 18.0f * dpi / 96.0f;
        const float rowH = 38.0f * dpi / 96.0f;
        const float top = pad + rowH;
        m_d2dContext->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(0,0,size.width,size.height),
                              12.0f * dpi / 96.0f, 12.0f * dpi / 96.0f), bg.Get());

        const std::wstring title = L"Unicode symbols  •  1–9 select  •  Enter commit  •  Esc cancel";
        m_d2dContext->DrawText(title.c_str(), static_cast<UINT32>(title.size()),
                               m_textFormat.Get(),
                               D2D1::RectF(pad,pad,size.width-pad,top), fg.Get());

        const auto& rows = db.Sorted();
        const size_t n = std::min<size_t>(9, rows.size());
        for (size_t i=0; i<n; ++i) {
            float y = top + i*rowH;
            if (i == selected)
                m_d2dContext->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(pad-5,y+2,size.width-pad+5,y+rowH-3),
                                      6,6), accent.Get());

            std::wstring line = std::to_wstring(i+1) + L"   " + rows[i].text +
                                L"      " + rows[i].code + L"      " +
                                std::to_wstring(rows[i].uses);
            auto brush = (i == selected) ? bg.Get() : fg.Get();
            m_d2dContext->DrawText(line.c_str(), static_cast<UINT32>(line.size()),
                                   m_textFormat.Get(),
                                   D2D1::RectF(pad,y,size.width-pad,y+rowH),
                                   brush);
        }
        m_d2dContext->EndDraw();

        m_on12->ReleaseWrappedResources(resources, 1);
        m_d3d11Context->Flush();

        m_swapChain->Present(1, 0);
        ++m_fenceValue;
        m_queue->Signal(m_fence.Get(), m_fenceValue);
    }

    void Shutdown() {
        if (!m_ready && !m_device) return;
        WaitGpu();
        m_d2dTarget.Reset();
        for (auto& r : m_wrapped) r.Reset();
        m_textFormat.Reset();
        m_writeFactory.Reset();
        m_d2dContext.Reset();
        m_d2dDevice.Reset();
        m_d2dFactory.Reset();
        m_on12.Reset();
        m_d3d11Context.Reset();
        m_d3d11.Reset();
        m_fence.Reset();
        m_swapChain.Reset();
        m_queue.Reset();
        m_device.Reset();
        m_factory.Reset();
        if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
        m_ready = false;
    }

private:
    void WaitGpu() {
        if (!m_queue || !m_fence || !m_fenceEvent || m_fenceValue == 0) return;
        if (m_fence->GetCompletedValue() < m_fenceValue) {
            m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    HWND m_hwnd{};
    bool m_ready{};
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_queue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent{};
    UINT64 m_fenceValue{};

    ComPtr<ID3D11Device> m_d3d11;
    ComPtr<ID3D11DeviceContext> m_d3d11Context;
    ComPtr<ID3D11On12Device> m_on12;

    ComPtr<ID2D1Factory3> m_d2dFactory;
    ComPtr<ID2D1Device2> m_d2dDevice;
    ComPtr<ID2D1DeviceContext2> m_d2dContext;
    ComPtr<IDWriteFactory> m_writeFactory;
    ComPtr<IDWriteTextFormat> m_textFormat;
    std::array<ComPtr<ID3D11Resource>, kFrameCount> m_wrapped;
    ComPtr<ID2D1Bitmap1> m_d2dTarget;
};

class App;

class KeyboardManager {
public:
    explicit KeyboardManager(App& app) : m_app(app) {}
    bool Start();
    void Stop();
    bool IsActive() const noexcept { return m_active.load(std::memory_order_acquire); }
    void SetActivationVk(UINT vk) noexcept { m_activationVk = vk; }

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
    UINT m_activationVk{VK_LMENU};
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
    const bool isActivation = (vk == self->m_activationVk);

    if (isActivation) {
        if (down && !self->m_activationDown) {
            self->m_activationDown = true;
            self->m_active.store(true, std::memory_order_release);
            PostMessageW(self->m_app.Window(),
                               WMU_ACTIVATE, 0, 0);
            return 1; // swallow activation key-down
        }
        if (up) {
            self->m_activationDown = false;
            self->m_active.store(false, std::memory_order_release);
            PostMessageW(self->m_app.Window(),
                               WMU_DEACTIVATE, 1, 0);
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

bool KeyboardManager::Start() {
    m_stop.store(false);
    m_thread = std::thread(&KeyboardManager::ThreadMain, this);

    // Wait briefly for the hook thread to publish its ID.
    for (int i=0; i<100 && m_threadId==0; ++i) Sleep(1);
    return m_threadId != 0;
}

void KeyboardManager::Stop() {
    m_stop.store(true, std::memory_order_release);
    if (m_threadId) PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
    if (m_thread.joinable()) m_thread.join();
    m_threadId = 0;
}

void KeyboardManager::ThreadMain() {
    g_keyboardManager = this;
    m_threadId = GetCurrentThreadId();

    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, HookProc, nullptr, 0);
    if (!m_hook) {
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

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // Per-monitor V2 is also declared in the manifest. The API call is a
    // defensive fallback and occurs before creating any HWND.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    App app;
    if (!app.Initialize(hInstance)) {
        MessageBoxW(nullptr, L"UnicodeOverlay initialization failed.",
                    L"UnicodeOverlay", MB_ICONERROR | MB_OK);
        return 1;
    }

    const int rc = app.Run();
    app.Shutdown();
    return rc;
}
