#include <spokk_platform.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define SPOKK_HR_CHECK(expr) ZOMBO_RETVAL_CHECK(S_OK, expr)

#include <d3d11_2.h>
#include <dxgi1_2.h>

// clang-format off
#include <initguid.h>  // must be included before dxgidebug.h
#include <dxgidebug.h>
// clang-format on

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
class D3d11Device {
public:
  D3d11Device() {}
  ~D3d11Device() {}

  void Create(IDXGIAdapter1* adapter, ID3D11Device* device, ID3D11DeviceContext* immediate_context,
      D3D_FEATURE_LEVEL feature_level) {
    adapter_ = adapter;
    logical_device_ = device;
    immediate_context_ = immediate_context;
    feature_level_ = feature_level;
  }
  void Destroy() {
    if (immediate_context_) {
      immediate_context_->Release();
      immediate_context_ = nullptr;
    }
    if (logical_device_) {
      logical_device_->Release();
      logical_device_ = nullptr;
    }
    if (adapter_) {
      adapter_->Release();
      adapter_ = nullptr;
    }
#if defined(_DEBUG)
    {
      IDXGIDebug1* dxgi_debug = nullptr;
      // This complicated dance is necessary to avoid calling DXGIGetDebugInterface1() on Windows 7,
      // which crashes the app immediately at startup (even before the call site is reached).
      HMODULE dxgi_debug_module = GetModuleHandleA("dxgidebug.dll");
      if (dxgi_debug_module) {
        typedef HRESULT(WINAPI * PDGDI1)(UINT Flags, REFIID riid, _COM_Outptr_ void** pDebug);
        PDGDI1 dxgi_get_debug_interface1 = (PDGDI1)GetProcAddress(dxgi_debug_module, "DXGIGetDebugInterface1");
        if (dxgi_get_debug_interface1) {
          HRESULT debug_hr = dxgi_get_debug_interface1(0, __uuidof(IDXGIDebug1), (void**)&dxgi_debug);
          if (debug_hr) {
            dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
            dxgi_debug->Release();
          }
        }
      }
    }
#endif
  }

  ID3D11Device* Logical() const { return logical_device_; }
  IDXGIAdapter1* Physical() const { return adapter_; }
  ID3D11DeviceContext* Context() const { return immediate_context_; }

  D3d11Device(const D3d11Device&) = delete;
  D3d11Device& operator=(const D3d11Device&) = delete;

private:
  IDXGIAdapter1* adapter_ = nullptr;
  ID3D11Device* logical_device_ = nullptr;
  ID3D11DeviceContext* immediate_context_ = nullptr;
  D3D_FEATURE_LEVEL feature_level_ = D3D_FEATURE_LEVEL_11_0;
};

class ElevenApp {
public:
  struct CreateInfo {
    std::string app_name = "Spokk Application";
    uint32_t window_width = 1280, window_height = 720;
  };

  explicit ElevenApp(const CreateInfo& ci);
  virtual ~ElevenApp();

  ElevenApp(const ElevenApp&) = delete;
  const ElevenApp& operator=(const ElevenApp&) = delete;

  int Run();

  virtual void Update(double dt);
  virtual void Render(ID3D11DeviceContext* context);

private:
  bool init_successful_ = false;
  bool force_exit_ = false;  // set to true to exit the main application loop on the next iteration

  std::shared_ptr<GLFWwindow> window_ = nullptr;

  D3d11Device device_ = {};

  IDXGISwapChain1* swapchain_ = nullptr;
  DXGI_SWAP_CHAIN_DESC swapchain_desc_ = {};
  ID3D11RenderTargetView* back_buffer_rtv_ = {};

  ID3D11VertexShader* shader_vs_ = nullptr;
  ID3D11PixelShader* shader_ps_ = nullptr;

  ID3D11RasterizerState* rasterizer_state_ = nullptr;
  ID3D11BlendState* blend_state_ = nullptr;
  ID3D11DepthStencilState* depth_stencil_state_ = nullptr;

  uint32_t frame_index_ = 0;
};

void MyGlfwErrorCallback(int error, const char* description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

ElevenApp::ElevenApp(const CreateInfo& ci) {
  HRESULT hr = S_OK;

  // Initialize GLFW
  glfwSetErrorCallback(MyGlfwErrorCallback);
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window_ =
      std::shared_ptr<GLFWwindow>(glfwCreateWindow(ci.window_width, ci.window_height, ci.app_name.c_str(), NULL, NULL),
          [](GLFWwindow* w) { glfwDestroyWindow(w); });
  HWND hwnd = glfwGetWin32Window(window_.get());

#if defined(_DEBUG)
  {
    IDXGIDebug1* dxgi_debug = nullptr;
    // This complicated dance is necessary to avoid calling DXGIGetDebugInterface1() on Windows 7,
    // which crashes the app immediately at startup (even before the call site is reached).
    HMODULE dxgi_debug_module = GetModuleHandleA("dxgidebug.dll");
    if (dxgi_debug_module) {
      typedef HRESULT(WINAPI * PDGDI1)(UINT Flags, REFIID riid, _COM_Outptr_ void** pDebug);
      PDGDI1 dxgi_get_debug_interface1 = (PDGDI1)GetProcAddress(dxgi_debug_module, "DXGIGetDebugInterface1");
      if (dxgi_get_debug_interface1) {
        HRESULT debug_hr = dxgi_get_debug_interface1(0, __uuidof(IDXGIDebug1), (void**)&dxgi_debug);
        if (debug_hr == S_OK) {
          dxgi_debug->EnableLeakTrackingForThread();
          dxgi_debug->Release();
        }
      }
    }
  }
#endif

  // Enumerate adapters. This lets the app choose which physical GPU to target (or
  // a software reference implementation). For now, just grab the first one you
  // find that's a hardware device; I'm not picky.
  IDXGIFactory2* dxgi_factory = nullptr;
  SPOKK_HR_CHECK(CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&dxgi_factory));
  IDXGIAdapter1* adapter = nullptr;
  {
    IDXGIAdapter1* a1 = nullptr;
    UINT i = 0;
    while (dxgi_factory->EnumAdapters1(i, &a1) != DXGI_ERROR_NOT_FOUND) {
      IDXGIAdapter2* a2 = (IDXGIAdapter2*)a1;
      DXGI_ADAPTER_DESC2 desc2 = {};
      SPOKK_HR_CHECK(a2->GetDesc2(&desc2));
      if ((desc2.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
        adapter = a1;
        break;
      }
    }
  }
  dxgi_factory->Release();
  if (!adapter) {
    ZOMBO_ERROR("No hardware adapter found");
    return;
  }

  // Initialize device
  RECT client_rect = {};
  BOOL rect_error = GetClientRect(hwnd, &client_rect);
  if (!rect_error) {
    return;
  }
  UINT client_width = client_rect.right - client_rect.left;
  UINT client_height = client_rect.bottom - client_rect.top;

  UINT create_device_flags = 0;
#ifdef _DEBUG
  create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  const std::vector<D3D_FEATURE_LEVEL> feature_levels = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
  };

  swapchain_desc_ = {};
  swapchain_desc_.BufferCount = 2;
  swapchain_desc_.BufferDesc.Width = client_width;
  swapchain_desc_.BufferDesc.Height = client_height;
  swapchain_desc_.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  swapchain_desc_.BufferDesc.RefreshRate.Numerator = 60;
  swapchain_desc_.BufferDesc.RefreshRate.Denominator = 1;
  swapchain_desc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc_.OutputWindow = hwnd;
  swapchain_desc_.SampleDesc.Count = 1;
  swapchain_desc_.SampleDesc.Quality = 0;
  swapchain_desc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  swapchain_desc_.Windowed = TRUE;
  hr = E_FAIL;
  {
    ID3D11Device* device = nullptr;
    D3D_FEATURE_LEVEL device_feature_level = {};
    ID3D11DeviceContext* device_context = nullptr;
    IDXGISwapChain* swapchain0 = nullptr;
    SPOKK_HR_CHECK(D3D11CreateDeviceAndSwapChain(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, create_device_flags,
        feature_levels.data(), (UINT)feature_levels.size(), D3D11_SDK_VERSION, &swapchain_desc_, &swapchain0, &device,
        &device_feature_level, &device_context));
    SPOKK_HR_CHECK(swapchain0->QueryInterface<IDXGISwapChain1>(&swapchain_));
    swapchain0->Release();
    device_.Create(adapter, device, device_context, device_feature_level);
  }

  // Create render target view for swapchain back buffer
  ID3D11Texture2D* back_buffer_texture = nullptr;
  SPOKK_HR_CHECK(swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer_texture));
  SPOKK_HR_CHECK(device_.Logical()->CreateRenderTargetView(back_buffer_texture, nullptr, &back_buffer_rtv_));
  back_buffer_texture->Release();
  device_.Context()->OMSetRenderTargets(1, &back_buffer_rtv_, nullptr);

  // Load some shaders
  FILE* vs_file = zomboFopen("data/test_vs.cso", "rb");
  ZOMBO_ASSERT(vs_file, "failed to load VS");
  fseek(vs_file, 0, SEEK_END);
  size_t vs_nbytes = ftell(vs_file);
  fseek(vs_file, 0, SEEK_SET);
  std::vector<uint8_t> vs(vs_nbytes);
  size_t vs_read_nbytes = fread(vs.data(), 1, vs_nbytes, vs_file);
  fclose(vs_file);
  ZOMBO_ASSERT(vs_nbytes == vs_read_nbytes, "file I/O error while reading VS");
  SPOKK_HR_CHECK(device_.Logical()->CreateVertexShader(vs.data(), vs.size(), nullptr, &shader_vs_));

  FILE* ps_file = zomboFopen("data/test_ps.cso", "rb");
  ZOMBO_ASSERT(ps_file, "failed to load PS");
  fseek(ps_file, 0, SEEK_END);
  size_t ps_nbytes = ftell(ps_file);
  fseek(ps_file, 0, SEEK_SET);
  std::vector<uint8_t> ps(ps_nbytes);
  size_t ps_read_nbytes = fread(ps.data(), 1, ps_nbytes, ps_file);
  fclose(ps_file);
  ZOMBO_ASSERT(ps_nbytes == ps_read_nbytes, "file I/O error while reading PS");
  SPOKK_HR_CHECK(device_.Logical()->CreatePixelShader(ps.data(), ps.size(), nullptr, &shader_ps_));

  // Create render state objects
  D3D11_RASTERIZER_DESC rasterizer_desc = {};
  rasterizer_desc.FillMode = D3D11_FILL_SOLID;
  rasterizer_desc.CullMode = D3D11_CULL_BACK;
  rasterizer_desc.FrontCounterClockwise = TRUE;
  rasterizer_desc.ScissorEnable = TRUE;
  SPOKK_HR_CHECK(device_.Logical()->CreateRasterizerState(&rasterizer_desc, &rasterizer_state_));
  D3D11_BLEND_DESC blend_desc = {};
  blend_desc.RenderTarget[0].RenderTargetWriteMask = 0xF;
  SPOKK_HR_CHECK(device_.Logical()->CreateBlendState(&blend_desc, &blend_state_));
  D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {};
  SPOKK_HR_CHECK(device_.Logical()->CreateDepthStencilState(&depth_stencil_desc, &depth_stencil_state_));
  init_successful_ = true;
}
ElevenApp::~ElevenApp() {
  depth_stencil_state_->Release();
  blend_state_->Release();
  rasterizer_state_->Release();
  shader_vs_->Release();
  shader_ps_->Release();

  back_buffer_rtv_->Release();
  swapchain_->Release();
  device_.Destroy();

  window_.reset();
  glfwTerminate();
}

int ElevenApp::Run() {
  if (!init_successful_) {
    return -1;
  }

  glfwPollEvents();  // prime for first iteration
  while (!force_exit_ && !glfwWindowShouldClose(window_.get())) {
    Render(device_.Context());

    // in D3D11, Present() automatically updates the back buffer pointer(s). Simpler times, man.
    UINT sync_interval = 1;  // 1 = wait for vsync
    UINT present_flags = 0;
    swapchain_->Present(sync_interval, present_flags);

    glfwPollEvents();
    ++frame_index_;
  }
  return 0;
}

void ElevenApp::Update(double /*dt*/) {}

void ElevenApp::Render(ID3D11DeviceContext* context) {
  context->ClearState();

  float clear_color[4] = {0.5f, fmodf((float)frame_index_ * 0.01f, 0.5f), 0.3f, 1.0f};
  context->ClearRenderTargetView(back_buffer_rtv_, clear_color);

  // Setup the viewport
  D3D11_VIEWPORT viewport = {};
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = (float)swapchain_desc_.BufferDesc.Width;
  viewport.Height = (float)swapchain_desc_.BufferDesc.Height;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  context->RSSetViewports(1, &viewport);
  D3D11_RECT scissor_rect = {};
  scissor_rect.left = 0;
  scissor_rect.top = 0;
  scissor_rect.right = swapchain_desc_.BufferDesc.Width;
  scissor_rect.bottom = swapchain_desc_.BufferDesc.Height;
  context->RSSetScissorRects(1, &scissor_rect);

  // Bind state
  context->OMSetRenderTargets(1, &back_buffer_rtv_, nullptr);
  context->VSSetShader(shader_vs_, nullptr, 0);
  context->PSSetShader(shader_ps_, nullptr, 0);
  context->RSSetState(rasterizer_state_);
  context->OMSetDepthStencilState(depth_stencil_state_, 0);
  context->OMSetBlendState(blend_state_, NULL, 0xFFFFFFFF);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->Draw(3, 0);
}

}  // namespace

#if 0
int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/) {
#else
int main(int /*argc*/, char** /*argv*/) {
#endif
ElevenApp::CreateInfo app_ci = {};
app_ci.app_name = "Eleven!";

ElevenApp app(app_ci);
int run_error = app.Run();

return run_error;
}
