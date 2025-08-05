

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