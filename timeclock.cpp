#include <Windows.h>
#include <tchar.h>
#include <CommCtrl.h>
#include <Windowsx.h>
#include <wingdi.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void OnSetColor(HWND hwnd, COLORREF& color);
void OnSetFont(HWND hwnd);
void OnSetBackgroundColor(HWND hwnd);
void UpdateTimeBitmap(HWND hwnd, HDC hdc);
void ShowMenu(HWND hwnd, BOOL bShow);
void SaveSettings();
void LoadSettings();
void ModifyWindowStyle(HWND hwnd);
HBRUSH hBrush; // 用于保存背景颜色的画刷
HFONT hFont;   // 用于保存字体
COLORREF textColor = RGB(0, 0, 0);
COLORREF bgColor = RGB(255, 255, 255); // 白色背景
POINT offset; // 用于保存拖拽时的偏移量
HBITMAP hBitmap;

#define IDC_TIMER 1001
#define SETTINGS_KEY _T("Software\\TimeDisplaySettings")

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 注册窗体类
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("Time Display / 时间显示窗体");
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
 
    HWND hwnd = CreateWindowEx(
        WS_EX_ACCEPTFILES | WS_EX_TOPMOST,  // 添加 WS_EX_TOPMOST 样式以使窗口始终置顶
        wc.lpszClassName,
        _T("Time Display / 时间显示窗体"),
        WS_OVERLAPPEDWINDOW & ~WS_CAPTION,  // 移除 WS_CAPTION 样式以隐藏标题栏
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 150,
        NULL,
        NULL,
        hInstance,
        NULL
    );
    if (!hwnd) {
        return 1;
    }

    // 初始化画刷
    hBrush = CreateSolidBrush(bgColor);

    // 初始化字体
    hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    // 创建定时器
    SetTimer(hwnd, IDC_TIMER, 1000, NULL);

    // 创建Bitmap
    hBitmap = CreateCompatibleBitmap(GetDC(hwnd), 300, 150);

    // 加载设置
    LoadSettings();

    // 显示窗体
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 释放画刷和字体资源
    DeleteObject(hBrush);
    DeleteObject(hFont);
    DeleteObject(hBitmap);

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static int cxClient, cyClient;
    static POINT offset;
    static BOOL isDragging = FALSE;

    switch (uMsg) {
    case WM_CREATE:
        // 隐藏标题栏
        ModifyWindowStyle(hwnd);
        break;

    case WM_SIZE:
        // 更新窗口尺寸
        cxClient = LOWORD(lParam);
        cyClient = HIWORD(lParam);

        // 更新Bitmap尺寸
        DeleteObject(hBitmap);
        hBitmap = CreateCompatibleBitmap(GetDC(hwnd), cxClient, cyClient);

        ShowMenu(hwnd, TRUE);
        break;

    case WM_TIMER:
        // 更新窗口文本为当前时间
    {
        HDC hdc = GetDC(hwnd);
        HDC hdcMem = CreateCompatibleDC(hdc);
        SelectObject(hdcMem, hBitmap);

        // 绘制背景
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        FillRect(hdcMem, &rcClient, hBrush);

        // 绘制时间
        UpdateTimeBitmap(hwnd, hdcMem);

        BitBlt(hdc, 0, 0, cxClient, cyClient, hdcMem, 0, 0, SRCCOPY);

        DeleteDC(hdcMem);
        ReleaseDC(hwnd, hdc);
    }
    break;

    case WM_DESTROY:
        KillTimer(hwnd, IDC_TIMER); // 销毁定时器
        SaveSettings(); // 保存设置
        PostQuitMessage(0);
        break;

    case WM_CTLCOLORSTATIC: {
        // 设置标签控件的文本颜色和背景颜色
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, textColor);   // 设置文本颜色
        SetBkColor(hdcStatic, bgColor); // 设置背景颜色
        return (INT_PTR)hBrush; // 返回画刷句柄作为背景
    }

    case WM_ERASEBKGND:
        // 擦除背景时填充窗口背景色
        return 1;

    case WM_COMMAND:
        // 处理菜单项命令
        switch (LOWORD(wParam)) {
        case 1001:
            OnSetColor(hwnd, textColor);
            break;
        case 1002:
            OnSetBackgroundColor(hwnd);
            break;
        case 1003:
            OnSetFont(hwnd);
            break;
        case 1004: // 新增的退出菜单项
            // 提示用户是否确认退出
           // 获取窗口客户区的矩形区域
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);

            // 计算消息框的中心坐标
            int xPos = (rcClient.right - rcClient.left) / 2;
            int yPos = (rcClient.bottom - rcClient.top) / 2;

            // 提示用户是否确认退出
            if (MessageBox(hwnd, _T("确定要退出吗？"), _T("退出确认"), MB_ICONQUESTION | MB_YESNO | MB_APPLMODAL | MB_SETFOREGROUND) == IDYES) {
                // 用户确认退出，发送退出消息
                PostQuitMessage(0);
            }
            break;
        }
        break;

    case WM_LBUTTONDOWN:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (pt.x < cxClient && pt.y < cyClient) {
            // 点击窗口内部，开始拖拽
            offset = pt;
            isDragging = TRUE;
            SetCapture(hwnd);
        }
    }
    break;

    case WM_MOUSEMOVE:
        if (isDragging) {
            // 计算鼠标的相对位移
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            int dx = pt.x - offset.x; // 使用鼠标位置与偏移量的差值
            int dy = pt.y - offset.y; // 使用鼠标位置与偏移量的差值

            // 移动窗口
            RECT rc;
            GetWindowRect(hwnd, &rc);
            int newX = rc.left + dx;
            int newY = rc.top + dy;
            newX = max(newX, 0); // 限制窗口移动到屏幕边界以内
            newY = max(newY, 0); // 限制窗口移动到屏幕边界以内
            newX = min(newX, GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)); // 限制窗口移动到屏幕边界以内
            newY = min(newY, GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)); // 限制窗口移动到屏幕边界以内
            SetWindowPos(hwnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;

    case WM_LBUTTONUP:
        isDragging = FALSE;
        ReleaseCapture();
        break;

    case WM_RBUTTONDOWN:
    {
        //// 右键点击时，显示菜单
        HMENU hMenu = CreatePopupMenu();
        AppendMenu(hMenu, MF_STRING, 1001, _T("Font Color / 字体颜色"));
        AppendMenu(hMenu, MF_STRING, 1002, _T("Background Color / 背景颜色"));
        AppendMenu(hMenu, MF_STRING, 1003, _T("Font / 字体"));
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL); // 分隔线
        AppendMenu(hMenu, MF_STRING, 1004, _T("退出")); // 新增的退出菜单项
        
        // 获取当前鼠标位置
        POINT pt;
        GetCursorPos(&pt);

        // 显示菜单
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    }
    break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

void OnSetColor(HWND hwnd, COLORREF& color) {
    // 选择颜色对话框
    CHOOSECOLOR cc = { 0 };
    static COLORREF acrCustClr[16];

    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = (LPDWORD)acrCustClr;
    cc.rgbResult = color;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColor(&cc) == TRUE) {
        // 更新全局变量的颜色
        color = cc.rgbResult;
        // 设置新的颜色
        InvalidateRect(hwnd, NULL, TRUE); // 强制刷新窗口
    }
}

void OnSetFont(HWND hwnd) {
    // 选择字体对话框
    CHOOSEFONT cf = { 0 };
    static LOGFONT lf;

    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = hwnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_EFFECTS;

    if (ChooseFont(&cf) == TRUE) {
        // 设置新字体
        hFont = CreateFontIndirect(&lf);
        SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
        // 更新窗口背景色
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void OnSetBackgroundColor(HWND hwnd) {
    // 选择颜色对话框
    CHOOSECOLOR cc = { 0 };
    static COLORREF acrCustClr[16];

    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = (LPDWORD)acrCustClr;
    cc.rgbResult = bgColor;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColor(&cc) == TRUE) {
        // 更新全局变量的背景颜色
        bgColor = cc.rgbResult;
        // 设置新的文本颜色和背景颜色
        hBrush = CreateSolidBrush(bgColor);
        InvalidateRect(hwnd, NULL, TRUE); // 强制刷新窗口
    }
}

void UpdateTimeBitmap(HWND hwnd, HDC hdc) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    TCHAR timeString[9]; // HH:mm:ss\0
    _stprintf_s(timeString, _T("%02d:%02d:%02d"), st.wHour, st.wMinute, st.wSecond);

    // 获取窗口客户区尺寸
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // 动态计算新的字体大小
    int fontWidth = rcClient.right * 8 / 10; // 文字宽度为窗口宽度的80%
    int fontHeight = MulDiv(fontWidth, 1, 3); // 每个文字的宽度占总宽度的1/3

    // 重新创建字体
    HFONT hNewFont = CreateFont(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Arial"));

    // 保存旧的字体
    HFONT hOldFont = (HFONT)SelectObject(hdc, hNewFont);

    // 绘制时间
    SetTextColor(hdc, textColor);
    SetBkColor(hdc, bgColor);

    // 绘制在中间
    DrawText(hdc, timeString, -1, &rcClient, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // 恢复旧的字体
    SelectObject(hdc, hOldFont);

    // 释放新的字体
    DeleteObject(hNewFont);
}

void ShowMenu(HWND hwnd, BOOL bShow) {
    if (bShow) {
        // 显示窗口菜单
        SetMenu(hwnd, GetMenu(hwnd));
        // 重新绘制窗口
        RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    else {
        // 隐藏窗口菜单
        SetMenu(hwnd, NULL);
        // 重新绘制窗口
        RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}

void SaveSettings() {
    // 保存设置到注册表
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, SETTINGS_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, _T("TextColor"), 0, REG_DWORD, (BYTE*)&textColor, sizeof(COLORREF));
        RegSetValueEx(hKey, _T("BgColor"), 0, REG_DWORD, (BYTE*)&bgColor, sizeof(COLORREF));
        // TODO: 保存字体设置
        RegCloseKey(hKey);
    }
}

void LoadSettings() {
    // 从注册表读取设置
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, SETTINGS_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dwSize = sizeof(COLORREF);
        RegQueryValueEx(hKey, _T("TextColor"), NULL, NULL, (LPBYTE)&textColor, &dwSize);
        RegQueryValueEx(hKey, _T("BgColor"), NULL, NULL, (LPBYTE)&bgColor, &dwSize);
        // TODO: 加载字体设置
        RegCloseKey(hKey);
    }
}

void ModifyWindowStyle(HWND hwnd) {
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~WS_CAPTION;  // 移除标题栏样式
    style |= WS_BORDER;    // 添加边框样式，以便移动窗口
    SetWindowLong(hwnd, GWL_STYLE, style);
}
