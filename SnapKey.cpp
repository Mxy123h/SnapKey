// SnapKey 1.2.9
// github.com/cafali/SnapKey

#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <regex>
#include <vector>
#include <filesystem>
#include <array>
#include <random>

using namespace std;
namespace fs = std::filesystem;

#define ID_TRAY_APP_ICON                1001
#define ID_TRAY_EXIT_CONTEXT_MENU_ITEM  3000
#define ID_TRAY_VERSION_INFO            3001
#define ID_TRAY_REBIND_KEYS             3002
#define ID_TRAY_LOCK_FUNCTION           3003
#define ID_TRAY_RESTART_SNAPKEY         3004
#define ID_TRAY_HELP                    3005
#define ID_TRAY_CHECKUPDATE             3006
#define ID_TRAY_LAYOUTS                 3007
#define ID_TRAY_STATUS                  3008
#define ID_TRAY_SHOW_WINDOW             3009
#define WM_TRAYICON                     (WM_USER + 1)
#define ID_LAYOUT_BASE                  4000 // 1.2.9
#define ID_UI_STATUS_LABEL              5001
#define ID_UI_PROFILE_COMBO             5002
#define ID_UI_APPLY_PROFILE             5003
#define ID_UI_TOGGLE_LOCK               5004
#define ID_UI_EDIT_CONFIG               5005
#define ID_UI_RESTART                   5006
#define ID_UI_HELP                      5007
#define ID_UI_ABOUT                     5008
#define ID_UI_EXIT                      5009
#define ID_UI_KEY1_COMBO                5010
#define ID_UI_KEY2_COMBO                5011
#define ID_UI_KEY3_COMBO                5012
#define ID_UI_KEY4_COMBO                5013
#define ID_UI_SAVE_KEYS                 5014
#define ID_UI_OPEN_CONFIG_FILE          5015
#define ID_UI_DELAY_MIN_EDIT            5016
#define ID_UI_DELAY_MAX_EDIT            5017

struct KeyState {
    bool registered = false;
    bool keyDown = false;
    int group;
    bool simulated = false;
};

struct GroupState {
    int previousKey;
    int activeKey;
};

unordered_map<int, GroupState> GroupInfo;
unordered_map<int, KeyState> KeyInfo;

HHOOK hHook = NULL;
HANDLE hMutex = NULL;
NOTIFYICONDATAW nid;
HWND hMainWindow = NULL;
HWND hStatusLabel = NULL;
HWND hProfileCombo = NULL;
HWND hToggleButton = NULL;
HWND hKeyCombos[4] = { NULL, NULL, NULL, NULL };
HWND hDelayMinEdit = NULL;
HWND hDelayMaxEdit = NULL;
HFONT hUiFont = NULL;
bool isLocked = false;
int releaseDelayMinMs = 1;
int releaseDelayMaxMs = 8;

struct KeyOption {
    int code;
    const wchar_t* label;
};

const vector<KeyOption> KEY_OPTIONS = {
    {65, L"A"}, {66, L"B"}, {67, L"C"}, {68, L"D"}, {69, L"E"}, {70, L"F"},
    {71, L"G"}, {72, L"H"}, {73, L"I"}, {74, L"J"}, {75, L"K"}, {76, L"L"},
    {77, L"M"}, {78, L"N"}, {79, L"O"}, {80, L"P"}, {81, L"Q"}, {82, L"R"},
    {83, L"S"}, {84, L"T"}, {85, L"U"}, {86, L"V"}, {87, L"W"}, {88, L"X"},
    {89, L"Y"}, {90, L"Z"},
    {38, L"方向键 上"}, {40, L"方向键 下"}, {37, L"方向键 左"}, {39, L"方向键 右"},
    {32, L"空格"}, {27, L"ESC"}, {8, L"退格"}, {46, L"DEL"},
    {160, L"左 Shift"}, {161, L"右 Shift"}, {162, L"左 Ctrl"}, {163, L"右 Ctrl"},
    {164, L"Alt"},
    {96, L"小键盘 0"}, {97, L"小键盘 1"}, {98, L"小键盘 2"}, {99, L"小键盘 3"},
    {100, L"小键盘 4"}, {101, L"小键盘 5"}, {102, L"小键盘 6"}, {103, L"小键盘 7"},
    {104, L"小键盘 8"}, {105, L"小键盘 9"}
};

// Forward declarations
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void InitNotifyIconData(HWND hwnd);
bool LoadConfig(const std::string& filename);
void CreateDefaultConfig(const std::string& filename);
void RestoreConfigFromBackup(const std::string& backupFilename, const std::string& destinationFilename);
std::string GetVersionInfo();
void SendKey(int target, bool keyDown);
void SleepRandomReleaseDelay();
void NormalizeReleaseDelayConfig();
void UpdateTrayIconState();
void ToggleSnapKeyState();
void OpenHelpDocument();
std::wstring GetLayoutDisplayName(const std::string& layoutName);
std::wstring GetVersionInfoText();
void CreateMainWindowControls(HWND hwnd);
void RefreshMainWindowControls();
void ShowMainWindow(HWND hwnd);
void FillProfileCombo(HWND comboBox);
void ApplySelectedProfile(HWND hwnd);
void FillKeyCombo(HWND comboBox, int selectedKeyCode);
std::array<int, 4> ReadConfigKeyValues();
bool SaveConfigKeyValues(const std::array<int, 4>& keys);
void RefreshKeyEditorControls();
bool ReadDelayEditorValues(HWND hwnd, int& minMs, int& maxMs);
void SaveKeysFromEditor(HWND hwnd);

// select layout via context menu v 1.2.9
vector<string> ListLayouts() {
    vector<string> layouts;
    string path = "meta\\profiles";
    if (!fs::exists(path)) return layouts;

    for (auto& entry : fs::directory_iterator(path)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".cfg") {
                layouts.push_back(entry.path().stem().string()); // ignore file extension
            }
        }
    }
    return layouts;
}

// apply layout (replace config.cfg content) v 1.2.9
void ApplyLayout(const string& layoutName) {
    string sourcePath = "meta\\profiles\\" + layoutName + ".cfg";
    string destPath = "config.cfg";

    ifstream src(sourcePath, ios::binary);
    ofstream dst(destPath, ios::binary | ios::trunc);

    if (!src.is_open() || !dst.is_open()) {
        MessageBoxW(NULL, L"无法应用该配置方案，请检查配置文件是否存在或是否被占用。",
                    L"SnapKey 错误", MB_ICONERROR | MB_OK);
        return;
    }

    dst << src.rdbuf(); // copy file contents
}

std::array<int, 4> ReadConfigKeyValues() {
    std::array<int, 4> keys = {65, 68, 83, 87};
    std::ifstream configFile("config.cfg");
    if (!configFile.is_open()) {
        return keys;
    }

    string line;
    int keyIndex = 0;
    while (getline(configFile, line) && keyIndex < 4) {
        istringstream iss(line);
        string key;
        int value;
        if (getline(iss, key, '=') && (iss >> value) && key.find("key") != string::npos) {
            keys[keyIndex] = value;
            keyIndex++;
        }
    }

    return keys;
}

bool SaveConfigKeyValues(const std::array<int, 4>& keys) {
    std::ofstream configFile("config.cfg", ios::trunc);
    if (!configFile.is_open()) {
        return false;
    }

    configFile << "[Group]\n";
    configFile << "key1=" << keys[0] << "\n";
    configFile << "key2=" << keys[1] << "\n\n";
    configFile << "[Group]\n";
    configFile << "key3=" << keys[2] << "\n";
    configFile << "key4=" << keys[3] << "\n\n";
    configFile << "[Settings]\n";
    configFile << "releaseDelayMinMs=" << releaseDelayMinMs << "\n";
    configFile << "releaseDelayMaxMs=" << releaseDelayMaxMs << "\n\n\n";
    configFile << "# 修改并保存后，请在界面点击“重启并应用”。\n";
    configFile << "# 也可以继续使用界面中的按键编辑区调整这四个按键。\n";
    configFile << "# releaseDelayMinMs / releaseDelayMaxMs：同组新按键接管旧按键时，释放旧按键前的随机延迟范围（毫秒）。\n";
    configFile << "# 更多按键绑定说明：https://github.com/cafali/SnapKey/wiki/Rebinding-Keys\n\n";
    configFile << "# 常用默认方案：\n";
    configFile << "# AZERTY: key1=81 key2=68 / key3=90 key4=83\n";
    configFile << "# QWERTY: key1=65 key2=68 / key3=83 key4=87\n";
    configFile << "# 方向键: key1=38 key2=40 / key3=37 key4=39\n";

    return true;
}

// restart
void RestartSnapKey() {
    WCHAR szExeFileName[MAX_PATH];
    GetModuleFileNameW(NULL, szExeFileName, MAX_PATH);
    ShellExecuteW(NULL, NULL, szExeFileName, NULL, NULL, SW_SHOWNORMAL);
    PostQuitMessage(0);
}

// Main entry
int main() {
    if (!LoadConfig("config.cfg")) {
        return 1;
    }

    hMutex = CreateMutexW(NULL, TRUE, L"SnapKeyMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"SnapKey 已在运行，无需重复启动。", L"SnapKey", MB_ICONINFORMATION | MB_OK);
        return 1;
    }

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SnapKeyClass";

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"窗口注册失败，请尝试重新启动 SnapKey。", L"SnapKey 错误", MB_ICONEXCLAMATION | MB_OK);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"SnapKey 中文版", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 560, 580,
                                NULL, NULL, wc.hInstance, NULL);

    if (hwnd == NULL) {
        MessageBoxW(NULL, L"窗口创建失败，请尝试重新启动 SnapKey。", L"SnapKey 错误", MB_ICONEXCLAMATION | MB_OK);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;
    }

    hMainWindow = hwnd;
    InitNotifyIconData(hwnd);
    ShowMainWindow(hwnd);

    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    if (hHook == NULL) {
        MessageBoxW(NULL, L"键盘监听安装失败，请以普通方式重新启动，或检查安全软件拦截。", L"SnapKey 错误", MB_ICONEXCLAMATION | MB_OK);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hHook);
    Shell_NotifyIconW(NIM_DELETE, &nid);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return 0;
}

// Key handling
void handleKeyDown(int keyCode) {
    KeyState& currentKeyInfo = KeyInfo[keyCode];
    GroupState& currentGroupInfo = GroupInfo[currentKeyInfo.group];
    if (!currentKeyInfo.keyDown) {
        currentKeyInfo.keyDown = true;
        SendKey(keyCode, true);
        if (currentGroupInfo.activeKey == 0 || currentGroupInfo.activeKey == keyCode) {
            currentGroupInfo.activeKey = keyCode;
        } else {
            currentGroupInfo.previousKey = currentGroupInfo.activeKey;
            currentGroupInfo.activeKey = keyCode;
            SleepRandomReleaseDelay();
            SendKey(currentGroupInfo.previousKey, false);
        }
    }
}

void handleKeyUp(int keyCode) {
    KeyState& currentKeyInfo = KeyInfo[keyCode];
    GroupState& currentGroupInfo = GroupInfo[currentKeyInfo.group];
    if (currentGroupInfo.previousKey == keyCode && !currentKeyInfo.keyDown) {
        currentGroupInfo.previousKey = 0;
    }
    if (currentKeyInfo.keyDown) {
        currentKeyInfo.keyDown = false;
        if (currentGroupInfo.activeKey == keyCode && currentGroupInfo.previousKey != 0) {
            SendKey(keyCode, false);
            currentGroupInfo.activeKey = currentGroupInfo.previousKey;
            currentGroupInfo.previousKey = 0;
            SendKey(currentGroupInfo.activeKey, true);
        } else {
            currentGroupInfo.previousKey = 0;
            if (currentGroupInfo.activeKey == keyCode) currentGroupInfo.activeKey = 0;
            SendKey(keyCode, false);
        }
    }
}

bool isSimulatedKeyEvent(DWORD flags) { return flags & 0x10; }

void SleepRandomReleaseDelay() {
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution(releaseDelayMinMs, releaseDelayMaxMs);
    int delayMs = distribution(generator);
    if (delayMs <= 0) {
        return;
    }

    static LARGE_INTEGER frequency = []() {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        return value;
    }();
    LARGE_INTEGER start;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&start);

    LONGLONG targetTicks = (frequency.QuadPart * delayMs) / 1000;
    do {
        QueryPerformanceCounter(&now);
    } while (now.QuadPart - start.QuadPart < targetTicks);
}

void NormalizeReleaseDelayConfig() {
    if (releaseDelayMinMs < 0) releaseDelayMinMs = 0;
    if (releaseDelayMaxMs < 0) releaseDelayMaxMs = 0;
    if (releaseDelayMinMs > 1000) releaseDelayMinMs = 1000;
    if (releaseDelayMaxMs > 1000) releaseDelayMaxMs = 1000;
    if (releaseDelayMinMs > releaseDelayMaxMs) {
        int temp = releaseDelayMinMs;
        releaseDelayMinMs = releaseDelayMaxMs;
        releaseDelayMaxMs = temp;
    }
}

void SendKey(int targetKey, bool keyDown) {
    INPUT input = {0};
    input.ki.wVk = targetKey;
    input.ki.wScan = MapVirtualKey(targetKey, 0);
    input.type = INPUT_KEYBOARD;

    DWORD flags = KEYEVENTF_SCANCODE;
    input.ki.dwFlags = keyDown ? flags : flags | KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (!isLocked && nCode >= 0) {
        KBDLLHOOKSTRUCT *pKeyBoard = (KBDLLHOOKSTRUCT *)lParam;
        if (!isSimulatedKeyEvent(pKeyBoard->flags)) {
            if (KeyInfo[pKeyBoard->vkCode].registered) {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) handleKeyDown(pKeyBoard->vkCode);
                if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) handleKeyUp(pKeyBoard->vkCode);
                return 1;
            }
        }
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

void InitNotifyIconData(HWND hwnd) {
    memset(&nid, 0, sizeof(NOTIFYICONDATAW));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;

    HICON hIcon = (HICON)LoadImageW(NULL, L"icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    nid.hIcon = hIcon ? hIcon : LoadIcon(NULL, IDI_APPLICATION);
    lstrcpynW(nid.szTip, L"SnapKey - 已启用（右键打开菜单，双击暂停）", ARRAYSIZE(nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void UpdateTrayIconState() {
    HICON hIcon = isLocked
        ? (HICON)LoadImageW(NULL, L"icon_off.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE)
        : (HICON)LoadImageW(NULL, L"icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);

    if (hIcon) {
        nid.hIcon = hIcon;
    }

    lstrcpynW(
        nid.szTip,
        isLocked ? L"SnapKey - 已暂停（双击恢复）" : L"SnapKey - 已启用（右键打开菜单，双击暂停）",
        ARRAYSIZE(nid.szTip)
    );
    Shell_NotifyIconW(NIM_MODIFY, &nid);

    if (hIcon) {
        DestroyIcon(hIcon);
    }
}

void ToggleSnapKeyState() {
    isLocked = !isLocked;
    UpdateTrayIconState();
    RefreshMainWindowControls();
}

void FillProfileCombo(HWND comboBox) {
    SendMessageW(comboBox, CB_RESETCONTENT, 0, 0);

    vector<string> layouts = ListLayouts();
    for (auto& layout : layouts) {
        wstring label = GetLayoutDisplayName(layout);
        SendMessageW(comboBox, CB_ADDSTRING, 0, (LPARAM)label.c_str());
    }

    if (!layouts.empty()) {
        SendMessageW(comboBox, CB_SETCURSEL, 0, 0);
    }
}

void FillKeyCombo(HWND comboBox, int selectedKeyCode) {
    SendMessageW(comboBox, CB_RESETCONTENT, 0, 0);

    int selectedIndex = 0;
    for (int i = 0; i < (int)KEY_OPTIONS.size(); i++) {
        wstringstream label;
        label << KEY_OPTIONS[i].label << L" (" << KEY_OPTIONS[i].code << L")";
        SendMessageW(comboBox, CB_ADDSTRING, 0, (LPARAM)label.str().c_str());
        SendMessageW(comboBox, CB_SETITEMDATA, i, KEY_OPTIONS[i].code);
        if (KEY_OPTIONS[i].code == selectedKeyCode) {
            selectedIndex = i;
        }
    }

    SendMessageW(comboBox, CB_SETCURSEL, selectedIndex, 0);
}

void RefreshKeyEditorControls() {
    std::array<int, 4> keys = ReadConfigKeyValues();
    for (int i = 0; i < 4; i++) {
        if (hKeyCombos[i]) {
            FillKeyCombo(hKeyCombos[i], keys[i]);
        }
    }

    if (hDelayMinEdit) {
        SetWindowTextW(hDelayMinEdit, std::to_wstring(releaseDelayMinMs).c_str());
    }
    if (hDelayMaxEdit) {
        SetWindowTextW(hDelayMaxEdit, std::to_wstring(releaseDelayMaxMs).c_str());
    }
}

void RefreshMainWindowControls() {
    if (hStatusLabel) {
        SetWindowTextW(hStatusLabel, isLocked ? L"当前状态：已暂停，按键处理暂不生效" : L"当前状态：已启用，按键处理正在运行");
    }

    if (hToggleButton) {
        SetWindowTextW(hToggleButton, isLocked ? L"启用 SnapKey" : L"暂停 SnapKey");
    }

    if (hProfileCombo && SendMessageW(hProfileCombo, CB_GETCOUNT, 0, 0) == 0) {
        FillProfileCombo(hProfileCombo);
    }
}

bool ReadDelayEditorValues(HWND hwnd, int& minMs, int& maxMs) {
    wchar_t minText[16] = {0};
    wchar_t maxText[16] = {0};
    GetWindowTextW(hDelayMinEdit, minText, ARRAYSIZE(minText));
    GetWindowTextW(hDelayMaxEdit, maxText, ARRAYSIZE(maxText));

    wchar_t* minEnd = nullptr;
    wchar_t* maxEnd = nullptr;
    long parsedMin = wcstol(minText, &minEnd, 10);
    long parsedMax = wcstol(maxText, &maxEnd, 10);

    if (minText[0] == L'\0' || maxText[0] == L'\0' || *minEnd != L'\0' || *maxEnd != L'\0') {
        MessageBoxW(hwnd, L"随机延迟请输入 0 到 1000 之间的整数。", L"SnapKey 配置提示", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    if (parsedMin < 0 || parsedMax < 0 || parsedMin > 1000 || parsedMax > 1000) {
        MessageBoxW(hwnd, L"随机延迟范围必须在 0 到 1000 毫秒之间。", L"SnapKey 配置提示", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    if (parsedMin > parsedMax) {
        MessageBoxW(hwnd, L"随机延迟最小值不能大于最大值。", L"SnapKey 配置提示", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    minMs = (int)parsedMin;
    maxMs = (int)parsedMax;
    return true;
}

void SaveKeysFromEditor(HWND hwnd) {
    std::array<int, 4> keys = {65, 68, 83, 87};

    for (int i = 0; i < 4; i++) {
        int selectedIndex = (int)SendMessageW(hKeyCombos[i], CB_GETCURSEL, 0, 0);
        if (selectedIndex < 0) {
            MessageBoxW(hwnd, L"请把四个按键都选择完整。", L"SnapKey", MB_ICONINFORMATION | MB_OK);
            return;
        }
        keys[i] = (int)SendMessageW(hKeyCombos[i], CB_GETITEMDATA, selectedIndex, 0);
    }

    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (keys[i] == keys[j]) {
                MessageBoxW(hwnd, L"四个按键不能重复，请重新选择。", L"SnapKey 配置提示", MB_ICONEXCLAMATION | MB_OK);
                return;
            }
        }
    }

    int newDelayMinMs = releaseDelayMinMs;
    int newDelayMaxMs = releaseDelayMaxMs;
    if (!ReadDelayEditorValues(hwnd, newDelayMinMs, newDelayMaxMs)) {
        return;
    }

    releaseDelayMinMs = newDelayMinMs;
    releaseDelayMaxMs = newDelayMaxMs;

    if (!SaveConfigKeyValues(keys)) {
        MessageBoxW(hwnd, L"保存配置失败，请检查 config.cfg 是否被占用。", L"SnapKey 错误", MB_ICONERROR | MB_OK);
        return;
    }

    if (MessageBoxW(hwnd, L"配置已保存。需要重启 SnapKey 后才会生效，是否现在重启？",
                    L"保存成功", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        RestartSnapKey();
    }
}

void CreateMainWindowControls(HWND hwnd) {
    hUiFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    auto createText = [&](const wchar_t* text, int x, int y, int width, int height, int id = 0) -> HWND {
        HWND control = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                                      x, y, width, height, hwnd, (HMENU)(INT_PTR)id, NULL, NULL);
        SendMessageW(control, WM_SETFONT, (WPARAM)hUiFont, TRUE);
        return control;
    };

    auto createButton = [&](const wchar_t* text, int x, int y, int width, int height, int id) -> HWND {
        HWND control = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                      x, y, width, height, hwnd, (HMENU)(INT_PTR)id, NULL, NULL);
        SendMessageW(control, WM_SETFONT, (WPARAM)hUiFont, TRUE);
        return control;
    };

    auto createEdit = [&](int x, int y, int width, int height, int id) -> HWND {
        HWND control = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
                                      x, y, width, height, hwnd, (HMENU)(INT_PTR)id, NULL, NULL);
        SendMessageW(control, WM_SETFONT, (WPARAM)hUiFont, TRUE);
        SendMessageW(control, EM_SETLIMITTEXT, 4, 0);
        return control;
    };

    createText(L"SnapKey 中文版", 24, 20, 300, 28);
    createText(L"用于管理按键互斥、配置方案和运行状态。关闭窗口后程序仍会留在托盘运行。", 24, 52, 450, 24);

    hStatusLabel = createText(L"", 24, 92, 440, 24, ID_UI_STATUS_LABEL);
    hToggleButton = createButton(L"", 24, 126, 140, 36, ID_UI_TOGGLE_LOCK);
    createButton(L"打开配置文件", 178, 126, 140, 36, ID_UI_OPEN_CONFIG_FILE);
    createButton(L"重启并应用", 332, 126, 140, 36, ID_UI_RESTART);

    createText(L"配置方案", 24, 184, 100, 24);
    hProfileCombo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                    104, 180, 214, 180, hwnd, (HMENU)(INT_PTR)ID_UI_PROFILE_COMBO, NULL, NULL);
    SendMessageW(hProfileCombo, WM_SETFONT, (WPARAM)hUiFont, TRUE);
    createButton(L"应用方案", 332, 178, 140, 36, ID_UI_APPLY_PROFILE);

    createText(L"界面化编辑按键", 24, 238, 160, 24);
    createText(L"第一组按键", 24, 272, 90, 24);
    hKeyCombos[0] = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                    104, 268, 150, 180, hwnd, (HMENU)(INT_PTR)ID_UI_KEY1_COMBO, NULL, NULL);
    hKeyCombos[1] = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                    270, 268, 150, 180, hwnd, (HMENU)(INT_PTR)ID_UI_KEY2_COMBO, NULL, NULL);
    createText(L"第二组按键", 24, 314, 90, 24);
    hKeyCombos[2] = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                    104, 310, 150, 180, hwnd, (HMENU)(INT_PTR)ID_UI_KEY3_COMBO, NULL, NULL);
    hKeyCombos[3] = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                    270, 310, 150, 180, hwnd, (HMENU)(INT_PTR)ID_UI_KEY4_COMBO, NULL, NULL);

    for (int i = 0; i < 4; i++) {
        SendMessageW(hKeyCombos[i], WM_SETFONT, (WPARAM)hUiFont, TRUE);
    }

    createText(L"随机延迟", 24, 356, 90, 24);
    hDelayMinEdit = createEdit(104, 352, 70, 26, ID_UI_DELAY_MIN_EDIT);
    createText(L"到", 186, 356, 24, 24);
    hDelayMaxEdit = createEdit(214, 352, 70, 26, ID_UI_DELAY_MAX_EDIT);
    createText(L"毫秒", 296, 356, 60, 24);

    createButton(L"保存配置", 104, 398, 150, 34, ID_UI_SAVE_KEYS);
    createText(L"保存后需要重启才会生效。随机延迟只作用于同组新按键接管旧按键。", 24, 448, 500, 24);

    createButton(L"使用说明", 24, 492, 104, 34, ID_UI_HELP);
    createButton(L"关于", 142, 492, 104, 34, ID_UI_ABOUT);
    createButton(L"退出程序", 368, 492, 104, 34, ID_UI_EXIT);

    FillProfileCombo(hProfileCombo);
    RefreshKeyEditorControls();
    RefreshMainWindowControls();
}

void ShowMainWindow(HWND hwnd) {
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    RefreshMainWindowControls();
}

void ApplySelectedProfile(HWND hwnd) {
    int selectedIndex = (int)SendMessageW(hProfileCombo, CB_GETCURSEL, 0, 0);
    vector<string> layouts = ListLayouts();
    if (selectedIndex < 0 || selectedIndex >= (int)layouts.size()) {
        MessageBoxW(hwnd, L"请先选择一个配置方案。", L"SnapKey", MB_ICONINFORMATION | MB_OK);
        return;
    }

    if (MessageBoxW(hwnd, L"应用方案后会自动重启 SnapKey，是否继续？", L"应用配置方案",
                    MB_YESNO | MB_ICONQUESTION) == IDYES) {
        ApplyLayout(layouts[selectedIndex]);
        RestartSnapKey();
    }
}

std::wstring GetLayoutDisplayName(const std::string& layoutName) {
    static const unordered_map<string, wstring> layoutLabels = {
        {"ARROW Keys", L"方向键方案"},
        {"AZERTY Layout", L"AZERTY 键盘方案"},
        {"CUSTOM Profile", L"自定义方案"},
        {"ESDF Keys", L"ESDF 方案"},
        {"WASD Keys", L"WASD 默认方案"}
    };

    auto found = layoutLabels.find(layoutName);
    if (found != layoutLabels.end()) {
        return found->second;
    }

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, layoutName.c_str(), -1, NULL, 0);
    if (sizeNeeded <= 0) {
        return L"未命名方案";
    }

    wstring result(sizeNeeded, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, layoutName.c_str(), -1, &result[0], sizeNeeded);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

void OpenHelpDocument() {
    if (fs::exists("README.md")) {
        ShellExecuteW(NULL, L"open", L"README.md", NULL, NULL, SW_SHOWNORMAL);
        return;
    }

    ShellExecuteW(NULL, L"open", L"README.pdf", NULL, NULL, SW_SHOWNORMAL);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateMainWindowControls(hwnd);
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONDOWN) {
            POINT curPoint;
            GetCursorPos(&curPoint);
            SetForegroundWindow(hwnd);

            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_GRAYED, ID_TRAY_STATUS, isLocked ? L"当前状态：已暂停" : L"当前状态：已启用");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW_WINDOW, L"打开主界面");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_REBIND_KEYS, L"编辑按键配置");

            // 配置方案只改变 config.cfg，不改按键处理逻辑。
            HMENU hSubMenu = CreatePopupMenu();
            vector<string> layouts = ListLayouts();
            if (!layouts.empty()) {
                int id = 0;
                for (auto& layout : layouts) {
                    wstring label = GetLayoutDisplayName(layout);
                    AppendMenuW(hSubMenu, MF_STRING, ID_LAYOUT_BASE + id, label.c_str());
                    id++;
                }
            } else {
                AppendMenuW(hSubMenu, MF_GRAYED, 0, L"未找到配置方案");
            }
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, L"选择配置方案");

            AppendMenuW(hMenu, MF_STRING, ID_TRAY_RESTART_SNAPKEY, L"重启并应用配置");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_LOCK_FUNCTION, isLocked ? L"启用 SnapKey" : L"暂停 SnapKey");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_HELP, L"使用说明");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_CHECKUPDATE, L"检查更新");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_VERSION_INFO, L"关于 SnapKey");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT_CONTEXT_MENU_ITEM, L"退出 SnapKey");

            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, curPoint.x, curPoint.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        else if (lParam == WM_LBUTTONDBLCLK) {
            ShowMainWindow(hwnd);
        }
        break;

    case WM_COMMAND:
        {
            int commandId = LOWORD(wParam);
            vector<string> layouts = ListLayouts();
            int layoutIndex = commandId - ID_LAYOUT_BASE;

            if (layoutIndex >= 0 && layoutIndex < (int)layouts.size()) {
                ApplyLayout(layouts[layoutIndex]);
                RestartSnapKey(); // restart after applying layout
                break;
            }

            switch (commandId) {
            case ID_TRAY_SHOW_WINDOW:
                ShowMainWindow(hwnd);
                break;
            case ID_TRAY_EXIT_CONTEXT_MENU_ITEM:
            case ID_UI_EXIT:
                PostQuitMessage(0);
                break;
            case ID_TRAY_VERSION_INFO:
            case ID_UI_ABOUT:
                MessageBoxW(hwnd, GetVersionInfoText().c_str(), L"关于 SnapKey", MB_OK | MB_ICONINFORMATION);
                break;
            case ID_TRAY_REBIND_KEYS:
            case ID_UI_EDIT_CONFIG:
                ShowMainWindow(hwnd);
                break;
            case ID_UI_OPEN_CONFIG_FILE:
                ShellExecuteW(NULL, L"open", L"config.cfg", NULL, NULL, SW_SHOWNORMAL);
                break;
            case ID_TRAY_HELP:
            case ID_UI_HELP:
                OpenHelpDocument();
                break;
            case ID_TRAY_CHECKUPDATE:
                if (MessageBoxW(NULL,
                               L"即将打开 SnapKey 发布页面，用于查看新版本。是否继续？",
                               L"检查更新",
                               MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    ShellExecuteW(NULL, L"open", L"https://github.com/cafali/SnapKey/releases", NULL, NULL, SW_SHOWNORMAL);
                }
                break;
            case ID_TRAY_RESTART_SNAPKEY:
            case ID_UI_RESTART:
                RestartSnapKey();
                break;
            case ID_TRAY_LOCK_FUNCTION:
            case ID_UI_TOGGLE_LOCK:
                ToggleSnapKeyState();
                break;
            case ID_UI_APPLY_PROFILE:
                ApplySelectedProfile(hwnd);
                break;
            case ID_UI_SAVE_KEYS:
                SaveKeysFromEditor(hwnd);
                break;
            }
        }
        break;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

std::string GetVersionInfo() {
    return "SnapKey v1.2.9 (R18)\n"
           "Version Date: August 8, 2025\n"
           "Repository: github.com/cafali/SnapKey\n"
           "License: MIT License\n";
}

std::wstring GetVersionInfoText() {
    return L"SnapKey v1.2.9 (R18)\n"
           L"版本日期：2025 年 8 月 8 日\n"
           L"项目地址：github.com/cafali/SnapKey\n"
           L"许可证：MIT License\n\n"
           L"操作提示：\n"
           L"- 右键托盘图标：打开菜单\n"
           L"- 双击托盘图标：暂停 / 启用\n"
           L"- 修改 config.cfg 后，请重启 SnapKey 生效\n";
}

void RestoreConfigFromBackup(const std::string& backupFilename, const std::string& destinationFilename) {
    std::string sourcePath = "meta\\" + backupFilename;
    std::string destinationPath = destinationFilename;

    if (CopyFile(sourcePath.c_str(), destinationPath.c_str(), FALSE)) {
        MessageBoxW(NULL, L"已从备份恢复默认配置。请重新启动 SnapKey。", L"SnapKey", MB_ICONINFORMATION | MB_OK);
    } else {
        MessageBoxW(NULL, L"无法从备份恢复默认配置，请检查 meta 目录是否完整。", L"SnapKey 错误", MB_ICONERROR | MB_OK);
    }
}

void CreateDefaultConfig(const std::string& filename) {
    RestoreConfigFromBackup("backup.snapkey", filename);
}

bool LoadConfig(const std::string& filename) {
    std::ifstream configFile(filename);
    if (!configFile.is_open()) {
        CreateDefaultConfig(filename);
        return false;
    }

    string line;
    int id = 0;
    while (getline(configFile, line)) {
        size_t firstChar = line.find_first_not_of(" \t\r\n");
        if (firstChar == string::npos || line[firstChar] == '#') {
            continue;
        }

        istringstream iss(line);
        string key;
        int value;
        regex secPat(R"(\s*\[Group\]\s*)");
        if (regex_match(line, secPat)) {
            id++;
        } else if (getline(iss, key, '=') && (iss >> value)) {
            if (key == "releaseDelayMinMs") {
                releaseDelayMinMs = value;
            } else if (key == "releaseDelayMaxMs") {
                releaseDelayMaxMs = value;
            } else if (key.find("key") != string::npos) {
                if (!KeyInfo[value].registered) {
                    KeyInfo[value].registered = true;
                    KeyInfo[value].group = id;
                } else {
                    MessageBoxW(NULL,
                               L"配置文件中存在重复按键，请检查 config.cfg 中的 key 数值。",
                               L"SnapKey 配置错误", MB_ICONEXCLAMATION | MB_OK);
                    return false;
                }
            }
        }
    }
    NormalizeReleaseDelayConfig();
    return true;
}
