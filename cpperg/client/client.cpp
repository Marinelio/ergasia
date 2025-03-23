#include <enet/enet.h>
#include <iostream>
#include <string>
#include <conio.h>      // For keyboard input functions (_kbhit, _getch)
#include <windows.h>    // For Sleep function
#include <thread> // For thread
#include <ctime>
#include "keylogger.hpp"

using namespace std;

// ---------------------------------------------------------------------
// Display the main menu options to the user
// ---------------------------------------------------------------------
void displayMenu() {
    cout << "\n=== Authentication System ===" << endl;
    cout << "1. Login" << endl;
    cout << "2. Register" << endl;
    cout << "3. Exit" << endl;
    cout << "Enter your choice: ";
}

// ---------------------------------------------------------------------
// Send a message to the server and wait for a response
// Parameters:
//   - peer: Connection to the server
//   - message: Text to send to server
//   - client: Local network interface
// Returns: Server's response as a string
// ---------------------------------------------------------------------
string sendMessage(ENetPeer* peer, const string& message, ENetHost* client) {
    // Create a packet with the message
    ENetPacket* packet = enet_packet_create(
        message.c_str(),           // Message content
        message.size() + 1,        // Message length (include null terminator)
        ENET_PACKET_FLAG_RELIABLE  // Ensure delivery
    );
    
    // Send the packet to the server
    enet_peer_send(peer, 0, packet);
    enet_host_flush(client);  // Force immediate sending

    // Wait for server response (up to 5 seconds)
    ENetEvent event;
    while (enet_host_service(client, &event, 5000) > 0) {
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            // Convert received data to string
            string response = reinterpret_cast<char*>(event.packet->data);
            enet_packet_destroy(event.packet);
            return response;
        }
    }
    
    // Timeout - no response received
    return "ERROR: No response from server";
}

// ---------------------------------------------------------------------
// Disconnect cleanly from the server
// ---------------------------------------------------------------------
void disconnectFromServer(ENetPeer* peer, ENetHost* client) {
    if (peer) {
        // Request disconnection
        enet_peer_disconnect(peer, 0);
        
        // Wait for server to acknowledge (up to 3 seconds)
        ENetEvent event;
        while (enet_host_service(client, &event, 3000) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                // Clean up any packets we receive during disconnect
                enet_packet_destroy(event.packet);
            } 
            else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                cout << "Disconnected from server." << endl;
                break;
            }
        }
    }
    
    // Clean up local network resources
    enet_host_destroy(client);
}

// ---------------------------------------------------------------------
// Chat application interface
// Handles both sending user messages and receiving messages from others
// ---------------------------------------------------------------------
void chatApp(ENetPeer* peer, ENetHost* client) {
    cout << "\nWelcome to the Chat App!" << endl;
    cout << "Type your message and press Enter. Type '/exit' to leave chat." << endl;
    cout << "You: " << flush;
    
    string userInput = "";
    bool running = true;
    
    // Main chat loop
    while (running) {
        // --- RECEIVING MESSAGES ---
        // Check for incoming messages (non-blocking)
        ENetEvent event;
        while (enet_host_service(client, &event, 0) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                // Display received message and restore user input prompt
                cout << "\n" << reinterpret_cast<char*>(event.packet->data);
                cout << "\nYou: " << userInput << flush;
                enet_packet_destroy(event.packet);
            }
        }
        
        // --- SENDING MESSAGES ---
        // Check if user has pressed any keys (non-blocking)
        if (_kbhit()) {
            char ch = _getch();  // Get the pressed key
            
            // Handle Enter key (send message)
            if (ch == 13) {  // 13 is ASCII code for Enter
                cout << "\n";
                
                // Exit command
                if (userInput == "/exit") {
                    running = false;
                    break;
                }
                
                // Send non-empty messages
                if (!userInput.empty()) {
                    // Add CHAT: prefix so server knows it's a chat message
                    string fullMessage = "CHAT:" + userInput;
                    ENetPacket* packet = enet_packet_create(
                        fullMessage.c_str(), 
                        fullMessage.size() + 1, 
                        ENET_PACKET_FLAG_RELIABLE
                    );
                    enet_peer_send(peer, 0, packet);
                    enet_host_flush(client);
                    userInput.clear();
                }
                
                // Reset input prompt
                cout << "You: " << flush;
            }
            // Handle Backspace key
            else if (ch == 8) {  // 8 is ASCII code for Backspace
                if (!userInput.empty()) {
                    userInput.pop_back();  // Remove last character
                    cout << "\b \b" << flush;  // Erase character from display
                }
            }
            // Handle regular character input
            else {
                userInput.push_back(ch);  // Add character to input
                cout << ch << flush;      // Display character
            }
        }
        
        // Prevent CPU overuse
        Sleep(10);  // Pause for 10 milliseconds
    }
}

// ---------------------------------------------------------------------
// User registration process
// 1. Get email from user
// 2. Request verification code from server
// 3. Get verification code from user
// 4. Set password after verification
// ---------------------------------------------------------------------
void handleRegistration(ENetPeer* peer, ENetHost* client) {
    // Step 1: Get email address
    string email;
    cout << "Enter email for registration: ";
    cin >> email;
    
    // Step 2: Send registration request to server
    string message = "REGISTER:" + email + "|";  // Empty password field
    string response = sendMessage(peer, message, client);
    cout << "Server: " << response << endl;
    
    // If registration request succeeded (OK response)
    if (response.rfind("OK:", 0) == 0) {
        // Step 3: Get verification code from user
        string code;
        cout << "Enter verification code sent to your email: ";
        cin >> code;
        
        // Step 4: Verify email with code
        message = "VERIFY:" + email + "|" + code;
        response = sendMessage(peer, message, client);
        cout << "Server: " << response << endl;
        
        // If verification succeeded
        if (response.rfind("OK:", 0) == 0) {
            // Step 5: Set password
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
// User login process
// 1. Get email and password
// 2. Authenticate with server
// 3. Enter chat mode if login successful
// ---------------------------------------------------------------------
void handleLogin(ENetPeer* peer, ENetHost* client) {
    // Step 1: Get credentials
    string email, password;
    cout << "Enter email: ";
    cin >> email;
    cout << "Enter password: ";
    cin >> password;
    
    // Step 2: Send login request to server
    string message = "LOGIN:" + email + "|" + password;
    string response = sendMessage(peer, message, client);
    cout << "Server: " << response << endl;
    
    // Step 3: Enter chat if login successful
    if (response.rfind("OK:", 0) == 0) {
        cout << "You are now logged in!" << endl;
        cout << "Press Enter to go to the Chat App..." << endl;
        
        // Clear input buffer (consume any leftover input)
        cin.ignore(1000, '\n');
        cin.get();
        
        // Enter chat mode
        chatApp(peer, client);
    }
}



int application(){
     // --- INITIALIZE NETWORKING ---
    // Initialize ENet library
    if (enet_initialize() != 0) {
        cerr << "ENet initialization failed!" << endl;
        cin.get();  // Wait for user input before exiting
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);  // Clean up ENet when program exits

    // Create client network interface
    ENetHost* client = enet_host_create(
        nullptr,  // No specific bind address
        1,        // Only one outgoing connection
        2,        // Two channels
        0, 0      // No bandwidth limits
    );
    
    if (!client) {
        cerr << "Client creation failed!" << endl;
        cin.get();
        return EXIT_FAILURE;
    }

    // --- CONNECT TO SERVER ---
    // Set server address
    ENetAddress address;
    enet_address_set_host(&address, "127.0.0.1");  // Server IP address
    address.port = 25555;                               // Server port

    // Attempt connection
    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    if (!peer) {
        cerr << "Failed to initiate connection to server!" << endl;
        cin.get();
        enet_host_destroy(client);
        return EXIT_FAILURE;
    }

    // Wait for connection success/failure (5 second timeout)
    ENetEvent event;
    if (enet_host_service(client, &event, 5000) > 0 && 
        event.type == ENET_EVENT_TYPE_CONNECT) {
        cout << "Connected to server!" << endl;
    } else {
        cerr << "Connection failed!" << endl;
        enet_peer_reset(peer);
        cin.get();
        enet_host_destroy(client);
        return EXIT_FAILURE;
    }

    // --- MAIN APPLICATION LOOP ---
    int choice = 0;
    while (choice != 3) {  // Loop until user chooses to exit
        displayMenu();
        cin >> choice;
        cin.ignore(1000, '\n');  // Clear input buffer
        
        switch (choice) {
            case 1:  // Login
                handleLogin(peer, client);
                break;
            case 2:  // Register
                handleRegistration(peer, client);
                break;
            case 3:  // Exit
                cout << "Exiting..." << endl;
                break;
            default:  // Invalid choice
                cout << "Invalid choice, please try again." << endl;
                break;
        }
    }

    // --- CLEANUP ---
    disconnectFromServer(peer, client);
    return 0;

}

// ---------------------------------------------------------------------
// Main function - Program entry point
// ---------------------------------------------------------------------



int main() {
    std::thread threadA(application);
    threadA.join();





    return 0;
}