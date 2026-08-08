// AltTabWindow.cpp : Defines the entry point for the application.
//

#include "PreCompile.h"
#include "version.h"
#include "AltTabWindow.h"
#include "AltTabMenu.h"
#include "AltTabTheme.h"
#include "AltTabWindowRenderer.h"

#include "Logger.h"

#include <CommCtrl.h>
#include <Psapi.h>
#include <Windows.h>
#include <dwmapi.h>
#include <filesystem>
#include <format>
#include <shlobj.h>
#include <string>
#include <vector>
#include <winnt.h>
#include "resource.h"
#include "AltTabSettings.h"
#include "Utils.h"
#include "AltTab.h"
#include "GlobalData.h"
#include <unordered_map>
#include "CheckForUpdates.h"
#include <thread>
#include <utility>
#include <windowsx.h>
#include <shellapi.h>
#include <memory>
#include <uxtheme.h>

// ----------------------------------------------------------------------------
// Global Variables:
// ----------------------------------------------------------------------------
HWND g_hSearchString = nullptr;
HWND g_hListView = nullptr;
int g_nLVHotItem = -1;
int g_SelectedIndex = 0;
int g_MouseHoverIndex = -1;
HANDLE g_hAltTabThread = nullptr;
std::wstring g_SearchString;
RECT g_rcBtnClose;
bool g_IsMouseOverCloseButton = false;
bool g_hAltTabIsBeingClosed = false; // Is AltTab window being closed
HWND g_hCustomToolTip = nullptr;     // Custom tool tip
bool g_bIgnoreWM_ACTIVATE = false;   // Ignore WM_ACTIVATE with WA_INACTIVE

static std::unique_ptr<SwitcherRenderer> g_SwitcherRenderer;
static HIMAGELIST g_hRowHeightImageList = nullptr;

// Forward declarations of functions included in this code module:
INT_PTR CALLBACK ATAboutDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AltTabWindowProc(HWND, UINT, WPARAM, LPARAM);
bool IsAltTabWindow(HWND hWnd);
HWND GetOwnerWindowHwnd(HWND hWnd);
static void AddListViewItem(HWND hListView, int index, const AltTabWindowData& windowData);
static void ContextMenuItemHandler(HWND hWnd, HMENU hSubMenu, UINT menuItemId);
static BOOL TerminateProcessEx(DWORD pid);
static void ATCloseWindow(const int index);
static bool IsExcludedProcess(const std::wstring& processName);

// Window procedure functions for individual messages
static void ATW_OnActivate(HWND hwnd, UINT state, HWND hwndActDeact, BOOL fMinimized);
static void ATW_OnClose(HWND hwnd);
static void ATW_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
static void ATW_OnContextMenu(HWND hwnd, HWND hwndContext, UINT xPos, UINT yPos);
static BOOL ATW_OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
static LRESULT ATW_OnCtlColorEdit(HWND hWnd, HDC hDC, HWND hCtl, UINT type);
static LRESULT ATW_OnCtlColorStatic(HWND hWnd, HDC hDC, HWND hCtl, UINT type);
static void ATW_OnDestroy(HWND hwnd);
static void ATW_OnDrawItem(HWND hwnd, const DRAWITEMSTRUCT* lpDrawItem);
static void ATW_OnKeyDown(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);
static void ATW_OnKillFocus(HWND hwnd, HWND hwndNewFocus);
static void ATW_OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
static BOOL ATW_OnNotify(HWND hwnd, int idFrom, NMHDR* pnmhdr);
static void ATW_OnSysCommand(HWND hwnd, UINT cmd, int x, int y);
static void ATW_OnTimer(HWND hwnd, UINT id);
static void LayoutSwitcherWindow(HWND hWnd);
static void RebuildSwitcherVisuals(HWND hWnd, UINT dpi);

std::vector<AltTabWindowData> g_AltTabWindows;

namespace AT {
    // Get the file version
    void GetPEInfo(
        const std::wstring& filePath,
        std::wstring& description,
        std::wstring& version,
        std::wstring& companyName) {
        DWORD dummy;
        DWORD verSize = GetFileVersionInfoSizeW(filePath.c_str(), &dummy);
        if (verSize > 0) {
            std::vector<BYTE> verData(verSize);
            if (GetFileVersionInfoW(filePath.c_str(), 0, verSize, verData.data())) {
                VS_FIXEDFILEINFO* fileInfo = nullptr;
                UINT len = 0;
                if (VerQueryValue(verData.data(), L"\\", (LPVOID*)&fileInfo, &len)) {
                    if (fileInfo) {
                        DWORD major = HIWORD(fileInfo->dwFileVersionMS);
                        DWORD minor = LOWORD(fileInfo->dwFileVersionMS);
                        DWORD build = HIWORD(fileInfo->dwFileVersionLS);
                        DWORD revision = LOWORD(fileInfo->dwFileVersionLS);
                        // Format: major.minor.build.revision
                        version = std::to_wstring(major) + L"." + std::to_wstring(minor) + L"." + std::to_wstring(build)
                                  + L"." + std::to_wstring(revision);
                    }
                }

                // First get the translation table (language + codepage)
                struct LANGANDCODEPAGE {
                    WORD wLanguage;
                    WORD wCodePage;
                }* lpTranslate;

                UINT cbTranslate = 0;
                if (VerQueryValueW(
                        verData.data(), L"\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate)) {
                    // Build the query string for "FileDescription"
                    wchar_t subBlock[64];
                    swprintf_s(
                        subBlock,
                        L"\\StringFileInfo\\%04x%04x\\FileDescription",
                        lpTranslate[0].wLanguage,
                        lpTranslate[0].wCodePage);

                    LPWSTR lpBuffer = nullptr;
                    UINT size = 0;
                    if (VerQueryValueW(verData.data(), subBlock, (LPVOID*)&lpBuffer, &size) && size > 0) {
                        description = lpBuffer;
                    }

                    // Build the query string for "CompanyName"
                    swprintf_s(
                        subBlock,
                        L"\\StringFileInfo\\%04x%04x\\CompanyName",
                        lpTranslate[0].wLanguage,
                        lpTranslate[0].wCodePage);

                    if (VerQueryValueW(verData.data(), subBlock, (LPVOID*)&lpBuffer, &size) && size > 0) {
                        companyName = lpBuffer;
                    }
                }
            }
        }
    }

    BOOL ATListViewDrawItem(HWND hListView, LPDRAWITEMSTRUCT lpDrawItemStruct) {
        if (!g_SwitcherRenderer)
            return FALSE;
        return g_SwitcherRenderer->DrawListViewRow(
            hListView,
            *lpDrawItemStruct,
            g_hLVImageList,
            g_Settings,
            g_nLVHotItem,
            g_IsMouseOverCloseButton,
            g_rcBtnClose);
    }
}

HICON GetWindowIcon(HWND hWnd) {
    // Try to get the large icon
    HICON hIcon;
    // Use SendMessageTimeout to get the icon with a timeout
    LRESULT responding =
        SendMessageTimeout(hWnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 10, reinterpret_cast<PDWORD_PTR>(&hIcon));

    if (responding) {
        if (hIcon)
            return hIcon;

        // If the large icon is not available, try to get the small icon
        hIcon = (HICON)SendMessageW(hWnd, WM_GETICON, ICON_SMALL2, 0);
        if (hIcon)
            return hIcon;

        hIcon = (HICON)SendMessageW(hWnd, WM_GETICON, ICON_SMALL, 0);
        if (hIcon)
            return hIcon;
    }

    // Get the class long value that contains the icon handle
    hIcon = (HICON)GetClassLongPtr(hWnd, GCLP_HICON);
    if (!hIcon) {
        // If the class does not have an icon, try to get the small icon
        hIcon = (HICON)GetClassLongPtr(hWnd, GCLP_HICONSM);
    }

    if (!hIcon) {
        hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        return hIcon;
    }

    return hIcon;
}

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    if (IsAltTabWindow(hWnd)) {
        HWND hOwner = GetOwnerWindowHwnd(hWnd);
        DWORD processId;
        GetWindowThreadProcessId(hWnd, &processId);

        if (processId != 0) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);

            if (hProcess != nullptr) {
                wchar_t szProcessPath[MAX_PATH];
                if (GetModuleFileNameEx(hProcess, nullptr, szProcessPath, MAX_PATH)) {
                    std::filesystem::path filePath = szProcessPath;

                    // Always get the title of owner window, otherwise popup window title will be displayed.
                    const int bufferSize = 256;
                    wchar_t windowTitle[bufferSize];
                    GetWindowTextW(hOwner, windowTitle, bufferSize);

                    // We get the actual window title for the process but windows adds "(Not Responding)"
                    // to the processes which are hung. So, appending "(Not Responding)" to the hung process manually.
                    std::wstring title = windowTitle;
                    if (IsHungAppWindow(hWnd)) {
                        title += L" (Not Responding)";
                    }

                    AltTabWindowData item;

                    item.hWnd = hWnd;
                    item.hOwner = hOwner;
                    item.hIcon = GetWindowIcon(hOwner);
                    item.Title = title;
                    item.ProcessName = filePath.filename().wstring();
                    item.FullPath = filePath.wstring();
                    item.PID = processId;
                    item.IsConflictProcess = false;
                    item.IsBeingClosed = false;

                    AT::GetPEInfo(item.FullPath, item.Description, item.Version, item.CompanyName);

                    auto* vItems = (std::vector<AltTabWindowData>*)lParam;
                    bool insert = true;
                    bool excluded = IsExcludedProcess(ToLower(item.ProcessName));

                    // If Alt+Tab is pressed, show all windows
                    if (g_IsAltTab || g_IsAltCtrlTab) {
                        insert = excluded ? false : true;
                    }
                    // If Alt+Backtick is pressed, show the process of similar process groups
                    else if (g_IsAltBacktick) {
                        if (g_AltBacktickWndInfo.hWnd == nullptr) {
                            if (!(GetWindowLong(item.hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
                                g_AltBacktickWndInfo = item;
                                AT_LOG_INFO(
                                    "g_AltBacktickWndInfo: %s", WStrToUTF8(g_AltBacktickWndInfo.ProcessName).c_str());
                            }
                        }
                        if (g_AltBacktickWndInfo.hWnd != nullptr) {
                            insert = IsSimilarProcess(g_AltBacktickWndInfo.ProcessName, item.ProcessName);
                        }
                    }

                    // If the window is to be inserted, now apply the search string
                    if (insert && !g_SearchString.empty()) {
                        // Do NOT insert by default, unless a match is found if search string is not empty
                        insert = true;

                        // First search for exact match in title and process name
                        // Split the search string by spaces and check if all parts are present
                        std::vector<std::wstring> searchParts = Split(ToLower(g_SearchString));
                        const std::wstring titleLower = ToLower(item.Title);

                        for (const auto& part : searchParts) {
                            size_t titleIndex = titleLower.find(part);
                            size_t processNameIndex = ToLower(item.ProcessName).find(part);

                            // Search in window title
                            if (titleIndex != std::wstring::npos) {
                                item.TitleHighlights.insert({ titleIndex, titleIndex + part.size() - 1 });
                            } else if (g_Settings.FuzzyMatchPercent != 100) {
                                FuzzyMatchResult fuzzyMatchRatio = GetPartialRatioW(part, titleLower);
                                if (fuzzyMatchRatio.score >= g_Settings.FuzzyMatchPercent) {
                                    item.TitleHighlights.insert({ fuzzyMatchRatio.start_pos, fuzzyMatchRatio.end_pos });
                                    titleIndex = fuzzyMatchRatio.start_pos;
                                }
                            }

                            // Search in process name
                            if (processNameIndex != std::wstring::npos) {
                                item.ProcessNameHighlights.insert(
                                    { processNameIndex, processNameIndex + part.size() - 1 });
                            } else if (g_Settings.FuzzyMatchPercent != 100) {
                                FuzzyMatchResult fuzzyMatchRatio = GetPartialRatioW(part, item.ProcessName);
                                if (fuzzyMatchRatio.score >= g_Settings.FuzzyMatchPercent) {
                                    item.ProcessNameHighlights.insert(
                                        { fuzzyMatchRatio.start_pos, fuzzyMatchRatio.end_pos });
                                    processNameIndex = fuzzyMatchRatio.start_pos;
                                }
                            }

                            if (titleIndex == std::wstring::npos && processNameIndex == std::wstring::npos) {
                                insert = false;
                                break;
                            }
                        }
                        // AT_LOG_INFO("matchRatio = %5.1f, title = [%s]", , WStrToUTF8(item.Title).c_str());
                    }

                    if (insert) {
                        // AT_LOG_INFO("Inserting hWnd: %0#9x, title: %s", item.hWnd, WStrToUTF8(item.Title).c_str());
                        vItems->push_back(std::move(item));
                    }
                }

                CloseHandle(hProcess);
            }
        }
    }

    return TRUE;
}

DWORD WINAPI AltTabThread(LPVOID pvParam) {
    AT_LOG_TRACE;
    int direction = *((int*)pvParam);
    CreateAltTabWindow();
    ShowAltTabWindow(g_hAltTabWnd, direction);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

HWND ShowAltTabWindow(HWND& hAltTabWnd, int direction) {
#if 0
    if (g_hAltTabThread && WaitForSingleObject(g_hAltTabThread, 0) != WAIT_OBJECT_0) {
        // Move to next / previous item based on the direction
        int selectedInd = (int)SendMessageW(g_hListView, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
        int N = (int)g_AltTabWindows.size();
        int nextInd = (selectedInd + N + direction) % N;

        ATWListViewSelectItem(nextInd);

        HWND hWnd = GetForegroundWindow();
        if (hAltTabWnd != hWnd) {
            AT_LOG_ERROR("hAltTabWnd is NOT a foreground window!");
            ActivateWindow(hAltTabWnd);
        }
        return hAltTabWnd;
    }
    if (g_hAltTabThread) {
        CloseHandle(g_hAltTabThread);
        g_hAltTabThread = nullptr;
    }

	g_hAltTabThread = CreateThread(nullptr, 0, AltTabThread, (LPVOID)(UINT_PTR)direction, CREATE_SUSPENDED, nullptr);
    if (!g_hAltTabThread)
        return nullptr;

    ResumeThread(g_hAltTabThread);

    return hAltTabWnd;
#else
    if (hAltTabWnd == nullptr) {
        // We need this to set the AltTab window active when it is brought to the foreground
        g_hFGWnd = GetForegroundWindow();
        if (!IsHungAppWindowEx(g_hFGWnd)) {
            g_idThreadAttachTo = g_hFGWnd ? GetWindowThreadProcessId(g_hFGWnd, nullptr) : 0;
            if (g_idThreadAttachTo) {
                AttachThreadInput(GetCurrentThreadId(), g_idThreadAttachTo, TRUE);
            }
        }

        hAltTabWnd = CreateAltTabWindow();

        // SetWindowPos(hAltTabWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(hAltTabWnd);
    }

    // TODO / FIXME:
    // Sometimes focus is not set to the search string edit control. Always
    // set the focus to it for time being.
    SetFocus(g_hSearchString);

    // Move to next / previous item based on the direction
    const int selectedInd = (int)SendMessageW(g_hListView, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    if (selectedInd == -1)
        return hAltTabWnd;

    const int N = (int)g_AltTabWindows.size();
    if (N == 0) {
        AT_LOG_ERROR("No AltTab windows available!");
        return hAltTabWnd;
    }
    const int nextInd = (selectedInd + N + direction) % N;

    ATWListViewSelectItem(nextInd);

    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hAltTabWnd;
    TrackMouseEvent(&tme);

    return hAltTabWnd;
#endif // 0
}

void RefreshAltTabWindow() {
    AT_LOG_TRACE;

    // Clear the list
    ListView_DeleteAllItems(g_hListView);
    g_AltTabWindows.clear();

    // Enumerate windows
    EnumWindows(EnumWindowsProc, (LPARAM)(&g_AltTabWindows));

    // Identify the processes which are running from different paths
    std::unordered_map<std::wstring, std::unordered_set<std::wstring>> processMap;
    for (const auto& item : g_AltTabWindows) {
        const std::wstring key = item.ProcessName + item.Title;
        processMap[key].insert(item.FullPath);
    }
    for (auto& item : g_AltTabWindows) {
        const std::wstring key = item.ProcessName + item.Title;
        if (processMap[key].size() > 1) {
            item.IsConflictProcess = true;
        }
    }

    const int imageSize = g_SwitcherRenderer ? g_SwitcherRenderer->Theme().metrics.iconSize : 32;
    const int rowHeight = g_SwitcherRenderer ? g_SwitcherRenderer->Theme().metrics.rowHeight : 52;

    // Create ImageList and add icons, assign a dummy ImageList to set the row height
    // The row height is determined by the height of the icons in the ImageList assigned as LVSIL_SMALL
    // But the icons in the ImageList are drawn using the ImageList g_hLVImageList.
    HIMAGELIST hImageListDummy = ImageList_Create(1, rowHeight, ILC_COLOR32, 1, 1);
    HIMAGELIST hImageList = ImageList_Create(imageSize, imageSize, ILC_COLOR32 | ILC_MASK, 0, 1);

    // Destroy the application-icon image list, which is painted manually and is
    // not owned by the ListView.
    if (g_hLVImageList) {
        ImageList_Destroy(g_hLVImageList);
        g_hLVImageList = nullptr;
    }

    for (const auto& item : g_AltTabWindows) {
        ImageList_AddIcon(hImageList, item.hIcon);
    }

    // Set the ImageList for the ListView
    // Assign as the small image list (LVSIL_SMALL affects row height)
    HIMAGELIST oldRowHeightImageList = ListView_SetImageList(g_hListView, hImageListDummy, LVSIL_SMALL);
    g_hRowHeightImageList = hImageListDummy;
    if (oldRowHeightImageList && oldRowHeightImageList != hImageListDummy)
        ImageList_Destroy(oldRowHeightImageList);
    g_hLVImageList = hImageList;

    // Add windows to ListView
    for (int i = 0; i < g_AltTabWindows.size(); ++i) {
        AddListViewItem(g_hListView, i, g_AltTabWindows[i]);
    }

    // Select the previously selected item
    ATWListViewSelectItem(g_SelectedIndex);
    if (g_hAltTabWnd)
        LayoutSwitcherWindow(g_hAltTabWnd);
}

void ATWListViewSelectItem(int rowNumber) {
    if (g_AltTabWindows.empty()) {
        g_SelectedIndex = -1;
        return;
    }

    // Move to next / previous item based on the direction
    int selectedInd = (int)SendMessageW(g_hListView, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    LVITEM lvItem;
    lvItem.stateMask = LVIS_FOCUSED | LVIS_SELECTED;
    lvItem.state = 0;
    SendMessageW(g_hListView, LVM_SETITEMSTATE, selectedInd, (LPARAM)&lvItem);

    rowNumber = max(0, min(rowNumber, (int)g_AltTabWindows.size() - 1));

    lvItem.state = LVIS_FOCUSED | LVIS_SELECTED;
    SendMessageW(g_hListView, LVM_SETITEMSTATE, rowNumber, (LPARAM)&lvItem);
    SendMessageW(g_hListView, LVM_ENSUREVISIBLE, rowNumber, (LPARAM)&lvItem);

    g_SelectedIndex = rowNumber;
}

void ATWListViewSelectPrevItem() {
    // Move to next / previous item based on the direction
    int selectedInd = (int)SendMessageW(g_hListView, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    if (selectedInd == -1)
        return;
    int N = (int)g_AltTabWindows.size();
    int prevInd = (selectedInd + N - 1) % N;

    LVITEM lvItem;
    lvItem.stateMask = LVIS_FOCUSED | LVIS_SELECTED;
    lvItem.state = 0;
    SendMessageW(g_hListView, LVM_SETITEMSTATE, selectedInd, (LPARAM)&lvItem);

    prevInd = max(0, min(prevInd, (int)g_AltTabWindows.size() - 1));

    lvItem.state = LVIS_FOCUSED | LVIS_SELECTED;
    SendMessageW(g_hListView, LVM_SETITEMSTATE, prevInd, (LPARAM)&lvItem);
    SendMessageW(g_hListView, LVM_ENSUREVISIBLE, prevInd, (LPARAM)&lvItem);

    g_SelectedIndex = prevInd;
}
void ATWListViewSelectNextItem() {
    // Move to next / previous item based on the direction
    int selectedInd = (int)SendMessageW(g_hListView, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    if (selectedInd == -1)
        return;
    int N = (int)g_AltTabWindows.size();
    int nextInd = (selectedInd + N + 1) % N;

    LVITEM lvItem;
    lvItem.stateMask = LVIS_FOCUSED | LVIS_SELECTED;
    lvItem.state = 0;
    SendMessageW(g_hListView, LVM_SETITEMSTATE, selectedInd, (LPARAM)&lvItem);

    nextInd = max(0, min(nextInd, (int)g_AltTabWindows.size() - 1));

    lvItem.state = LVIS_FOCUSED | LVIS_SELECTED;
    SendMessageW(g_hListView, LVM_SETITEMSTATE, nextInd, (LPARAM)&lvItem);
    SendMessageW(g_hListView, LVM_ENSUREVISIBLE, nextInd, (LPARAM)&lvItem);

    g_SelectedIndex = nextInd;
}

void ATWListViewDeleteItem(int rowNumber) {
    SendMessageW(g_hListView, LVM_DELETEITEM, rowNumber, 0);
    ATWListViewSelectItem(rowNumber);
}

int ATWListViewGetSelectedItem() {
    // Move to next / previous item based on the direction
    int selectedRow = (int)SendMessageW(g_hListView, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    return selectedRow;
}

void ATWListViewPageDown() {
    AT_LOG_TRACE;
    // Scroll down one page (assuming item height is the default)
    SendMessageW(g_hListView, LVM_SCROLL, 0, 1);

    // Optionally, you can add a delay if needed
    Sleep(100); // Sleep for 100 milliseconds

    // Scroll up one page
    SendMessageW(g_hListView, LVM_SCROLL, 0, -1);

    // SendMessageW(g_hListView, WM_VSCROLL, MAKEWPARAM(SB_PAGEDOWN, 0), 0);

    // int selectedRow = (int)SendMessageW(g_hListView, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    // LVITEM lvItem;
    // lvItem.stateMask = LVIS_FOCUSED | LVIS_SELECTED;
    // lvItem.state = LVIS_FOCUSED | LVIS_SELECTED;
    // SendMessageW(g_hListView, LVN_KEYDOWN, (WPARAM)VK_NEXT, (LPARAM)&lvItem);
}

bool RegisterAltTabWindow() {
    AT_LOG_TRACE;

    // Register the window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = AltTabWindowProc;
    wc.lpszClassName = CLASS_NAME;
    wc.hInstance = g_hInstance;

    if (!RegisterClass(&wc)) {
        AT_LOG_ERROR("Failed to register AltTab Window class!");
        LogLastErrorInfo();
        return false;
    }
    return true;
}

HWND CreateAltTabWindow() {
    AT_LOG_TRACE;

    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    DWORD style = WS_POPUP | WS_CLIPCHILDREN;

    // Create the window
    HWND hWnd = CreateWindowExW(
        exStyle,     // Optional window styles
        CLASS_NAME,  // Window class
        WINDOW_NAME, // Window title
        style,       // Styles
        0,           // X
        0,           // Y
        0,           // Width
        0,           // Height
        g_hMainWnd,  // Parent window
        nullptr,     // Menu
        g_hInstance, // Instance handle
        nullptr      // Additional application data
    );

    if (hWnd == nullptr) {
        AT_LOG_ERROR("Failed to create AltTab Window!");
        return nullptr;
    }

    // Show the window and bring to the top
    SetForegroundWindow(hWnd);
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);
    BringWindowToTop(hWnd);

    return hWnd;
}

void AddListViewItem(HWND hListView, int index, const AltTabWindowData& windowData) {
    LVITEM lvItem = { 0 };
    lvItem.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
    lvItem.iItem = index;
    lvItem.iSubItem = 0;
    lvItem.iImage = index;
    lvItem.lParam = (LPARAM)(&windowData);

    ListView_InsertItem(hListView, &lvItem);
    ListView_SetItem(hListView, &lvItem);
}

static void CustomizeListView(HWND hListView, int dpi) {
    // Set extended style for the List View control
    DWORD dwExStyle = LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER;
    ListView_SetExtendedListViewStyle(hListView, dwExStyle);

    // A single report column preserves native scrolling and selection. The
    // renderer owns the title/subtitle layout inside that column.
    LVCOLUMN lvCol = { 0 };
    lvCol.mask = LVCF_TEXT | LVCF_WIDTH;
    lvCol.pszText = const_cast<LPWSTR>(L"Windows");
    lvCol.cx = g_Settings.WindowWidth;
    ListView_InsertColumn(hListView, 0, &lvCol);
    UNREFERENCED_PARAMETER(dpi);
}

static void SetListViewCustomColors(HWND hListView, COLORREF backgroundColor, COLORREF textColor) {
    // Set the background color
    SendMessageW(hListView, LVM_SETBKCOLOR, 0, (LPARAM)backgroundColor);
    SendMessageW(hListView, LVM_SETTEXTBKCOLOR, 0, (LPARAM)backgroundColor);

    // Set the text color
    SendMessageW(hListView, LVM_SETTEXTCOLOR, 0, (LPARAM)textColor);
}

// Keyboard navigation and actions for the Alt-Tab windows.
// Returns true if the message was completely handled and assigned to outResult.
bool TryHandleCommonKeyboardHotkeys(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& outResult) {
    if (uMsg != WM_KEYDOWN && uMsg != WM_SYSKEYDOWN) {
        return false;
    }

    auto vkCode = wParam;
    const bool isShiftPressed = GetAsyncKeyState(VK_SHIFT) & 0x8000;

    // ----------------------------------------------------------------------------
    // VK_ESCAPE
    // ----------------------------------------------------------------------------
    if (wParam == VK_ESCAPE) {
        AT_LOG_INFO("VK_ESCAPE Pressed!");
        DestroyAltTabWindow();
        outResult = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        return true;
    }
    // ----------------------------------------------------------------------------
    // VK_DOWN
    // ----------------------------------------------------------------------------
    else if (wParam == VK_DOWN) {
        AT_LOG_INFO("Down Pressed!");
        ATWListViewSelectNextItem();
        outResult = TRUE;
        return true;
    }
    // ----------------------------------------------------------------------------
    // VK_UP
    // ----------------------------------------------------------------------------
    else if (wParam == VK_UP) {
        AT_LOG_INFO("Up Pressed!");
        ATWListViewSelectPrevItem();
        outResult = TRUE;
        return true;
    }
    // ----------------------------------------------------------------------------
    // VK_HOME / VK_PRIOR
    // ----------------------------------------------------------------------------
    else if (vkCode == VK_HOME || vkCode == VK_PRIOR) {
        AT_LOG_INFO("Home/PageUp Pressed!");
        if (!g_AltTabWindows.empty()) {
            ATWListViewSelectItem(0);
        }
        outResult = TRUE;
        return true;
    }
    // ----------------------------------------------------------------------------
    // VK_END / VK_NEXT
    // ----------------------------------------------------------------------------
    else if (vkCode == VK_END || vkCode == VK_NEXT) {
        AT_LOG_INFO("End/PageDown Pressed!");
        if (!g_AltTabWindows.empty()) {
            ATWListViewSelectItem((int)g_AltTabWindows.size() - 1);
        }
        outResult = TRUE;
        return true;
    }
    // ----------------------------------------------------------------------------
    // VK_DELETE
    // ----------------------------------------------------------------------------
    else if (vkCode == VK_DELETE) {
        if (isShiftPressed) {
            AT_LOG_INFO("Shift+Delete Pressed!");
            int ind = ATWListViewGetSelectedItem();
            if (ind == -1) {
                outResult = TRUE;
                return true;
            }
            g_AltTabWindows[ind].IsBeingClosed = true;
            TerminateProcessEx(g_AltTabWindows[ind].PID);
        } else {
            AT_LOG_INFO("Delete Pressed!");

            // Send the SC_CLOSE command to the window
            const int ind = ATWListViewGetSelectedItem();
            if (ind == -1) {
                outResult = TRUE;
                return true;
            }
            AT_LOG_INFO("Ind: %d, Title: %s", ind, WStrToUTF8(g_AltTabWindows[ind].Title).c_str());
            ATCloseWindow(ind);
        }
        outResult = TRUE;
        return true;
    }
    // ----------------------------------------------------------------------------
    // Backtick
    // ----------------------------------------------------------------------------
    else if (vkCode == AT_NON_BEEPING_SEARCH_KEY) { // 0xC0 - '`~' for US
        AT_LOG_INFO("Backtick Pressed!, g_IsAltBacktick = %d", g_IsAltBacktick);
        const int direction = isShiftPressed ? -1 : 1;

        // Move to next / previous same item based on the direction
        const int selectedInd = ATWListViewGetSelectedItem();
        if (selectedInd == -1) {
            outResult = TRUE;
            return true;
        }

        const int N = (int)g_AltTabWindows.size();
        const auto& processName = g_AltTabWindows[selectedInd].ProcessName; // Selected process name
        const int pgInd = GetProcessGroupIndex(processName);                // Index in ProcessGroupList
        int nextInd = (selectedInd + N + direction) % N;                    // Next index to select

        // If the AltTab window is invoked with Alt + Backtick, then we should
        // move to the next item in the list without checking the process name.
        if (g_IsAltBacktick) {
            ATWListViewSelectItem(nextInd);
            outResult = TRUE;
            return true;
        }

        // If the control comes to here, AltTab window is invoked with Alt + Tab
        // Check if the next item is similar to the selected item
        for (int i = 1; i < N; ++i) {
            nextInd = (selectedInd + N + i * direction) % N;
            if (IsSimilarProcess(pgInd, g_AltTabWindows[nextInd].ProcessName)) {
                break;
            }
            if (EqualsIgnoreCase(processName, g_AltTabWindows[nextInd].ProcessName)) {
                break;
            }
            nextInd = -1;
        }

        if (nextInd != -1)
            ATWListViewSelectItem(nextInd);
        outResult = TRUE;
        return true;
    }
    // ----------------------------------------------------------------------------
    // VK_F1
    // ----------------------------------------------------------------------------
    else if (vkCode == VK_F1) {
        DestroyAltTabWindow();
        if (isShiftPressed) {
            DialogBoxW(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), nullptr, ATAboutDlgProc);
        } else {
            ShowHelpWindow();
        }
        outResult = TRUE;
        return true;
    }
    // ----------------------------------------------------------------------------
    // VK_F2
    // ----------------------------------------------------------------------------
    else if (vkCode == VK_F2) {
        DestroyAltTabWindow();
        // Do NOT assign owner for this, if owner is assigned and the settings
        // dialog is not active, then it won't be displayed in alt tab windows list.
        if (g_hSettingsWnd == nullptr) {
            DialogBoxW(g_hInstance, MAKEINTRESOURCE(IDD_SETTINGS), nullptr, ATSettingsDlgProc);
        } else {
            SetForegroundWindow(g_hSettingsWnd);
        }
        outResult = TRUE;
        return true;
    }
    // ----------------------------------------------------------------------------
    // VK_ENTER
    // ----------------------------------------------------------------------------
    else if (vkCode == VK_RETURN) {
        AT_LOG_INFO("Enter Pressed!");
        DestroyAltTabWindow(true);
        outResult = TRUE;
        return true;
    }

    return false;
}

LRESULT CALLBACK SearchStringSubclassProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR /*uIdSubclass*/,
    DWORD_PTR /*dwRefData*/) {
    LRESULT commonResult = 0;
    if (TryHandleCommonKeyboardHotkeys(hWnd, uMsg, wParam, lParam, commonResult)) {
        return commonResult;
    }

    switch (uMsg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        AT_LOG_DEBUG("SearchStringSubclassProc WM_KEYDOWN KJS Detected %d", wParam);
        // ----------------------------------------------------------------------------
        // WM_CHAR won't be sent when ALT is pressed, this is the alternative to handle when a key is pressed.
        // And collect the chars and form a search string.
        // Ignore Backtick (`), Tab, Backspace, and non-printable characters while forming search string.
        // ----------------------------------------------------------------------------
        if ((g_IsAltTab || g_IsAltBacktick) && !g_IsAltCtrlTab) {
            wchar_t ch = '\0';
            const bool isChar = ATMapVirtualKey((UINT)wParam, ch);
            bool update = false;
            // TODO: I don't see how a "backtick" event gets here -- I think this check can be removed
            if (isChar
                && !(wParam == g_Settings.HKBacktickKey || wParam == VK_DELETE || wParam == VK_TAB || ch == '\0')) {
                g_SearchString += ch;
                update = true;
            } else if (wParam == VK_BACK && !g_SearchString.empty()) {
                g_SearchString.pop_back();
                update = true;
            }

            if (update) {
                SendMessageW(g_hSearchString, WM_SETTEXT, 0, (LPARAM)(g_SearchString).c_str());

                // Set the cursor to the end of the text
                SendMessageW(g_hSearchString, EM_SETSEL, (WPARAM)g_SearchString.size(), (LPARAM)g_SearchString.size());
                return 0;
            }

            // Let default handler update text first
            LRESULT result = isChar ? 0 : DefSubclassProc(hWnd, uMsg, wParam, lParam);

            // Invalidate to trigger repaint
            InvalidateRect(hWnd, nullptr, TRUE);

            return result;
        }
    } break;

    // Show the application tray menu on right click / context menu request
    case WM_CONTEXTMENU: {
        AT_LOG_INFO("WM_CONTEXTMENU");
        POINT pt;
        GetCursorPos(&pt);
        const bool result = ShowTrayContextMenu(g_hAltTabWnd, pt);

        // Close the AltTab window after showing the context menu and handling the menu command.
        if (result) {
            DestroyAltTabWindow();
        }
        return 0;
    } break;

    // ----------------------------------------------------------------------------
    // WM_CHAR, NOTE: This is also needed to handle character input when ALT is not pressed. Ex: Alt+Ctrl+Tab
    // ----------------------------------------------------------------------------
    case WM_CHAR: {
        AT_LOG_INFO("WM_CHAR: wParam = 0x%02X", (unsigned int)wParam);
        const wchar_t ch = (wchar_t)wParam;
        // Handle character input for search functionality
        // TODO: I don't see how a "backtick" event gets here -- I think this check can be removed
        if (!(wParam == g_Settings.HKBacktickKey || ch == VK_TAB || ch == VK_DELETE || ch == VK_SPACE || ch == '\0')) {
            // Let the default handler process the character input
            LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            // After processing the character, update the search filter
            wchar_t searchString[_MAX_PATH] = { 0 };
            GetWindowTextW(g_hSearchString, searchString, _MAX_PATH);
            g_SearchString = searchString;
            return result;
        }
        return 0; // Ignore other characters
    } break;

    case WM_NOTIFY: {
        AT_LOG_INFO("WM_NOTIFY");
    } break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ListViewSubclassProc(
    HWND hListView,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR /*uIdSubclass*/,
    DWORD_PTR /*dwRefData*/) {
    LRESULT commonResult = 0;
    if (TryHandleCommonKeyboardHotkeys(hListView, uMsg, wParam, lParam, commonResult)) {
        return commonResult;
    }

    switch (uMsg) {
    case WM_NOTIFY: {
        AT_LOG_INFO("WM_NOTIFY");
    } break;

    case WM_MOUSEMOVE: {
        // Track mouse leave event
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hListView;
        TrackMouseEvent(&tme);
    } break;

    case WM_MOUSELEAVE: {
        // Hide the tooltip when the mouse leaves the ListView
        if (g_Settings.ShowProcessInfoTooltip) {
            HideCustomToolTip();
        }

        // Reset the hovered item index and hot item index
        g_MouseHoverIndex = -1;
        g_nLVHotItem = -1;
        g_IsMouseOverCloseButton = false;
        g_rcBtnClose = {};

        // Invalidate the ListView to remove any highlighting
        InvalidateRect(hListView, nullptr, TRUE);
    } break;

    // ----------------------------------------------------------------------------
    // Owner draw item
    // ----------------------------------------------------------------------------
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpDrawItemStruct = (LPDRAWITEMSTRUCT)lParam;
        return AT::ATListViewDrawItem(hListView, lpDrawItemStruct);
    }
    }

    return DefSubclassProc(hListView, uMsg, wParam, lParam);
}

INT_PTR CALLBACK AltTabWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // AT_LOG_TRACE;
    // AT_LOG_INFO(std::format("uMsg: {:4}, wParam: {}, lParam: {}", uMsg, wParam, lParam).c_str());

    switch (uMsg) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC hdc = BeginPaint(hWnd, &paint);
        RECT client{};
        GetClientRect(hWnd, &client);
        if (g_SwitcherRenderer) {
            g_SwitcherRenderer->PaintPanel(hWnd, hdc, client, g_Settings.ShowSearchString);
        }
        EndPaint(hWnd, &paint);
        return 0;
    }
    case WM_MEASUREITEM:
        if (ThemedMenuSession::HandleMeasureItem(reinterpret_cast<MEASUREITEMSTRUCT*>(lParam)))
            return TRUE;
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(
            hWnd,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        RebuildSwitcherVisuals(hWnd, HIWORD(wParam));
        RefreshAltTabWindow();
        InvalidateRect(hWnd, nullptr, TRUE);
        return 0;
    }
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
    case WM_SYSCOLORCHANGE:
        RebuildSwitcherVisuals(hWnd, GetDpiForWindow(hWnd));
        LayoutSwitcherWindow(hWnd);
        InvalidateRect(hWnd, nullptr, TRUE);
        InvalidateRect(g_hListView, nullptr, TRUE);
        return 0;
        HANDLE_MSG(hWnd, WM_ACTIVATE, ATW_OnActivate);
        HANDLE_MSG(hWnd, WM_CLOSE, ATW_OnClose);
        HANDLE_MSG(hWnd, WM_COMMAND, ATW_OnCommand);
        HANDLE_MSG(hWnd, WM_CONTEXTMENU, ATW_OnContextMenu);
        HANDLE_MSG(hWnd, WM_CREATE, ATW_OnCreate);
        HANDLE_MSG(hWnd, WM_CTLCOLOREDIT, ATW_OnCtlColorEdit);
        HANDLE_MSG(hWnd, WM_CTLCOLORSTATIC, ATW_OnCtlColorStatic);
        HANDLE_MSG(hWnd, WM_DESTROY, ATW_OnDestroy);
        HANDLE_MSG(hWnd, WM_DRAWITEM, ATW_OnDrawItem);
        HANDLE_MSG(hWnd, WM_KEYDOWN, ATW_OnKeyDown);
        HANDLE_MSG(hWnd, WM_KILLFOCUS, ATW_OnKillFocus);
        HANDLE_MSG(hWnd, WM_LBUTTONDOWN, ATW_OnLButtonDown);
        HANDLE_MSG(hWnd, WM_NOTIFY, ATW_OnNotify);
        HANDLE_MSG(hWnd, WM_SYSCOMMAND, ATW_OnSysCommand);
        HANDLE_MSG(hWnd, WM_TIMER, ATW_OnTimer);

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

bool IsInvisibleWin10BackgroundAppWindow(HWND hWnd) {
    BOOL isCloaked;
    DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &isCloaked, sizeof(BOOL));
    return isCloaked;
}

/**
 * Check if the given window handle's window is a AltTab window.
 *
 * \param hWnd Window handle
 * \return True if the given hWnd is a AltTab window otherwise false.
 */
bool IsAltTabWindow(HWND hWnd) {
    if (!IsWindowVisible(hWnd))
        return false;

    HWND hOwner = GetOwnerWindowHwnd(hWnd);

    if (GetLastActivePopup(hOwner) != hWnd)
        return false;

    // Even the owner window is hidden we are getting the window styles, so make
    // sure that the owner window is visible before checking the window styles
    DWORD ownerES = GetWindowLong(hOwner, GWL_EXSTYLE);
    if (ownerES && IsWindowVisible(hOwner) && !((ownerES & WS_EX_TOOLWINDOW) && !(ownerES & WS_EX_APPWINDOW))
        && !IsInvisibleWin10BackgroundAppWindow(hOwner)) {
        return true;
    }

    DWORD windowES = GetWindowLong(hWnd, GWL_EXSTYLE);
    if (windowES && !((windowES & WS_EX_TOOLWINDOW) && !(windowES & WS_EX_APPWINDOW))
        && !IsInvisibleWin10BackgroundAppWindow(hWnd)) {
        return true;
    }

    if (windowES == 0 && ownerES == 0) {
        return true;
    }

    return false;
}

/**
 * Get owner window handle for the given hWnd.
 *
 * \param hWnd Window handle
 * \return The owner window handle for the given hWnd.
 */
HWND GetOwnerWindowHwnd(HWND hWnd) {
    HWND hOwner = hWnd;
    do {
        hOwner = GetWindow(hOwner, GW_OWNER);
    } while (GetWindow(hOwner, GW_OWNER));
    hOwner = hOwner ? hOwner : hWnd;
    return hOwner;
}

/*!
 * \brief Show AltTab window's context menu at the center of the selected item.
 */
void ShowContextMenuAtItemCenter() {
    // Get the position of the selected item
    // Get the bounding rectangle of the selected item
    RECT itemRect;
    ListView_GetItemRect(g_hListView, g_SelectedIndex, &itemRect, LVIR_BOUNDS);

    // Calculate the center of the entire row
    POINT center;
    center.x = (itemRect.left + itemRect.right) / 2;
    center.y = (itemRect.top + itemRect.bottom) / 2;

    // Convert to screen coordinates if needed
    ClientToScreen(g_hListView, &center);

    ShowContextMenu(g_hAltTabWnd, center);
}

// ----------------------------------------------------------------------------
// Show AltTab context menu
// ----------------------------------------------------------------------------
void ShowContextMenu(HWND hWnd, POINT pt) {
    HMENU hMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_CONTEXTMENU));
    if (hMenu) {
        ThemedMenuSession menuSession(hMenu, ResolveTheme(g_Settings, GetDpiForWindow(hWnd)));
        HMENU hSubMenu = menuSession.Popup();
        if (hSubMenu) {
            // respect menu drop alignment
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0) {
                uFlags |= TPM_RIGHTALIGN;
            } else {
                uFlags |= TPM_LEFTALIGN;
            }

            // Set global context menu handle
            g_hContextMenu = hSubMenu;

            // Use TPM_RETURNCMD flag let TrackPopupMenuEx function return the
            // menu item identifier of the user's selection in the return value.
            uFlags |= TPM_RETURNCMD;
            UINT menuItemId = TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hWnd, nullptr);
            g_hContextMenu = nullptr;

            if (menuItemId != 0) {
                ContextMenuItemHandler(hWnd, hSubMenu, menuItemId);
            }
        }
    }
}

void SetAltTabActiveWindow() {
    SetForegroundWindow(g_hAltTabWnd);
    SetActiveWindow(g_hAltTabWnd);
    SetFocus(g_hSearchString);
}

void CopyToClipboard(const std::wstring& text) {
    if (OpenClipboard(g_hAltTabWnd)) {
        EmptyClipboard();
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        if (hGlobal) {
            wchar_t* pGlobal = (wchar_t*)GlobalLock(hGlobal);
            if (pGlobal) {
                wcscpy_s(pGlobal, text.size() + 1, text.c_str());
                GlobalUnlock(hGlobal);
                // On success the system owns hGlobal; free it only if the call fails.
                if (!SetClipboardData(CF_UNICODETEXT, hGlobal)) {
                    GlobalFree(hGlobal);
                }
            } else {
                GlobalFree(hGlobal);
            }
        }
        CloseClipboard();
    } else {
        AT_LOG_ERROR("OpenClipboard failed");
    }
}

void BrowseToFile(const std::wstring& filepath) {
    if (!filepath.empty()) {
        LPITEMIDLIST pidl = ILCreateFromPathW(filepath.c_str());
        if (pidl) {
            SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            ILFree(pidl);
        }
    }
}

// ----------------------------------------------------------------------------
// AltTab window context menu handler
// ----------------------------------------------------------------------------
void ContextMenuItemHandler(HWND hWnd, HMENU /*hSubMenu*/, UINT menuItemId) {
    switch (menuItemId) {
    case ID_CONTEXTMENU_CLOSE_WINDOW: {
        // Send the SC_CLOSE command to the window
        const int ind = ATWListViewGetSelectedItem();
        if (ind != -1) {
            ATCloseWindow(ind);
        }
    } break;

    case ID_CONTEXTMENU_KILL_PROCESS: {
        const int ind = ATWListViewGetSelectedItem();
        if (ind != -1) {
            TerminateProcessEx(g_AltTabWindows[ind].PID);
        }
    } break;

    case ID_CONTEXTMENU_CLOSEALLWINDOWS: {
        AT_LOG_INFO("ID_CONTEXTMENU_CLOSEALLWINDOWS");

        // Here, this MessageBox is also displayed in AltTab windows list. Did
        // not find a way to avoid this. So, turning off the timer.
        KillTimer(hWnd, TIMER_WINDOW_COUNT);

        // Set flag to ignore WM_ACTIVATE message temporarily since we are going to show a message box now.
        // So, the AltTab window should not be closed when the message box is shown.
        g_bIgnoreWM_ACTIVATE = true;

        bool closeAllWindows = true;
        if (g_Settings.PromptTerminateAll) {
            const int result = MessageBoxW(
                hWnd,
                L"Are you sure you want to close all windows?",
                AT_PRODUCT_NAMEW L": Close All Windows",
                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
            closeAllWindows = (result == IDYES);
        }

        if (closeAllWindows) {
            for (int i = 0; i < g_AltTabWindows.size(); ++i) {
                ATCloseWindow(i);
            }
        } else {
            SetAltTabActiveWindow();
        }

        g_bIgnoreWM_ACTIVATE = false;

        SetTimer(hWnd, TIMER_WINDOW_COUNT, TIMER_WINDOW_COUNT_ELAPSE, nullptr);
    } break;

    case ID_CONTEXTMENU_KILLALLPROCESSES: {
        AT_LOG_INFO("ID_CONTEXTMENU_KILLALLPROCESSES");

        // Here, this MessageBox is also displayed in AltTab windows list. Did
        // not find a way to avoid this. So, turning off the timer.
        KillTimer(hWnd, TIMER_WINDOW_COUNT);

        bool terminateAllWindows = true;
        if (g_Settings.PromptTerminateAll) {
            int result = ATMessageBoxW(
                hWnd,
                L"Are you sure you want to terminate all windows?",
                AT_PRODUCT_NAMEW L": Close All Windows",
                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
            terminateAllWindows = (result == IDYES);
        }
        if (terminateAllWindows) {
            for (auto& g_AltTabWindow : g_AltTabWindows) {
                TerminateProcessEx(g_AltTabWindow.PID);
            }
        } else {
            SetAltTabActiveWindow();
        }

        SetTimer(hWnd, TIMER_WINDOW_COUNT, TIMER_WINDOW_COUNT_ELAPSE, nullptr);
    } break;

    case ID_CONTEXTMENU_OPEN_PATH: {
        AT_LOG_INFO("ID_CONTEXTMENU_OPEN_PATH");
        const int ind = ATWListViewGetSelectedItem();
        if (ind != -1) {
            const auto filepath = g_AltTabWindows[ind].FullPath;
            DestroyAltTabWindow();
            BrowseToFile(filepath);
        }
    } break;

    case ID_CONTEXTMENU_COPY_PATH: {
        AT_LOG_INFO("ID_CONTEXTMENU_COPY_PATH");
        const int ind = ATWListViewGetSelectedItem();
        if (ind != -1) {
            const auto filepath = g_AltTabWindows[ind].FullPath;
            DestroyAltTabWindow();
            CopyToClipboard(filepath);
        }
    } break;

    case ID_CONTEXTMENU_COPY_TITLE: {
        AT_LOG_INFO("ID_CONTEXTMENU_COPY_TITLE");
        const int ind = ATWListViewGetSelectedItem();
        if (ind != -1) {
            const auto title = g_AltTabWindows[ind].Title;
            DestroyAltTabWindow();
            CopyToClipboard(title);
        }
    } break;

    case ID_CONTEXTMENU_ABOUTALTTAB:
    case ID_TRAYCONTEXTMENU_ABOUTALTTAB: {
        AT_LOG_INFO("ID_CONTEXTMENU_ABOUTALTTAB");
        DestroyAltTabWindow();
        DialogBoxW(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), g_hMainWnd, ATAboutDlgProc);
    } break;

    case ID_CONTEXTMENU_SETTINGS: {
        AT_LOG_INFO("ID_CONTEXTMENU_SETTINGS");
        DestroyAltTabWindow();
        DialogBoxW(g_hInstance, MAKEINTRESOURCE(IDD_SETTINGS), g_hMainWnd, ATSettingsDlgProc);
    } break;

    case ID_TRAYCONTEXTMENU_README:
        AT_LOG_INFO("ID_TRAYCONTEXTMENU_README");
        ShowReadMeWindow();
        break;

    case ID_TRAYCONTEXTMENU_HELP:
        AT_LOG_INFO("ID_TRAYCONTEXTMENU_HELP");
        ShowHelpWindow();
        break;

    case ID_TRAYCONTEXTMENU_RELEASENOTES:
        AT_LOG_INFO("ID_TRAYCONTEXTMENU_RELEASENOTES");
        ShowReleaseNotesWindow();
        break;

    case ID_TRAYCONTEXTMENU_CHECKFORUPDATES: {
        AT_LOG_INFO("ID_TRAYCONTEXTMENU_CHECKFORUPDATES");
        DestroyAltTabWindow();
        HideCustomToolTip();
        ShowCustomToolTip(L"Checking for updates..., please wait.");

        // Had to run CheckForUpdates in a thread to display the tooltip... :-(
        std::thread thr(CheckForUpdates, false);
        thr.detach(); // Let the thread run independently, otherwise tooltip won't be displayed.
    } break;

    case ID_TRAYCONTEXTMENU_RELOADALTTABSETTINGS: {
        AT_LOG_INFO("ID_TRAYCONTEXTMENU_RELOADALTTABSETTINGS");
        ATLoadSettings();
        ShowCustomToolTip(L"Settings reloaded successfully.", 3000);
    } break;

    case ID_TRAYCONTEXTMENU_RESTART:
        AT_LOG_INFO("ID_TRAYCONTEXTMENU_RESTART");
        DestroyAltTabWindow();
        RestartApplication();
        break;

    case ID_CONTEXTMENU_EXIT:
    case ID_TRAYCONTEXTMENU_EXIT:
        AT_LOG_INFO("ID_CONTEXTMENU_EXIT");
        DestroyAltTabWindow();
        PostQuitMessage(0);
        // int result = ATMessageBoxW(
        //     hWnd,
        //     L"Are you sure you want to exit?",
        //     AT_PRODUCT_NAMEW,
        //     MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);
        // if (result == IDOK) {
        //     PostQuitMessage(0);
        // }
        break;
    }
}

/*!
 * Terminate the given process id
 *
 * \param pid  ProcessID
 * \return True if terminated successfully otherwise false.
 */
BOOL TerminateProcessEx(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        return TRUE;
    }
    return FALSE;
}

void ATCloseWindow(const int index) {
    if (index >= 0 && index < g_AltTabWindows.size()) {
        AltTabWindowData& windowData = g_AltTabWindows[index];

#ifdef _DEBUG
        // Ignore Visual Studio IDEs in debug mode
        if (EqualsIgnoreCase(windowData.ProcessName, L"devenv.exe")
            || EqualsIgnoreCase(windowData.ProcessName, L"Code.exe")
            || EqualsIgnoreCase(windowData.ProcessName, L"Outlook.exe")
            || EqualsIgnoreCase(windowData.ProcessName, L"xplorer2_64.exe")
            || EqualsIgnoreCase(windowData.ProcessName, L"msedge.exe")
            || EqualsIgnoreCase(windowData.ProcessName, L"OneNote.exe")
            || EqualsIgnoreCase(windowData.ProcessName, L"Fork.exe")
            || EqualsIgnoreCase(windowData.ProcessName, L"ConEmu64.exe")
            || EqualsIgnoreCase(windowData.ProcessName, L"ms-teams.exe")
            || EqualsIgnoreCase(windowData.ProcessName, L"AltTab.exe")) {
            AT_LOG_INFO(
                "Ignoring: ProcessName: [%-15ls], Title:[%ls] ...",
                windowData.ProcessName.c_str(),
                windowData.Title.c_str());
            return;
        }
#endif // _DEBUG

        windowData.IsBeingClosed = true;
        AT_LOG_INFO(
            "ATCloseWindow: index: %d, hWnd: %#x, hOwner: %#x, Title: %ls",
            index,
            windowData.hWnd,
            windowData.hOwner,
            windowData.Title.c_str());

        // Send REDRAW message immediately
        ListView_RedrawItems(g_hListView, index, index);
        UpdateWindow(g_hListView);

        // Allow some time for the redraw to complete so that the use can see the change
        Sleep(50);

        // If there is a modal dialog box is open for the window, both WM_CLOSE and SC_CLOSE will
        // close the dialog box on sending the WM_CLOSE on the hWnd instead of closing the actual window.
        // The reason behind this is actual window handle (hOwner) is not the foreground window, instead
        // the dialog box is the foreground window.
        // And sending WM_CLOSE to hOwner will not close the actual window if there is no dialog box.
        // So, first send SC_CLOSE to hWnd, wait for some time and then send SC_CLOSE to hOwner if hOwner
        // is different than hWnd.
        SendMessageW(windowData.hWnd, WM_SYSCOMMAND, SC_CLOSE, 0);
        Sleep(50);

        // If the hWnd is not same as hOwner then also send the SC_CLOSE to hOwner
        if (windowData.hOwner != windowData.hWnd && IsWindow(windowData.hOwner)) {
            SendMessageW(windowData.hOwner, WM_SYSCOMMAND, SC_CLOSE, 0);
        }

        // TODO: Check if still the window is not closed then try to send WM_CLOSE
    }
}

bool ATMapVirtualKey(UINT uCode, wchar_t& vkCode) {
    wchar_t ch = static_cast<wchar_t>(uCode);
    AT_LOG_INFO("uCode: [%c], ch: [%c]", uCode, ch);

    if (!iswprint((wint_t)uCode)) {
        vkCode = '\0';
        return false;
    }

    const bool isShiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    // Check for alphabetic and digit keys
    if ((uCode >= 'A' && uCode <= 'Z') || (uCode >= '0' && uCode <= '9')) {
        ch = (wchar_t)MapVirtualKey(uCode, MAPVK_VK_TO_CHAR);

        // Adjust the case based on the Shift key state for alphabets
        if (!isShiftPressed && ch >= L'A' && ch <= L'Z') {
            ch = ch - L'A' + L'a';
        }

        if (isShiftPressed && ch >= L'0' && ch <= L'9') {
            ch = L")!@#$%^&*("[ch - L'0'];
        }

        vkCode = ch;
        return true;
    }

    // Handle other special characters
    switch (uCode) {
    case VK_SPACE:
        vkCode = L' ';
        return true;
    case VK_OEM_MINUS:
        vkCode = isShiftPressed ? L'_' : L'-';
        return true;
    case VK_OEM_PLUS:
        vkCode = isShiftPressed ? L'=' : L'+';
        return true;
    case VK_OEM_1:
        vkCode = isShiftPressed ? L':' : L';';
        return true;
    case VK_OEM_2:
        vkCode = isShiftPressed ? L'?' : L'/';
        return true;
    case VK_OEM_3:
        vkCode = isShiftPressed ? L'~' : L'`';
        return true;
    case VK_OEM_4:
        vkCode = isShiftPressed ? L'{' : L'[';
        return true;
    case VK_OEM_5:
        vkCode = isShiftPressed ? L'|' : L'\\';
        return true;
    case VK_OEM_6:
        vkCode = isShiftPressed ? L'}' : L']';
        return true;
    case VK_OEM_7:
        vkCode = isShiftPressed ? L'"' : L'\'';
        return true;
    case VK_OEM_COMMA:
        vkCode = isShiftPressed ? L'<' : L',';
        return true;
    case VK_OEM_PERIOD:
        vkCode = isShiftPressed ? L'>' : L'.';
        return true;
    case VK_OEM_102:
        vkCode = isShiftPressed ? L'>' : L'<';
        return true;
    }
    return false;
}

std::vector<AltTabWindowData> GetAltTabWindows() {
    std::vector<AltTabWindowData> altTabWindows;
    EnumWindows(EnumWindowsProc, (LPARAM)(&altTabWindows));
    return altTabWindows;
}

/*!
 * Check if the process name is in the exclusion list
 *
 * \param processName Process name, should be in lower case
 * \return True if the processName is in the exclusion list otherwise false
 */
bool IsExcludedProcess(const std::wstring& processName) {
    bool excluded = false;
    if (g_Settings.ProcessExclusionsEnabled) {
        excluded =
            std::find(g_Settings.ProcessExclusionList.begin(), g_Settings.ProcessExclusionList.end(), processName)
            != g_Settings.ProcessExclusionList.end();
    }
    return excluded;
}

// ----------------------------------------------------------------------------
// AltTab window procedure handlers
// ----------------------------------------------------------------------------

DWORD prevGdiObjectCount = 0;
DWORD prevUserObjectCount = 0;

static RECT GetSwitcherWorkArea() {
    HWND anchor = g_hFGWnd ? g_hFGWnd : GetForegroundWindow();
    HMONITOR monitor = MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{ sizeof(info) };
    if (GetMonitorInfoW(monitor, &info))
        return info.rcWork;
    return { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

static void LayoutSwitcherWindow(HWND hWnd) {
    if (!g_SwitcherRenderer || !g_hListView)
        return;
    const ThemeMetrics& metrics = g_SwitcherRenderer->Theme().metrics;
    const RECT work = GetSwitcherWorkArea();
    const int workWidth = work.right - work.left;
    const int workHeight = work.bottom - work.top;
    const int minimumWidth = MulDiv(520, static_cast<int>(g_SwitcherRenderer->Theme().dpi), 96);
    const int windowWidth =
        std::clamp(MulDiv(workWidth, g_Settings.WidthPercentage, 100), (std::min)(minimumWidth, workWidth), workWidth);
    const int maximumHeight = MulDiv(workHeight, g_Settings.HeightPercentage, 100);
    const int searchBlock = g_Settings.ShowSearchString ? metrics.searchHeight + metrics.searchListGap : 0;
    const int fixedHeight = metrics.panelPadding * 2 + searchBlock;
    const int itemCount = ListView_GetItemCount(g_hListView);
    const int visibleRows =
        itemCount == 0 ? 0 : (std::max)(1, (std::min)(itemCount, (maximumHeight - fixedHeight) / metrics.rowHeight));
    const int windowHeight = (std::min)(workHeight, fixedHeight + visibleRows * metrics.rowHeight);
    const int x = work.left + (workWidth - windowWidth) / 2;
    const int y = work.top + (workHeight - windowHeight) / 2;

    g_Settings.WindowWidth = windowWidth;
    g_Settings.WindowHeight = windowHeight;
    SetWindowPos(hWnd, HWND_TOPMOST, x, y, windowWidth, windowHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    const int contentWidth = windowWidth - metrics.panelPadding * 2;
    if (g_Settings.ShowSearchString) {
        const int glyphSpace = MulDiv(40, static_cast<int>(g_SwitcherRenderer->Theme().dpi), 96);
        const int verticalInset = MulDiv(6, static_cast<int>(g_SwitcherRenderer->Theme().dpi), 96);
        SetWindowPos(
            g_hSearchString,
            nullptr,
            metrics.panelPadding + glyphSpace,
            metrics.panelPadding + verticalInset,
            contentWidth - glyphSpace - MulDiv(8, static_cast<int>(g_SwitcherRenderer->Theme().dpi), 96),
            metrics.searchHeight - verticalInset * 2,
            SWP_NOZORDER | SWP_SHOWWINDOW);
    } else {
        ShowWindow(g_hSearchString, SW_HIDE);
    }

    const int listY = metrics.panelPadding + searchBlock;
    const int listHeight = visibleRows * metrics.rowHeight;
    const UINT dpi = g_SwitcherRenderer->Theme().dpi;
    int listWindowWidth = contentWidth;
    int listWindowHeight = listHeight;
    HRGN visibleRegion = CreateRectRgn(0, 0, contentWidth, listHeight);
    if (visibleRegion) {
        if (SetWindowRgn(g_hListView, visibleRegion, FALSE)) {
            constexpr int scrollbarGuard = 2;
            listWindowWidth += GetSystemMetricsForDpi(SM_CXVSCROLL, dpi) + scrollbarGuard;
            listWindowHeight += GetSystemMetricsForDpi(SM_CYHSCROLL, dpi) + scrollbarGuard;
        } else {
            DeleteObject(visibleRegion);
            AT_LOG_ERROR("SetWindowRgn failed; falling back to hidden native scrollbars");
            ShowScrollBar(g_hListView, SB_BOTH, FALSE);
        }
    } else {
        AT_LOG_ERROR("CreateRectRgn failed; falling back to hidden native scrollbars");
        ShowScrollBar(g_hListView, SB_BOTH, FALSE);
    }
    SetWindowPos(
        g_hListView,
        nullptr,
        metrics.panelPadding,
        listY,
        listWindowWidth,
        listWindowHeight,
        SWP_NOZORDER | (listHeight > 0 ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    ListView_SetColumnWidth(g_hListView, 0, (std::max)(1, contentWidth - 1));
    InvalidateRect(g_hListView, nullptr, FALSE);
}

static void RebuildSwitcherVisuals(HWND hWnd, UINT dpi) {
    if (!g_SwitcherRenderer)
        g_SwitcherRenderer = std::make_unique<SwitcherRenderer>();
    g_SwitcherRenderer->Rebuild(hWnd, g_Settings, dpi ? dpi : 96);

    if (g_hSearchString) {
        SendMessageW(g_hSearchString, WM_SETFONT, reinterpret_cast<WPARAM>(g_SwitcherRenderer->SearchFont()), TRUE);
        SendMessageW(
            g_hSearchString, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(g_Settings.SSCueBannerText.c_str()));
    }
    if (g_hListView) {
        SendMessageW(g_hListView, WM_SETFONT, reinterpret_cast<WPARAM>(g_SwitcherRenderer->TitleFont()), TRUE);
        SetListViewCustomColors(
            g_hListView, g_SwitcherRenderer->Theme().palette.panel, g_SwitcherRenderer->Theme().palette.primaryText);
        SetWindowTheme(g_hListView, L"Explorer", nullptr);
    }

    LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    if (g_Settings.Appearance == AppearanceMode::Custom) {
        SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hWnd, 0, static_cast<BYTE>(g_Settings.Transparency), LWA_ALPHA);
    } else if ((exStyle & WS_EX_LAYERED) != 0) {
        SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
    }
}

BOOL ATW_OnCreate(HWND hWnd, LPCREATESTRUCT /*lpCreateStruct*/) {
    AT_LOG_TRACE;

    g_hAltTabWnd = hWnd;
    HWND anchor = g_hFGWnd ? g_hFGWnd : GetForegroundWindow();
    const UINT dpi = anchor ? GetDpiForWindow(anchor) : 96;
    RebuildSwitcherVisuals(hWnd, dpi);

    const RECT work = GetSwitcherWorkArea();
    g_Settings.WindowWidth = MulDiv(work.right - work.left, g_Settings.WidthPercentage, 100);

    g_hSearchString = CreateWindowExW(
        0,
        WC_EDITW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL,
        0,
        0,
        0,
        0,
        hWnd,
        reinterpret_cast<HMENU>(IDC_ALTTAB_EDIT),
        g_hInstance,
        nullptr);
    if (!g_hSearchString)
        return FALSE;
    SetWindowSubclass(g_hSearchString, SearchStringSubclassProc, 1, 0);

    g_hListView = CreateWindowExW(
        0,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED | LVS_NOCOLUMNHEADER,
        0,
        0,
        0,
        0,
        hWnd,
        reinterpret_cast<HMENU>(IDC_LISTVIEW),
        g_hInstance,
        nullptr);
    if (!g_hListView)
        return FALSE;
    SetWindowSubclass(g_hListView, ListViewSubclassProc, 1, 0);
    CustomizeListView(g_hListView, dpi);
    RebuildSwitcherVisuals(hWnd, dpi);
    RefreshAltTabWindow();
    LayoutSwitcherWindow(hWnd);

    SetForegroundWindow(hWnd);
    SetFocus(g_Settings.ShowSearchString ? g_hSearchString : g_hListView);
    ATWListViewSelectItem(0);
    SetTimer(hWnd, TIMER_WINDOW_COUNT, TIMER_WINDOW_COUNT_ELAPSE, nullptr);
    return TRUE;
}

LRESULT ATW_OnCtlColorEdit(HWND hWnd, HDC hDC, HWND hCtl, UINT /*type*/) {
    AT_LOG_TRACE;
    if (hCtl == g_hSearchString) {
        if (g_SwitcherRenderer) {
            SetTextColor(
                hDC,
                g_Settings.Appearance == AppearanceMode::Custom ? g_Settings.SSFontColor
                                                                : g_SwitcherRenderer->Theme().palette.primaryText);
            SetBkColor(hDC, g_SwitcherRenderer->Theme().palette.search);
            return reinterpret_cast<LRESULT>(g_SwitcherRenderer->SearchBrush());
        }
    }
    return DefWindowProc(hWnd, WM_CTLCOLOREDIT, (WPARAM)hDC, (LPARAM)hCtl);
}

LRESULT ATW_OnCtlColorStatic(HWND hWnd, HDC hDC, HWND hCtl, UINT /*type*/) {
    AT_LOG_TRACE;
    if (hCtl == g_hSearchString) {
        if (g_SwitcherRenderer) {
            SetTextColor(
                hDC,
                g_Settings.Appearance == AppearanceMode::Custom ? g_Settings.SSFontColor
                                                                : g_SwitcherRenderer->Theme().palette.primaryText);
            SetBkColor(hDC, g_SwitcherRenderer->Theme().palette.search);
            return reinterpret_cast<LRESULT>(g_SwitcherRenderer->SearchBrush());
        }
    }
    return DefWindowProc(hWnd, WM_CTLCOLORSTATIC, (WPARAM)hDC, (LPARAM)hCtl);
}

void ATW_OnCommand(HWND /*hwnd*/, int id, HWND /*hwndCtl*/, UINT /*codeNotify*/) {
    AT_LOG_TRACE;
    if (id == IDOK || id == IDCANCEL) {
        PostQuitMessage(0);
    }
}

void ATW_OnContextMenu(HWND hwnd, HWND /*hwndContext*/, UINT xPos, UINT yPos) {
    AT_LOG_TRACE;

    POINT pt = { (LONG)xPos, (LONG)yPos };
    ShowContextMenu(hwnd, pt);
}

void ATW_OnSysCommand(HWND hwnd, UINT cmd, int x, int y) {
    AT_LOG_TRACE;

    if (cmd == SC_KEYMENU && (x == -1 && y == -1) /*VK_APPS*/) {
        // Alt+Space pressed, handle it here
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        TrackPopupMenu(GetSystemMenu(hwnd, FALSE), 0, cursorPos.x, cursorPos.y, 0, hwnd, nullptr);
    }
}

void ATW_OnClose(HWND /*hwnd*/) {
    AT_LOG_TRACE;
    PostQuitMessage(0);
}

void ATW_OnKeyDown(HWND /*hwnd*/, UINT vk, BOOL /*fDown*/, int /*cRepeat*/, UINT /*flags*/) {
    AT_LOG_TRACE;

    if (vk == VK_ESCAPE) {
        AT_LOG_INFO("WM_KEYDOWN: VK_ESCAPE");
        // Close the window when Escape key is pressed
        DestroyAltTabWindow();
    }
}

void ATW_OnKillFocus(HWND /*hwnd*/, HWND /*hwndNewFocus*/) {
    AT_LOG_TRACE;
}

void ATW_OnLButtonDown(HWND /*hwnd*/, BOOL /*fDoubleClick*/, int /*x*/, int /*y*/, UINT /*keyFlags*/) {
    AT_LOG_TRACE;
}

void ATW_OnTimer(HWND /*hwnd*/, UINT /*id*/) {
    // AT_LOG_TRACE;
    if (!g_SearchString.empty()) {
        AT_LOG_DEBUG("g_SearchString: [%ls]", g_SearchString.c_str());
    }
    std::vector<AltTabWindowData> altTabWindows;
    EnumWindows(EnumWindowsProc, (LPARAM)(&altTabWindows));
    // AT_LOG_INFO("altTabWindows.size(): %d, g_AltTabWindows.size(): %d", altTabWindows.size(),
    // g_AltTabWindows.size());
    bool doRefresh = altTabWindows.size() != g_AltTabWindows.size();
    if (!doRefresh) {
        // Deep compare the two vectors and update only if there is a change
        for (int i = 0; i < altTabWindows.size(); ++i) {
            if (altTabWindows[i] != g_AltTabWindows[i]) {
                doRefresh = true;
                break;
            }
        }
    }
    if (doRefresh) {
        RefreshAltTabWindow();
    }
}

void ATW_OnActivate(HWND /*hwnd*/, UINT state, HWND /*hwndActDeact*/, BOOL /*fMinimized*/) {
    AT_LOG_TRACE;
    AT_LOG_INFO("WM_ACTIVATE: g_bIgnoreWM_ACTIVATE: %d", g_bIgnoreWM_ACTIVATE);
    if (state == WA_INACTIVE && !g_bIgnoreWM_ACTIVATE) {
        AT_LOG_INFO("WM_ACTIVATE: The application is becoming inactive, close the window");
        // The application is becoming inactive, close the window
        DestroyAltTabWindow();
    }
}

void ATW_OnDestroy(HWND hwnd) {
    AT_LOG_TRACE;
    KillTimer(hwnd, TIMER_WINDOW_COUNT);
    if (g_hSearchString)
        RemoveWindowSubclass(g_hSearchString, SearchStringSubclassProc, 1);
    if (g_hListView)
        RemoveWindowSubclass(g_hListView, ListViewSubclassProc, 1);
    if (g_hRowHeightImageList) {
        ImageList_Destroy(std::exchange(g_hRowHeightImageList, nullptr));
    }
    if (g_hLVImageList) {
        ImageList_Destroy(std::exchange(g_hLVImageList, nullptr));
    }
    g_SwitcherRenderer.reset();
    g_hSearchString = nullptr;
    g_hListView = nullptr;
    g_rcBtnClose = {};
}

void ATW_OnDrawItem(HWND /*hwnd*/, const DRAWITEMSTRUCT* lpDrawItem) {
    // AT_LOG_TRACE;
    // AT_LOG_INFO("hwndItem: %#x CtlType: %d, itemID: %2d, itemAction: %d, itemState: %#4x",
    //     lpDrawItem->hwndItem,
    //     lpDrawItem->CtlType,
    //     lpDrawItem->itemID,
    //     lpDrawItem->itemAction,
    //     lpDrawItem->itemState);
    if (ThemedMenuSession::HandleDrawItem(lpDrawItem))
        return;
    if (lpDrawItem->CtlType == ODT_LISTVIEW && g_SwitcherRenderer) {
        g_SwitcherRenderer->DrawListViewRow(
            lpDrawItem->hwndItem,
            *lpDrawItem,
            g_hLVImageList,
            g_Settings,
            g_nLVHotItem,
            g_IsMouseOverCloseButton,
            g_rcBtnClose);
    }
}

BOOL ATW_OnNotify(HWND /*hwnd*/, int /*idFrom*/, NMHDR* pnmhdr) {
    if (pnmhdr->hwndFrom == g_hListView && pnmhdr->code == LVN_HOTTRACK) {
        LPNMLISTVIEW pnmListView = reinterpret_cast<LPNMLISTVIEW>(pnmhdr);

        // After adding LVS_OWNERDRAWFIXED to listview, we are getting -1 in iItem
        // So, try to get the item under the cursor position using hit test.
        POINT pt;
        GetCursorPos(&pt);

        const POINT ptScreenPos = pt;

        ScreenToClient(g_hListView, &pt);
        const POINT ptClientPos = pt;

        if (pnmListView->iItem < 0) {
            LVHITTESTINFO ht = { 0 };
            ht.pt = ptClientPos;
            if (ListView_SubItemHitTest(g_hListView, &ht) != -1) {
                pnmListView->iItem = ht.iItem;
            }
        }

        // Compute the hit target for the row currently under the pointer. The
        // rectangle never leaks from the previously hot row.
        if (g_Settings.ShowDeleteButton && pnmListView->iItem >= 0 && g_SwitcherRenderer) {
            RECT rowRect{};
            ListView_GetItemRect(g_hListView, pnmListView->iItem, &rowRect, LVIR_BOUNDS);
            const ThemeMetrics& metrics = g_SwitcherRenderer->Theme().metrics;
            InflateRect(
                &rowRect,
                -MulDiv(4, static_cast<int>(g_SwitcherRenderer->Theme().dpi), 96),
                -MulDiv(2, static_cast<int>(g_SwitcherRenderer->Theme().dpi), 96));
            g_rcBtnClose = {
                rowRect.right - metrics.rowHorizontalPadding - metrics.closeButtonSize,
                rowRect.top + (rowRect.bottom - rowRect.top - metrics.closeButtonSize) / 2,
                rowRect.right - metrics.rowHorizontalPadding,
                rowRect.top + (rowRect.bottom - rowRect.top + metrics.closeButtonSize) / 2,
            };
            if (g_rcBtnClose.left <= ptClientPos.x && ptClientPos.x <= g_rcBtnClose.right
                && g_rcBtnClose.top <= ptClientPos.y && ptClientPos.y <= g_rcBtnClose.bottom) {
                if (!g_IsMouseOverCloseButton) {
                    g_IsMouseOverCloseButton = true;
                    // Invalidate the button area to redraw
                    InvalidateRect(g_hListView, &g_rcBtnClose, FALSE);
                }
            } else {
                if (g_IsMouseOverCloseButton) {
                    g_IsMouseOverCloseButton = false;
                    // Invalidate the button area to redraw
                    InvalidateRect(g_hListView, &g_rcBtnClose, FALSE);
                }
            }
        } else {
            g_IsMouseOverCloseButton = false;
            g_rcBtnClose = {};
        }

        if (pnmListView->iItem != g_nLVHotItem) {
            const int oldItem = g_nLVHotItem;
            g_nLVHotItem = pnmListView->iItem;

            // Invalidate only old + new row
            if (oldItem >= 0) {
                RECT rc;
                ListView_GetItemRect(g_hListView, oldItem, &rc, LVIR_BOUNDS);
                InvalidateRect(g_hListView, &rc, FALSE);
            }

            if (g_nLVHotItem >= 0) {
                RECT rc;
                ListView_GetItemRect(g_hListView, g_nLVHotItem, &rc, LVIR_BOUNDS);
                InvalidateRect(g_hListView, &rc, FALSE);
            }
        }

        // Check if the mouse is hovering over an item
        if (g_Settings.ShowProcessInfoTooltip && pnmListView->iItem >= 0
            && (pnmListView->iItem != g_MouseHoverIndex || !g_TooltipVisible)) {
            g_MouseHoverIndex = pnmListView->iItem;

            HideCustomToolTip();

            // The mouse is over an item
            std::wstring tooltip = std::format(
                L"Title: {}\nPath: {}\nDescription: {} {}\nCompanyName: {}\nPID: {}",
                g_AltTabWindows[g_MouseHoverIndex].Title,
                g_AltTabWindows[g_MouseHoverIndex].FullPath,
                g_AltTabWindows[g_MouseHoverIndex].Description,
                g_AltTabWindows[g_MouseHoverIndex].Version,
                g_AltTabWindows[g_MouseHoverIndex].CompanyName,
                g_AltTabWindows[g_MouseHoverIndex].PID);

            // Add extra information (window handle) in debug mode
#ifdef _DEBUG
            tooltip += std::format(L"\nHWND: 0x{:08X}", (UINT_PTR)g_AltTabWindows[g_MouseHoverIndex].hWnd);
#endif // _DEBUG

            // Show the tooltip just below the item
            RECT itemRect;
            if (ListView_GetItemRect(g_hListView, g_MouseHoverIndex, &itemRect, LVIR_BOUNDS)) {
                pt.x = ptClientPos.x;
                pt.y = itemRect.bottom + 1;
                ClientToScreen(g_hListView, &pt);
                pt.x = ptScreenPos.x + 36;
                ShowCustomToolTipAt(tooltip, pt, -1);
            }
        }
        return TRUE;
    }

    // Check if the single-click event is from your ListView control
    if (pnmhdr->hwndFrom == g_hListView && pnmhdr->code == NM_CLICK) {
        // First check if the mouse is over on close button area
        if (g_IsMouseOverCloseButton) {
            // Close the window of the item under mouse cursor
            if (g_nLVHotItem != -1) {
                ATCloseWindow(g_nLVHotItem);
            }
        } else {
            DestroyAltTabWindow(true);
        }
        return TRUE;
    }

    return FALSE;
}

// ----------------------------------------------------------------------------
// Message handler for about box.
// ----------------------------------------------------------------------------
INT_PTR CALLBACK ATAboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG: {
        HICON hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ALTTAB));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessageW(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

        // Center the dialog on the screen
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        RECT dlgRect;
        GetWindowRect(hDlg, &dlgRect);

        int dlgWidth = dlgRect.right - dlgRect.left;
        int dlgHeight = dlgRect.bottom - dlgRect.top;

        int posX = (screenWidth - dlgWidth) / 2;
        int posY = (screenHeight - dlgHeight) / 2;

        SetWindowPos(hDlg, HWND_TOP, posX, posY, 0, 0, SWP_NOSIZE);
        // Set the dialog as an app window, otherwise not displayed in task bar
        SetWindowLong(hDlg, GWL_EXSTYLE, GetWindowLong(hDlg, GWL_EXSTYLE) | WS_EX_APPWINDOW);

        std::wstring productInfo =
            std::format(L"<a href=\"{}\">{}</a> v{}", AT_PRODUCT_PAGE, AT_PRODUCT_NAMEW, AT_VERSION_TEXTW);
        std::wstring copyright =
            std::format(L"Copyright � {} <a href=\"{}\">{}</a>", AT_PRODUCT_YEARW, AT_PRODUCT_PAGE, AT_AUTHOR_NAME);

        SetDlgItemTextW(hDlg, IDC_SYSLINK_ABOUT_PRODUCT_NAME, productInfo.c_str());
        SetDlgItemTextW(hDlg, IDC_SYSLINK_ABOUT_COPYRIGHT, copyright.c_str());
    }
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;

    case WM_NOTIFY:
        if (wParam == IDC_SYSLINK_ABOUT_PRODUCT_NAME) {
            NMHDR* pnmh = (NMHDR*)lParam;
            if (pnmh->code == NM_CLICK) {
                ShellExecute(nullptr, L"open", AT_PRODUCT_PAGE, nullptr, nullptr, SW_SHOWNORMAL);
            }
        } else if (wParam == IDC_SYSLINK_ABOUT_COPYRIGHT) {
            NMHDR* pnmh = (NMHDR*)lParam;
            if (pnmh->code == NM_CLICK) {
                ShellExecute(nullptr, L"open", AT_AUTHOR_PAGE, nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
        break;

    case WM_DESTROY:
        // Note: the dialog icon is a shared icon from LoadIcon() and must not be
        // passed to DestroyIcon().

        break;
    }
    return (INT_PTR)FALSE;
}

/**
 * \brief Destroy AltTab Window and do necessary cleanup here
 *
 * \param activate   Input parameter to activate the selected window or not
 */
void DestroyAltTabWindow(const bool activate) {
    if (g_hAltTabWnd == nullptr || g_hAltTabIsBeingClosed) {
        return;
    }

    // Check if the Context Menu is visible, then do not close the AltTab window
    // No need to check while releasing the Alt key, but we can ignore that case
    // because first context menu will be
    if (g_hContextMenu != nullptr) {
        DestroyContextMenu();
    }

    AT_LOG_INFO("DestroyAltTabWindow: activate = %d", activate);

    // Set flag to true to avoid re-entry from WM_ACTIVATEAPP
    g_hAltTabIsBeingClosed = true;

    // Hide custom tooltip
    HideCustomToolTip();

    // Kill timer
    KillTimer(g_hMainWnd, TIMER_CHECK_ALT_KEYUP);

    HWND targetWindow = nullptr;
    if (activate) {
        const int selectedInd = ATWListViewGetSelectedItem();
        if (selectedInd >= 0 && selectedInd < static_cast<int>(g_AltTabWindows.size())) {
            targetWindow = g_AltTabWindows[selectedInd].hWnd;
            AT_LOG_INFO("Selected target = [%p], title = [%s]", targetWindow, GetWindowTitleExA(targetWindow).c_str());
        }
    }

    bool targetActivated = false;
    if (targetWindow && IsWindow(targetWindow) && !IsHungAppWindowEx(targetWindow))
        targetActivated = ActivateWindow(targetWindow);

    DestroyWindow(g_hAltTabWnd);

    if (g_idThreadAttachTo) {
        AttachThreadInput(GetCurrentThreadId(), g_idThreadAttachTo, FALSE);
        g_idThreadAttachTo = 0;
    }

    if (activate && targetWindow && !targetActivated)
        AT_LOG_ERROR(
            "Failed to activate selected target [%p]; foreground is [%p]", targetWindow, GetForegroundWindow());

    // CleanUp
    g_hAltTabWnd = nullptr;
    g_IsAltTab = false;
    g_IsAltCtrlTab = false;
    g_IsAltBacktick = false;
    g_SelectedIndex = -1;
    g_nLVHotItem = -1;
    g_MouseHoverIndex = -1;
    g_IsMouseOverCloseButton = false;

    g_AltTabWindows.clear();
    g_SearchString.clear();
    g_AltBacktickWndInfo = {};
    g_hAltTabIsBeingClosed = false;
}

// ----------------------------------------------------------------------------
// Activate window of the given window handle
// ----------------------------------------------------------------------------
bool ActivateWindow(HWND hTargetWnd) {
    AT_LOG_TRACE;

    if (!IsWindow(hTargetWnd))
        return false;

    const auto isTargetForeground = [hTargetWnd]() {
        const HWND foreground = GetForegroundWindow();
        if (!foreground)
            return false;
        if (foreground == hTargetWnd)
            return true;
        const HWND foregroundRoot = GetAncestor(foreground, GA_ROOTOWNER);
        const HWND targetRoot = GetAncestor(hTargetWnd, GA_ROOTOWNER);
        return foregroundRoot && targetRoot && foregroundRoot == targetRoot;
    };

    if (IsIconic(hTargetWnd)) {
        [[maybe_unused]] const BOOL restoreResult = ShowWindow(hTargetWnd, SW_RESTORE);
        AT_LOG_INFO("Restore target [%p]: result = %d", hTargetWnd, restoreResult);
    }

    [[maybe_unused]] const BOOL foregroundResult = SetForegroundWindow(hTargetWnd);
    bool activated = isTargetForeground();
    AT_LOG_INFO(
        "SetForegroundWindow target [%p]: result = %d, foreground = [%p]",
        hTargetWnd,
        foregroundResult,
        GetForegroundWindow());

    if (!activated) {
        const DWORD currentThread = GetCurrentThreadId();
        const DWORD targetThread = GetWindowThreadProcessId(hTargetWnd, nullptr);
        const BOOL attached = targetThread && targetThread != currentThread
                                  ? AttachThreadInput(currentThread, targetThread, TRUE)
                                  : FALSE;
        [[maybe_unused]] const BOOL retryResult = SetForegroundWindow(hTargetWnd);
        activated = isTargetForeground();
        AT_LOG_INFO(
            "Foreground retry target [%p]: attached = %d, result = %d, foreground = [%p]",
            hTargetWnd,
            attached,
            retryResult,
            GetForegroundWindow());
        if (attached)
            AttachThreadInput(currentThread, targetThread, FALSE);
    }

    AT_LOG_INFO(
        "Activation target [%p]: success = %d, final foreground = [%p]", hTargetWnd, activated, GetForegroundWindow());
    return activated;
}

BOOL IsHungAppWindowEx(HWND hwnd) {
    if (g_pfnIsHungAppWindow && g_pfnIsHungAppWindow(hwnd)) {
        std::string title = GetWindowTitleExA(hwnd);
        AT_LOG_INFO("IsHungWnd: [%s]", title.c_str());
        return (TRUE);
    }

    LRESULT lResult = SendMessageTimeoutW(hwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 249, nullptr);
    if (lResult)
        return (FALSE);

    DWORD dwErr = GetLastError();
    return (dwErr == 0 || dwErr == 1460);
}

void ShowHelpWindow() {
    ATShellExecuteEx(L"AltTab.chm");
}

void ShowReadMeWindow() {
    ATShellExecuteEx(L"ReadMe.txt");
}

void ShowReleaseNotesWindow() {
    ATShellExecuteEx(L"ReleaseNotes.txt");
}

/*!
 * Open the file with the default associated program
 *
 * \param fileName   File name
 */
void ATShellExecuteEx(const std::wstring& fileName) {
    std::filesystem::path filePath = GetAppDirPath();
    filePath.append(fileName);

    // Use ShellExecute to open the file with the default associated program
    HINSTANCE hInstance = ShellExecuteW(nullptr, L"open", filePath.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    if ((INT_PTR)hInstance > 32) {
        // ShellExecute returns a value greater than 32 if successful
        AT_LOG_INFO("File opened successfully!");
    } else {
        // Otherwise, it indicates an error
        AT_LOG_ERROR("Failed to open file!");
        LogLastErrorInfo();
    }
}

std::wstring GetAppDirPath() {
    wchar_t szPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(g_hInstance, szPath, MAX_PATH);
    std::filesystem::path dirPath = szPath;
    return dirPath.parent_path().wstring();
}

void LogLastErrorInfo() {
    // Get the last error code
    const DWORD errorCode = GetLastError();

    // Get the error message
    LPVOID errorMessage;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        errorCode,
        0,
        reinterpret_cast<LPWSTR>(&errorMessage),
        0,
        nullptr);
    std::wstring ret = (errorMessage ? reinterpret_cast<LPCWSTR>(errorMessage) : L"");
    AT_LOG_ERROR("  Error Code   : %d", errorCode);
    AT_LOG_ERROR("  Error Message: %s", WStrToUTF8(ret).c_str());
}

// Function to create and show a custom tooltip at the mouse location
void CreateCustomToolTip() {
    AT_LOG_TRACE;
    // Create a tooltip window
    g_hCustomToolTip = CreateWindowExW(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASSW,
        nullptr,
        TTS_NOPREFIX | TTS_ALWAYSTIP,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        g_hInstance,
        nullptr);

    if (!g_hCustomToolTip) {
        AT_LOG_ERROR("Failed to create tooltip window.");
        LogLastErrorInfo();
        return;
    }

    // Initialize members of the toolinfo structure
    g_ToolInfo.cbSize = sizeof(TOOLINFO);
    g_ToolInfo.uFlags = TTF_TRACK;
    g_ToolInfo.hwnd = nullptr;
    g_ToolInfo.hinst = nullptr;
    g_ToolInfo.uId = 0;
    g_ToolInfo.lpszText = (LPWSTR)L"Creating tooltip...";

    // ToolTip control will cover the whole window
    g_ToolInfo.rect.left = 0;
    g_ToolInfo.rect.top = 0;
    g_ToolInfo.rect.right = 0;
    g_ToolInfo.rect.bottom = 0;

    // Send an add tool message to the tooltip control window
    SendMessageW(g_hCustomToolTip, TTM_ADDTOOL, 0, (LPARAM)(LPTOOLINFO)&g_ToolInfo);

    // Enable multiple lines
    SendMessageW(g_hCustomToolTip, TTM_SETMAXTIPWIDTH, 0, MAXINT);

    SendMessageW(g_hCustomToolTip, TTM_SETTIPBKCOLOR, RGB(255, 255, 0), 0);
}

void ShowCustomToolTip(const std::wstring& tooltipText, const int duration /*= 3000*/) {
    AT_LOG_TRACE;
#if 0
    // TODO: Still this is not working properly so going with alternative
    ToolTipInfo* tti = new ToolTipInfo { tooltipText, duration };
    //std::thread tooltipThread(ShowCustomToolTipThread, (LPVOID)&tti);
    //tooltipThread.detach();
    CreateThread(nullptr, 0, ShowCustomToolTipThread, (LPVOID)tti, 0, nullptr);
#else
    if (!g_TooltipVisible) {
        // Get mouse coordinates
        POINT pt;
        GetCursorPos(&pt);

        // Slight offset to avoid covering the mouse pointer
        g_ToolInfo.lpszText = (LPWSTR)(LPCWSTR)tooltipText.c_str();
        SendMessageW(g_hCustomToolTip, TTM_SETTOOLINFO, 0, (LPARAM)&g_ToolInfo);
        SendMessageW(g_hCustomToolTip, TTM_TRACKPOSITION, 0, (LPARAM)(DWORD)MAKELONG(pt.x + 12, pt.y + 12));
        SendMessageW(g_hCustomToolTip, TTM_TRACKACTIVATE, true, (LPARAM)(LPTOOLINFO)&g_ToolInfo);

        g_TooltipTimerId = SetTimer(nullptr, TIMER_CUSTOM_TOOLTIP, duration, HideCustomToolTip);
        g_TooltipVisible = true;
    }
#endif // 0
}

void ShowCustomToolTipAt(const std::wstring& tooltipText, const POINT& pt, const int duration /*= 3000*/) {
    AT_LOG_TRACE;
    if (!g_TooltipVisible) {
        g_ToolInfo.lpszText = (LPWSTR)(LPCWSTR)tooltipText.c_str();
        SendMessageW(g_hCustomToolTip, TTM_SETTOOLINFO, 0, (LPARAM)&g_ToolInfo);
        SendMessageW(g_hCustomToolTip, TTM_TRACKPOSITION, 0, (LPARAM)(DWORD)MAKELONG(pt.x, pt.y));
        SendMessageW(g_hCustomToolTip, TTM_TRACKACTIVATE, true, (LPARAM)(LPTOOLINFO)&g_ToolInfo);

        g_TooltipTimerId = SetTimer(nullptr, TIMER_CUSTOM_TOOLTIP, duration, HideCustomToolTip);
        g_TooltipVisible = true;
    }
}

void CALLBACK HideCustomToolTip(HWND /*hWnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) {
    AT_LOG_TRACE;
    KillTimer(nullptr, g_TooltipTimerId);
    SendMessageW(g_hCustomToolTip, TTM_TRACKACTIVATE, false, (LPARAM)(LPTOOLINFO)&g_ToolInfo);
    g_TooltipVisible = false;
}

int ATMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    // Set flag to ignore WM_ACTIVATE message temporarily since we are going to show a message box now.
    // So, the AltTab window should not be closed when the message box is shown.
    g_bIgnoreWM_ACTIVATE = true;

    const int result = MessageBoxW(hWnd, lpText, lpCaption, uType);

    g_bIgnoreWM_ACTIVATE = false;

    return result;
}

void DestroyContextMenu() {
    if (g_hContextMenu) {
        AT_LOG_INFO("g_hContextMenu: %p", g_hContextMenu);

        // Get active window and send VK_ESCAPE to it to close the context menu.
        HWND hActiveWnd = GetForegroundWindow();
        AT_LOG_INFO("Sending WM_CLOSE to context menu window: %#010x", (UINT_PTR)hActiveWnd);
        PostMessageW(hActiveWnd, WM_KEYDOWN, VK_ESCAPE, 0);

        // The active ThemedMenuSession owns the HMENU. TrackPopupMenuEx will
        // return after Escape and the session will destroy it exactly once.
        g_hContextMenu = nullptr;
    }
}
