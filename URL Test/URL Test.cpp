#include <windows.h>
#include <winuser.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>    
#include <WebView2.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/event.h>
#include <Objbase.h>
#include <gdiplus.h>
#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "ole32.lib")

#ifndef VK_KEY_K
#define VK_KEY_K 0x4B
#endif

#ifndef VK_KEY_L
#define VK_KEY_L 0x4C
#endif

#define UNICODE
#define _UNICODE

#define ID_BUTTON_CLICK 1001
#define ID_BUTTON_NAVIGATE 1002

#define ID_TEXT_LABEL 2001 
#define ID_EDIT_FIELD 2002
#define ID_BUTTON_OK 2003
#define ID_BUTTON_ABOUT 2004
#define ID_BUTTON_CHANGELOG 2005

#define ID_TOOLBAR 3000
#define ID_ADDRESS_BAR 3001
#define ID_BACK_BUTTON 3002
#define ID_FORWARD_BUTTON 3003
#define ID_REFRESH_BUTTON 3004
#define ID_SEARCH_BAR 3005
#define TOOLBAR_HEIGHT 30

#define WM_NAVIGATE_NEW_URL (WM_USER + 1)
#define WM_UPDATE_URL_TEXT (WM_USER + 2)
#define WM_NAVIGATE_NEW_SEARCH (WM_USER + 3)
#define WM_UPDATE_SEARCH_TEXT (WM_USER + 4)

#define MAX_URL_LENGTH 512
TCHAR g_currentUrl[MAX_URL_LENGTH] = TEXT("about:blank");
TCHAR g_currentSearch[MAX_URL_LENGTH] = TEXT("Search");


const TCHAR CLASS_NAME[] = TEXT("MyWindowClass");
const TCHAR SECONDARY_CLASS_NAME[] = TEXT("SecondaryWindowClass");
const TCHAR NAVIGATOR_CLASS_NAME[] = TEXT("NavigatorWindowClass");
const TCHAR ABOUT_CLASS_NAME[] = TEXT("AboutWindowClass");
const TCHAR CHANGELOG_CLASS_NAME[] = TEXT("ChangelogWindowClass");
HINSTANCE g_hInstance = NULL;
HWND g_hToolbar = NULL;
HWND g_hAddressBar = NULL;
HWND g_hSearchBar = NULL;

using namespace Microsoft::WRL;
ComPtr<ICoreWebView2Controller> g_webviewController = nullptr;
ComPtr<ICoreWebView2> g_webview = nullptr;

void FormatSearchQuery(const TCHAR* input, TCHAR* output, size_t outputSize)
{
    const TCHAR* prefix = TEXT("https://www.google.com/search?q=");
    _tcsncpy_s(output, outputSize, prefix, _TRUNCATE);

    size_t offset = _tcslen(output);

    for (size_t i = 0; input[i] != TEXT('\0') && offset < outputSize - 1; ++i)
    {
        if (input[i] == TEXT(' '))
        {
            if (offset < outputSize - 1)
            {
                output[offset++] = TEXT('+');
            }
        }
        else
        {
            if (offset < outputSize - 1)
            {
                output[offset++] = input[i];
            }
        }
    }
    output[offset] = TEXT('\0'); // Null-terminate the string
}

void EnsureProtocol(TCHAR* url) {
    if (_tcsncmp(url, TEXT("http://"), 7) != 0 &&
        _tcsncmp(url, TEXT("https://"), 8) != 0 &&
        _tcsncmp(url, TEXT("file:///"), 7) != 0 &&
        _tcsncmp(url, TEXT("about:"), 6) != 0)
    {
        // Check if the URL is long enough to modify
        if (_tcslen(url) + 9 < MAX_URL_LENGTH) { // 9 for "https://" + null terminator
            TCHAR tempUrl[MAX_URL_LENGTH];
            _tcscpy_s(tempUrl, MAX_URL_LENGTH, url);
            _tcscpy_s(url, MAX_URL_LENGTH, TEXT("https://"));
            _tcscat_s(url, MAX_URL_LENGTH, tempUrl);
        }
    }
}

HWND GetNavigatorWindowHandle()
{
    return FindWindow(NAVIGATOR_CLASS_NAME, NULL);
}

class FaviconChangeHandler : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    ICoreWebView2FaviconChangedEventHandler>
{
public:
    FaviconChangeHandler(HWND hwnd) : m_hwnd(hwnd) {}

    // 1. When the favicon changes, this event is raised
    IFACEMETHOD(Invoke)(ICoreWebView2* sender, IUnknown* args) override
    {
        ComPtr<ICoreWebView2_15> webview15;
        HRESULT hr = sender->QueryInterface(IID_PPV_ARGS(&webview15));
        if (FAILED(hr) || !webview15)
            return hr;

        return webview15->GetFavicon(
            COREWEBVIEW2_FAVICON_IMAGE_FORMAT_PNG,
            Microsoft::WRL::Callback<ICoreWebView2GetFaviconCompletedHandler>(
                [this](HRESULT errorCode, IStream* imageStream) -> HRESULT
                {
                    if (SUCCEEDED(errorCode) && imageStream != nullptr)
                    {
                        HICON hNewIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1));
                        if (hNewIcon)
                        {
                            SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hNewIcon);
                            SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hNewIcon);
                        }
                    }
                    return S_OK;
                }).Get());
    }
private:
    HWND m_hwnd;
};

LRESULT CALLBACK AddressBarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (msg)
    {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            HWND hwndParent = (HWND)dwRefData;
            GetWindowText(hwnd, g_currentUrl, MAX_URL_LENGTH);
            EnsureProtocol(g_currentUrl);

            if (g_webview) {
                g_webview->Navigate(g_currentUrl);
            }

            return 0;
        }
        break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SearchBarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (msg)
    {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            // 1. Get the text the user typed
            TCHAR searchQuery[MAX_URL_LENGTH];
            GetWindowText(hwnd, searchQuery, MAX_URL_LENGTH);

            // 2. Prepare the final search URL
            TCHAR finalUrl[MAX_URL_LENGTH];
            FormatSearchQuery(searchQuery, finalUrl, MAX_URL_LENGTH);

            // 3. Save it to the global URL variable
            _tcsncpy_s(g_currentUrl, MAX_URL_LENGTH, finalUrl, _TRUNCATE);

            // 4. Navigate WebView2
            if (g_webview) {
                g_webview->Navigate(finalUrl);
            }

            // Optional: Update the address bar to show the resulting URL
            // (Note: The SourceChanged event will also handle this, but it's good practice)
            HWND hwndParent = (HWND)dwRefData;
            SetWindowText(GetDlgItem(hwndParent, ID_ADDRESS_BAR), finalUrl);

            // Optional: Clear the search bar after searching
            SetWindowText(hwnd, TEXT(""));

            return 0;
        }
        break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK NavigatorWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {

    case WM_CREATE: {
        HRESULT hrCoInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(hrCoInit)) {
            MessageBox(hwnd, TEXT("COM initialization failed."), TEXT("Error"), MB_ICONERROR);
            return -1;
        }

        // Initialize common controls for the toolbar
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);

        g_hToolbar = CreateWindowEx(
            0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_TOOLBAR, g_hInstance, NULL
        );

        TBBUTTON tbButtons[] = {
            { 0, ID_BACK_BUTTON,    MAKELPARAM(FALSE, 0), BTNS_AUTOSIZE, {0}, 0, (INT_PTR)TEXT("Back") },
            { 1, ID_FORWARD_BUTTON, MAKELPARAM(FALSE, 0), BTNS_AUTOSIZE, {0}, 0, (INT_PTR)TEXT("Forward") },
            { 2, ID_REFRESH_BUTTON, MAKELPARAM(FALSE, 0), BTNS_AUTOSIZE, {0}, 0, (INT_PTR)TEXT("Refresh") },
            { 3, 0, MAKELPARAM(FALSE, 0), BTNS_SEP, {0}, 0, 0 } // Separator (Note: Image index changed to 3)
        };

        SendMessage(g_hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
        SendMessage(g_hToolbar, TB_ADDBUTTONS, _countof(tbButtons), (LPARAM)&tbButtons);
        SendMessage(g_hToolbar, TB_AUTOSIZE, 0, 0);
        SendMessage(g_hToolbar, TB_ENABLEBUTTON, ID_REFRESH_BUTTON, (LPARAM)MAKELONG(TRUE, 0));

        g_hAddressBar = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            TEXT("EDIT"), g_currentUrl,
            WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_ADDRESS_BAR, g_hInstance, NULL
        );

        g_hSearchBar = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            TEXT("EDIT"), g_currentSearch,
            WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_SEARCH_BAR, g_hInstance, NULL
        );

        SetWindowSubclass(g_hAddressBar, AddressBarProc, 1, (DWORD_PTR)hwnd);
        SetWindowSubclass(g_hSearchBar, SearchBarProc, 2, (DWORD_PTR)hwnd);

        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [hwnd](HRESULT envResult, ICoreWebView2Environment* environment) -> HRESULT {
                    if (envResult != S_OK) return envResult;

                    return environment->CreateCoreWebView2Controller(
                        hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [hwnd](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                                if (controllerResult != S_OK) return controllerResult;

                                g_webviewController = controller;
                                g_webviewController->get_CoreWebView2(&g_webview);

                                g_webview->Navigate(g_currentUrl);

                                // ** 5. Add Navigation Event Handlers **
                                // 5a. SourceChanged - to update the Address Bar
                                EventRegistrationToken tokenSourceChanged;
                                g_webview->add_SourceChanged(
                                    Callback<ICoreWebView2SourceChangedEventHandler>(
                                        [hwnd](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args) -> HRESULT {
                                            LPWSTR uri;
                                            sender->get_Source(&uri);
                                            SetWindowText(GetDlgItem(hwnd, ID_ADDRESS_BAR), uri);
                                            CoTaskMemFree(uri);
                                            return S_OK;
                                        }).Get(), &tokenSourceChanged);

                                // 5b. NavigationCompleted - to update Back/Forward button state
                                EventRegistrationToken tokenNavigationCompleted;
                                g_webview->add_NavigationCompleted(
                                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                        [hwnd](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                            BOOL canGoBack, canGoForward;
                                            sender->get_CanGoBack(&canGoBack);
                                            sender->get_CanGoForward(&canGoForward);

                                            SendMessage(g_hToolbar, TB_ENABLEBUTTON, ID_BACK_BUTTON, (LPARAM)MAKELONG(canGoBack, 0));
                                            SendMessage(g_hToolbar, TB_ENABLEBUTTON, ID_FORWARD_BUTTON, (LPARAM)MAKELONG(canGoForward, 0));

                                            LPWSTR uri;
                                            sender->get_Source(&uri);
                                            SetWindowText(GetDlgItem(hwnd, ID_ADDRESS_BAR), uri);
                                            CoTaskMemFree(uri);

                                            HWND hAddress = GetDlgItem(hwnd, ID_ADDRESS_BAR);
                                            if (hAddress)
                                            {
                                                SetFocus(hAddress);
                                                SendMessage(hAddress, EM_SETSEL, 0, -1);
                                            }

                                            return S_OK;
                                        }).Get(), &tokenNavigationCompleted);

                                EventRegistrationToken tokenGotFocus;
                                g_webviewController->add_GotFocus(
                                    Callback<ICoreWebView2FocusChangedEventHandler>(
                                        [hwnd](ICoreWebView2Controller* sender, IUnknown* args) -> HRESULT
                                        {
                                            HWND hAddress = GetDlgItem(hwnd, ID_ADDRESS_BAR);
                                            if (hAddress)
                                                SetFocus(hAddress);
                                            return S_OK;
                                        }
                                    ).Get(),
                                    &tokenGotFocus
                                );
                                EventRegistrationToken tokenAccel;
                                g_webviewController->add_AcceleratorKeyPressed(
                                    Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                                        [hwnd](ICoreWebView2Controller* sender,
                                            ICoreWebView2AcceleratorKeyPressedEventArgs* args) -> HRESULT
                                        {
                                            COREWEBVIEW2_KEY_EVENT_KIND kind;
                                            args->get_KeyEventKind(&kind);

                                            if (kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN)
                                            {
                                                UINT key;
                                                args->get_VirtualKey(&key);

                                                BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000);

                                                if (ctrl && key == 'L')
                                                {
                                                    HWND hAddress = GetDlgItem(hwnd, ID_ADDRESS_BAR);
                                                    if (hAddress)
                                                    {
                                                        SetFocus(hAddress);
                                                        SendMessage(hAddress, EM_SETSEL, 0, -1);
                                                    }

                                                    args->put_Handled(TRUE);
                                                    return S_OK;
                                                }

                                                if (ctrl && key == 'K')
                                                {
                                                    HWND hSearch = GetDlgItem(hwnd, ID_SEARCH_BAR);
                                                    if (hSearch)
                                                    {
                                                        SetFocus(hSearch);
                                                        SendMessage(hSearch, EM_SETSEL, 0, -1);
                                                    }

                                                    args->put_Handled(TRUE);
                                                    return S_OK;
                                                }
                                            }

                                            return S_OK;
                                        }
                                    ).Get(),
                                    &tokenAccel
                                );


                                // Initial bounds calculation will be done in WM_SIZE
                                RECT bounds;
                                GetClientRect(hwnd, &bounds);
                                PostMessage(hwnd, WM_SIZE, 0, MAKELPARAM(bounds.right - bounds.left, bounds.bottom - bounds.top));
                                g_webviewController->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);

                                return S_OK;
                            }).Get()
                                );
                }).Get()
                    );

        if (FAILED(hr)) {
            MessageBox(hwnd, TEXT("Error when initializing WebView2. Please install or update WebView2."), TEXT("Browser Error"), MB_ICONERROR);
        }
        return 0;
    }

    case WM_NAVIGATE_NEW_URL: {
        if (g_webview != nullptr) {
            EnsureProtocol(g_currentUrl);
            g_webview->Navigate(g_currentUrl);
            SetWindowText(GetDlgItem(hwnd, ID_ADDRESS_BAR), g_currentUrl);
        }
        return 0;
    }

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        const int buttonWidth = 50;
        const int addressBarLeft = 140;
        const int searchBarPadding = 10;
        const int searchBarFixedWidth = 200; // Define a fixed width for the search bar

        // Calculate the address bar width to leave space for the search bar
        const int addressBarWidth = width - addressBarLeft - searchBarFixedWidth - (searchBarPadding * 2);

        // Calculate the search bar position
        const int searchBarLeft = addressBarLeft + addressBarWidth + searchBarPadding;
        const int searchBarWidth = searchBarFixedWidth;

        const int buttonHeight = TOOLBAR_HEIGHT - 4;
        const int buttonTop = 2;

        MoveWindow(g_hToolbar, 0, 0, addressBarLeft, TOOLBAR_HEIGHT, TRUE);
        SendMessage(g_hToolbar, TB_AUTOSIZE, 0, 0);

        MoveWindow(
            GetDlgItem(hwnd, ID_ADDRESS_BAR),
            addressBarLeft, buttonTop, addressBarWidth, buttonHeight, TRUE);

        MoveWindow(
            GetDlgItem(hwnd, ID_SEARCH_BAR),
            searchBarLeft, buttonTop, searchBarWidth, buttonHeight, TRUE); // **NOW USES CORRECT ID**

        if (g_webviewController != nullptr) {
            RECT bounds = { 0, TOOLBAR_HEIGHT, width, height };
            g_webviewController->put_Bounds(bounds);
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 0x4C)
        {
            HWND hAddress = GetDlgItem(hwnd, ID_ADDRESS_BAR);
            if (hAddress)
            {
                SetFocus(hAddress);
                SendMessage(hAddress, EM_SETSEL, 0, -1);

                if (GetFocus() != hAddress)
                {
                    MessageBox(
                        hwnd,
                        TEXT("Ctrl+L didn't select the address bar. Please check the code."),
                        TEXT("Error"),
                        MB_ICONWARNING | MB_OK
                    );
                }
            }
            return 0;
        }

        else if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == VK_KEY_K)
        {
            HWND hSearch = GetDlgItem(hwnd, ID_SEARCH_BAR);
            if (hSearch)
            {
                SetFocus(hSearch);
                SendMessage(hSearch, EM_SETSEL, 0, -1);

                if (GetFocus() != hSearch)
                {
                    MessageBox(
                        hwnd,
                        TEXT("Ctrl+K didn't select the search bar. Please check the code."),
                        TEXT("Error"),
                        MB_ICONWARNING | MB_OK
                    );
                }
            }
            return 0;
        }
        break;
    }


    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        if (wmId == ID_BACK_BUTTON) {
            g_webview->GoBack();
            return 0;
        }
        else if (wmId == ID_FORWARD_BUTTON) {
            g_webview->GoForward();
            return 0;
        }
        else if (wmId == ID_REFRESH_BUTTON) {
            g_webview->Reload();
            return 0;
        }

        break;
    }
    
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY: {
        if (g_webviewController != nullptr) {
            g_webviewController->Close();
            g_webviewController = nullptr;
            g_webview = nullptr;
        }
        CoUninitialize();
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}


LRESULT CALLBACK SecondaryWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        CreateWindowEx(
            0, TEXT("STATIC"), TEXT("Please type an URL (ex: https://www.google.com/)"),
            WS_VISIBLE | WS_CHILD,
            10, 10, 350, 20,
            hwnd, (HMENU)ID_TEXT_LABEL, g_hInstance, NULL
        );
        
        CreateWindowEx(
            WS_EX_CLIENTEDGE,
            TEXT("EDIT"), g_currentUrl,
            WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
            10, 40, 360, 25,
            hwnd, (HMENU)ID_EDIT_FIELD, g_hInstance, NULL
        );

        CreateWindowEx(
            0, TEXT("BUTTON"), TEXT("OK"),
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            295, 230, 80, 25,
            hwnd, (HMENU)ID_BUTTON_OK, g_hInstance, NULL
        );

        CreateWindowEx(
            0, TEXT("BUTTON"), TEXT("Cancel"),
            WS_TABSTOP | WS_VISIBLE | WS_CHILD,
            205, 230, 80, 25,
            hwnd, (HMENU)IDCANCEL, g_hInstance, NULL
        );
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);

        if (wmId == ID_BUTTON_OK) {
            GetDlgItemText(hwnd, ID_EDIT_FIELD, g_currentUrl, MAX_URL_LENGTH);

            // Removed PostMessage(hwndParent, WM_UPDATE_URL_TEXT, 0, 0);

            // Find the browser window and tell it to navigate if it's open.
            HWND hwndNavigator = FindWindow(NAVIGATOR_CLASS_NAME, NULL);
            if (hwndNavigator) {
                PostMessage(hwndNavigator, WM_NAVIGATE_NEW_URL, 0, 0);
            }

            DestroyWindow(hwnd);
        }
        else if (wmId == IDCANCEL) {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HFONT hFont = CreateFont(
            24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_SWISS, TEXT("Mojang")
        );

        HFONT hSmallFont = CreateFont(
            24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_SWISS, TEXT("Windows XP Tahoma")
        );

        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        const TCHAR* titleText = TEXT("Internet Explorer 95 for Windows 11");

        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        TextOut(
            hdc,
            10, 5,
            titleText,
            (int)_tcslen(titleText)
        );

        const TCHAR* instructionsText = TEXT("Trying to revive Internet Explorer from Windows 9x/2000");

        SelectObject(hdc, hSmallFont);

        TextOut(
            hdc,
            10, 35,
            instructionsText,
            (int)_tcslen(instructionsText)
        );

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        DeleteObject(hSmallFont);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);

        HWND hwndSecondary = NULL;
        HWND hwndNavigator = NULL;
        HWND hwndAbout = NULL;
        HWND hwndChangelog = NULL;

        if (wmId == ID_BUTTON_CLICK) {
            hwndSecondary = CreateWindowEx(
                0, SECONDARY_CLASS_NAME, TEXT("Choose an URL"),
                WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU,
                200, 200, 400, 300,
                hwnd, NULL, g_hInstance, NULL
            );
            if (hwndSecondary) {
                ShowWindow(hwndSecondary, SW_SHOW);
                UpdateWindow(hwndSecondary);
            }
        }
        else if (wmId == ID_BUTTON_NAVIGATE) {
            hwndNavigator = CreateWindowEx(
                0, NAVIGATOR_CLASS_NAME, g_currentUrl,
                WS_OVERLAPPEDWINDOW,
                50, 50, 800, 600,
                NULL, NULL, g_hInstance, NULL
            );
            if (hwndNavigator) {
                ShowWindow(hwndNavigator, SW_SHOW);
                UpdateWindow(hwndNavigator);
            }
        }
        else if (wmId == ID_BUTTON_ABOUT) {
            hwndAbout = CreateWindowEx(
                0, ABOUT_CLASS_NAME, TEXT("About this Program"),
                WS_OVERLAPPEDWINDOW,
                50, 50, 800, 600,
                NULL, NULL, g_hInstance, NULL
            );
            if (hwndAbout) {
                ShowWindow(hwndAbout, SW_SHOW);
                UpdateWindow(hwndAbout);
            }
        }
        else if (wmId == ID_BUTTON_CHANGELOG) {
            hwndChangelog = CreateWindowEx(
                0, CHANGELOG_CLASS_NAME, TEXT("Changelog"),
                WS_OVERLAPPEDWINDOW,
                50, 50, 800, 600,
                NULL, NULL, g_hInstance, NULL
            );
            if (hwndChangelog) {
                ShowWindow(hwndChangelog, SW_SHOW);
                UpdateWindow(hwndChangelog);
            }
        }

        break;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK AboutWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HFONT hFont = CreateFont(
            24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_SWISS, TEXT("Mojang")
        );

        HFONT hSmallFont = CreateFont(
            24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_SWISS, TEXT("Windows XP Tahoma")
        );

        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        const TCHAR* titleText = TEXT("About this Program");

        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        TextOut(
            hdc,
            10, 5,
            titleText,
            (int)_tcslen(titleText)
        );

        const TCHAR* smallText1 = TEXT("This is a project i started for fun, and i decided to publish it to see how people react ");

        const TCHAR* smallText2 = TEXT("(they're probably not gonna do it).");

        const TCHAR* smallText3 = TEXT("If you are reading this, congrats! You are one of the few people who have read this.");

        const TCHAR* smallText4 = TEXT("WebView2 and The IE95 logo are a trademark of Microsoft.");


        SelectObject(hdc, hSmallFont);

        TextOut(
            hdc,
            10, 35,
            smallText1,
            (int)_tcslen(smallText1)
        );

        TextOut(
            hdc,
            10, 60,
            smallText2,
            (int)_tcslen(smallText2)
        );

        TextOut(
            hdc,
            10, 85,
            smallText3,
            (int)_tcslen(smallText3)
        );

        TextOut(
            hdc,
            10, 135,
            smallText4,
            (int)_tcslen(smallText4)
        );

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        DeleteObject(hSmallFont);
        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK ChangelogWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HFONT hFont = CreateFont(
            24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_SWISS, TEXT("Mojang")
        );

        HFONT hSmallFont = CreateFont(
            24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_SWISS, TEXT("Windows XP Tahoma")
        );

        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        const TCHAR* titleText1 = TEXT("Internet Explorer Version 0.5 beta");

        const TCHAR* titleText2 = TEXT("Changelog:");

        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        TextOut(
            hdc,
            10, 5,
            titleText1,
            (int)_tcslen(titleText1)
        );

        TextOut(
            hdc,
            10, 40,
            titleText2,
            (int)_tcslen(titleText2)
        );

        const TCHAR* smallText1 = TEXT("- 0.5 beta: Added the basics (Search Bar, address bar, basic web navigation, etc.)");

        SelectObject(hdc, hSmallFont);

        TextOut(
            hdc,
            10, 75,
            smallText1,
            (int)_tcslen(smallText1)
        );

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        DeleteObject(hSmallFont);
        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    g_hInstance = hInstance;

    HICON hAppIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

    HRESULT hrCoInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hrCoInit)) {
        MessageBox(NULL, TEXT("COM initialization failed."), TEXT("Error"), MB_ICONERROR);
        return 0;
    }

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = hAppIcon;
    RegisterClass(&wc);

    WNDCLASS wc2 = {};
    wc2.lpfnWndProc = SecondaryWindowProc;
    wc2.hInstance = hInstance;
    wc2.lpszClassName = SECONDARY_CLASS_NAME;
    wc2.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClass(&wc2);

    WNDCLASS wc3 = {};
    wc3.lpfnWndProc = NavigatorWindowProc;
    wc3.hInstance = hInstance;
    wc3.lpszClassName = NAVIGATOR_CLASS_NAME;
    wc3.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc3);

    WNDCLASS wc4 = {};
    wc4.lpfnWndProc = AboutWindowProc;
    wc4.hInstance = hInstance;
    wc4.lpszClassName = ABOUT_CLASS_NAME;
    wc4.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc4.hIcon = hAppIcon;
    RegisterClass(&wc4);

    WNDCLASS wc5 = {};
    wc5.lpfnWndProc = ChangelogWindowProc;
    wc5.hInstance = hInstance;
    wc5.lpszClassName = CHANGELOG_CLASS_NAME;
    wc5.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc5.hIcon = hAppIcon;
    RegisterClass(&wc5);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, TEXT("Internet Explorer"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        CoUninitialize();
        return 0;
    }

    CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Choose an URL"),
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        10, 75, 125, 50,
        hwnd, (HMENU)ID_BUTTON_CLICK, hInstance, NULL
    );

    CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Open Webpage"),
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        145, 75, 150, 50,
        hwnd, (HMENU)ID_BUTTON_NAVIGATE, hInstance, NULL
    );

    CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("About this Program"),
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        305, 75, 150, 50,
        hwnd, (HMENU)ID_BUTTON_ABOUT, hInstance, NULL
    );

    CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Changelog"),
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        465, 75, 125, 50,
        hwnd, (HMENU)ID_BUTTON_CHANGELOG, hInstance, NULL
    );

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (IsDialogMessage(hwnd, &msg)) {
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();

    return 0;
}