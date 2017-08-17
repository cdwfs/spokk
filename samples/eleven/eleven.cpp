#include <spokk_platform.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define SPOKK_HR_CHECK(expr) ZOMBO_RETVAL_CHECK(S_OK, expr)

#include <d3d11_4.h>
#include <dxgi1_2.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
class D3d11Device {
public:
  D3d11Device() {}
  ~D3d11Device() {
    Destroy();  // safe even if it's already been destroyed
  }

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
  if (!glfwVulkanSupported()) {
    fprintf(stderr, "Vulkan is not available :(\n");
    return;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window_ =
      std::shared_ptr<GLFWwindow>(glfwCreateWindow(ci.window_width, ci.window_height, ci.app_name.c_str(), NULL, NULL),
          [](GLFWwindow* w) { glfwDestroyWindow(w); });
  HWND hwnd = glfwGetWin32Window(window_.get());

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

  init_successful_ = true;
}
ElevenApp::~ElevenApp() {
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

  context->OMSetRenderTargets(1, &back_buffer_rtv_, nullptr);
  float clear_color[4] = {1.0f, fmodf((float)frame_index_ * 0.01f, 1.0f), 0.3f, 1.0f};
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
