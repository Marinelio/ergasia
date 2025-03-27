#include <iostream>
#include <cstdlib>
#include <string>

void portscanner() {
    std::cout << "Ports Scanned\tEstimated Scan Time\n";
    std::cout << "--------------------------------\n";
    std::cout << "1,000\t\t2–4 minutes\n";
    std::cout << "10,000\t\t10–15 minutes\n";
    std::cout << "65,535\t\t2–5 hours\n";

    std::string target;
    
    std::cout << "Enter the target hostname or IP address: ";
    std::cin >> target;
    
    // Construct the command to call the Python script with the target argument
    std::string command = "python ./devscript/portS.py " + target;
    
    // Call the Python script
    system(command.c_str());
}

void dos() {
    std::string command = "python ./devscript/dos.py ";
    // Execute the command
    std::cout << "Executing Python script..." << std::endl;
    int result = system(command.c_str());

    // Check if the Python script was called successfully
    if (result == 0) {
        std::cout << "Python script executed successfully." << std::endl;
    } else {
        std::cerr << "Failed to execute Python script." << std::endl;
    }
}



int main() {
    int choice;

    std::cout << "Select an option: " << std::endl;
    std::cout << "1. Port Scanner" << std::endl;
    std::cout << "2. DoS Attack" << std::endl;
    std::cout << "3. Exit" << std::endl;
    std::cin >> choice;
        while (true){
        


        while(choice>3 || choice <1 ){

            std::cout << "Invalid choice, please enter a number between 1 and 3: ";
            std::cin >> choice;

        }   





        if(choice==1){
            portscanner();
        }
        else if(choice==2){
            dos();
        }
        else if (choice == 3){
            return 0;
        }

        }




    return 0;
}
