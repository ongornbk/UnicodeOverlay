#pragma once

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

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d2d1_3.h>
#include <dwrite.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#include <windows.h>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;