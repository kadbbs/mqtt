#pragma once
#include <mqtt/async_client.h>
#include <string>
#include <iostream>
#include "ElegantLog.hpp"
class callback : public virtual mqtt::callback
{
    void message_arrived(mqtt::const_message_ptr msg) override
    {
        LOG_INFO("Message arrived: {}", msg->get_payload_str());
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
            LOG_INFO("Connected to EMQX broker");

        }
        catch (const mqtt::exception &exc)
        {
            LOG_ERROR("Error: {}", exc.what());
            return 1;
        }
        return -1;
    }

    void subtopic()
    {
        // Subscribe to topic
        client.subscribe(TOPIC, 1)->wait();
        LOG_INFO("Subscribed to topic: {}", TOPIC);

    }
    void subtopic(std::string topic, int qos=1)
    {
        // Subscribe to topic
        client.subscribe(topic, qos)->wait();
        LOG_INFO("Subscribed to topic: {}", topic);

        // Keep running to receive messages
        LOG_INFO("Press Enter to exit...");
        std::cin.get();
    }

    void disconnect()
    {
        client.disconnect()->wait();
        LOG_INFO("Disconnected from EMQX broker");
    }
};

