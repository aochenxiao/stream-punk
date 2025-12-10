#include "web_server.hpp"

#include <windows.h>
#include <string>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <cmath>
#include <thread>

// 全局变量
struct TrailPoint {
    int x, y;
    COLORREF color;
};

std::vector<TrailPoint> mouseTrail;
bool enhancedMode = false;

// 函数声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void PrintMousePosition(int x, int y);
std::wstring GetMousePositionString(int x, int y);
COLORREF GetRainbowColor(int index, int total);
void DrawGradientBackground(HDC hdc, RECT& rect);
void DrawMouseTrail(HDC hdc);
void DrawBasicUI(HDC hdc, int x, int y);
void DrawEnhancedUI(HDC hdc, int x, int y);
void ShowUsage();

// 显示使用说明
void ShowUsage() {
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"🖱️  鼠标位置追踪器 - 统一版本" << std::endl;
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"使用方法:" << std::endl;
    std::wcout << L"  mousPos_unified.exe         - 基础模式" << std::endl;
    std::wcout << L"  mousPos_unified.exe basic   - 基础模式" << std::endl;
    std::wcout << L"  mousPos_unified.exe enhanced - 增强模式" << std::endl;
    std::wcout << L"========================================" << std::endl;
}



std::ostringstream oss;
SpObjProtocolOutput o{ oss };

// 在控制台打印鼠标位置
void PrintMousePosition(int x, int y) {
    static int lastX = -1, lastY = -1;
    
    // 只在位置变化时打印
    if (x != lastX || y != lastY) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        struct tm timeinfo;
        localtime_s(&timeinfo, &time_t);
        
        if (enhancedMode) {
            std::wcout << L"["
                       << std::put_time(&timeinfo, L"%H:%M:%S")
                       << L"] 🖱️ 鼠标位置: X=" << std::setw(4) << x 
                       << L", Y=" << std::setw(4) << y 
                       << L" | 轨迹点数: " << mouseTrail.size() << std::endl;
            auto wsSvr = std::dynamic_pointer_cast<WebSocketChat>(DrClassMap::getSingleInstance("WebSocketChat"));
            MousePosition pos;
            pos.x = x;
            pos.y = y;
            o << pos;
            auto msg = oss.str();
            o.o.clear();
            oss.str("");
            wsSvr->connectionManager_.broadcastToAll(msg);
        } else {
            std::wcout << L"["
                       << std::put_time(&timeinfo, L"%Y-%m-%d %H:%M:%S")
                       << L"] 鼠标位置: X=" << x << L", Y=" << y << std::endl;
        }
        
        lastX = x;
        lastY = y;
    }
}

// 获取鼠标位置字符串
std::wstring GetMousePositionString(int x, int y) {
    std::wstringstream ss;
    ss << L"X: " << x << L"  Y: " << y;
    return ss.str();
}

// 获取彩虹色
COLORREF GetRainbowColor(int index, int total) {
    if (total == 0) return RGB(255, 0, 0);
    
    float hue = (float)index / total * 360.0f;
    float saturation = 0.8f;
    float brightness = 0.9f;
    
    float c = brightness * saturation;
    float x = c * (1 - abs(fmod(hue / 60.0f, 2) - 1));
    float m = brightness - c;
    
    float r, g, b;
    if (hue < 60) {
        r = c; g = x; b = 0;
    } else if (hue < 120) {
        r = x; g = c; b = 0;
    } else if (hue < 180) {
        r = 0; g = c; b = x;
    } else if (hue < 240) {
        r = 0; g = x; b = c;
    } else if (hue < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }
    
    return RGB((r + m) * 255, (g + m) * 255, (b + m) * 255);
}

// 绘制渐变背景
void DrawGradientBackground(HDC hdc, RECT& rect) {
    // 创建渐变效果
    for (int i = 0; i < rect.bottom; i++) {
        int r = 220 + (i * 35 / rect.bottom);
        int g = 230 + (i * 25 / rect.bottom);
        int b = 240 + (i * 15 / rect.bottom);
        
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        
        MoveToEx(hdc, rect.left, rect.top + i, nullptr);
        LineTo(hdc, rect.right, rect.top + i);
        
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);
    }
}

// 绘制鼠标轨迹
void DrawMouseTrail(HDC hdc) {
    if (mouseTrail.size() < 2) return;
    
    for (size_t i = 1; i < mouseTrail.size(); i++) {
        // 创建画笔
        HPEN hPen = CreatePen(PS_SOLID, 3, mouseTrail[i].color);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        
        // 绘制线段
        MoveToEx(hdc, mouseTrail[i-1].x, mouseTrail[i-1].y, nullptr);
        LineTo(hdc, mouseTrail[i].x, mouseTrail[i].y);
        
        // 绘制轨迹点
        HBRUSH hBrush = CreateSolidBrush(mouseTrail[i].color);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
        
        Ellipse(hdc, mouseTrail[i].x - 4, mouseTrail[i].y - 4, 
                mouseTrail[i].x + 4, mouseTrail[i].y + 4);
        
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);
        DeleteObject(hBrush);
    }
}

// 绘制基础UI
void DrawBasicUI(HDC hdc, int x, int y) {
    // 设置字体
    HFONT hFont = CreateFontW(
        24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei"
    );
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    // 设置文本颜色和背景模式
    SetTextColor(hdc, RGB(0, 51, 102));
    SetBkMode(hdc, TRANSPARENT);
    
    // 绘制标题
    RECT titleRect = {50, 30, 750, 70};
    DrawTextW(hdc, L"🖱️ 鼠标位置追踪器", -1, &titleRect, DT_CENTER | DT_VCENTER);
    
    // 创建坐标显示字体
    HFONT hCoordFont = CreateFontW(
        28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Consolas"
    );
    SelectObject(hdc, hCoordFont);
    
    // 绘制坐标信息
    std::wstring coordStr = GetMousePositionString(x, y);
    RECT coordRect = {200, 450, 600, 490};
    DrawTextW(hdc, coordStr.c_str(), -1, &coordRect, DT_CENTER | DT_VCENTER);
    
    // 绘制说明文字
    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(80, 80, 80));
    RECT infoRect = {50, 550, 750, 580};
    DrawTextW(hdc, L"在窗口内移动鼠标查看实时位置", -1, &infoRect, DT_CENTER | DT_VCENTER);
    
    // 清理资源
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hCoordFont);
}

// 绘制增强UI
void DrawEnhancedUI(HDC hdc, int x, int y) {
    // 设置字体
    HFONT hFont = CreateFontW(
        28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei"
    );
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    // 设置文本颜色和背景模式
    SetTextColor(hdc, RGB(0, 51, 102));
    SetBkMode(hdc, TRANSPARENT);
    
    // 绘制主标题
    RECT titleRect = {50, 30, 750, 80};
    DrawTextW(hdc, L"🖱️ 增强版鼠标位置追踪器", -1, &titleRect, DT_CENTER | DT_VCENTER);
    
    // 创建更大的字体用于坐标显示
    HFONT hCoordFont = CreateFontW(
        36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Consolas"
    );
    SelectObject(hdc, hCoordFont);
    
    // 绘制坐标信息背景
    RECT coordBgRect = {200, 450, 600, 520};
    HBRUSH hBgBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &coordBgRect, hBgBrush);
    DeleteObject(hBgBrush);
    
    // 绘制坐标信息边框
    HPEN hBorderPen = CreatePen(PS_SOLID, 2, RGB(100, 150, 200));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hBorderPen);
    Rectangle(hdc, coordBgRect.left, coordBgRect.top, coordBgRect.right, coordBgRect.bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBorderPen);
    
    // 绘制坐标信息
    std::wstring coordStr = GetMousePositionString(x, y);
    RECT coordRect = {200, 450, 600, 520};
    DrawTextW(hdc, coordStr.c_str(), -1, &coordRect, DT_CENTER | DT_VCENTER);
    
    // 绘制说明文字
    HFONT hInfoFont = CreateFontW(
        18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei"
    );
    SelectObject(hdc, hInfoFont);
    
    SetTextColor(hdc, RGB(80, 80, 80));
    RECT infoRect = {50, 550, 750, 580};
    DrawTextW(hdc, L"✨ 移动鼠标查看彩虹轨迹效果 | 控制台输出实时坐标", -1, &infoRect, DT_CENTER | DT_VCENTER);
    
    // 绘制状态信息
    SetTextColor(hdc, RGB(60, 120, 60));
    RECT statusRect = {50, 100, 750, 140};
    std::wstring statusStr = L"状态: 追踪中... | 轨迹点数: " + std::to_wstring(mouseTrail.size());
    DrawTextW(hdc, statusStr.c_str(), -1, &statusRect, DT_CENTER | DT_VCENTER);
    
    // 清理资源
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hCoordFont);
    DeleteObject(hInfoFont);
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static int mouseX = 0, mouseY = 0;
    
    switch (uMsg) {
        case WM_MOUSEMOVE: {
            mouseX = LOWORD(lParam);
            mouseY = HIWORD(lParam);
            
            // 打印鼠标位置到控制台
            PrintMousePosition(mouseX, mouseY);
            
            // 增强模式下记录轨迹
            if (enhancedMode) {
                TrailPoint point;
                point.x = mouseX;
                point.y = mouseY;
                point.color = GetRainbowColor(mouseTrail.size(), 100);
                mouseTrail.push_back(point);
                
                // 限制轨迹点数量
                if (mouseTrail.size() > 100) {
                    mouseTrail.erase(mouseTrail.begin());
                }
            }
            
            // 重绘窗口
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            if (enhancedMode) {
                // 增强模式：绘制渐变背景
                DrawGradientBackground(hdc, rect);
                // 绘制鼠标轨迹
                DrawMouseTrail(hdc);
                // 绘制增强UI
                DrawEnhancedUI(hdc, mouseX, mouseY);
            } else {
                // 基础模式：使用简单背景
                HBRUSH hBrush = CreateSolidBrush(RGB(245, 245, 245));
                FillRect(hdc, &rect, hBrush);
                DeleteObject(hBrush);
                
                // 绘制基础UI
                DrawBasicUI(hdc, mouseX, mouseY);
            }
            
            EndPaint(hwnd, &ps);
            break;
        }
        
        case WM_DESTROY: {
            PostQuitMessage(0);
            break;
        }
        
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// WinMain - 程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 解析命令行参数
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    // 显示使用说明
    ShowUsage();
    
    // 处理命令行参数
    if (argc > 1) {
        std::wstring modeArg = argv[1];
        if (modeArg == L"enhanced") {
            enhancedMode = true;
            std::wcout << L"🌈 启动增强模式..." << std::endl;
        } else if (modeArg == L"basic") {
            enhancedMode = false;
            std::wcout << L"📍 启动基础模式..." << std::endl;
        } else {
            std::wcout << L"❓ 未知参数，使用基础模式..." << std::endl;
            enhancedMode = false;
        }
    } else {
        std::wcout << L"📍 使用基础模式 (添加 'enhanced' 参数启用增强模式)..." << std::endl;
        enhancedMode = false;
    }
    
    auto ws_thread = std::thread([]() {
        app().addListener("127.0.0.1", 12345).run();
    });

    std::wcout << L"========================================" << std::endl;
    
    // 注册窗口类
    const wchar_t CLASS_NAME[] = L"MousePositionTracker";
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }
    
    // 创建窗口
    HWND hwnd = CreateWindowExW(
        enhancedMode ? WS_EX_LAYERED : 0,  // 增强模式使用分层窗口
        CLASS_NAME,
        enhancedMode ? L"增强版鼠标位置追踪器" : L"鼠标位置追踪器",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 700,
        nullptr, nullptr, hInstance, nullptr
    );
    
    if (hwnd == nullptr) {
        MessageBoxW(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }
    
    // 增强模式设置窗口透明度
    if (enhancedMode) {
        SetLayeredWindowAttributes(hwnd, 0, 240, LWA_ALPHA);
    }
    
    // 显示窗口
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    // 消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    app().quit();
    ws_thread.join();

    LocalFree(argv);
    return 0;
}