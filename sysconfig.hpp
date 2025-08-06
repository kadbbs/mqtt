#pragma once

#include <bits/stdc++.h>
#include "nlohmann/json.hpp"
#include "frame_comm.hpp"

class sysconfig
{

    using json = nlohmann::json;

public:
    static sysconfig &instance()
    {

        static sysconfig instance("sysconf.json");
        return instance;
    }

    SerialConfig get_SerialConfig()
    {
        SerialConfig ret;
        ret.com = j["serial_config"]["com"];
        ret.baud_rate = j["serial_config"]["baud_rate"];
        ret.data_bits = j["serial_config"]["data_bits"];
        ret.parity = j["serial_config"]["parity"];
        ret.stop_bits = j["serial_config"]["stop_bits"];
        return ret;
    }

    void get_TimerConfig()
    {
        SerialConfig ret;
        ret.com = j["serial_config"]["com"];
        ret.baud_rate = j["serial_config"]["baud_rate"];
        ret.data_bits = j["serial_config"]["data_bits"];
        ret.parity = j["serial_config"]["parity"];
        ret.stop_bits = j["serial_config"]["stop_bits"];
    }


private:
    sysconfig(const std::string filename)
    {
        std::ifstream f(filename);
        if (!f.is_open())
        {
            throw std::runtime_error("无法打开配置文件 sysconfig.json");
        }
        j = json::parse(f);
    }

    json j;
};