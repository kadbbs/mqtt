#pragma once
#include <mqtt/async_client.h>
#include <string>
#include <iostream>

class callback : public virtual mqtt::callback
{
    void message_arrived(mqtt::const_message_ptr msg) override
    {
        std::cout << "Message received: " << msg->get_payload_str() << std::endl;
    }
};

class Subscriber
{
private:
    std::string SERVER_ADDRESS;
    std::string CLIENT_ID;
    std::string TOPIC;

    mqtt::async_client client;
    callback cb;

public:
    Subscriber(std::string server = "broker.emqx.io:1883", std::string client_id = "cpp_publisher", std::string topic = "yun/topic") : SERVER_ADDRESS(server),
                                                                                                                                       CLIENT_ID(client_id), TOPIC(topic), client(SERVER_ADDRESS, CLIENT_ID)
    {
        client.set_callback(cb);
        connect();
    }
    ~Subscriber(){
        disconnect();
    };
    int connect()
    {
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);

        try
        {
            // Connect to EMQX broker
            client.connect(connOpts)->wait();
            std::cout << "Connected to EMQX broker" << std::endl;

        }
        catch (const mqtt::exception &exc)
        {
            std::cerr << "Error: " << exc.what() << std::endl;
            return 1;
        }
    }

    void subtopic()
    {
        // Subscribe to topic
        client.subscribe(TOPIC, 1)->wait();
        std::cout << "Subscribed to topic: " << TOPIC << std::endl;

    }
    void subtopic(std::string topic, int qos=1)
    {
        // Subscribe to topic
        client.subscribe(topic, 1)->wait();
        std::cout << "Subscribed to topic: " << TOPIC << std::endl;

        // Keep running to receive messages
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
    }

    void disconnect()
    {
        client.disconnect()->wait();
        std::cout << "Disconnected" << std::endl;
    }
};

