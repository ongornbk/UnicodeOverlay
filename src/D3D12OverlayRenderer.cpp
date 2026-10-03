#include "D3D12OverlayRenderer.h"

D3D12OverlayRenderer::~D3D12OverlayRenderer()
{
    Shutdown();
}

bool D3D12OverlayRenderer::Initialize(HWND hwnd)
{
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

bool D3D12OverlayRenderer::Resize()
{
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

void D3D12OverlayRenderer::Render(const CharacterDatabase& db, size_t selected, UINT dpi)
{
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
        }
        else {
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
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.035f, 0.04f, 0.05f, 0.96f), &bg);
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.94f, 0.95f, 0.97f, 1.0f), &fg);
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.30f, 0.65f, 1.0f, 1.0f), &accent);
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.16f, 0.18f, 0.22f, 1.0f), &grid);

    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    const float pad = 18.0f * dpi / 96.0f;
    const float rowH = 38.0f * dpi / 96.0f;
    const float top = pad + rowH;
    m_d2dContext->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(0, 0, size.width, size.height),
            12.0f * dpi / 96.0f, 12.0f * dpi / 96.0f), bg.Get());

    const std::wstring title = L"Unicode symbols  •  1–9 select  •  Enter commit  •  Esc cancel";
    m_d2dContext->DrawText(title.c_str(), static_cast<UINT32>(title.size()),
        m_textFormat.Get(),
        D2D1::RectF(pad, pad, size.width - pad, top), fg.Get());

    const auto& rows = db.Sorted();
    const size_t n = std::min<size_t>(9, rows.size());
    for (size_t i = 0; i < n; ++i) {
        float y = top + i * rowH;
        if (i == selected)
            m_d2dContext->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(pad - 5, y + 2, size.width - pad + 5, y + rowH - 3),
                    6, 6), accent.Get());

        std::wstring line = std::to_wstring(i + 1) + L"   " + rows[i].text +
            L"      " + rows[i].code + L"      " +
            std::to_wstring(rows[i].uses);
        auto brush = (i == selected) ? bg.Get() : fg.Get();
        m_d2dContext->DrawText(line.c_str(), static_cast<UINT32>(line.size()),
            m_textFormat.Get(),
            D2D1::RectF(pad, y, size.width - pad, y + rowH),
            brush);
    }
    m_d2dContext->EndDraw();

    m_on12->ReleaseWrappedResources(resources, 1);
    m_d3d11Context->Flush();

    m_swapChain->Present(1, 0);
    ++m_fenceValue;
    m_queue->Signal(m_fence.Get(), m_fenceValue);
}

void D3D12OverlayRenderer::WaitGpu()
{
    if (!m_queue || !m_fence || !m_fenceEvent || m_fenceValue == 0) return;
    if (m_fence->GetCompletedValue() < m_fenceValue) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}