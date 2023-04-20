// Please keep headers sorted.
#include <iostream>
#include <memory>
#include <string>
#include <vector>


#define NOMINMAX
#include <d3d11.h>
#include <Windows.h>
#include <strsafe.h>

#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "FalconWindow.h"
#include "Header.h"
#include "imgui.h"


// Forward declare message handler from imgui_impl_win32.cpp (outside of anonymous namespace). See imgui_impl_win32.h.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

// The UI abstraction is contained here. Subclasses implement the look and feel.
class GenericUI {
public:
  virtual ~GenericUI() {}

  // Fires up any initialization of the UI and runs until the program ends.
  virtual void Run() = 0;

protected:
  // Used for PickOptions.
  struct SelectionState {
    int selection = -1;
    bool selected = false;
  };

private:
  virtual std::string PickOption(SelectionState& selection_state, const std::string& title, const std::vector<std::string>& options) = 0;
};

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// TODO(sassah): implement a windows UI (WindowUI) that inherits from GenericUI.
class WindowUI : public GenericUI {
public:
  static constexpr auto kAppName = L"Falcon UI Editor";

  ~WindowUI() override {}

  // Helper functions
  static bool CreateDeviceD3D(HWND hWnd) {
      // Setup swap chain
      DXGI_SWAP_CHAIN_DESC sd;
      ZeroMemory(&sd, sizeof(sd));
      sd.BufferCount = 2;
      sd.BufferDesc.Width = 0;
      sd.BufferDesc.Height = 0;
      sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      sd.BufferDesc.RefreshRate.Numerator = 60;
      sd.BufferDesc.RefreshRate.Denominator = 1;
      sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
      sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      sd.OutputWindow = hWnd;
      sd.SampleDesc.Count = 1;
      sd.SampleDesc.Quality = 0;
      sd.Windowed = TRUE;
      sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

      UINT createDeviceFlags = 0;
      //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
      D3D_FEATURE_LEVEL featureLevel;
      const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
      if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK) {
          return false;
      }

      CreateRenderTarget();
      return true;
  }

  static void CleanupDeviceD3D() {
      CleanupRenderTarget();
      if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
      if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
      if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
  }

  static void CreateRenderTarget() {
      ID3D11Texture2D* pBackBuffer;
      g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
      g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
      pBackBuffer->Release();
  }

  static void CleanupRenderTarget() {
      if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
  }

  static LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) { return true; }

    RECT rcClient;
    int i;

    switch (uMsg) {
    case WM_CREATE: // creating main window  
        return 0;
    case WM_SIZE:   // main window changed size 
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
        return 0;
    case WM_CLOSE:
        // Create the message box. If the user clicks 
        // the Yes button, destroy the main window. 
        if (MessageBox(hwnd, L"Close?", kAppName, MB_YESNOCANCEL) == IDYES) {
            DestroyWindow(hwnd);
        } else {
            return 0;
        }
    case WM_DESTROY:
        // Post the WM_QUIT message to 
        // quit the application terminate. 
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
  }

  // Routine to display windows messages. Must be called right after the error because relies on GetLastError.
  void DisplayError() const {
      LPVOID lpMsgBuf;
      LPVOID lpDisplayBuf;
      DWORD dw = GetLastError();

      FormatMessage(
          FORMAT_MESSAGE_ALLOCATE_BUFFER |
          FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
          NULL,
          dw,
          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
          (LPTSTR)&lpMsgBuf,
          0, NULL);

      lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
          (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)kAppName) + 40) * sizeof(TCHAR));
      StringCchPrintf((LPTSTR)lpDisplayBuf,
          LocalSize(lpDisplayBuf) / sizeof(TCHAR),
          TEXT("%s failed with error %d: %s"),
          L"Run", dw, lpMsgBuf);
      MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

      LocalFree(lpMsgBuf);
      LocalFree(lpDisplayBuf);
  }

  void Run() override {
    const wchar_t CLASS_NAME[] = L"UI Editor";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = (HINSTANCE)GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Create the window.
    hwnd_ = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        kAppName,                       // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // Parent window    
        NULL,       // Menu
        wc.hInstance,  // Instance handle
        NULL        // Additional application data
    );
    if (hwnd_ == nullptr) {
        DisplayError();
        return;
    }

    if (!CreateDeviceD3D(hwnd_)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        DisplayError();
        return;
    }

    falcon_installs_ = GetAllBMSInstallations();
    if (falcon_installs_.empty()) {
      MessageBox(NULL, (LPCTSTR)"Unable to find any Falcon install", TEXT("Error"), MB_OK);
      return;
    }

    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable some options
    // Initialize Platform + Renderer backends (here: using imgui_impl_win32.cpp + imgui_impl_dx11.cpp)
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Event loop.
    // Main loop
    bool done = false;
    while (!done) {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) { done = true; }
        }
        if (done) {
            break;        
        }

        DoImGuiFrame();
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd_);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
  }

  void DoImGuiFrame() {
    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    struct RunOnExit {
      RunOnExit(WindowUI& thiz) : thiz_(thiz) {}
      ~RunOnExit() {
        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { thiz_.clear_color_.x * thiz_.clear_color_.w, thiz_.clear_color_.y * thiz_.clear_color_.w, thiz_.clear_color_.z * thiz_.clear_color_.w, thiz_.clear_color_.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(0, 0); // Present without vsync
      }
      WindowUI& thiz_;
    } run_on_exit(*this);

    if (show_demo_window_) {
        ImGui::ShowDemoWindow(&show_demo_window_);
    }

    // End will be called by run_on_exit destructor.
    ImGui::Begin("Falcon UI Editor");

    // Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        // TODO remove this.
        ImGui::Checkbox("Demo Window", &show_demo_window_);      // Edit bools storing our window open/close state

        if (ImGui::Button("Test Main")) {
            selected_install_state_.selected = true;
            falcon_install_dir_ = "n:\\Falcon BMS 4.37 (Internal)";
            selected_theater_state_.selected = true;
            falcon_theater_ = "Default";
            ui_set_selected_ = "main";
            selected_window_state_.selected = true;
            window_selected_ = "art\\main\\main_win.scf";
        }

        std::string falcon_install;
        if (!selected_install_state_.selected) {
        falcon_install = PickOption(selected_install_state_, "Pick Installation", falcon_installs_);
        if (selected_install_state_.selected) {
            falcon_install_dir_ = InstallDirForInstallation(falcon_install);
        }
        return;
        }

        // No install.
        if (falcon_install_dir_.empty()) {
        PostQuitMessage(/*error_code=*/1);
        return;
        }
          
        // Pick theater.
        if (!selected_theater_state_.selected) {
        std::vector<std::string> theaters = ListTheaters(falcon_install_dir_);
        falcon_theater_ = PickOption(selected_theater_state_, "Pick Theater", theaters);
        return;
        }

        if (falcon_theater_.empty()) {
        PostQuitMessage(/*error_code=*/2);
        return;
        }

        // Display all UI files for the theater.
        if (ui_set_selected_.empty()) {
        ui_set_selected_ = PickUISet();
        return;
        }

        // Open the window list.
        if (!selected_window_state_.selected) {
        window_selected_ = PickWindow(ui_set_selected_);
        return;
        }
          
        if (window_selected_.empty()) {
            PostQuitMessage(/*error_code=*/3);
            return;
        }

        if (!window_.SetupDone()) {
        SetupWindow();
        return;
        } else {
        window_.Draw();
        }
    }
  }

private:
  std::string PickUISet() {
      // Hardcoded, see internal_resolution.h:
      // static void LoadMainWindow();
      // static void LoadTacticalWindows();
      // static void LoadCampaignWindows();
      // static void LoadDogFightWindows();
      // static void LoadSetupWindows();
      // static void LoadCampaignSelectWindows();
      // static void LoadTacEngSelectWindows();
      // static void LoadInstantActionWindows();
      // static void LoadPlannerWindows();
      // static void LoadTacticalReferenceWindows();
      // static void LoadLogBookWindows();
      // static void LoadCommWindows();
      std::string data_dir = falcon_install_dir_ + DataDirForTheater(falcon_theater_);
      std::vector<std::string> ui_set = {
          "Main", "Tactical???", "Campaign", "Dogfight", "Setup", "Campaign Select", "Tactical Engagement", "Instant Action", "Planner", "Tactical Reference", "Logbook", "Comms"
      };
      return PickOption(selected_theater_state_, "Pick UI", ui_set);
  }

  // Picks a window of the UI set (for example, the main window is composed of several sets).
  std::string PickWindow(const std::string& ui_set) {
      return PickOption(selected_window_state_, "Pick Window", GetWindowList(falcon_install_dir_ + DataDirForTheater(falcon_theater_) + '\\', ui_set));
  }

  std::string PickOption(SelectionState& selection_state, const std::string& title, const std::vector<std::string>& options) override {
    for (int n = 0; n < options.size(); ++n) {
      if (ImGui::Selectable(options[n].c_str(), selection_state.selection == n)) {
          selection_state.selection = n;
      }
    }
    if (ImGui::Button("OK")) {
      selection_state.selected = true;
      if (selection_state.selection == -1 || selection_state.selection >= options.size()) return "";
      return options[selection_state.selection];
    }
    return "";
  }

  void SetupWindow() {
      window_.SetupFromFile(falcon_install_dir_ + DataDirForTheater(falcon_theater_) + "\\" + window_selected_);
  }

  bool show_demo_window_ = true;
  ImVec4 clear_color_ = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  SelectionState selected_install_state_;
  std::vector<std::string> falcon_installs_;
  std::string falcon_install_dir_;
  SelectionState selected_theater_state_;
  std::string falcon_theater_;
  std::string ui_set_selected_;
  SelectionState selected_window_state_;
  std::string window_selected_;
  bool window_setup_done_ = false;
  falcon_ui::Window window_;

  HWND hwnd_ = nullptr;
};


// This is the main UI entry (factory). It is the one which picks which UI it should start.
std::unique_ptr<GenericUI> CreateUI(int argc, char** argv) {
  return std::make_unique<WindowUI>();
}

}  // namespace


int main(int argc, char** argv) {
  auto ifg = CreateUI(argc, argv);
  ifg->Run();
  return 0;
}