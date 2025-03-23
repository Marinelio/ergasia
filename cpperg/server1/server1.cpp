#include <enet/enet.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>

using namespace std;

// ---------------------------------------------------------------------
// User structure - stores basic user account information
// ---------------------------------------------------------------------
struct User {
    string email;
    string password;
    bool verified;
};

// ---------------------------------------------------------------------
// Global Variables
// ---------------------------------------------------------------------
vector<User> users;           // Stores all registered users
string pendingEmail = "";     // Email waiting for verification
string pendingCode = "";      // Verification code for pending email

// ---------------------------------------------------------------------
// File Operations
// ---------------------------------------------------------------------

// Load users from file when server starts
void loadUsers() {
    ifstream file("users.txt");
    if (!file) {
        cout << "User file not found. Will create when users register." << endl;
        return;
    }
    
    string email, password, verified_str;
    while (file >> email >> password >> verified_str) {
        bool isVerified = false;
        if (verified_str == "1") {
            isVerified = true;
        }
        users.push_back({ email, password, isVerified });
    }
    file.close();
    cout << "Loaded " << users.size() << " users from file." << endl;
}

// Save all users to file
void saveUsers() {
    ofstream file("users.txt");
    if (!file) {
        cerr << "Error: Cannot open users.txt for writing!" << endl;
        return;
    }
    
    for (const auto& user : users) {
        string verifiedFlag;
        if (user.verified) {
            verifiedFlag = "1";
        } else {
            verifiedFlag = "0";
        }
        file << user.email << " " << user.password << " " << verifiedFlag << endl;
    }
    file.close();
    cout << "Users saved to file." << endl;
}

// ---------------------------------------------------------------------
// User Management Functions
// ---------------------------------------------------------------------

// Check if a user already exists with the given email
bool userExists(const string& email) {
    for (const auto& user : users) {
        if (user.email == email) {
            return true;
        }
    }
    return false;
}

// Validate login credentials
bool validateLogin(const string& email, const string& password) {
    for (const auto& user : users) {
        if (user.email == email && user.password == password && user.verified) {
            return true;
        }
    }
    return false;
}

// Generate a random 6-digit verification code
string generateVerificationCode() {
    // Use modern C++ random generators for better randomness
    random_device rd;
    mt19937 generator(rd());
    uniform_int_distribution<int> distribution(0, 9);
    
    // Create a 6-digit code
    stringstream code;
    for (int i = 0; i < 6; i++) {
        code << distribution(generator);
    }
    return code.str();
}

// Send verification code via external Python script
void sendEmail(const string& email, const string& code) {
    // Call Python script to send the actual email
    string command = "python ../script/send_email.py " + email + " " + code;
    system(command.c_str());
    cout << "Verification email sent to: " << email << endl;
}

// ---------------------------------------------------------------------
// User Registration and Authentication Functions
// ---------------------------------------------------------------------

// Register a new user
string registerUser(const string& email, const string& password) {
    // Check if user already exists
    if (userExists(email)) {
        return "ERROR:User already exists";
    }
    
    // Generate and send verification code
    pendingEmail = email;
    pendingCode = generateVerificationCode();
    sendEmail(email, pendingCode);
    
    return "OK:Verification code sent to your email";
}

// Verify user's email with provided code
string verifyUser(const string& email, const string& code) {
    // Check if email and code match pending verification
    if (email == pendingEmail && code == pendingCode) {
        // Add user to the system (password will be set separately)
        users.push_back({ email, "", true });
        saveUsers();
        
        // Clear pending verification
        pendingEmail = "";
        pendingCode = "";
        
        return "OK:Verified";
    }
    return "ERROR:Invalid verification code";
}

// Set password for a verified user
string setPassword(const string& email, const string& password) {
    for (auto& user : users) {
        if (user.email == email && user.verified) {
            user.password = password;
            saveUsers();
            return "OK:Password set successfully";
        }
    }
    return "ERROR:User not found or not verified";
}

// ---------------------------------------------------------------------
// Main Server Function
// ---------------------------------------------------------------------
int main() {
    // Initialize ENet networking library
    if (enet_initialize() != 0) {
        cerr << "Failed to initialize ENet!" << endl;
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);  // Clean up ENet when program exits

    // Set up server address and port
    ENetAddress address;
    address.host = ENET_HOST_ANY;  // Listen on all network interfaces
    address.port = 25555;          // Port number

    // Create server that can handle up to 32 clients and 2 channels
    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (!server) {
        cerr << "Failed to create server!" << endl;
        return EXIT_FAILURE;
    }
    cout << "Server started on port " << address.port << endl;
    
    // Load existing users from file
    loadUsers();

    // Main server loop
    ENetEvent event;
    while (true) {
        // Process network events with a 1-second timeout
        while (enet_host_service(server, &event, 1000) > 0) {
            // Handle different types of network events
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                // New client connected
                cout << "Client connected from: " 
                     << event.peer->address.host << ":" 
                     << event.peer->address.port << endl;
            } 
            else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                // Received data from a client
                string message(reinterpret_cast<char*>(event.packet->data));
                cout << "Received: " << message << endl;
                string response;
                
                // Process commands based on message prefix
                if (message.rfind("REGISTER:", 0) == 0) {
                    // Format: REGISTER:email|password
                    size_t separator = message.find('|');
                    string email = message.substr(9, separator - 9);
                    string password = message.substr(separator + 1);
                    response = registerUser(email, password);
                }
                else if (message.rfind("VERIFY:", 0) == 0) {
                    // Format: VERIFY:email|code
                    size_t separator = message.find('|');
                    string email = message.substr(7, separator - 7);
                    string code = message.substr(separator + 1);
                    response = verifyUser(email, code);
                }
                else if (message.rfind("SETPASSWORD:", 0) == 0) {
                    // Format: SETPASSWORD:email|password
                    size_t separator = message.find('|');
                    string email = message.substr(12, separator - 12);
                    string password = message.substr(separator + 1);
                    response = setPassword(email, password);
                }
                else if (message.rfind("LOGIN:", 0) == 0) {
                    // Format: LOGIN:email|password
                    size_t separator = message.find('|');
                    string email = message.substr(6, separator - 6);
                    string password = message.substr(separator + 1);
                    
                    // Replaced ternary operator with if-else for better readability
                    if (validateLogin(email, password)) {
                        response = "OK:Login successful";
                    } else {
                        response = "ERROR:Invalid credentials or account not verified";
                    }
                }
                else if (message.rfind("CHAT:", 0) == 0) {
                    // Format: CHAT:message
                    // Broadcast chat message to all connected clients except the sender
                    string chatMsg = message.substr(5);
                    string broadcastMessage = "CHAT from client: " + chatMsg;
                    
                    // Send to all connected clients except the sender
                    for (size_t i = 0; i < server->peerCount; i++) {
                        ENetPeer* peer = &server->peers[i];
                        // Only send to connected peers that are not the sender
                        if (peer->state == ENET_PEER_STATE_CONNECTED && peer != event.peer) {
                            ENetPacket* packet = enet_packet_create(
                                broadcastMessage.c_str(),
                                broadcastMessage.size() + 1, 
                                ENET_PACKET_FLAG_RELIABLE
                            );
                            enet_peer_send(peer, 0, packet);
                        }
                    }
                    // Send acknowledgment to sender
                    response = "Message sent";
                }
                else {
                    response = "ERROR:Unknown command";
                }
                
                // Send response back to the client
                ENetPacket* packet = enet_packet_create(
                    response.c_str(), 
                    response.size() + 1, 
                    ENET_PACKET_FLAG_RELIABLE
                );
                enet_peer_send(event.peer, 0, packet);
                enet_packet_destroy(event.packet);
            } 
            else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                // Client disconnected
                cout << "Client disconnected." << endl;
            }
        }
    }

    // Clean up (note: this code is never reached in the current implementation)
    enet_host_destroy(server);
    return 0;
}