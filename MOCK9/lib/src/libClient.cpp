#include "libClient.h"
#include <cerrno>  // dùng errno
#include <cstring> // dùng strerror(errno)
#include <fcntl.h> // Thêm thư viện cho O_NONBLOCK
#include <cstdlib> // exit
#include <mqueue.h>
#include <sys/types.h>
#include <variant>
#include <sstream>  // Dùng để xử lý chuỗi
#include <ctime>  // Để sử dụng struct timespec
#include <limits> // std::numeric_limits

#include <thread>
#include <chrono>

void initializeMessage(Message &message, const char *from_queue_name, CommandType command_type, DataType data_type, std::variant<Volume_t, Temperature_t> data_value) {
    // Gán chuỗi cho src_queue_name
    strncpy(message.from_queue_name, from_queue_name, sizeof(message.from_queue_name) - 1); 
    message.from_queue_name[sizeof(message.from_queue_name) - 1] = '\0';  // Đảm bảo chuỗi kết thúc bằng ký tự null

    // Gán giá trị cho command_type
    message.command_type = command_type;

    // Gán giá trị cho data_type
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



// Hàm mã hóa Message thành const char*
const char* encodeMessage(const Message& message) {
    static char buffer[256];  // Một buffer tạm thời đủ lớn để chứa toàn bộ thông tin struct
    std::memset(buffer, 0, sizeof(buffer)); // Đảm bảo buffer không chứa giá trị cũ

    // Khởi tạo phần đầu buffer với thông tin cơ bản
    std::sprintf(buffer, "%s,%d,%d,", 
                 message.from_queue_name, 
                 static_cast<int>(message.command_type), 
                 static_cast<int>(message.data_type));

    // Nếu command_type là GET hoặc SUBSCRIBE hoặc UNSUBCRIBE,...(ngoại trừ CommandType::SET), thì không mã hóa dữ liệu message.data
    if (message.command_type == CommandType::SET) {
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

    // Nếu command_type là SET, thì phân giải dữ liệu message.data
    if ((message.command_type == CommandType::SET) || (message.command_type == CommandType::RESPOND_GETDATA) || (message.command_type == CommandType::NOTIFY)) {
        // Gán giá trị data
        std::getline(ss, token, ',');  // data_value
        data_value = std::stoi(token);  // Chuyển đổi từ chuỗi sang số nguyên
        // Giải mã dữ liệu vào đúng kiểu dữ liệu tương ứng
        if (message.data_type == DataType::VOLUME) {
            message.data = Volume_t{data_value};            // Gán vào Volume_t
        } else if (message.data_type == DataType::TEMPERATURE) {
            message.data = Temperature_t{data_value};  // Gán vào Temperature_t
        } else {
            return false;  // Nếu data_type không hợp lệ
        }
    }
    return true;  // Giải mã thành công
}

Client::Client(const std::string& queue_name, DataType data_type) 
    : client_queue_name(queue_name), data_type(data_type){
    
    this->releaseResources = false;

    struct mq_attr attr = {
      .mq_flags = 0,     
      .mq_maxmsg = 100, 
      .mq_msgsize = 256, 
      .mq_curmsgs = 0   
    };

    this->mqd_client = mq_open(queue_name.c_str(), O_CREAT | O_RDONLY, 0644, &attr);
    if (mqd_client == -1) {
      std::cerr << "mq_open failed: " << queue_name << " " << strerror(errno) << std::endl;
    exit(EXIT_FAILURE);
    } else {
      std::cout << "Message queue '" << queue_name << "' opened successfully for writing." << std::endl;
    }
}

Client::~Client() {
  
  if (releaseResources == false) {
      if (mq_close(mqd_client) == -1) {
          std::cerr << "Failed to close message queue: " << client_queue_name << " - Error: " << strerror(errno) << std::endl;
      } else {
          std::cout << "Message queue closed successfully: " << client_queue_name << std::endl;
      }

      if (mq_unlink(client_queue_name.c_str()) == -1) {
          std::cerr << "Failed to unlink message queue: " << client_queue_name << " - Error: " << strerror(errno) << std::endl;
      } else {
          std::cout << "Message queue unlinked successfully: " << client_queue_name << std::endl;
      }
  }
  std::cout << "Destructor has been called.\n";
}

void Client::setTemperature(int temp) {
    if (data_type == DataType::TEMPERATURE) {
        std::get<Temperature_t>(data).temperature = temp;
        std::cout << "Temperature has been set to: " << std::get<Temperature_t>(data).temperature << std::endl;
    } else {
        std::cout << "Invalid data type: Expected TEMPERATURE" << std::endl;
        exit(1);
    }
}

int Client::getTemperature() const {
    if (data_type == DataType::TEMPERATURE) {
        return std::get<Temperature_t>(data).temperature;
    } else {
        std::cout << "Invalid data type: Expected TEMPERATURE" << std::endl;
        exit(1);
    }
}

void Client::setVolume(int vol) {
    if (data_type == DataType::VOLUME) {
        std::get<Volume_t>(data).volume = vol;
        std::cout << "Volume has been set to: " << std::get<Volume_t>(data).volume<< " %" << std::endl;
    } else {
        std::cout <<"Invalid data type: Expected VOLUME" << std::endl;
        exit(1);
    }
}

int Client::getVolume() const {
    if (data_type == DataType::VOLUME) {
        return std::get<Volume_t>(data).volume;
    } else {
        std::cout <<"Invalid data type: Expected VOLUME" << std::endl;
        exit(1);
    }
}

// Phương thức in ra giá trị của `std::variant<Volume_t, Temperature_t> data`
void Client::printData() const {
    // Sử dụng std::visit để in giá trị tùy thuộc vào kiểu của variant
    std::visit([](auto&& arg) {
        if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, Volume_t>) {
            std::cout << "Volume: " << arg.volume << " %" << std::endl;
        } else if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, Temperature_t>) {
            std::cout << "Temperature: " << arg.temperature << " C" << std::endl;
        }
    }, data);
}

void Client::send(const char* dest_queue, const char* message) {

    mqd_t mqd_dest = mq_open(dest_queue, O_WRONLY | O_NONBLOCK);
    if (mqd_dest == -1) {
        std::cerr << "mq_open for " << dest_queue << " (for destination queue) failed. Error: " << strerror(errno) << std::endl;
        exit(1);
    } else {
        /*
        std::cout << "Message queue '" << dest_queue << "' (destination queue) opened successfully for writing" << std::endl;
        */
    }

    // Gửi thông điệp tới message queue đích
    if (mq_send(mqd_dest, message, strlen(message), 0) == -1) {
        perror("mq_send failed");
        std::cerr << "mq_send for " << dest_queue << " failed. Error: " << strerror(errno) << std::endl;
    } else {
        std::cout << "Message sent to destination queue '" << dest_queue << "'" <<" is: "<< message << std::endl;
    }

    mq_close(mqd_dest);  // Đóng message queue đích sau khi gửi
}

  void Client::setData(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type, std::variant<Volume_t, Temperature_t> data_value){

    createAndSendMessage(dest_queue, from_queue_name,  command_type, data_type, data_value);
    /*
    Message message_send;
    
    initializeMessage(message_send, from_queue_name, command_type, data_type, data_value);

    // Mã hóa message thành chuỗi
    const char* encodedMessage = encodeMessage(message_send);

    // In ra thông điệp đã mã hóa // ĐỂ TEST THỬ
    std::cout << "Encoded Message: " << encodedMessage << std::endl; // ĐỂ TEST THỬ

    send(dest_queue, encodedMessage);
    */
  }

void Client::createAndSendMessage(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type, 
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

    // Gửi thông điệp đến hàng đợi đích
    send(dest_queue, encodedMessage);
}

  void Client::set(){
    std::string dest_queue;


    // Nhập thông tin từ người dùng
    std::cout << "Enter destination queue (e.g., /service_queue): ";
    std::getline(std::cin, dest_queue);

    /*
    int data_type_input;
    std::cout << "Enter data type (0 for TEMPERATURE, 1 for VOLUME): ";
    std::cin >> data_type_input;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Xử lý buffer
    
    // Chuyển đổi giá trị nhập thành enum
    DataType data_type = static_cast<DataType>(data_type_input);
    */

    // Nhập giá trị cho Volume_t hoặc Temperature_t
    std::variant<Volume_t, Temperature_t> data_value;

    if (this->data_type == DataType::VOLUME) {
        Volume_t volume;
        std::cout << "Enter volume value: ";
        std::cin >> volume.volume;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Xử lý buffer
        data_value = volume;
    } else if (this->data_type == DataType::TEMPERATURE) {
        Temperature_t temperature;
        std::cout << "Enter temperature value: ";
        std::cin >> temperature.temperature;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Xử lý buffer
        data_value = temperature;
    } else {
        std::cerr << "Invalid data type!" << std::endl;
        return;
    }

    // Gọi hàm setData với các giá trị vừa nhập
    setData(dest_queue.c_str(), this->client_queue_name.c_str(), CommandType::SET, this->data_type, data_value);
    std::this_thread::sleep_for(std::chrono::seconds(1));  // Chờ xử lý
  }



  void Client::getData_timeout_1s(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type){
    
    createAndSendMessage(dest_queue, from_queue_name,  command_type, data_type);
    /*
    Message message_sent;
    
    initializeMessage(message_sent, from_queue_name,command_type, data_type);

    // Mã hóa message thành chuỗi
    const char* encodedMessage = encodeMessage(message_sent);

    // In ra thông điệp đã mã hóa // ĐỂ TEST THỬ
    std::cout << "Encoded Message: " << encodedMessage << std::endl; // ĐỂ TEST THỬ

    send(dest_queue, encodedMessage);
    */
    receive_timeout_1s();
  }

ssize_t Client::read_mq_timeout_1s(){
  char buffer[256];
  ssize_t bytes_read = -1;

  struct timespec timeout;
  clock_gettime(CLOCK_REALTIME, &timeout);  // Lấy thời gian hiện tại
  timeout.tv_sec += 1;  // Thêm 1 giây vào thời gian hiện tại

  // Đọc một tin nhắn từ hàng đợi tin nhắn
  bytes_read = mq_timedreceive(this->mqd_client, buffer, sizeof(buffer), nullptr, &timeout);
  
  if (bytes_read >= 0) {
    buffer[bytes_read] = '\0'; // Kết thúc chuỗi

    // Gán dữ liệu vào thuộc tính data_receive của client
    this->data_receive = std::string(buffer);

    /*
    std::cout << "char[] " << strlen(buffer)<< std::endl;
    */
    std::cout << "Client: Received data: " << buffer << std::endl;

  }else if (errno == ETIMEDOUT) {
        std::cerr << "Timeout: No message received within 1 second" << std::endl;
  }
  else {
    perror("mq_timedreceive failed");
  }

  return bytes_read;
}

ssize_t Client::read_mq(){
  char buffer[256];
  ssize_t bytes_read = -1;

  // Đọc một tin nhắn từ hàng đợi tin nhắn
  bytes_read = mq_receive(this->mqd_client, buffer, sizeof(buffer), nullptr);
  if (bytes_read >= 0) {
    buffer[bytes_read] = '\0'; // Kết thúc chuỗi

    // Gán dữ liệu vào thuộc tính data_receive của client
    this->data_receive = std::string(buffer);

    std::cout << "char[] " << strlen(buffer)<< std::endl;
    //std::cout << "string " << data_receive.length() << std::endl;

    std::cout << "Client: Received data: " << buffer << std::endl;
    //std::cout << "Service: Received data: " << data << std::endl;
  } else if (errno == EAGAIN) {
    // Nếu queue trống, trả về thông báo hoặc xử lý an toàn
    std::cout << "Client: Queue is empty. No messages available." << std::endl;
  }
  else {
    perror("Client: mq_receive failed");
  }
  return bytes_read;
}

  // Phương thức getter trả về dữ liệu hiện tại trong biến `data_receive`
  std::string Client::receivedMessage() const {
    return data_receive;
  }

void Client::receive(){
  ssize_t bytes_read = read_mq();
  if (bytes_read >= 0) {
    receivedMessage();
    Message message_receive;
    decodeMessage(this->data_receive.c_str(), message_receive);
    handleCommand(message_receive);
  }
}

void Client::receive_timeout_1s(){
  ssize_t bytes_read = read_mq_timeout_1s();
  /*
  std::cout << "called receive_timeout_1s()" << std::endl;
  */
  if (bytes_read >= 0) {    // Nếu nhận dữ liệu thành công thì tiếp tục
    Message message_receive;
    decodeMessage(this->data_receive.c_str(), message_receive);
    handleCommand(message_receive);
  }
}

void Client::readAndHandleMessage() {
    std::cout << std::endl;
    struct mq_attr attr;

    // Lấy thuộc tính của hàng đợi
    if (mq_getattr(this->mqd_client, &attr) == -1) {
        perror("Client: mq_getattr failed");
        return;
    }

    // Kiểm tra số lượng tin nhắn hiện có
    while (attr.mq_curmsgs > 0) {

      //ssize_t bytes_read = read_mq();
      ssize_t bytes_read = read_mq_timeout_1s();
      if (bytes_read >= 0) {
          Message message_receive;
          decodeMessage(this->data_receive.c_str(), message_receive);
          handleCommand(message_receive);
      }else {
            perror("Client: Error reading from message queue");
            break;
        }

        // Cập nhật lại thuộc tính để kiểm tra số lượng tin nhắn còn lại
        if (mq_getattr(this->mqd_client, &attr) == -1) {
            perror("Client: mq_getattr failed during loop");
            break;
        }

    }
    // Xử lý lại thông báo sau khi đọc
    setupNotification();
}


void Client::setupNotification() {
    struct sigevent sev{};
    sev.sigev_notify = SIGEV_THREAD;  // Thông báo qua một thread riêng
    sev.sigev_notify_function = [](union sigval sv) {
        Client* client = static_cast<Client*>(sv.sival_ptr);
        client->readAndHandleMessage();  // Gọi hàm xử lý tin nhắn
    };
    sev.sigev_notify_attributes = nullptr;  // Sử dụng thread mặc định
    sev.sigev_value.sival_ptr = this;  // Truyền con trỏ tới Service hiện tại

    if (mq_notify(mqd_client, &sev) == -1) {
        perror("Client: mq_notify failed");
    } else {
        /*
        std::cout << "Client: Notification setup successfully for queue '" << client_queue_name << "'." << std::endl;
        */
    }
}
void Client::handleCommand(const Message& message_receive) {
  switch (message_receive.command_type) {
      case CommandType::GET:
          //handleGet(message_receive);  // Gọi hàm xử lý GET
          std::cerr << "Client does not handle GET requests.\n";
          break;

      case CommandType::SET:
          //handleSet();  // Gọi hàm xử lý SET
          std::cerr << "Client does not handle SET requests.\n";
          break;

      case CommandType::EXIT:
          handleExit(message_receive);  // Gọi hàm xử lý EXIT
          break;

      case CommandType::RESPOND_GETDATA:
          handle_Respond_Get_Data(message_receive);  // Gọi hàm xử lý RESPOND_GETDATA
          break;

      case CommandType::NOTIFY:
          handle_Notify(message_receive);  // Gọi hàm xử lý NOTIFY
          break;

      case CommandType::RESPOND_SUBSCRIBE:
          handle_Respond_Subscribe(message_receive);  // Gọi hàm xử lý NOTIFY
          break;

      case CommandType::SUBSCRIBE:
          //handleSubscribe();  // Gọi hàm xử lý SUBSCRIBE
          std::cerr << "Client does not handle SUBSCRIBE requests.\n";
          break;

      case CommandType::UNSUBSCRIBE:
          //handleUnsubscribe();  // Gọi hàm xử lý UNSUBSCRIBE
          std::cerr << "Client does not handle UNSUBSCRIBE requests.\n";
          break;

      default:
          std::cerr << "Unknown command type!\n";
          break;
  }
}

void Client::handle_Respond_Get_Data(const Message& message_receive){
  if (message_receive.data_type == DataType::VOLUME) {
    // Lấy giá trị từ message_receive.data (Volume_t) và gán vào std::variant data của Client
    const Volume_t& volume = std::get<Volume_t>(message_receive.data);
    data = volume;  // Gán giá trị Volume_t vào std::variant
    std::cout << "Updated Volume: " << volume.volume << std::endl;  
  }
  else if (message_receive.data_type == DataType::VOLUME){
    // Lấy giá trị từ message_receive.data (Temperature_t) và gán vào std::variant data của Client
    const Temperature_t& temperature = std::get<Temperature_t>(message_receive.data);
    data = temperature;  // Gán giá trị Temperature_t vào std::variant
    std::cout << "Updated Temperature: " << temperature.temperature << std::endl;
  }
  else {
      std::cerr << "Error: Unknown DataType in message!" << std::endl;
  }

}

void Client::handle_Notify(const Message& message_receive){
  if (message_receive.data_type == DataType::VOLUME) {
    // Lấy giá trị từ message_receive.data (Volume_t) và gán vào std::variant data của Client
    const Volume_t& volume = std::get<Volume_t>(message_receive.data);
    data = volume;  // Gán giá trị Volume_t vào std::variant
    std::cout << "Updated Volume: " << volume.volume << std::endl;  
  }
  else if (message_receive.data_type == DataType::TEMPERATURE){
    // Lấy giá trị từ message_receive.data (Temperature_t) và gán vào std::variant data của Client
    const Temperature_t& temperature = std::get<Temperature_t>(message_receive.data);
    data = temperature;  // Gán giá trị Temperature_t vào std::variant
    std::cout << "Updated Temperature: " << temperature.temperature << std::endl;
  }
  else {
      std::cerr << "Error: Unknown DataType in message!" << std::endl;
  }

}

void Client::handle_Respond_Subscribe(const Message& message_receive){
  storeSubscription(message_receive.from_queue_name, message_receive.data_type);
}

void Client::get(){
  std::string dest_queue;
  
  // Nhập thông tin từ người dùng
  std::cout << "Enter destination queue (e.g., /service_queue): ";
  std::getline(std::cin, dest_queue);

  /*
  std::string from_queue_name;
  std::cout << "Enter source queue name (e.g., /volume_queue, /temperature_queue): ";
  std::getline(std::cin, from_queue_name);
  */

  /*
  int data_type_input;
  std::cout << "Enter data type (0 for TEMPERATURE, 1 for VOLUME): ";
  std::cin >> data_type_input;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Xử lý buffer
  std::cout << std::endl;

  DataType data_type = static_cast<DataType>(data_type_input);
  */
  
  getData_timeout_1s(dest_queue.c_str(), this->client_queue_name.c_str(), CommandType::GET, this->data_type);

}

void Client::send_message_subscription(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type){
    createAndSendMessage(dest_queue, from_queue_name,  command_type, data_type);
    /*
    Message message_send;
    initializeMessage(message_send, from_queue_name, command_type, data_type);
    // Mã hóa message thành chuỗi
    const char* encodedMessage = encodeMessage(message_send);
    // In ra thông điệp đã mã hóa // ĐỂ TEST THỬ
    std::cout << "Encoded Message: " << encodedMessage << std::endl; // ĐỂ TEST THỬ

    send(dest_queue, encodedMessage);
    */
    
}

void Client::subscribe(){
  std::string dest_queue;

  // Nhập thông tin từ người dùng
  std::cout << "Enter destination queue (e.g., /service_queue): ";
  std::getline(std::cin, dest_queue);

  /*
  int data_type_input;
  std::cout << "Enter data type (0 for TEMPERATURE, 1 for VOLUME): ";
  std::cin >> data_type_input;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Xử lý buffer
  std::cout << std::endl;

  // Kiểm tra giá trị nhập vào có hợp lệ không
  DataType data_type = static_cast<DataType>(data_type_input);
  */
  send_message_subscription(dest_queue.c_str(), client_queue_name.c_str(), CommandType::SUBSCRIBE, this->data_type);
  std::this_thread::sleep_for(std::chrono::seconds(1));  // Chờ xử lý
}

void Client::send_message_unsubscribe(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type){
    
    createAndSendMessage(dest_queue, from_queue_name,  command_type, data_type);
    /*
    Message message_send;
    
    initializeMessage(message_send, from_queue_name, command_type, data_type);

    // Mã hóa message thành chuỗi
    const char* encodedMessage = encodeMessage(message_send);

    // In ra thông điệp đã mã hóa // ĐỂ TEST THỬ
    std::cout << "Encoded Message: " << encodedMessage << std::endl; // ĐỂ TEST THỬ

    send(dest_queue, encodedMessage);
    */
}

void Client::storeSubscription(const std::string& dest_queue, DataType data_type) {
    // Lưu thông tin dịch vụ vào danh sách đăng ký của client
    if (data_type == DataType::VOLUME) {
      subscriptions[DataType::VOLUME].insert(dest_queue);
      // In ra thông báo đăng ký thành công
      std::cout << "Client " << this->client_queue_name << " has subscribed to queue " 
                << dest_queue << " with DataType " << "DataType::VOLUME" << std::endl;
      printClientSubscriptions();
    }
    else if (data_type == DataType::TEMPERATURE){
      subscriptions[DataType::TEMPERATURE].insert(dest_queue);
      std::cout << "Client " << this->client_queue_name << " has subscribed to queue " 
                << dest_queue << " with DataType " << "DataType::TEMPERATURE" << std::endl;
      printClientSubscriptions();
    }
    else {
        std::cerr << "Error: Unknown DataType in message!" << std::endl;
    }

}

void Client::printClientSubscriptions() const {
    std::cout << std::endl;
    std::cout << "List of subscriptions for client " << this->client_queue_name << ": " << std::endl;

    if (subscriptions.empty()) {
      std::cout << "  No active subscriptions." << std::endl;
    } else {
      for (const auto& [data_type, queues] : subscriptions) {
        std::cout << "DataType: " << (data_type == DataType::VOLUME ? "VOLUME" : "TEMPERATURE") << std::endl;

        if (queues.empty()) {
          std::cout << "  No queues subscribed for this DataType." << std::endl;
        } else{
          for (const auto& queue : queues) {
              std::cout << "  - " << queue << std::endl;
          }
        }
      }
    }
    std::cout << std::endl;
}

// void Client::printClientSubscriptions() const {
//     std::cout << std::endl;
//     std::cout << "List of subscriptions for client " << this->client_queue_name << ": " << std::endl;
//     for (const auto& [data_type, queues] : subscriptions) {
//         std::cout << "DataType: " << (data_type == DataType::VOLUME ? "VOLUME" : "TEMPERATURE") << std::endl;
//         for (const auto& queue : queues) {
//             std::cout << "  - " << queue << std::endl;
//         }
//     }
//     std::cout << std::endl;
// }
void Client::unsubscribe(){
  std::string dest_queue;
  
  // Nhập thông tin từ người dùng
  std::cout << "Enter destination queue (e.g., /service_queue): ";
  std::getline(std::cin, dest_queue);
  
  /*
  int data_type_input;
  std::cout << "Enter data type (0 for TEMPERATURE, 1 for VOLUME): ";
  std::cin >> data_type_input;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Xử lý buffer
  std::cout << std::endl;

  // Kiểm tra giá trị nhập vào có hợp lệ không
  DataType data_type = static_cast<DataType>(data_type_input);
  */
  send_message_subscription(dest_queue.c_str(), this->client_queue_name.c_str(), CommandType::UNSUBSCRIBE, this->data_type);
}

void Client::handleExit(const Message& message_receive){
  // Lấy tên hàng đợi (from_queue_name) từ message_receive
  std::string queue_name(message_receive.from_queue_name);

  // Kiểm tra loại dữ liệu và xóa khỏi danh sách đăng ký
  if (message_receive.data_type == DataType::VOLUME) {
      // Tìm và xóa khỏi danh sách VOLUME
      auto& volume_services = subscriptions[DataType::VOLUME];
      if (volume_services.erase(queue_name) > 0) {
          std::cout << "Client " << this->client_queue_name << " successfully unsubscribed from VOLUME updates for queue " << queue_name << "." << std::endl;
      } else {
          std::cout << "Queue " << queue_name << " was not subscribed to VOLUME updates for client " << this->client_queue_name << "." << std::endl;
      }
      printClientSubscriptions();
  } else if (message_receive.data_type == DataType::TEMPERATURE) {
      // Tìm và xóa khỏi danh sách TEMPERATURE
      auto& temperature_services = subscriptions[DataType::TEMPERATURE];
      if (temperature_services.erase(queue_name) > 0) {
          std::cout << "Client " << this->client_queue_name << " successfully unsubscribed from TEMPERATURE updates for queue " << queue_name << "." << std::endl;
      } else {
          std::cout << "Queue " << queue_name << " was not subscribed to TEMPERATURE updates for client " << this->client_queue_name << "." << std::endl;
      }
      printClientSubscriptions();
  } else {
      std::cerr << "Error: Unknown DataType received in the message for client " << this->client_queue_name << "." << std::endl;
  }

}

void Client::exit_ipc(){
  unsubscribe_all(); // Gửi thông điệp hủy đăng ký
  close_and_unlink_queue(); // Đóng và hủy hàng đợi
  this->releaseResources = true;

}

void Client::unsubscribe_all() {
  if (this->subscriptions.empty()) {
    std::cout << "No subscriptions to unsubscribe from." << std::endl;
  }
  else {
    // Gửi thông điệp hủy đăng ký (UNSUBSCRIBE) cho tất cả các hàng đợi
    for (const auto& [data_type, queues] : this->subscriptions) {
      if (queues.empty()) {
        std::cout << "  No queues subscribed for this DataType. (No need to send Exit message)" << std::endl;
      }
      else {
        for (const auto& queue : queues) {
          // Gửi thông điệp hủy đăng ký
          send_message_subscription(queue.c_str(), this->client_queue_name.c_str(), CommandType::UNSUBSCRIBE, data_type);
        }
      }
    }
  }
}

void Client::close_and_unlink_queue() {
    // Đóng và hủy hàng đợi sau khi gửi thông điệp hủy đăng ký
    if (this->mqd_client >= 0) {
        if (mq_close(this->mqd_client) == -1) {
            std::cerr << "Failed to close message queue: " << this->client_queue_name << " - Error: " << strerror(errno) << std::endl;
        } else {
            std::cout << "Message queue closed successfully: " << this->client_queue_name << std::endl;
        }

        if (mq_unlink(this->client_queue_name.c_str()) == -1) {
            std::cerr << "Failed to unlink message queue: " << this->client_queue_name << " - Error: " << strerror(errno) << std::endl;
        } else {
            std::cout << "Message queue unlinked successfully: " << this->client_queue_name << std::endl;
        }
    }
}























