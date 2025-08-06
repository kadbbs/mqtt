#include <iostream>
#include <cstring> // for memcpy
#include <iomanip> // for std::hex, std::setw
#include <bits/stdc++.h> // for std::vector

float hexBytesToFloat(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    // 将 4 个字节合并为一个 32 位整数
    uint32_t hexValue = (static_cast<uint32_t>(b0) << 24) |
                        (static_cast<uint32_t>(b1) << 16) |
                        (static_cast<uint32_t>(b2) << 8)  |
                        (static_cast<uint32_t>(b3));

    // 将 32 位整数解释为 float（IEEE 754）
    float result;
    memcpy(&result, &hexValue, sizeof(float)); // 避免类型双关（type-punning）问题

    return result;
}

int main() {
    // 示例：将 4 个字节转换为 float

    uint32_t hexValue = 0x41E7A950;
    uint8_t hexarray[4]= {0xf1,0x7d,0xa0,0x42};
    float floatValue;
    memcpy(&floatValue, hexarray, sizeof(float)); // 避免类型双关（type-punning）问题
    std::cout << "\nFloat Value: " << floatValue << std::endl;


    return 0;
}