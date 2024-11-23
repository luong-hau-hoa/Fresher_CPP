#include "libClient.h"
#include <iostream>
#include <cstring>
#include <algorithm>  // Để dùng std::transform

int main() {

    Client app_volume("/volume_queue2", DataType::VOLUME);
    app_volume.setupNotification();

    while (true) {
        std::string command;
        std::cout << "Enter command (GET, SET, EXIT, SUBSCRIBE, UNSUBSCRIBE): " << std::endl;
        std::getline(std::cin, command);

        // Chuyển đổi lệnh nhập vào thành chữ in hoa (không phân biệt chữ thường/in hoa)
        std::transform(command.begin(), command.end(), command.begin(), ::toupper);

        if (command == "GET") {
            app_volume.get();
        } else if (command == "SET") {
            app_volume.set();
        } else if (command == "SUBSCRIBE") {
            app_volume.subscribe();
        } else if (command == "UNSUBSCRIBE") {
            app_volume.unsubscribe();    
        } else if (command == "EXIT") {
            app_volume.exit_ipc();
            break;
        } else {
            std::cout << "Invalid command. Please enter GET, SET, EXIT, SUBSCRIBE, or UNSUBSCRIBE." << std::endl;
        }
    }

    return 0;
}