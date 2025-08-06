#include <string>
#include <iostream>
#include "publisher.hpp"
#include "subscriber.hpp"

int main(int argc, char const *argv[])
{
    Publisher publisher("broker.emqx.io:1883", "cpp_publisher", "yun/topic");
    Subscriber subscriber("broker.emqx.io:1883", "cpp_subscriber", "yun/topic");

    try
    {

        // Subscribe to the topic and wait for messages
        subscriber.subtopic();
        // Publish a message
        publisher.publish("Hello, MQTT!");
        while(1){}
    }
    catch (const mqtt::exception &exc)
    {
        std::cerr << "Error: " << exc.what() << std::endl;
        return 1;
    }

    return 0;
}
