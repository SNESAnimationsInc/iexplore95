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
#include <commdlg.h>
#include "resource.h"
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")

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

#define ID_FILE_NEW          4001
#define ID_FILE_OPEN         4002
#define ID_FILE_STARTPAGE    4003
#define ID_FILE_SAVEAS       4004
#define ID_FILE_PRINT        4005
#define ID_FILE_EXIT         4006

#define ID_EDIT_CUT          4101
#define ID_EDIT_COPY         4102
#define ID_EDIT_PASTE        4103
#define ID_EDIT_SELECTALL    4104
#define ID_EDIT_FIND         4105

#define ID_VIEW_TOOLBAR       4201
#define ID_VIEW_STATUSBAR   4202

#define ID_COMBO_BOX 4203
#define ID_TAB_CONTROL 4204

#define WM_NAVIGATE_NEW_URL (WM_USER + 1)
#define WM_UPDATE_URL_TEXT (WM_USER + 2)
#define WM_NAVIGATE_NEW_SEARCH (WM_USER + 3)
#define WM_UPDATE_SEARCH_TEXT (WM_USER + 4)

#define ID_FONT_SMALLEST 5001
#define ID_FONT_SMALL    5002
#define ID_FONT_MEDIUM   5003
#define ID_FONT_LARGE    5004
#define ID_FONT_LARGEST  5005

#define ADDRESS_BAR_HEIGHT 30
#define TAB_BAR_HEIGHT 30

UINT g_currentFontID = ID_FONT_MEDIUM;

#define MAX_URL_LENGTH 512
TCHAR g_currentUrl[MAX_URL_LENGTH] = TEXT("http://www.msn.com/?ocid=U220DHP&pc=U220");
TCHAR g_currentSearch[MAX_URL_LENGTH] = TEXT("Search");


const TCHAR CLASS_NAME[] = TEXT("MyWindowClass");
const TCHAR SECONDARY_CLASS_NAME[] = TEXT("SecondaryWindowClass");
const TCHAR NAVIGATOR_CLASS_NAME[] = TEXT("NavigatorWindowClass");
const TCHAR ABOUT_CLASS_NAME[] = TEXT("AboutWindowClass");
const TCHAR CHANGELOG_CLASS_NAME[] = TEXT("ChangelogWindowClass");
const TCHAR SETTINGS_CLASS_NAME[] = TEXT("SettingsWindowClass");
const TCHAR* g_searchEngine = TEXT("https://www.google.com/search?q=");
HINSTANCE g_hInstance = NULL;
HWND g_hToolbar = NULL;
HWND g_hAddressBar = NULL;
HWND g_hSearchBar = NULL;

HICON g_hAppIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1));

using namespace Microsoft::WRL;
ComPtr<ICoreWebView2Controller> g_webviewController = nullptr;
ComPtr<ICoreWebView2> g_webview = nullptr;

LRESULT CALLBACK SettingsWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct SearchEngine {
    const TCHAR* name;
    const TCHAR* url;
};

SearchEngine g_engines[] = {
    { TEXT("Google"), TEXT("https://www.google.com/search?q=") },
    { TEXT("Bing"),   TEXT("https://www.bing.com/search?q=") },
    { TEXT("DuckDuckGo"), TEXT("https://duckduckgo.com/?q=") }
};

struct TabInfo {
    HWND hTabWindow;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    std::wstring title;
};

std::vector<TabInfo> g_tabs;
int g_currentTabIndex = -1;
HWND g_hTabCtrl = NULL;

void CreateNewTab(HWND hParent, const TCHAR* url) {
    TabInfo newTab;
    newTab.hTabWindow = hParent; // Store parent
    g_tabs.push_back(newTab);
    int tabIndex = (int)g_tabs.size() - 1;

    // Add the tab to the actual UI control
    TCITEM tie;
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPTSTR)TEXT("Loading...");
    TabCtrl_InsertItem(g_hTabCtrl, tabIndex, &tie);

    CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hParent, tabIndex, url](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
                env->CreateCoreWebView2Controller(hParent,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hParent, tabIndex, url](HRESULT hr, ICoreWebView2Controller* controller) -> HRESULT {

                            g_tabs[tabIndex].controller = controller;
                            g_tabs[tabIndex].controller->get_CoreWebView2(&g_tabs[tabIndex].webview);

                            // Update bounds for the new webview
                            RECT rect;
                            GetClientRect(hParent, &rect);
                            RECT bounds = { 0, 90, rect.right, rect.bottom };
                            g_tabs[tabIndex].controller->put_Bounds(bounds);

                            // Sync visibility with current selection
                            if (tabIndex != TabCtrl_GetCurSel(g_hTabCtrl)) {
                                g_tabs[tabIndex].controller->put_IsVisible(FALSE);
                            }
                            else {
                                g_currentTabIndex = tabIndex;
                            }

                            // Event: Update Tab Title when document title changes
                            // Event: Update Tab Title and Main Window Title when document title changes
                            g_tabs[tabIndex].webview->add_DocumentTitleChanged(
                                Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                                    [tabIndex, hParent](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
                                        PWSTR title;
                                        sender->get_DocumentTitle(&title);
                                        if (title) {
                                            // 1. Update the Tab Control text
                                            TCITEM tie;
                                            tie.mask = TCIF_TEXT;
                                            tie.pszText = title;
                                            TabCtrl_SetItem(g_hTabCtrl, tabIndex, &tie);

                                            // 2. Update Main Window Title if this is the active tab
                                            if (tabIndex == g_currentTabIndex) {
                                                std::wstring fullTitle = std::wstring(title) + L" - Internet Explorer";
                                                SetWindowText(hParent, fullTitle.c_str());
                                            }
                                            CoTaskMemFree(title);
                                        }
                                        return S_OK;
                                    }).Get(), nullptr);

                            g_tabs[tabIndex].webview->Navigate(url);
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}

void CloseTab(int index, HWND hwnd) {
    if (index < 0 || index >= g_tabs.size()) return;

    // 1. Shut down the WebView2 controller
    if (g_tabs[index].controller) {
        g_tabs[index].controller->Close();
    }

    // 2. Remove from the vector and UI
    g_tabs.erase(g_tabs.begin() + index);
    TabCtrl_DeleteItem(g_hTabCtrl, index);

    // 3. Handle selection change
    if (g_tabs.empty()) {
        for (auto& tab : g_tabs) {
            if (tab.controller) tab.controller->Close();
        }
        g_tabs.clear();
        CoUninitialize();
        DestroyWindow(hwnd);
    }
    else {
        // Switch to the nearest tab
        int nextTab = (index == 0) ? 0 : index - 1;
        TabCtrl_SetCurSel(g_hTabCtrl, nextTab);

        // Trigger the selection change logic to show the new webview
        NMHDR nmh;
        nmh.code = TCN_SELCHANGE;
        nmh.hwndFrom = g_hTabCtrl;
        nmh.idFrom = ID_TAB_CONTROL;
        SendMessage(hwnd, WM_NOTIFY, ID_TAB_CONTROL, (LPARAM)&nmh);
    }
}

void LaunchAppSettings(HWND hwndParent) {
    const TCHAR CLASS_NAME[] = TEXT("SettingsWindowClass");

    WNDCLASS wc = { 0 };
    if (!GetClassInfo(GetModuleHandle(NULL), CLASS_NAME, &wc)) {
        wc.lpfnWndProc = SettingsWindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = SETTINGS_CLASS_NAME;
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hIcon = g_hAppIcon;
        RegisterClass(&wc);
    }

    HWND hSettings = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_CONTEXTHELP,
        SETTINGS_CLASS_NAME, TEXT("App Settings"),
        WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 280,
        hwndParent, NULL, GetModuleHandle(NULL), NULL
    );
}

void RegINetOptionsKey(TCHAR* buffer, DWORD bufferSize) {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER,
        TEXT("Software\Microsoft\Internet Explorer\Main"),
        0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueEx(hKey, TEXT("Start Page"), NULL, NULL, (LPBYTE)buffer, &bufferSize) != ERROR_SUCCESS) {
            _tcscpy_s(buffer, bufferSize / sizeof(TCHAR), TEXT("http://www.msn.com/?ocid=U220DHP&pc=U220"));
        }
        RegCloseKey(hKey);
    }
}

std::wstring ShowOpenFileDialog(HWND hwnd) {
    TCHAR szFile[MAX_PATH] = { 0 };
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = TEXT("HTML Files (*.htm;*.html)\0*.htm;*.html\0")
        TEXT("Text Files (*.txt)\0*.txt\0")
        TEXT("GIF Files (*.gif)\0*.gif\0")
        TEXT("JPEG Files (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0")
        TEXT("AU Files (*.au)\0*.au\0")
        TEXT("AIFF Files (*.aif;*.aiff)\0*.aif;*.aiff\0")
        TEXT("XBM Files (*.xbm)\0*.xbm\0")
        TEXT("All Files (*.*)\0*.*\0");
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileName(&ofn)) {
        return std::wstring(szFile);
    }
    return L"";
}

void ShowSaveFileDialog(HWND hwnd) {
    TCHAR szFile[MAX_PATH] = { 0 };
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = TEXT("HTML (*.htm;*.html)\0*.htm;*.html\0Plain Text (*.txt)\0*.txt\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = TEXT("html");
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn)) {
        MessageBox(hwnd, ofn.lpstrFile, TEXT("File Name:"), MB_OK);
    }
}

void FormatSearchQuery(const TCHAR* input, TCHAR* output, size_t outputSize)
{
    _tcsncpy_s(output, outputSize, g_searchEngine, _TRUNCATE);

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
    output[offset] = TEXT('\0');
}

void FormatFileUrl(const TCHAR* input, TCHAR* output, size_t outputSize)
{
    const TCHAR* prefix = TEXT("file:///");
    _tcscpy_s(output, outputSize, prefix);
    size_t offset = _tcslen(output);

    for (size_t i = 0; input[i] != TEXT('\0') && offset < outputSize - 1; ++i)
    {
        if (input[i] == TEXT('"')) continue;

        if (input[i] == TEXT('\\')) {
            output[offset++] = TEXT('/');
        }
        else if (input[i] == TEXT(' ')) {
            if (offset + 3 < outputSize) {
                _tcscat_s(output + offset, outputSize - offset, TEXT("%20"));
                offset += 3;
            }
        }
        else {
            output[offset++] = input[i];
        }
    }
    output[offset] = TEXT('\0');
}

void EnsureProtocol(TCHAR* url) {
    if (_tcsncmp(url, TEXT("http://"), 7) == 0 ||
        _tcsncmp(url, TEXT("https://"), 8) == 0 ||
        _tcsncmp(url, TEXT("file:///"), 8) == 0 ||
        _tcsncmp(url, TEXT("about:"), 6) == 0)
    {
        return;
    }

    if (url[0] == TEXT('"') || (_istalpha(url[0]) && url[1] == TEXT(':'))) {
        TCHAR cleanedPath[MAX_URL_LENGTH] = { 0 };
        int j = 0;
        for (int i = 0; url[i] != TEXT('\0') && j < MAX_URL_LENGTH - 10; i++) {
            if (url[i] == TEXT('"')) continue;
            cleanedPath[j++] = (url[i] == TEXT('\\')) ? TEXT('/') : url[i];
        }
        _stprintf_s(url, MAX_URL_LENGTH, TEXT("file:///%s"), cleanedPath);
    }
    else {
        TCHAR tempUrl[MAX_URL_LENGTH];
        _tcscpy_s(tempUrl, MAX_URL_LENGTH, url);
        _stprintf_s(url, MAX_URL_LENGTH, TEXT("https://%s"), tempUrl);
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

HMENU CreateClassicIEMenu() {
    HMENU hMenuBar = CreateMenu();

    HMENU hFontsSubMenu = CreatePopupMenu();
    AppendMenu(hFontsSubMenu, MF_STRING, ID_FONT_SMALLEST, TEXT("Smallest"));
    AppendMenu(hFontsSubMenu, MF_STRING, ID_FONT_SMALL, TEXT("Small"));
    AppendMenu(hFontsSubMenu, MF_STRING, ID_FONT_MEDIUM, TEXT("Medium"));
    AppendMenu(hFontsSubMenu, MF_STRING, ID_FONT_LARGE, TEXT("Large"));
    AppendMenu(hFontsSubMenu, MF_STRING, ID_FONT_LARGEST, TEXT("Largest"));
    
    CheckMenuRadioItem(hFontsSubMenu, ID_FONT_SMALLEST, ID_FONT_LARGEST, ID_FONT_MEDIUM, MF_BYCOMMAND);

    HMENU hFile = CreatePopupMenu();
    AppendMenu(hFile, MF_STRING, 4002, TEXT("&Open...\tCtrl+O"));
    AppendMenu(hFile, MF_STRING, 4010, TEXT("Open S&tart Page"));
    AppendMenu(hFile, MF_STRING, 4003, TEXT("&Save As...\tCtrl+S"));
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING, 4004, TEXT("Page Se&tup..."));
    AppendMenu(hFile, MF_STRING, 4005, TEXT("&Print\tCtrl+P"));
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING, 4011, TEXT("Create &Shortcut"));
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING, WM_CLOSE, TEXT("E&xit"));
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING, WM_CLOSE, TEXT("&More History..."));

    HMENU hEdit = CreatePopupMenu();
    AppendMenu(hEdit, MF_STRING, ID_REFRESH_BUTTON, TEXT("Cu&t\tCtrl+X"));
    AppendMenu(hEdit, MF_STRING, ID_REFRESH_BUTTON, TEXT("&Copy\tCtrl+C"));
    AppendMenu(hEdit, MF_STRING, ID_REFRESH_BUTTON, TEXT("&Copy\tCtrl+V"));
    AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEdit, MF_STRING, 4006, TEXT("Select &All\tCtrl+A"));
    AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEdit, MF_STRING, 4007, TEXT("Find...\tCtrl+F"));
    AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEdit, MF_STRING, 4008, TEXT("System Settings\tCtrl+P"));
    AppendMenu(hEdit, MF_STRING, 4009, TEXT("App Settings\tCtrl+Shift+P"));

    HMENU hView = CreatePopupMenu();
    AppendMenu(hView, MF_STRING, ID_REFRESH_BUTTON, TEXT("&Back\tBackspace"));
    AppendMenu(hView, MF_STRING, ID_REFRESH_BUTTON, TEXT("&Forward"));
    AppendMenu(hView, MF_SEPARATOR, 0, NULL);
    AppendMenu(hView, MF_STRING | MF_CHECKED, ID_VIEW_TOOLBAR, TEXT("&Toolbar"));
    AppendMenu(hView, MF_STRING | MF_CHECKED, ID_ADDRESS_BAR, TEXT("&Address Bar"));
    AppendMenu(hView, MF_STRING | MF_CHECKED, ID_VIEW_STATUSBAR, TEXT("&Status Bar"));
    AppendMenu(hView, MF_SEPARATOR, 0, NULL);
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_POPUP, (UINT_PTR)hFontsSubMenu, TEXT("&Fonts"));

    HMENU hFavs = CreatePopupMenu();
    AppendMenu(hFavs, MF_STRING, 4010, TEXT("&Add to Favorites..."));
    AppendMenu(hFavs, MF_STRING, 4011, TEXT("&Open Favorites..."));
    AppendMenu(hFavs, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFavs, MF_GRAYED, 4012, TEXT("(Empty)"));

    HMENU hHelp = CreatePopupMenu();
    AppendMenu(hHelp, MF_STRING, 4012, TEXT("&Help Topics"));
    AppendMenu(hHelp, MF_SEPARATOR, 0, NULL);
    AppendMenu(hHelp, MF_STRING, ID_BUTTON_ABOUT, TEXT("&About Internet Explorer"));

    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFile, TEXT("&File"));
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hEdit, TEXT("&Edit"));
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hView, TEXT("&View"));
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFavs, TEXT("F&avorites"));
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hHelp, TEXT("&Help"));

    return hMenuBar;
}

LRESULT CALLBACK FontsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (msg)
    {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            
            return 0;
        }
        break;
    }
    return 0;
}

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

            if (g_currentTabIndex != -1 && g_tabs[g_currentTabIndex].webview) {
                g_tabs[g_currentTabIndex].webview->Navigate(g_currentUrl);
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

            if (g_currentTabIndex != -1 && g_tabs[g_currentTabIndex].webview) {
                g_tabs[g_currentTabIndex].webview->Navigate(g_currentUrl);
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
    LPNMHDR lpnmhdr = nullptr;
    switch (uMsg) {

    case WM_CREATE: {
        HRESULT hrCoInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

        // 1. Setup UI Components
        HMENU hMenu = CreateClassicIEMenu();
        if (hMenu) SetMenu(hwnd, hMenu);

        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES; // Added Tab classes
        InitCommonControlsEx(&icex);

        // Toolbar
        g_hToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS,
            0, 0, 0, 0, hwnd, (HMENU)ID_TOOLBAR, g_hInstance, NULL);

        // Tab Control
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        g_hTabCtrl = CreateWindow(WC_TABCONTROL, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, TOOLBAR_HEIGHT, clientRect.right, 30,
            hwnd, (HMENU)ID_TAB_CONTROL, g_hInstance, NULL);

        // Set a fixed width/height for tabs to look like Chrome/Edge
        SendMessage(g_hTabCtrl, TCM_SETITEMSIZE, 0, MAKELPARAM(180, 28));
        // Force the tab control to use the fixed size
        DWORD dwStyle = GetWindowLong(g_hTabCtrl, GWL_STYLE);
        SetWindowLong(g_hTabCtrl, GWL_STYLE, dwStyle | TCS_FIXEDWIDTH);

        // Address Bar & Search Bar
        g_hAddressBar = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), g_currentUrl,
            WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)ID_ADDRESS_BAR, g_hInstance, NULL);

        g_hSearchBar = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), g_currentSearch,
            WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)ID_SEARCH_BAR, g_hInstance, NULL);

        // Subclassing for Enter key handling
        SetWindowSubclass(g_hAddressBar, AddressBarProc, 1, (DWORD_PTR)hwnd);
        SetWindowSubclass(g_hSearchBar, SearchBarProc, 2, (DWORD_PTR)hwnd);

        // 2. Initialize First Tab
        CreateNewTab(hwnd, g_currentUrl);
        return 0;
    }
    case WM_NOTIFY:
    {
        LPNMHDR lpnm = (LPNMHDR)lParam;

        switch (lpnm->code)
        {
        case TCN_SELCHANGE:
        {
            int newIndex = TabCtrl_GetCurSel(g_hTabCtrl);

            // Hide the old tab's webview
            if (g_currentTabIndex != -1 && g_currentTabIndex < (int)g_tabs.size()) {
                g_tabs[g_currentTabIndex].controller->put_IsVisible(FALSE);
            }

            // Show the new tab's webview
            g_currentTabIndex = newIndex;
            if (g_tabs[g_currentTabIndex].controller) {
                g_tabs[g_currentTabIndex].controller->put_IsVisible(TRUE);

                // Update Address Bar
                LPWSTR uri;
                g_tabs[g_currentTabIndex].webview->get_Source(&uri);
                SetWindowText(g_hAddressBar, uri);
                CoTaskMemFree(uri);

                // Update Main Window Title for the newly selected tab
                PWSTR currentTitle;
                if (SUCCEEDED(g_tabs[g_currentTabIndex].webview->get_DocumentTitle(&currentTitle)) && currentTitle) {
                    std::wstring fullTitle = std::wstring(currentTitle) + L" - Internet Explorer";
                    SetWindowText(hwnd, fullTitle.c_str());
                    CoTaskMemFree(currentTitle);
                }
            }
            break;
        }

        case NM_RCLICK:
        {
            if (lpnm->hwndFrom == g_hTabCtrl) {
                // 1. Identify which tab was right-clicked
                TCHITTESTINFO hti;
                GetCursorPos(&hti.pt);
                ScreenToClient(g_hTabCtrl, &hti.pt);
                int tabIndex = TabCtrl_HitTest(g_hTabCtrl, &hti);

                if (tabIndex != -1) {
                    // 2. Show a simple popup menu
                    HMENU hPopup = CreatePopupMenu();
                    AppendMenu(hPopup, MF_STRING, 5001, L"New Tab");
                    AppendMenu(hPopup, MF_STRING, 5002, L"Close Tab");

                    POINT pt;
                    GetCursorPos(&pt);
                    int sel = TrackPopupMenu(hPopup, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);

                    if (sel == 5001) {
                        CreateNewTab(hwnd, TEXT("http://www.msn.com/?ocid=U220DHP&pc=U220"));
                    }
                    else if (sel == 5002) {
                        CloseTab(tabIndex, hwnd);
					}
                    DestroyMenu(hPopup);
                }
            }
            break;
        }
        }
        return 0;
    }

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        // 1. Toolbar at the very top
        MoveWindow(g_hToolbar, 0, 0, width, TOOLBAR_HEIGHT, TRUE);

        // 2. Address Bar and Search Bar below the Toolbar
        // Splitting the width: 70% for Address, 30% for Search
        int addressWidth = (int)(width * 0.7);
        int searchWidth = width - addressWidth;
        MoveWindow(g_hAddressBar, 0, TOOLBAR_HEIGHT, addressWidth, ADDRESS_BAR_HEIGHT, TRUE);
        MoveWindow(g_hSearchBar, addressWidth, TOOLBAR_HEIGHT, searchWidth, ADDRESS_BAR_HEIGHT, TRUE);

        // 3. Tab Control below the Address Bar
        int tabYStart = TOOLBAR_HEIGHT + ADDRESS_BAR_HEIGHT;
        MoveWindow(g_hTabCtrl, 0, tabYStart, width, TAB_BAR_HEIGHT, TRUE);

        // 4. WebView fills the remaining space
        int webViewYStart = tabYStart + TAB_BAR_HEIGHT;
        if (g_currentTabIndex != -1 && g_currentTabIndex < (int)g_tabs.size()) {
            if (g_tabs[g_currentTabIndex].controller) {
                RECT bounds = { 0, webViewYStart, width, height };
                g_tabs[g_currentTabIndex].controller->put_Bounds(bounds);
            }
        }
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);

        // Ensure we have a valid tab before executing browser commands
        if (g_currentTabIndex == -1 || g_currentTabIndex >= g_tabs.size()) break;
        auto currentWebview = g_tabs[g_currentTabIndex].webview;

        switch (wmId) {
        case ID_BACK_BUTTON:    currentWebview->GoBack(); break;
        case ID_FORWARD_BUTTON: currentWebview->GoForward(); break;
        case ID_REFRESH_BUTTON: currentWebview->Reload(); break;

        case 4002: // File -> Open
        {
            std::wstring filePath = ShowOpenFileDialog(hwnd);
            if (!filePath.empty()) {
                // Prepare the path (handling spaces and backslashes)
                TCHAR formattedPath[MAX_URL_LENGTH];
                _tcscpy_s(formattedPath, MAX_URL_LENGTH, filePath.c_str());

                // Ensure it uses file:/// protocol so WebView2 loads it correctly
                EnsureProtocol(formattedPath);

                // Open in a brand new tab
                CreateNewTab(hwnd, formattedPath);

                // Optional: Switch to the newly created tab automatically
                int newTabIndex = (int)g_tabs.size() - 1;
                TabCtrl_SetCurSel(g_hTabCtrl, newTabIndex);

                // Manually trigger the tab change logic to show the new WebView
                NMHDR nmh;
                nmh.code = TCN_SELCHANGE;
                nmh.hwndFrom = g_hTabCtrl;
                nmh.idFrom = ID_TAB_CONTROL;
                SendMessage(hwnd, WM_NOTIFY, ID_TAB_CONTROL, (LPARAM)&nmh);
            }
            break;
        }

        case 4003: ShowSaveFileDialog(hwnd); break;
        case 4009: LaunchAppSettings(hwnd); break;

        case ID_FILE_NEW: // You should add this ID to your defines
            CreateNewTab(hwnd, TEXT("https://www.google.com"));
            break;
        }
        return 0;
    }

    case WM_DESTROY: {
        // 1. Clean up the tabs for this specific window
        for (auto& tab : g_tabs) {
            if (tab.controller) tab.controller->Close();
        }
        g_tabs.clear();

        // 2. Check if the MAIN launcher window is still open
        HWND hwndMain = FindWindow(CLASS_NAME, NULL);
        // 3. Check if there are OTHER navigator windows open (excluding this one)
        // Note: Since this window is currently being destroyed, FindWindow might still see it.
        // We check if any OTHER navigator window exists.
        HWND hwndOtherNav = FindWindowEx(NULL, NULL, NAVIGATOR_CLASS_NAME, NULL);
        if (hwndOtherNav == hwnd) {
            hwndOtherNav = FindWindowEx(NULL, hwnd, NAVIGATOR_CLASS_NAME, NULL);
        }

        // 4. If nothing is left open, kill the process
        if (hwndMain == NULL && hwndOtherNav == NULL) {
            PostQuitMessage(0);
        }

        // Note: We don't CoUninitialize here anymore because other windows might need COM
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

LRESULT CALLBACK PagSetWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

        const TCHAR* titleText1 = TEXT("This function is still in development.");

        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        TextOut(
            hdc,
            10, 5,
            titleText1,
            (int)_tcslen(titleText1)
        );

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY: {
        // Check if any browser (Navigator) windows are currently open
        HWND hwndNavigator = FindWindow(NAVIGATOR_CLASS_NAME, NULL);

        if (hwndNavigator == NULL) {
            // No browser windows found, it's safe to exit the whole app
            PostQuitMessage(0);
        }
        else {
            // Browser windows are still open! 
            // We just let this window destroy itself without calling PostQuitMessage.
            // The process continues running because the message loop stays active.
        }
        break;
    }

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

        const TCHAR* titleText1 = TEXT("Internet Explorer Version 0.65 beta");

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

        const TCHAR* smallText1 = TEXT("- 0.65 beta: The main window can now be closed without exiting the app.");

        const TCHAR* smallText2 = TEXT("- 0.6 beta: Added the tab bar (currently in development); the browser can now open ");

        const TCHAR* smallText3 = TEXT("files; some bug fixes.");

        const TCHAR* smallText4 = TEXT("- 0.5 beta: Added the basics (Search Bar, address bar, basic web navigation, etc.)");

        SelectObject(hdc, hSmallFont);

        TextOut(
            hdc,
            10, 75,
            smallText1,
            (int)_tcslen(smallText1)
        );

        TextOut(
            hdc,
            35, 110,
            smallText2,
            (int)_tcslen(smallText2)
        );

        TextOut(
            hdc,
            10, 145,
            smallText3,
            (int)_tcslen(smallText3)
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

LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // 1. Create the Checkbox
        CreateWindow(TEXT("BUTTON"), TEXT("Show Toolbar"),
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 20, 150, 20, hwnd, (HMENU)1001, NULL, NULL);

        // 2. Create the Label
        CreateWindowEx(
            0, TEXT("STATIC"), TEXT("Search Engine:"),
            WS_VISIBLE | WS_CHILD,
            20, 50, 350, 20, // Adjusted Y to 50
            hwnd, (HMENU)ID_TEXT_LABEL, NULL, NULL
        );

        // 3. Create the Combobox
        HWND hCombo = CreateWindowEx(
            0, TEXT("COMBOBOX"), TEXT(""),
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
            20, 75, 200, 200, // Adjusted Y to 75
            hwnd, (HMENU)ID_COMBO_BOX, NULL, NULL
        );

        // 4. FILL THE DROPDOWN
        for (int i = 0; i < _countof(g_engines); i++) {
            // Send the name of the engine to the list
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)g_engines[i].name);

            // Bonus: If this engine is the current global setting, select it by default
            if (_tcscmp(g_searchEngine, g_engines[i].url) == 0) {
                SendMessage(hCombo, CB_SETCURSEL, i, 0);
            }
        }

        // 5. Create the OK Button
        CreateWindow(TEXT("BUTTON"), TEXT("OK"),
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            110, 200, 80, 30, hwnd, (HMENU)IDOK, NULL, NULL);

        return 0;
    }
    case WM_HELP: {
        HELPINFO* hi = (HELPINFO*)lParam;
        if (hi->iCtrlId == ID_COMBO_BOX) {
            MessageBox(hwnd, TEXT("Select your preferred search engine for the search bar."), TEXT("Help"), MB_OK | MB_ICONINFORMATION);
        }
        else if (hi->iCtrlId == 1001) {
            MessageBox(hwnd, TEXT("Toggle this to show or hide the navigation toolbar."), TEXT("Help"), MB_OK | MB_ICONINFORMATION);
        }
        return TRUE;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        if (wmId == ID_COMBO_BOX && wmEvent == CBN_SELCHANGE) {
            HWND hCombo = (HWND)lParam;
            int index = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            if (index != CB_ERR) {
                g_searchEngine = g_engines[index].url;
            }
        }

        if (wmId == IDOK) {
            EnableWindow(GetParent(hwnd), TRUE);
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE: {
        EnableWindow(GetParent(hwnd), TRUE);
        DestroyWindow(hwnd);
        return 0;
    }
    case WM_DESTROY:
    return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    g_hInstance = hInstance;

    if (_tcslen(lpCmdLine) > 0) {
        _tcscpy_s(g_currentUrl, MAX_URL_LENGTH, lpCmdLine);

        EnsureProtocol(g_currentUrl);
    }
    
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

    WNDCLASS wc6 = {};
    wc6.lpfnWndProc = SettingsWindowProc;
    wc6.hInstance = hInstance;
    wc6.lpszClassName = SETTINGS_CLASS_NAME;
    wc6.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc6.hIcon = hAppIcon;
    RegisterClass(&wc6);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, TEXT("Internet Explorer"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        CoUninitialize();
        return 0;
    }

    if (lpCmdLine[0] != L'\0') {
        // 1. Guardar la ruta del archivo en nuestra variable global
        _tcsncpy_s(g_currentUrl, MAX_URL_LENGTH, lpCmdLine, _TRUNCATE);

        // 2. Formatearla correctamente para WebView2
        EnsureProtocol(g_currentUrl);

        // 3. Simular que el usuario pulsó el botón "Open Webpage" automáticamente
        PostMessage(hwnd, WM_COMMAND, ID_BUTTON_NAVIGATE, 0);
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