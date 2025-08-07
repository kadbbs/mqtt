# 编译器设置
CROSS_COMPILE = aarch64-rockchip-linux-gnu-
CXX = $(CROSS_COMPILE)g++
CC = $(CROSS_COMPILE)gcc

# 项目设置
TARGET = sen_mqtt
SRC = main.cpp

# 头文件路径 (根据实际路径修改)
INC_DIRS = \
    -I result_dir/ssl_result/include \
    -I result_dir/paho.mqtt.cpp_result/include \
    -I result_dir/paho.mqtt.c_result/include \

# 库文件路径 (根据实际路径修改)
LIB_DIRS = \
    -L result_dir/ssl_result/lib \
    -L result_dir/paho.mqtt.cpp_result/lib \
    -L result_dir/paho.mqtt.c_result/lib

# 链接库
LIBS = \
    -lpaho-mqttpp3 \
    -lpaho-mqtt3a \
    -lssl \
    -lcrypto \
    -lpthread\
	

# 编译选项
CXXFLAGS = \
    -std=c++11 \
    -Wall \
    -g \
    $(INC_DIRS)

LDFLAGS = \
    $(LIB_DIRS) \
    $(LIBS) \
    -Wl,-rpath=/userdata/.local/lib

# 构建规则
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o bin/$@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

# 安装到用户目录
install: $(TARGET)
	install -D $(TARGET) $(HOME)/.local/bin/$(TARGET)

scp:
	scp out/build/linux_arm64/sen_mqtt root@kf:/userdata/yun/exec

binscp:
	scp -p bin/sen_mqtt root@kf:/userdata/yun/exec

.PHONY: all clean install scp


