#include <enet/enet.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <algorithm> // For std::find_if

using namespace std;

// ---------------------------------------------------------------------
// Structure to store user information
// ---------------------------------------------------------------------
struct User {
    string email;
    string password;
    bool verified;
};

// ---------------------------------------------------------------------
// Global Variables
// ---------------------------------------------------------------------
vector<User> users;           // List of registered users
string pendingEmail = "";     // Email awaiting verification
string pendingCode = "";      // Verification code for the pending email

// ---------------------------------------------------------------------
// Load users from a file ("users.txt") when the server starts
// ---------------------------------------------------------------------
void loadUsers() {
    ifstream file("users.txt");
    if (!file) {
        cout << "User file not found. Will create when needed." << endl;
        return;
    }
    
    string email, password, verified_str;
    while (file >> email >> password >> verified_str) {
        users.push_back({ email, password, (verified_str == "1") });
    }
    file.close();
    cout << "Loaded " << users.size() << " users from file." << endl;
}

// ---------------------------------------------------------------------
// Save all users to file ("users.txt")
// ---------------------------------------------------------------------
void saveAllUsers() {
    ofstream file("users.txt");
    if (!file) {
        cerr << "Error: Cannot open users.txt for writing!" << endl;
        return;
    }
    for (const auto& user : users) {
        file << user.email << " " << user.password << " " 
             << (user.verified ? "1" : "0") << endl;
    }
    file.close();
    cout << "Saved all users to file." << endl;
}

// ---------------------------------------------------------------------
// Check if a user with the given email already exists
// ---------------------------------------------------------------------
bool userExists(const string& email) {
    return find_if(users.begin(), users.end(), [&](const User& u) {
        return u.email == email;
    }) != users.end();
}

// ---------------------------------------------------------------------
// Verify login credentials (only verified users can log in)
// ---------------------------------------------------------------------
bool verifyLogin(const string& email, const string& password) {
    auto it = find_if(users.begin(), users.end(), [&](const User& u) {
        return u.email == email;
    });
    if (it != users.end() && it->password == password && it->verified)
        return true;
    return false;
}

// ---------------------------------------------------------------------
// Generate a random 6-digit verification code
// ---------------------------------------------------------------------
string generateVerificationCode() {
    srand(time(NULL));
    stringstream ss;
    for (int i = 0; i < 6; i++) {
        ss << rand() % 10;
    }
    return ss.str();
}

// ---------------------------------------------------------------------
// Send verification code via a Python script
// ---------------------------------------------------------------------
void sendEmail(const string& email, const string& code) {
    string command = "python ../script/send_email.py " + email + " " + code;
    system(command.c_str());
}

// ---------------------------------------------------------------------
// Register a new user: generate a code and send email for verification
// ---------------------------------------------------------------------
string registerUser(const string& email, const string& password) {
    if (userExists(email))
        return "ERROR:User already exists";
    
    pendingEmail = email;
    pendingCode = generateVerificationCode();
    sendEmail(email, pendingCode);
    
    return "OK:Verification code sent to your email";
}

// ---------------------------------------------------------------------
// Verify a user's email using the provided code
// ---------------------------------------------------------------------
string verifyUser(const string& email, const string& code) {
    if (email == pendingEmail && code == pendingCode) {
        // Add the user with an empty password (to be set later)
        users.push_back({ email, "", true });
        saveAllUsers();
        
        // Clear the pending verification info.
        pendingEmail = "";
        pendingCode = "";
        
        return "OK:Verified";
    }
    return "ERROR:Invalid verification code";
}

// ---------------------------------------------------------------------
// Set the password for a verified user
// ---------------------------------------------------------------------
string setPassword(const string& email, const string& password) {
    for (auto& user : users) {
        if (user.email == email && user.verified) {
            user.password = password;
            saveAllUsers();
            return "OK:Password set successfully";
        }
    }
    return "ERROR:User not found or not verified";
}

// ---------------------------------------------------------------------
// Main server loop: Handles authentication commands and chat messages.
// ---------------------------------------------------------------------
int main() {
    if (enet_initialize() != 0) {
        cerr << "ENet initialization failed!" << endl;
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    // Set up server address (listen on all interfaces) and port 25555
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 25555;

    // Create the server host with capacity for 32 clients and 2 channels
    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (!server) {
        cerr << "Server creation failed!" << endl;
        return EXIT_FAILURE;
    }
    cout << "Server started on port " << address.port << endl;
    
    // Load registered users from file
    loadUsers();

    ENetEvent event;
    while (true) {
        // Process network events with a timeout of 1000 ms
        while (enet_host_service(server, &event, 1000) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                cout << "Client connected from: " 
                     << event.peer->address.host << ":" 
                     << event.peer->address.port << endl;
                event.peer->data = NULL;
            } 
            else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                // Convert packet data to a string
                string message(reinterpret_cast<char*>(event.packet->data));
                cout << "Received: " << message << endl;
                string response;
                
                // Process commands based on message prefix
                if (message.rfind("REGISTER:", 0) == 0) {
                    size_t separator = message.find('|');
                    string email = message.substr(9, separator - 9);
                    string password = message.substr(separator + 1);
                    response = registerUser(email, password);
                }
                else if (message.rfind("VERIFY:", 0) == 0) {
                    size_t separator = message.find('|');
                    string email = message.substr(7, separator - 7);
                    string code = message.substr(separator + 1);
                    response = verifyUser(email, code);
                }
                else if (message.rfind("SETPASSWORD:", 0) == 0) {
                    size_t separator = message.find('|');
                    string email = message.substr(12, separator - 12);
                    string password = message.substr(separator + 1);
                    response = setPassword(email, password);
                }
                else if (message.rfind("LOGIN:", 0) == 0) {
                    size_t separator = message.find('|');
                    string email = message.substr(6, separator - 6);
                    string password = message.substr(separator + 1);
                    response = verifyLogin(email, password)
                                ? "OK:Login successful"
                                : "ERROR:Invalid credentials or account not verified";
                }
                else if (message.rfind("CHAT:", 0) == 0) {
                    // Chat command: Broadcast the chat message to all connected peers.
                    string chatMsg = message.substr(5); // Remove the "CHAT:" prefix
                    response = "CHAT from client: " + chatMsg;
                    // Loop through all connected peers and send the chat message.
                    for (size_t i = 0; i < server->peerCount; i++) {
                        ENetPeer* peer = &server->peers[i];
                        if (peer->state == ENET_PEER_STATE_CONNECTED) {
                            ENetPacket* packet = enet_packet_create(response.c_str(),
                                response.size() + 1, ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(peer, 0, packet);
                        }
                    }
                    enet_packet_destroy(event.packet);
                    continue; // Skip sending a separate reply below.
                }
                else {
                    response = "ERROR:Unknown command";
                }
                
                // Send response back to the client that sent the command.
                ENetPacket* packet = enet_packet_create(response.c_str(), 
                                                    response.size() + 1, 
                                                    ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(event.peer, 0, packet);
                enet_packet_destroy(event.packet);
            } 
            else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                cout << "Client disconnected." << endl;
                event.peer->data = NULL;
            }
        }
    }

    enet_host_destroy(server);
    return 0;
}
