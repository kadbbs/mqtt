#pragma once

#include <mqtt/async_client.h>
#include <string>
#include <iostream>


class Publisher
{

private:
    std::string SERVER_ADDRESS;
    std::string CLIENT_ID;
    std::string TOPIC;

    mqtt::async_client client;

public:
    Publisher(std::string server="broker.emqx.io:1883",std::string client_id="cpp_publisher",std::string topic="yun/topic") :SERVER_ADDRESS(server),
                                    CLIENT_ID(client_id),TOPIC(topic),client(SERVER_ADDRESS, CLIENT_ID) {}

    void connect()
    {
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);

        try
        {
            client.connect(connOpts)->wait();
            std::cout << "Connected to EMQX broker" << std::endl;
        }
        catch (const mqtt::exception &exc)
        {
            std::cerr << "Error: " << exc.what() << std::endl;
            throw;
        }
    }

    void publish(const std::string &payload)
    {
        mqtt::message_ptr pubmsg = mqtt::make_message(TOPIC, payload, 1, false);
        client.publish(pubmsg)->wait();
        std::cout << "Message published: " << payload << std::endl;
    }

    void disconnect()
    {
        client.disconnect()->wait();
        std::cout << "Disconnected" << std::endl;
    }
};


