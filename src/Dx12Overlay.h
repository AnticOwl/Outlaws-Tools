#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui_impl_dx12.h>
#include <atomic>
#include <cstdint>
#include <vector>

namespace outlaws {

class EnvironmentInspector;

class Dx12Overlay {
public:
    bool start(EnvironmentInspector* inspector) noexcept;
    void stop() noexcept;
    void toggle() noexcept;
    [[nodiscard]] bool ready() const noexcept { return initialized_.load(); }
    [[nodiscard]] bool visible() const noexcept { return visible_.load(); }

private:
    struct FrameContext {
        ID3D12CommandAllocator* allocator{};
        ID3D12Resource* backBuffer{};
        D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
        UINT64 fenceValue{};
    };

    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    using ExecuteCommandListsFn = void(__stdcall*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

    static HRESULT __stdcall hookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
    static HRESULT __stdcall hookResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format, UINT flags);
    static void __stdcall hookExecuteCommandLists(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists);
    static LRESULT CALLBACK hookWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool installHooks() noexcept;
    void removeHooks() noexcept;
    bool createProbeTargets(void** present, void** resizeBuffers, void** executeLists) noexcept;

    bool initializeForSwapChain(IDXGISwapChain* swapChain) noexcept;
    void shutdownImGui() noexcept;
    void releaseSwapChainResources() noexcept;
    bool createSwapChainResources() noexcept;
    void render(IDXGISwapChain* swapChain) noexcept;
    void waitForFrame(FrameContext& frame) noexcept;

    static void allocSrv(ImGui_ImplDX12_InitInfo* info,
                         D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                         D3D12_GPU_DESCRIPTOR_HANDLE* gpu);
    static void freeSrv(ImGui_ImplDX12_InitInfo* info,
                        D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                        D3D12_GPU_DESCRIPTOR_HANDLE gpu);

    void log(const char* text) const noexcept;

    static Dx12Overlay* instance_;
    static PresentFn originalPresent_;
    static ResizeBuffersFn originalResizeBuffers_;
    static ExecuteCommandListsFn originalExecuteCommandLists_;

    EnvironmentInspector* inspector_{};
    std::atomic_bool running_{};
    std::atomic_bool initialized_{};
    std::atomic_bool visible_{true};
    std::atomic<ID3D12CommandQueue*> commandQueue_{};

    IDXGISwapChain3* swapChain_{};
    ID3D12Device* device_{};
    ID3D12GraphicsCommandList* commandList_{};
    ID3D12DescriptorHeap* rtvHeap_{};
    ID3D12DescriptorHeap* srvHeap_{};
    ID3D12Fence* fence_{};
    HANDLE fenceEvent_{};
    UINT64 nextFenceValue_{1};

    HWND hwnd_{};
    WNDPROC originalWndProc_{};

    DXGI_FORMAT rtvFormat_{DXGI_FORMAT_R8G8B8A8_UNORM};
    UINT bufferCount_{};
    UINT rtvDescriptorSize_{};
    UINT srvDescriptorSize_{};
    UINT srvCapacity_{64};
    UINT srvNext_{};
    std::vector<UINT> srvFree_{};
    std::vector<FrameContext> frames_{};
};

}
