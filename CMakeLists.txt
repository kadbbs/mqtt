cmake_minimum_required(VERSION 3.10.0)
project(sen_mqtt VERSION 0.1.0 LANGUAGES C CXX)


add_executable(sen_mqtt main.cpp)
# 设置C++标准
set_target_properties(sen_mqtt PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
# 设置RPATH以找到共享库
target_link_options(sen_mqtt PRIVATE
    "-Wl,-rpath=/userdata/.local/lib"
    "-g"
)
# 包含头文件路径
target_include_directories(sen_mqtt
    PRIVATE 
        ${PROJECT_SOURCE_DIR}/result_dir/ssl_result/include
        ${PROJECT_SOURCE_DIR}/result_dir/paho.mqtt.cpp_result/include
        ${PROJECT_SOURCE_DIR}/result_dir/paho.mqtt.c_result/include
)

# 指定库搜索路径
target_link_directories(sen_mqtt
    PRIVATE
        ${PROJECT_SOURCE_DIR}/result_dir/ssl_result/lib
        ${PROJECT_SOURCE_DIR}/result_dir/paho.mqtt.cpp_result/lib
        ${PROJECT_SOURCE_DIR}/result_dir/paho.mqtt.c_result/lib
)

# 链接库
target_link_libraries(sen_mqtt
    PRIVATE
        paho-mqttpp3
        paho-mqtt3a
        ssl
        crypto
        pthread
)

