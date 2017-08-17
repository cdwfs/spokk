#include <spokk_platform.h>

#define SPOKK_HR_CHECK(expr) ZOMBO_RETVAL_CHECK(S_OK, expr)

#include <d3d11_4.h>
#include <dxgi1_2.h>

#include <cstdint>
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  PAINTSTRUCT ps = {};
  HDC hdc = {};

  switch (message) {
  case WM_PAINT:
    hdc = BeginPaint(hWnd, &ps);
    EndPaint(hWnd, &ps);
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  default:
    return DefWindowProcA(hWnd, message, wParam, lParam);
  }

  return 0;
}

class ElevenApp {
public:
  struct CreateInfo {
    std::string app_name = "Spokk Application";
    uint32_t window_width = 1280, window_height = 720;
    HINSTANCE hinstance = 0;
    int cmd_show = 0;
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

  HINSTANCE hinstance_ = nullptr;
  HWND hwnd_ = nullptr;
  D3d11Device device_ = {};

  IDXGISwapChain1* swapchain_ = nullptr;
  DXGI_SWAP_CHAIN_DESC swapchain_desc_ = {};
  ID3D11RenderTargetView* back_buffer_rtv_ = {};

  uint32_t frame_index_ = 0;
};

ElevenApp::ElevenApp(const CreateInfo& ci) {
  HRESULT hr = S_OK;
  hinstance_ = ci.hinstance;

  // Initialize window
  WNDCLASSEXA wcex = {};
  wcex.cbSize = sizeof(WNDCLASSEXA);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = ci.hinstance;
  wcex.hIcon = nullptr;
  wcex.hCursor = nullptr;
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszMenuName = nullptr;
  wcex.lpszClassName = "SpokkWindowClass";
  wcex.hIconSm = nullptr;
  if (!RegisterClassExA(&wcex)) {
    return;
  }
  RECT window_rect = {0, 0, (LONG)ci.window_width, (LONG)ci.window_height};
  AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);
  hwnd_ = CreateWindowA(wcex.lpszClassName, ci.app_name.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
      window_rect.right - window_rect.left, window_rect.bottom - window_rect.top, nullptr, nullptr, ci.hinstance,
      nullptr);
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, ci.cmd_show);

  // Enumerate adaptors. This lets the app choose which physical GPU to target (or
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
  BOOL rect_error = GetClientRect(hwnd_, &client_rect);
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
  swapchain_desc_.OutputWindow = hwnd_;
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
}

int ElevenApp::Run() {
  if (!init_successful_) {
    return -1;
  }

  // Main message loop
  MSG msg = {0};
  while (WM_QUIT != msg.message) {
    if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    } else {
      Render(device_.Context());

      // in D3D11, Present() automatically updates the back buffer pointer(s). Simpler times, man.
      UINT sync_interval = 1;  // 1 = wait for vsync
      UINT present_flags = 0;
      swapchain_->Present(sync_interval, present_flags);

      ++frame_index_;
    }
  }
  return (int)msg.wParam;
}

void ElevenApp::Update(double /*dt*/) {}

void ElevenApp::Render(ID3D11DeviceContext* context) {
  context->ClearState();

  context->OMSetRenderTargets(1, &back_buffer_rtv_, nullptr);
  float clear_color[4] = {1.0f, fmodf( (float)frame_index_ * 0.01f, 1.0f), 0.3f, 1.0f};
  context->ClearRenderTargetView(back_buffer_rtv_, clear_color);

  // Setup the viewport
  D3D11_VIEWPORT viewport = {};
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = (float)swapchain_desc_.BufferDesc.Width;
  viewport.Height = (float)swapchain_desc_.BufferDesc.Height;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  context->RSSetViewports( 1, &viewport );

}

}  // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int nCmdShow) {
  ElevenApp::CreateInfo app_ci = {};
  app_ci.app_name = "Eleven!";
  app_ci.hinstance = hInstance;
  app_ci.cmd_show = nCmdShow;

  ElevenApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
