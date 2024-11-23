#ifndef LIB_SERVICE_H
#define LIB_SERVICE_H

#include <mqueue.h>
#include <string>
#include <iostream>
#include <signal.h>  
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <optional> // std::optional

enum class DataType { TEMPERATURE, VOLUME };
enum class CommandType { SET, GET, EXIT, SUBSCRIBE, UNSUBSCRIBE, RESPOND_GETDATA, RESPOND_SUBSCRIBE, NOTIFY };

struct Volume_t {
    int volume;
};

struct Temperature_t {
    int temperature;
};

struct Message {
    char from_queue_name[30];      // Hàng đợi đích
    CommandType command_type;
    DataType data_type;           // Loại dữ liệu (vd: nhiệt độ, âm lượng, ...)
    std::variant<Volume_t, Temperature_t> data;
};

void initializeMessage(Message &message, const char *from_queue_name, CommandType command_type, DataType data_type, std::variant<Volume_t, Temperature_t> data_value);
void initializeMessage(Message &message, const char *from_queue_name, CommandType command_type, DataType data_type);

const char* encodeMessage(const Message& message);
bool decodeMessage(const char* encodedMessage, Message& message);

class Service {
public:
    Service(const char* service_queue_name);
    ~Service();
    void setupNotification(); 
    void exit_ipc();
private:
    mqd_t mq;
    const char* service_queue_name;
    std::string data_receive;
    Volume_t volume_data;
    Temperature_t temperature_data;
    // Sử dụng unordered_set để lưu danh sách các app client theo DataType
    std::unordered_map<DataType, std::unordered_set<std::string>> subscriptions;
    bool releaseResources;

    void handleCommand(const Message& message);
    void handleGet(const Message& message_receive);
    void handleSet(const Message& message_receive);
    void handleSubscribe(const Message& message_receive);
    void handleUnsubscribe(const Message& message_receive);
    void printSubscriptions() const;
    void readAndHandleMessage();

    void unsubscribe_all();
    void send_message_subscription(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type);
    void createAndSendMessage(const char* dest_queue, const char* from_queue_name, CommandType command_type, DataType data_type, 
                                  std::optional<std::variant<Volume_t, Temperature_t>> data_value = std::nullopt);
    void close_and_unlink_queue();


    void send(const char* dest_queue, const char* message);
    ssize_t read_mq();

    //--------------
    void send_to_mq_client(const std::string& client_queue, const std::string& message); // DÙNG ĐỂ TEST
    void receive(); // DÙNG ĐỂ TEST
    Volume_t getVolume() const; // DÙNG ĐỂ TEST
};

#endif
