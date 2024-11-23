#ifndef LIB_CLIENT_H
#define LIB_CLIENT_H

#include <mqueue.h>
#include <string>
#include <iostream>
#include <variant>
#include <signal.h>  
#include <unordered_map>
#include <unordered_set>
#include <optional> // std::optional

struct Volume_t {
    int volume;
};

struct Temperature_t {
    int temperature;
};

enum class DataType { TEMPERATURE, VOLUME };

enum class CommandType {SET, GET, EXIT, SUBSCRIBE, UNSUBSCRIBE, RESPOND_GETDATA, RESPOND_SUBSCRIBE, NOTIFY};

struct Message {
    char from_queue_name[30];      
    CommandType command_type;
    DataType data_type;           // Loại dữ liệu (vd: nhiệt độ, âm lượng, ...)
    std::variant<Volume_t, Temperature_t> data;
};

void initializeMessage(Message &message, const char *from_queue_name, CommandType command_type, DataType data_type, std::variant<Volume_t, Temperature_t> data_value);
void initializeMessage(Message &message, const char *from_queue_name, CommandType command_type, DataType data_type);

const char* encodeMessage(const Message& message);
bool decodeMessage(const char* encodedMessage, Message& message);

class Client {
public:
    Client(const std::string& queue_name, DataType data_type);
    ~Client();
         
    void get();
    void set();
    void subscribe();
    void unsubscribe();
    void exit_ipc();
    void setupNotification();

private:
    DataType data_type;           // Kiểu dữ liệu (TEMPERATURE hoặc VOLUME)
    std::variant<Volume_t, Temperature_t> data; // Sử dụng std::variant để lưu trữ Volume_t hoặc Temperature_t
    mqd_t mqd_client;
    std::string data_receive;
    std::string client_queue_name;
    bool releaseResources;
    std::unordered_map<DataType, std::unordered_set<std::string>> subscriptions;

    ssize_t read_mq(); // DÙNG ĐỂ TEST
    ssize_t read_mq_timeout_1s();
    void receive(); // DÙNG ĐỂ TEST
    void receive_timeout_1s();

    void getData_timeout_1s(const char* dest_queue, const char *from_queue_name, CommandType command_type, DataType data_type);

    void setData(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type, std::variant<Volume_t, Temperature_t> data_value);
    void send(const char* dest_queue, const char* message);
    void createAndSendMessage(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type, std::optional<std::variant<Volume_t, Temperature_t>> data_value = std::nullopt);
    std::string receivedMessage() const; // DÙNG ĐỂ TEST
    void handleCommand(const Message& message_receive);
    void handle_Respond_Get_Data(const Message& message_receive);
    void handle_Notify(const Message& message_receive);
    void handle_Respond_Subscribe(const Message& message_receive);
    void storeSubscription(const std::string& dest_queue, DataType data_type);
    void printClientSubscriptions() const;
    void handleExit(const Message& message_receive);


    void send_message_subscription(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type);
    void send_message_unsubscribe(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type);

    void unsubscribe_all();
    void close_and_unlink_queue();

    void readAndHandleMessage();

    void setTemperature(int temp); // Thiết lập và lấy dữ liệu tùy thuộc vào kiểu dữ liệu
    int getTemperature() const;
    void setVolume(int vol);
    int getVolume() const; 
    void printData() const; // Phương thức in ra giá trị hiện tại của std::variant
};

#endif
