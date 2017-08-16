#include <spokk_platform.h>

#include <d3d11_4.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {
class D3d11Device {
public:
  D3d11Device(ID3D11Device* device, D3D_DRIVER_TYPE driver_type, D3D_FEATURE_LEVEL feature_level)
    : device_(device), driver_type_(driver_type), feature_level_(feature_level) {}
  ~D3d11Device() {
    if (device_) {
      device_->Release();
      device_ = nullptr;
    }
  }
  D3d11Device(const D3d11Device&) = delete;
  D3d11Device& operator=(const D3d11Device&) = delete;

  ID3D11Device* Device() const { return device_; }

private:
  ID3D11Device* device_ = NULL;
  D3D_DRIVER_TYPE driver_type_ = D3D_DRIVER_TYPE_NULL;
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
  virtual void Render(ID3D11DeviceContext* context, uint32_t swapchain_image_index);

private:
  bool init_successful_ = false;

  HINSTANCE hinstance_ = nullptr;
  HWND hwnd_ = nullptr;
  D3d11Device* device_ = nullptr;
  ID3D11DeviceContext* immediate_context_ = nullptr;
  IDXGISwapChain* swapchain_ = nullptr;
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

  const std::vector<D3D_DRIVER_TYPE> driver_types = {
      // Try these driver types in the order listed
      D3D_DRIVER_TYPE_HARDWARE,
      D3D_DRIVER_TYPE_WARP,
      D3D_DRIVER_TYPE_REFERENCE,
  };
  const std::vector<D3D_FEATURE_LEVEL> feature_levels = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
  };

  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 2;
  sd.BufferDesc.Width = client_width;
  sd.BufferDesc.Height = client_height;
  sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd_;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;  // TODO(cort): FLIP_ is allegedly more efficient, but Win10+ only.
  sd.Windowed = TRUE;  // TODO(cort): FALSE -> fullscreen?
  hr = E_FAIL;
  for (auto driver_type : driver_types) {
    ID3D11Device* device = nullptr;
    D3D_FEATURE_LEVEL device_feature_level = {};
    ID3D11DeviceContext* device_context = nullptr;
    hr = D3D11CreateDeviceAndSwapChain(NULL, driver_type, NULL, create_device_flags, feature_levels.data(),
        (UINT)feature_levels.size(), D3D11_SDK_VERSION, &sd, &swapchain_, &device, &device_feature_level,
        &device_context);
    if (SUCCEEDED(hr)) {
      device_ = new D3d11Device(device, driver_type, device_feature_level);
      break;
    }
  }
  if (FAILED(hr)) {
    return;
  }

  init_successful_ = true;
}
ElevenApp::~ElevenApp() {
  if (device_) {
    delete device_;
  }
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
      Render(immediate_context_, 0);
    }
  }
  return (int)msg.wParam;
}

void ElevenApp::Update(double /*dt*/) {}

void ElevenApp::Render(ID3D11DeviceContext* /*context*/, uint32_t /*swapchain_image_index*/) {}

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
