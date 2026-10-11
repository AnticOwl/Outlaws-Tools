#include "Dx12Overlay.h"
#include "EnvironmentInspector.h"

#include <Windows.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace outlaws {
namespace {

constexpr wchar_t kProbeClass[] = L"OutlawsTools_DX12Probe";

std::filesystem::path logPath() {
    wchar_t buffer[MAX_PATH]{};
    const auto len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (!len || len >= std::size(buffer)) return L"OutlawsTools.log";
    std::filesystem::path p(buffer);
    p.replace_filename(L"OutlawsTools.log");
    return p;
}

LRESULT CALLBACK probeWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool suitableGameWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    DWORD pid{};
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return false;

    RECT rc{};
    if (!GetClientRect(hwnd, &rc)) return false;
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    return width >= 640 && height >= 360;
}

}

Dx12Overlay* Dx12Overlay::instance_ = nullptr;
Dx12Overlay::PresentFn Dx12Overlay::originalPresent_ = nullptr;
Dx12Overlay::ResizeBuffersFn Dx12Overlay::originalResizeBuffers_ = nullptr;
Dx12Overlay::ExecuteCommandListsFn Dx12Overlay::originalExecuteCommandLists_ = nullptr;

bool Dx12Overlay::start(EnvironmentInspector* inspector) noexcept {
    if (running_.exchange(true)) return true;

    inspector_ = inspector;
    instance_ = this;
    visible_ = true;

    log("DX12 overlay start");
    if (!installHooks()) {
        log("DX12 overlay hook installation FAILED");
        running_ = false;
        instance_ = nullptr;
        return false;
    }

    log("DX12 overlay hooks installed");
    return true;
}

void Dx12Overlay::stop() noexcept {
    if (!running_.exchange(false)) return;

    shutdownImGui();
    removeHooks();

    commandQueue_.store(nullptr, std::memory_order_relaxed);

    inspector_ = nullptr;
    instance_ = nullptr;
    log("DX12 overlay stopped");
}

void Dx12Overlay::toggle() noexcept {
    visible_ = !visible_.load();
}

bool Dx12Overlay::createProbeTargets(void** present, void** resizeBuffers, void** executeLists) noexcept {
    if (!present || !resizeBuffers || !executeLists) return false;
    *present = nullptr;
    *resizeBuffers = nullptr;
    *executeLists = nullptr;

    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = probeWndProc;
    wc.hInstance = inst;
    wc.lpszClassName = kProbeClass;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, kProbeClass, L"", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, nullptr, nullptr, inst, nullptr);
    if (!hwnd) {
        log("DX12 probe CreateWindowExW failed");
        UnregisterClassW(kProbeClass, inst);
        return false;
    }

    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    IDXGIFactory4* factory = nullptr;
    IDXGISwapChain1* swapChain1 = nullptr;

    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) {
        log("DX12 probe D3D12CreateDevice failed");
        DestroyWindow(hwnd);
        UnregisterClassW(kProbeClass, inst);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    if (FAILED(hr)) {
        log("DX12 probe CreateCommandQueue failed");
        device->Release();
        DestroyWindow(hwnd);
        UnregisterClassW(kProbeClass, inst);
        return false;
    }

    hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        log("DX12 probe CreateDXGIFactory1 failed");
        queue->Release();
        device->Release();
        DestroyWindow(hwnd);
        UnregisterClassW(kProbeClass, inst);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = 100;
    sd.Height = 100;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    hr = factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &swapChain1);
    if (SUCCEEDED(hr) && swapChain1) {
        void** swapVtable = *reinterpret_cast<void***>(swapChain1);
        void** queueVtable = *reinterpret_cast<void***>(queue);

        *present = swapVtable[8];
        *resizeBuffers = swapVtable[13];
        *executeLists = queueVtable[10];
    } else {
        log("DX12 probe CreateSwapChainForHwnd failed");
    }

    if (swapChain1) swapChain1->Release();
    factory->Release();
    queue->Release();
    device->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(kProbeClass, inst);

    return *present && *resizeBuffers && *executeLists;
}

bool Dx12Overlay::installHooks() noexcept {
    void* present{};
    void* resizeBuffers{};
    void* executeLists{};

    if (!createProbeTargets(&present, &resizeBuffers, &executeLists))
        return false;

    const MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        log("MinHook initialize failed");
        return false;
    }

    if (MH_CreateHook(present, &Dx12Overlay::hookPresent,
                      reinterpret_cast<void**>(&originalPresent_)) != MH_OK) {
        log("Present hook create failed");
        return false;
    }

    if (MH_CreateHook(resizeBuffers, &Dx12Overlay::hookResizeBuffers,
                      reinterpret_cast<void**>(&originalResizeBuffers_)) != MH_OK) {
        log("ResizeBuffers hook create failed");
        MH_RemoveHook(present);
        return false;
    }

    if (MH_CreateHook(executeLists, &Dx12Overlay::hookExecuteCommandLists,
                      reinterpret_cast<void**>(&originalExecuteCommandLists_)) != MH_OK) {
        log("ExecuteCommandLists hook create failed");
        MH_RemoveHook(resizeBuffers);
        MH_RemoveHook(present);
        return false;
    }

    if (MH_EnableHook(present) != MH_OK ||
        MH_EnableHook(resizeBuffers) != MH_OK ||
        MH_EnableHook(executeLists) != MH_OK) {
        log("MinHook enable failed");
        return false;
    }

    return true;
}

void Dx12Overlay::removeHooks() noexcept {
    if (originalPresent_) MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    originalPresent_ = nullptr;
    originalResizeBuffers_ = nullptr;
    originalExecuteCommandLists_ = nullptr;
}

void __stdcall Dx12Overlay::hookExecuteCommandLists(
    ID3D12CommandQueue* queue,
    UINT count,
    ID3D12CommandList* const* lists) {

    auto* self = instance_;
    if (self && queue && self->running_.load()) {
        const auto desc = queue->GetDesc();
        if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            auto* previous = self->commandQueue_.exchange(queue, std::memory_order_relaxed);
            if (previous != queue)
                self->log("DX12 active DIRECT command queue updated");
        }
    }

    if (originalExecuteCommandLists_)
        originalExecuteCommandLists_(queue, count, lists);
}

HRESULT __stdcall Dx12Overlay::hookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    auto* self = instance_;

    if (self && self->running_.load() && swapChain) {
        if (!self->initialized_.load()) {
            self->initializeForSwapChain(swapChain);
        }

        if (self->initialized_.load() && self->visible_.load())
            self->render(swapChain);
    }

    return originalPresent_ ? originalPresent_(swapChain, syncInterval, flags) : S_OK;
}

HRESULT __stdcall Dx12Overlay::hookResizeBuffers(
    IDXGISwapChain* swapChain,
    UINT bufferCount,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT flags) {

    auto* self = instance_;
    const bool ours = self && self->swapChain_ &&
                      reinterpret_cast<IDXGISwapChain*>(self->swapChain_) == swapChain;

    if (ours) {
        self->log("DX12 ResizeBuffers detected");
        self->shutdownImGui();
    }

    const HRESULT hr = originalResizeBuffers_
        ? originalResizeBuffers_(swapChain, bufferCount, width, height, format, flags)
        : E_FAIL;

    return hr;
}

bool Dx12Overlay::initializeForSwapChain(IDXGISwapChain* swapChain) noexcept {
    auto* queue = commandQueue_.load();
    if (!queue || !swapChain) return false;

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc))) return false;
    if (!suitableGameWindow(desc.OutputWindow)) return false;

    ID3D12Device* swapDevice = nullptr;
    if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&swapDevice))) || !swapDevice)
        return false;

    ID3D12Device* queueDevice = nullptr;
    if (FAILED(queue->GetDevice(IID_PPV_ARGS(&queueDevice))) || !queueDevice) {
        swapDevice->Release();
        return false;
    }

    const bool sameDevice = swapDevice == queueDevice;
    queueDevice->Release();
    if (!sameDevice) {
        swapDevice->Release();
        return false;
    }

    IDXGISwapChain3* sc3 = nullptr;
    if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&sc3))) || !sc3) {
        swapDevice->Release();
        return false;
    }

    swapChain_ = sc3;
    device_ = swapDevice;
    hwnd_ = desc.OutputWindow;
    rtvFormat_ = desc.BufferDesc.Format;
    bufferCount_ = std::max<UINT>(2, desc.BufferCount);

    if (!createSwapChainResources()) {
        log("DX12 swapchain resource initialization FAILED");
        shutdownImGui();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 7.0f;
    style.FrameRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);

    if (!ImGui_ImplWin32_Init(hwnd_)) {
        log("ImGui Win32 init FAILED");
        shutdownImGui();
        return false;
    }

    ImGui_ImplDX12_InitInfo info{};
    info.Device = device_;
    info.CommandQueue = queue;
    info.NumFramesInFlight = static_cast<int>(bufferCount_);
    info.RTVFormat = rtvFormat_;
    info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    info.SrvDescriptorHeap = srvHeap_;
    info.UserData = this;
    info.SrvDescriptorAllocFn = &Dx12Overlay::allocSrv;
    info.SrvDescriptorFreeFn = &Dx12Overlay::freeSrv;

    if (!ImGui_ImplDX12_Init(&info)) {
        log("ImGui DX12 init FAILED");
        ImGui_ImplWin32_Shutdown();
        shutdownImGui();
        return false;
    }

    originalWndProc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd_, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(&Dx12Overlay::hookWndProc)));

    initialized_ = true;
    log("DX12 in-game ImGui initialized");
    return true;
}

bool Dx12Overlay::createSwapChainResources() noexcept {
    if (!device_ || !swapChain_ || !bufferCount_) return false;

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = bufferCount_;
    if (FAILED(device_->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap_))))
        return false;

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = srvCapacity_;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device_->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&srvHeap_))))
        return false;

    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    srvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    frames_.resize(bufferCount_);
    auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < bufferCount_; ++i) {
        auto& frame = frames_[i];

        if (FAILED(device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&frame.allocator))))
            return false;

        if (FAILED(swapChain_->GetBuffer(i, IID_PPV_ARGS(&frame.backBuffer))))
            return false;

        frame.rtv = rtv;
        device_->CreateRenderTargetView(frame.backBuffer, nullptr, frame.rtv);
        rtv.ptr += rtvDescriptorSize_;
    }

    if (FAILED(device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            frames_[0].allocator,
            nullptr,
            IID_PPV_ARGS(&commandList_))))
        return false;

    commandList_->Close();

    if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_))))
        return false;

    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return fenceEvent_ != nullptr;
}

void Dx12Overlay::waitForFrame(FrameContext& frame) noexcept {
    if (!fence_ || !fenceEvent_ || frame.fenceValue == 0) return;

    if (fence_->GetCompletedValue() < frame.fenceValue) {
        if (SUCCEEDED(fence_->SetEventOnCompletion(frame.fenceValue, fenceEvent_)))
            WaitForSingleObject(fenceEvent_, 1000);
    }

    frame.fenceValue = 0;
}

void Dx12Overlay::render(IDXGISwapChain* swapChain) noexcept {
    if (!initialized_.load() || !swapChain_ || swapChain != swapChain_) return;

    auto* queue = commandQueue_.load();
    if (!queue || frames_.empty()) return;

    const UINT index = swapChain_->GetCurrentBackBufferIndex();
    if (index >= frames_.size()) return;

    auto& frame = frames_[index];
    waitForFrame(frame);

    if (FAILED(frame.allocator->Reset())) return;
    if (FAILED(commandList_->Reset(frame.allocator, nullptr))) return;

    D3D12_RESOURCE_BARRIER toRender{};
    toRender.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRender.Transition.pResource = frame.backBuffer;
    toRender.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toRender.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRender.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &toRender);

    commandList_->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);

    ID3D12DescriptorHeap* heaps[] = { srvHeap_ };
    commandList_->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (inspector_)
        inspector_->render();

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_);

    D3D12_RESOURCE_BARRIER toPresent = toRender;
    toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &toPresent);

    if (FAILED(commandList_->Close())) return;

    ID3D12CommandList* lists[] = { commandList_ };
    originalExecuteCommandLists_(queue, 1, lists);

    const UINT64 fenceValue = nextFenceValue_++;
    if (SUCCEEDED(queue->Signal(fence_, fenceValue)))
        frame.fenceValue = fenceValue;
}

void Dx12Overlay::releaseSwapChainResources() noexcept {
    if (fence_ && fenceEvent_) {
        const UINT64 value = nextFenceValue_++;
        if (auto* queue = commandQueue_.load()) {
            if (SUCCEEDED(queue->Signal(fence_, value)) &&
                fence_->GetCompletedValue() < value &&
                SUCCEEDED(fence_->SetEventOnCompletion(value, fenceEvent_))) {
                WaitForSingleObject(fenceEvent_, 1500);
            }
        }
    }

    if (commandList_) { commandList_->Release(); commandList_ = nullptr; }

    for (auto& frame : frames_) {
        if (frame.backBuffer) { frame.backBuffer->Release(); frame.backBuffer = nullptr; }
        if (frame.allocator) { frame.allocator->Release(); frame.allocator = nullptr; }
        frame.fenceValue = 0;
    }
    frames_.clear();

    if (fence_) { fence_->Release(); fence_ = nullptr; }
    if (fenceEvent_) { CloseHandle(fenceEvent_); fenceEvent_ = nullptr; }
    if (srvHeap_) { srvHeap_->Release(); srvHeap_ = nullptr; }
    if (rtvHeap_) { rtvHeap_->Release(); rtvHeap_ = nullptr; }

    srvNext_ = 0;
    srvFree_.clear();
}

void Dx12Overlay::shutdownImGui() noexcept {
    if (initialized_.exchange(false)) {
        if (originalWndProc_ && hwnd_ && IsWindow(hwnd_)) {
            SetWindowLongPtrW(hwnd_, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(originalWndProc_));
        }
        originalWndProc_ = nullptr;

        if (ImGui::GetCurrentContext()) {
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
    }

    releaseSwapChainResources();

    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }

    hwnd_ = nullptr;
    bufferCount_ = 0;
}

LRESULT CALLBACK Dx12Overlay::hookWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = instance_;
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    if (msg == WM_KEYUP && wParam == VK_F10) {
        self->toggle();
        return 0;
    }

    if (self->visible_.load() && ImGui::GetCurrentContext()) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
            return 1;
    }

    return self->originalWndProc_
        ? CallWindowProcW(self->originalWndProc_, hwnd, msg, wParam, lParam)
        : DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Dx12Overlay::allocSrv(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
    D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {

    if (!info || !info->UserData || !cpu || !gpu) return;
    auto* self = static_cast<Dx12Overlay*>(info->UserData);

    UINT index{};
    if (!self->srvFree_.empty()) {
        index = self->srvFree_.back();
        self->srvFree_.pop_back();
    } else {
        if (self->srvNext_ >= self->srvCapacity_) {
            cpu->ptr = 0;
            gpu->ptr = 0;
            return;
        }
        index = self->srvNext_++;
    }

    *cpu = self->srvHeap_->GetCPUDescriptorHandleForHeapStart();
    *gpu = self->srvHeap_->GetGPUDescriptorHandleForHeapStart();
    cpu->ptr += static_cast<SIZE_T>(index) * self->srvDescriptorSize_;
    gpu->ptr += static_cast<UINT64>(index) * self->srvDescriptorSize_;
}

void Dx12Overlay::freeSrv(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE cpu,
    D3D12_GPU_DESCRIPTOR_HANDLE) {

    if (!info || !info->UserData || !cpu.ptr) return;
    auto* self = static_cast<Dx12Overlay*>(info->UserData);
    if (!self->srvHeap_ || !self->srvDescriptorSize_) return;

    const auto start = self->srvHeap_->GetCPUDescriptorHandleForHeapStart().ptr;
    if (cpu.ptr < start) return;

    const SIZE_T delta = cpu.ptr - start;
    const UINT index = static_cast<UINT>(delta / self->srvDescriptorSize_);
    if (index < self->srvCapacity_)
        self->srvFree_.push_back(index);
}

void Dx12Overlay::log(const char* text) const noexcept {
    if (!text) return;
    std::ofstream out(logPath(), std::ios::out | std::ios::app);
    if (!out) return;
    out << text << "\n";
    out.flush();
}

}
