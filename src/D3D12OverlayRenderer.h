#pragma once
#include "SharedHeader.h"
#include "CharacterDatabase.h"

class D3D12OverlayRenderer
{
    constexpr static inline UINT kFrameCount = 2;
public:
    ~D3D12OverlayRenderer();

    bool Initialize(HWND hwnd);
    bool Resize();

    void Render(const CharacterDatabase& db, size_t selected, UINT dpi);

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
    void WaitGpu();

    HWND m_hwnd{};
    bool m_ready{};
    ComPtr<struct IDXGIFactory4> m_factory;
    ComPtr<struct ID3D12Device> m_device;
    ComPtr<struct ID3D12CommandQueue> m_queue;
    ComPtr<struct IDXGISwapChain3> m_swapChain;
    ComPtr<struct ID3D12Fence> m_fence;
    HANDLE m_fenceEvent{};
    UINT64 m_fenceValue{};

    ComPtr<struct ID3D11Device> m_d3d11;
    ComPtr<struct ID3D11DeviceContext> m_d3d11Context;
    ComPtr<struct ID3D11On12Device> m_on12;

    ComPtr<struct ID2D1Factory3> m_d2dFactory;
    ComPtr<struct ID2D1Device2> m_d2dDevice;
    ComPtr<struct ID2D1DeviceContext2> m_d2dContext;
    ComPtr<struct IDWriteFactory> m_writeFactory;
    ComPtr<struct IDWriteTextFormat> m_textFormat;
    std::array<ComPtr<struct ID3D11Resource>, kFrameCount> m_wrapped;
    ComPtr<struct ID2D1Bitmap1> m_d2dTarget;
};