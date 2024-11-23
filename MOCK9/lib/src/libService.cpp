#include "libService.h"
#include <cerrno>  // dùng errno
#include <cstring> // dùng strerror(errno)
#include <fcntl.h>
#include <mqueue.h>
#include <sstream>  // Dùng để xử lý chuỗi
#include <sys/types.h>

void initializeMessage(Message &message, const char *from_queue_name, CommandType command_type, DataType data_type, std::variant<Volume_t, Temperature_t> data_value) {
    // Gán chuỗi cho message.from_queue_name
    strncpy(message.from_queue_name, from_queue_name, sizeof(message.from_queue_name) - 1); 
    message.from_queue_name[sizeof(message.from_queue_name) - 1] = '\0';  // Đảm bảo chuỗi kết thúc bằng ký tự null

    message.command_type = command_type;
    message.data_type = data_type;

    // Gán giá trị cho data dựa trên data_type
    if (data_type == DataType::VOLUME) {
        message.data = data_value;  // Gán giá trị volume
    } else if (data_type == DataType::TEMPERATURE) {
        message.data = data_value;  // Gán giá trị temperature
    }
}

void initializeMessage(Message &message, const char *from_queue_name, CommandType command_type, DataType data_type) {
    // Gán chuỗi cho src_queue_name
    strncpy(message.from_queue_name, from_queue_name, sizeof(message.from_queue_name) - 1); 
    message.from_queue_name[sizeof(message.from_queue_name) - 1] = '\0';  // Đảm bảo chuỗi kết thúc bằng ký tự null

    // Gán giá trị cho command_type
    message.command_type = command_type;

    // Gán giá trị cho data_type
    message.data_type = data_type;
    
}
// Hàm giải mã từ chuỗi const char* thành struct Message
bool decodeMessage(const char* encodedMessage, Message& message) {
    std::istringstream ss(encodedMessage);  // Sử dụng stringstream để phân tách chuỗi
    std::string token;
    int command_type, data_type, data_value;

    // Đọc và phân tách từng phần từ chuỗi
    std::getline(ss, token, ',');  // from_queue_name
    std::strncpy(message.from_queue_name, token.c_str(), sizeof(message.from_queue_name) - 1);

    std::getline(ss, token, ',');  // command_type
    command_type = std::stoi(token);  // Chuyển đổi từ chuỗi sang số nguyên
    message.command_type = static_cast<CommandType>(command_type);  // Gán giá trị vào command_type

    std::getline(ss, token, ',');  // data_type
    data_type = std::stoi(token);  // Chuyển đổi từ chuỗi sang số nguyên
    message.data_type = static_cast<DataType>(data_type);  // Gán giá trị vào data_type

    // Nếu command_type là SET, phân giải dữ liệu message.data
    if (message.command_type == CommandType::SET) {
        // Gán giá trị data
        std::getline(ss, token, ',');  // data_value
        data_value = std::stoi(token);  // Chuyển đổi từ chuỗi sang số nguyên
        // Giải mã dữ liệu vào đúng kiểu dữ liệu tương ứng
        if (message.data_type == DataType::VOLUME) {
            message.data = Volume_t{data_value};  // Gán vào Volume_t
        } else if (message.data_type == DataType::TEMPERATURE) {
            message.data = Temperature_t{data_value};  // Gán vào Temperature_t
        } else {
            return false;  // Nếu data_type không hợp lệ
        }
    }
    return true;  // Giải mã thành công
}

// Hàm mã hóa Message thành const char*
const char* encodeMessage(const Message& message) {
    static char buffer[256];  // Một buffer tạm thời đủ lớn để chứa toàn bộ thông tin struct
    std::memset(buffer, 0, sizeof(buffer)); // Đảm bảo buffer không chứa giá trị cũ

    // Khởi tạo phần đầu buffer với thông tin cơ bản
    std::sprintf(buffer, "%s,%d,%d,", 
                 message.from_queue_name, 
                 static_cast<int>(message.command_type), 
                 static_cast<int>(message.data_type));

    if ((message.command_type == CommandType::SET) || (message.command_type == CommandType::RESPOND_GETDATA) || (message.command_type == CommandType::NOTIFY)) {
        // Gán dữ liệu vào buffer dựa trên data_type
        if (message.data_type == DataType::VOLUME) {
            const Volume_t& volume = std::get<Volume_t>(message.data);  // Truy xuất giá trị volume từ variant
            std::sprintf(buffer + std::strlen(buffer), "%d", volume.volume);  // Ghi giá trị volume vào buffer
        } else if (message.data_type == DataType::TEMPERATURE) {
            const Temperature_t& temperature = std::get<Temperature_t>(message.data);  // Truy xuất giá trị temperature từ variant
            std::sprintf(buffer + std::strlen(buffer), "%d", temperature.temperature);  // Ghi giá trị temperature vào buffer
        }
    }
    return buffer;  // Trả về con trỏ tới buffer đã mã hóa
}

Service::Service(const char* service_queue_name) : service_queue_name(service_queue_name) {

    volume_data.volume = 50;           // Gán giá trị ban đầu cho volume
    temperature_data.temperature = 25; // Gán giá trị ban đầu cho temperature
    std::cout << "Volume initialized to " << volume_data.volume << " %" <<  std::endl;
    std::cout << "Temperature initialized to " << temperature_data.temperature <<" C" << std::endl;

    this->releaseResources = false;

    struct mq_attr attr = {
        .mq_flags = 0,     
        .mq_maxmsg = 100,  
        .mq_msgsize = 256, 
        .mq_curmsgs = 0    
    };

    this->mq = mq_open(service_queue_name, O_CREAT | O_RDONLY| O_NONBLOCK, 0644, &attr);
    if (this->mq == -1) {
        perror("Service: mq_open failed");
        exit(EXIT_FAILURE);
    } else {
        std::cout << "Service: Message queue '" << service_queue_name
                << "' opened successfully for reading" << std::endl;
    }

}

Service::~Service() {
    if (releaseResources == false) {
      if (mq_close(mq) == -1) {
          std::cerr << "Failed to close message queue: " << service_queue_name << " - Error: " << strerror(errno) << std::endl;
      } else {
          std::cout << "Message queue closed successfully: " << service_queue_name << std::endl;
      }

      if (mq_unlink(service_queue_name) == -1) {
          std::cerr << "Failed to unlink message queue: " << service_queue_name << " - Error: " << strerror(errno) << std::endl;
      } else {
          std::cout << "Message queue unlinked successfully: " << service_queue_name << std::endl;
      }
    }
  std::cout << "Destructor has been called.\n";
}

Volume_t Service::getVolume() const {
    return volume_data;
}

void Service::send_to_mq_client(const std::string& client_queue, const std::string& message) {

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 100;
    attr.mq_msgsize = 256;
    attr.mq_curmsgs = 0;

    mqd_t client_mq = mq_open(client_queue.c_str(), O_CREAT | O_WRONLY | O_NONBLOCK , 0644, &attr);
    if (client_mq == -1) {
        perror("Service: mq_open for client failed");
        std::cerr << "Service: mq_open for " << client_queue << " failed. Error: " << strerror(errno) << std::endl;
        return;
    } else {
    std::cout << "Service: Message queue '" << client_queue
              << "' opened successfully for writing" << std::endl;
    }

    if (mq_send(client_mq, message.c_str(), message.size(), 0) == -1) {
        perror("Service: mq_send failed");
        std::cerr << "Service: mq_send for" << client_queue << " failed. Error: " << strerror(errno) << std::endl;
    } else {
        std::cout << "Service: Message sent to client queue '" << client_queue << "'" << ":" << message << std::endl;
    }

    mq_close(client_mq);  
}

  void Service::send(const char* dest_queue, const char* message){

    mqd_t mqd_dest = mq_open(dest_queue, O_WRONLY | O_NONBLOCK);
    if (mqd_dest == -1) {
        perror("Service: mq_open for destination queue failed");
        std::cerr << "Service: mq_open for " << dest_queue << " failed. Error: " << strerror(errno) << std::endl;
        return;
    } else {
        /*
        std::cout << "Service: Message queue '" << dest_queue << "' opened successfully for writing" << std::endl;
        */
    }

    if (mq_send(mqd_dest, message, strlen(message), 0) == -1) {
        std::cerr << "Service: mq_send for " << dest_queue << " failed. Error: " << strerror(errno) << std::endl;
    } else {
        std::cout << "Service: Message sent to destination queue '" << dest_queue << "' " <<" is: "<< message << std::endl;
    }

    mq_close(mqd_dest);  
 }

ssize_t Service::read_mq() {
    char buffer[256];
    ssize_t bytes_read;

    bytes_read = mq_receive(mq, buffer, sizeof(buffer), nullptr);
    if (bytes_read >= 0) {
      buffer[bytes_read] = '\0'; // Kết thúc chuỗi

      // Gán dữ liệu vào thuộc tính data_receive của Service
      this->data_receive = std::string(buffer);
      /*
      std::cout << "char[] size of buffer(Received data): " << strlen(buffer)<< std::endl;
      */
      std::cout << "Service: Received data: " << buffer << std::endl;
    } else if (errno == EAGAIN) {
        // Nếu queue trống, trả về thông báo hoặc xử lý an toàn
        std::cout << "Service: Queue is empty. No messages available." << std::endl;
    }
    else {
      perror("Service: mq_receive failed");
    }
    return bytes_read;
}

void Service::receive(){
    ssize_t bytes_read = read_mq();
    if (bytes_read >= 0) {
    Message message_receive;
    decodeMessage(data_receive.c_str(), message_receive);
    handleCommand(message_receive);
  }
}

void Service::readAndHandleMessage() {

    struct mq_attr attr;

    // Lấy thuộc tính của hàng đợi
    if (mq_getattr(mq, &attr) == -1) {
        perror("Service: mq_getattr failed");
        return;
    }

    // Kiểm tra số lượng tin nhắn hiện có
    while (attr.mq_curmsgs > 0) {

        ssize_t bytes_read = read_mq();
        if (bytes_read >= 0) {
            Message message_receive;
            decodeMessage(this->data_receive.c_str(), message_receive);
            handleCommand(message_receive);
         
        }else {
            perror("Service: Error reading from message queue");
            break;
        }

        // Cập nhật lại thuộc tính để kiểm tra số lượng tin nhắn còn lại
        if (mq_getattr(mq, &attr) == -1) {
            perror("Service: mq_getattr failed during loop");
            break;
        }
    }

    // Thiết lập lại thông báo để phát hiện tin nhắn mới
    setupNotification();
}

void Service::setupNotification() {
    struct sigevent sev{};
    sev.sigev_notify = SIGEV_THREAD;  // Thông báo qua một thread riêng
    sev.sigev_notify_function = [](union sigval sv) {
        Service* service = static_cast<Service*>(sv.sival_ptr);
        service->readAndHandleMessage();  // Gọi hàm xử lý tin nhắn
    };
    sev.sigev_notify_attributes = nullptr;  // Sử dụng thread mặc định
    sev.sigev_value.sival_ptr = this;  // Truyền con trỏ tới Service hiện tại

    if (mq_notify(mq, &sev) == -1) {
        perror("Service: mq_notify failed");
    } else {
        /*
        std::cout << "Service: Notification setup successfully for queue '" << service_queue_name << "'." << std::endl;
        */
    }

}

void Service::handleCommand(const Message& message_receive) {
    switch (message_receive.command_type) {
        case CommandType::GET:
            handleGet(message_receive);  // Gọi hàm xử lý GET
            break;

        case CommandType::SET:
            handleSet(message_receive);  // Gọi hàm xử lý SET
            break;

        case CommandType::EXIT:
            //handleExit();  // Gọi hàm xử lý EXIT
            break;

        case CommandType::SUBSCRIBE:
            handleSubscribe(message_receive);  // Gọi hàm xử lý SUBSCRIBE
            break;

        case CommandType::UNSUBSCRIBE:
            handleUnsubscribe(message_receive);  // Gọi hàm xử lý UNSUBSCRIBE
            break;

        default:
            std::cerr << "Unknown command type!\n";
            break;
    }
}

void Service::handleGet(const Message& message_receive){
    if (message_receive.data_type == DataType::VOLUME) {
        Message message_send;
        initializeMessage(message_send, service_queue_name, CommandType::RESPOND_GETDATA, DataType::VOLUME, volume_data);
        const char* encodedMessage = encodeMessage(message_send);
        send(message_receive.from_queue_name, encodedMessage);
    }
    else if (message_receive.data_type == DataType::TEMPERATURE){
        
        Message message_send;
        initializeMessage(message_send, service_queue_name, CommandType::RESPOND_GETDATA, DataType::TEMPERATURE, temperature_data);
        const char* encodedMessage = encodeMessage(message_send);
        send(message_receive.from_queue_name, encodedMessage);
    }
    else {
        std::cerr << "Error: Unknown DataType in message!" << std::endl;
    }
}

void Service::handleSet(const Message& message_receive){
    if (message_receive.data_type == DataType::VOLUME) {

        try {
            volume_data = std::get<Volume_t>(message_receive.data); // Truy xuất giá trị Volume_t
            std::cout << "Volume data updated: " << volume_data.volume << std::endl;
        } catch (const std::bad_variant_access& e) {
            std::cerr << "Error: Failed to get Volume_t from data - " << e.what() << std::endl;
        }
        printSubscriptions(); 

        // Gửi lại giá trị vừa cập nhật cho tất cả các hàng đợi đã đăng ký
        auto& volume_clients = subscriptions[DataType::VOLUME];
        for (const auto& client_queue : volume_clients) {
            Message message_send;
            initializeMessage(message_send, service_queue_name, CommandType::NOTIFY, DataType::VOLUME, volume_data);
            const char* encodedMessage = encodeMessage(message_send);
            send(client_queue.c_str(), encodedMessage);
            std::cout << "Sent updated Volume data to " << client_queue << std::endl;
        }

    }
    else if (message_receive.data_type == DataType::TEMPERATURE){
        try {
            temperature_data = std::get<Temperature_t>(message_receive.data); // Truy xuất giá trị Temperature_t
            std::cout << "Temperature data updated: " << temperature_data.temperature << std::endl;
        } catch (const std::bad_variant_access& e) {
            std::cerr << "Error: Failed to get Temperature from data - " << e.what() << std::endl;
        }
        printSubscriptions();
        // Gửi lại giá trị vừa cập nhật cho tất cả các hàng đợi đã đăng ký
        auto& temperature_clients = subscriptions[DataType::TEMPERATURE];
        for (const auto& client_queue : temperature_clients) {
            Message message_send;
            initializeMessage(message_send, service_queue_name, CommandType::NOTIFY, DataType::TEMPERATURE, temperature_data);
            const char* encodedMessage = encodeMessage(message_send);
            send(client_queue.c_str(), encodedMessage);
            std::cout << "Sent updated Temperature data to " << client_queue << std::endl;
        }
    }
    else {
        std::cerr << "Error: Unknown DataType in message!" << std::endl;
    }

}

void Service::handleSubscribe(const Message& message_receive){
    // Lấy tên hàng đợi (from_queue_name) từ message_receive
    std::string queue_name(message_receive.from_queue_name);

    if (message_receive.data_type == DataType::VOLUME) {
        // Thêm trực tiếp vào unordered_set, không cần kiểm tra trùng lặp
        subscriptions[DataType::VOLUME].insert(queue_name);
        std::cout << "Subscribed " << queue_name << " to VOLUME updates." << std::endl;
        printSubscriptions();

        Message message_send;
        initializeMessage(message_send, this->service_queue_name, CommandType::RESPOND_SUBSCRIBE, DataType::VOLUME);
        const char* encodedMessage = encodeMessage(message_send);
        send(message_receive.from_queue_name, encodedMessage);

    }
    else if (message_receive.data_type == DataType::TEMPERATURE){
        subscriptions[DataType::TEMPERATURE].insert(queue_name);
        std::cout << "Subscribed " << queue_name << " to TEMPERATURE updates." << std::endl;
        printSubscriptions();

        Message message_send;
        initializeMessage(message_send, this->service_queue_name, CommandType::RESPOND_SUBSCRIBE, DataType::TEMPERATURE);
        const char* encodedMessage = encodeMessage(message_send);
        send(message_receive.from_queue_name, encodedMessage);

    }
    else {
        std::cerr << "Error: Unknown DataType in message!" << std::endl;
    }
}

void Service::handleUnsubscribe(const Message& message_receive){
    // Lấy tên hàng đợi (from_queue_name) từ message_receive
    std::string queue_name(message_receive.from_queue_name);

    // Kiểm tra loại dữ liệu và xóa khỏi danh sách đăng ký
    if (message_receive.data_type == DataType::VOLUME) {
        // Tìm và xóa khỏi danh sách VOLUME
        auto& volume_clients = subscriptions[DataType::VOLUME];
        if (volume_clients.erase(queue_name) > 0) {
            std::cout << "Unsubscribed " << queue_name << " from VOLUME updates." << std::endl;
        } else {
            std::cout << queue_name << " is not subscribed to VOLUME updates." << std::endl;
        }
        printSubscriptions();
    } else if (message_receive.data_type == DataType::TEMPERATURE) {
        // Tìm và xóa khỏi danh sách TEMPERATURE
        auto& temperature_clients = subscriptions[DataType::TEMPERATURE];
        if (temperature_clients.erase(queue_name) > 0) {
            std::cout << "Unsubscribed " << queue_name << " from TEMPERATURE updates." << std::endl;
        } else {
            std::cout << queue_name << " is not subscribed to TEMPERATURE updates." << std::endl;
        }
        printSubscriptions();
    } else {
        std::cerr << "Error: Unknown DataType in message!" << std::endl;
    }
}

void Service::printSubscriptions() const {
    std::cout << std::endl;
    std::cout << "List of Apps registered to receive notifications when data changes: " << std::endl;
    if (subscriptions.empty()) {
        std::cout << "  No active subscriptions." << std::endl;
    } 
    else {
        for (const auto& [data_type, clients] : subscriptions) {
            std::cout << "DataType: " << (data_type == DataType::VOLUME ? "VOLUME" : "TEMPERATURE") << std::endl;
            
            if(clients.empty()) {
                std::cout << "  No queues subscribed for this DataType." << std::endl;
            } 
            else {
                for (const auto& client : clients) {
                    std::cout << "  - " << client << std::endl;
                }
            }
        }
    }
    std::cout << std::endl;
}

void Service::exit_ipc(){
    unsubscribe_all(); // Gửi thông điệp hủy đăng ký
    close_and_unlink_queue(); // Đóng và hủy hàng đợi
    this->releaseResources = true;
}

void Service::unsubscribe_all(){
    if (this->subscriptions.empty()) {
        std::cout << "No subscriptions to unsubscribe from." << std::endl;
    }
    else {
        // Gửi thông điệp hủy đăng ký (UNSUBSCRIBE) cho tất cả các hàng đợi
        for (const auto& [data_type, queues] : this->subscriptions) {
            std::cout << "DataType: " << (data_type == DataType::VOLUME ? "VOLUME" : "TEMPERATURE") << std::endl;
            if (queues.empty()) {
                std::cout << "  No queues subscribed for this DataType. (No need to send Exit message)" << std::endl;
            }
            else{
                for (const auto& queue : queues) {
                    // Gửi thông điệp hủy đăng ký
                    send_message_subscription(queue.c_str(), this->service_queue_name, CommandType::EXIT, data_type);
                }
            }
        }
    }
}

// void Service::unsubscribe_all(){
//     if (this->subscriptions.empty()) {
//       std::cout << "No subscriptions to unsubscribe from." << std::endl;
//     }
//     else {
//       // Gửi thông điệp hủy đăng ký (UNSUBSCRIBE) cho tất cả các hàng đợi
//       for (const auto& [data_type, queues] : this->subscriptions) {
//           for (const auto& queue : queues) {
//               // Gửi thông điệp hủy đăng ký
//               send_message_subscription(queue.c_str(), this->service_queue_name, CommandType::EXIT, data_type);
//           }
//       }
//     }
// }
void Service::send_message_subscription(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type){
    createAndSendMessage(dest_queue, from_queue_name,  command_type, data_type);  
}

void Service::createAndSendMessage(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type, 
                                  std::optional<std::variant<Volume_t, Temperature_t>> data_value) {
    Message message_send;

    // Khởi tạo thông điệp
    if (data_value.has_value()) {
        initializeMessage(message_send, from_queue_name, command_type, data_type, data_value.value());
    } else {
        initializeMessage(message_send, from_queue_name, command_type, data_type);
    }

    // Mã hóa thông điệp thành chuỗi
    const char* encodedMessage = encodeMessage(message_send);

    /*
    // In ra thông điệp đã mã hóa để kiểm tra
    std::cout << "Encoded Message: " << encodedMessage << std::endl;
    */

    send(dest_queue, encodedMessage);
}

void Service::close_and_unlink_queue() {
    // Đóng và hủy hàng đợi sau khi gửi thông điệp hủy đăng ký
    if (this->mq >= 0) {
        if (mq_close(mq) == -1) {
            std::cerr << "Failed to close message queue: " << this->service_queue_name << " - Error: " << strerror(errno) << std::endl;
        } else {
            std::cout << "Message queue closed successfully: " << this->service_queue_name << std::endl;
        }

        if (mq_unlink(this->service_queue_name) == -1) {
            std::cerr << "Failed to unlink message queue: " << this->service_queue_name << " - Error: " << strerror(errno) << std::endl;
        } else {
            std::cout << "Message queue unlinked successfully: " << this->service_queue_name << std::endl;
        }
    }
}







