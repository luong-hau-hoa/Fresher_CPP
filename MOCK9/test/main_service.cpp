#include "../lib/src/libService.h"
#include <cstring>
#include <algorithm>  // Để dùng std::transform

int main(){ 
    Service service("/service_queue"); // Tạo mqd cho chính Service
          
    // Thiết lập thông báo để phát hiện tin nhắn mới
    service.setupNotification();

    while (true) {
        std::string command;
        std::cout << "Enter command EXIT to stop the program: " << std::endl; 
        std::getline(std::cin, command);

        // Chuyển đổi lệnh nhập vào thành chữ in hoa (không phân biệt chữ thường/in hoa)
        std::transform(command.begin(), command.end(), command.begin(), ::toupper);

        if (command == "EXIT") {
            service.exit_ipc();
            break;
        } else {
            std::cout << "Invalid command. Please enter EXIT to stop the program" << std::endl;
        }
    }
    return 0;
}

