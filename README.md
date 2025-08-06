

### mtqq 库
```shell
git clone https://github.com/emqx/nanomq.git 
cd nanomq
git submodule update --init --recursive
mkdir build && cd build
cmake .. 
make
```
构建选项
除了标准选项之外，还有一些使用 CMake 定义指定的配置选项，例如：CMAKE_BUILD_TYPE
```shell
-DNNG_ENABLE_QUIC=ON：构建具有 QUIC 桥接功能的 NanoMQ
-DNNG_ENABLE_TLS=ON：使用 TLS 支持进行构建。（需要提前安装 mbedTLS）
-DBUILD_CLIENT=OFF：禁用 NanoMQ Tools 客户端套件（包括 Pub / Sub / Conn ）
-DBUILD_ZMQ_GATEWAY=ON：使用 ZeroMQ 网关工具进行构建nanomq_cli
-DBUILD_NFTP=ON：使用 NFT 客户端构建nanomq_cli
-DBUILD_DDS_PROXY=ON：使用 DDS 客户端（代理/子/发布）进行构建nanomq_cli
-DBUILD_BENCH=ON：构建 MQTT 工作台nanomq_cli
-DENABLE_JWT=ON：为 HTTP 服务器构建 JWT 依赖项
-DNNG_ENABLE_SQLITE=ON：构建支持 SQLite 的 nanoMQ
-DBUILD_STATIC_LIB=ON：将 nanoMQ 构建为静态库
-DBUILD_SHARED_LIBS=ON：将 nanoMQ 构建为共享库
-DDEBUG=ON：启用调试标志
-DASAN=ON：启用消毒器
-DDEBUG_TRACE=ON：启用 PTRACE （PTRACE 是一种允许一个进程“跟踪”另一个进程执行的机制。示踪剂能够 暂停执行，并在 tracee 进程中检查和修改内存和寄存器）

```




### 交叉编译

本文下载的是 openssl-OpenSSL_1_1_1g.tar.gz，
下载地址：https://codeload.github.com/openssl/openssl/tar.gz/refs/tags/OpenSSL_1_1_1g

```shell
cd openssl-OpenSSL_1_1_1g/
./config no-asm shared no-async --prefix=`pwd`/ssl_result  --cross-compile-prefix=aarch64-mix210-linux-
```

```shell
sed -i 's/-m64//' Makefile
```





### IEEE 754 数据转换

```txt


要将给定的四个16进制值 41 E7 A9 50 转换为 IEEE 754 标准的 32 位浮点数（float），我们可以按照以下步骤进行：

1. 将16进制值合并为32位二进制表示
给定的四个16进制字节是：41, E7, A9, 50。将它们合并为一个32位的16进制值：
0x41E7A950。

2. 转换为二进制
将 0x41E7A950 转换为二进制：
41E7A950（16进制） = 01000001111001111010100101010000（二进制）

3. 解析IEEE 754浮点数
IEEE 754 32位浮点数的结构如下：

符号位（1位）：0（正数）

指数部分（8位）：10000011（二进制）

尾数部分（23位）：11001111010100101010000（二进制）

计算指数：
指数部分的二进制 10000011 转换为十进制：
1*2^7 + 0*2^6 + 0*2^5 + 0*2^4 + 0*2^3 + 0*2^2 + 1*2^1 + 1*2^0
= 128 + 0 + 0 + 0 + 0 + 0 + 2 + 1
= 131

IEEE 754 指数是“移码表示”，实际指数 = 指数部分 - 127：
131 - 127 = 4

计算尾数：
尾数部分的二进制是 11001111010100101010000。
IEEE 754 的尾数隐含一个前导的 1.，因此完整的尾数是：
1.11001111010100101010000

将其转换为十进制：
1.11001111010100101010000（二进制） =
1 + 0.5 + 0.25 + 0 + 0 + 0.03125 + 0.015625 + 0.0078125 + 0.00390625 + 0 + 0.0009765625 + 0 + 0.000244140625 + 0 + 0 + 0.0000152587890625 + 0.00000762939453125 + 0 + 0.0000019073486328125 + 0 + 0
≈ 1.807425022125244140625

计算浮点数值：
浮点数值 = (-1)^符号位 * 尾数 * 2^指数
= 1 * 1.807425022125244140625 * 2^4
= 1.807425022125244140625 * 16
≈ 28.91880035400390625

4. 最终结果
41 E7 A9 50 对应的 IEEE 754 浮点数约为 28.9188。

```