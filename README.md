# High Performance Web Server (C++)

一个基于 Linux 环境下的高性能 Web 服务器，采用 C++11 标准开发。该项目实现了轻量级的 HTTP 请求处理，集成了线程池、半同步/半反应堆模式、非阻塞 I/O、信号处理、定时器和异步日志等核心技术。

## 🌟 主要特性

- **并发模型**：采用 Reactor 模式（Epoll + 非阻塞 I/O）+ 线程池，高效处理高并发请求。
- **资源管理**：使用 RAII 机制管理资源，防止内存泄漏；通过状态机解析 HTTP 请求。
- **定时器**：基于双向链表和最小堆（或链表实现的时间轮）处理非活动连接，自动关闭超时客户端。
- **日志系统**：支持异步日志写入，包含日志级别控制、文件滚动（按天/按大小）功能，减少 I/O 对主线程的阻塞。
- **访问控制**：支持 IP 白名单和黑名单机制，增强服务器安全性。
- **线程同步**：封装了互斥锁（Locker）和信号量（Semaphore），确保多线程环境下的数据安全。

## 📂 项目结构

| 文件名 | 描述 |
| :--- | :--- |
| `main.cpp` (需补充) | 程序入口，初始化服务器、事件循环等 |
| `httpcon.h/cpp` | HTTP 连接类，负责解析请求报文、生成响应报文及业务逻辑处理 |
| `locker.h/cpp` | 线程同步封装，包含互斥锁 (`pthread_mutex`) |
| `sem.h/cpp` | 信号量封装 (`sem_t`)，用于线程池任务队列同步 |
| `pool.h` | 线程池模板类，管理工作线程和任务队列 |
| `timer.h/cpp` | 定时器类，存储过期时间和回调函数 |
| `timer_list.h/cpp` | 定时器链表，管理所有定时器，执行超时回调（关闭连接） |
| `log.h/cpp` | 单例模式的日志系统，支持同步/异步写入 |
| `access_control.h/cpp` | 访问控制模块，加载并校验 IP 白名单/黑名单 |
| `macro.h` | 全局宏定义，如端口号、最大连接数、超时时间等 |

## 🛠️ 依赖项

- **操作系统**: Linux (推荐 Ubuntu 18.04+)
- **编译器**: g++ (支持 C++11 及以上)
- **库**: pthread, epoll (内置于内核)

## ⚙️ 编译方法

确保当前目录下包含所有 `.h` 和 `.cpp` 文件。

```
# 编译命令
g++ -std=c++11 -o webserver *.cpp -lpthread -I.

# 运行
./webserver

```



# High Performance Web Server (C++)

A high-performance web server based on the Linux environment, developed using the C++11 standard. This project implements lightweight HTTP request processing, integrating core technologies such as thread pool, half-sync/half-reactor pattern, non-blocking I/O, signal handling, timers, and asynchronous logging.

## 🌟 Key Features

- **Concurrency Model**: Adopts Reactor pattern (Epoll + Non-blocking I/O) + Thread Pool to efficiently handle high-concurrency requests.
- **Resource Management**: Uses RAII mechanism to manage resources and prevent memory leaks; parses HTTP requests via state machine.
- **Timer**: Handles inactive connections based on doubly-linked list and min-heap (or timing wheel implemented with linked list), automatically closing timed-out clients.
- **Logging System**: Supports asynchronous log writing, including log level control and file rotation (by day/by size), reducing I/O blocking on the main thread.
- **Access Control**: Supports IP whitelist and blacklist mechanisms to enhance server security.
- **Thread Synchronization**: Encapsulates mutex locks (Locker) and semaphores (Semaphore) to ensure data safety in multi-threaded environments.

## 📂 Project Structure

| File Name | Description |
| :--- | :--- |
| `main.cpp` (to be supplemented) | Program entry point, initializes server, event loop, etc. |
| `httpcon.h/cpp` | HTTP connection class, responsible for parsing request messages, generating response messages, and business logic processing |
| `locker.h/cpp` | Thread synchronization encapsulation, includes mutex lock (`pthread_mutex`) |
| `sem.h/cpp` | Semaphore encapsulation (`sem_t`), used for thread pool task queue synchronization |
| `pool.h` | Thread pool template class, manages worker threads and task queue |
| `timer.h/cpp` | Timer class, stores expiration time and callback functions |
| `timer_list.h/cpp` | Timer linked list, manages all timers, executes timeout callbacks (close connections) |
| `log.h/cpp` | Singleton pattern logging system, supports synchronous/asynchronous writing |
| `access_control.h/cpp` | Access control module, loads and validates IP whitelist/blacklist |
| `macro.h` | Global macro definitions, such as port number, maximum connections, timeout duration, etc. |

## 🛠️ Dependencies

- **Operating System**: Linux (Ubuntu 18.04+ recommended)
- **Compiler**: g++ (C++11 or above supported)
- **Libraries**: pthread, epoll (built into kernel)

## ⚙️ Compilation Method

Ensure all `.h` and `.cpp` files are included in the current directory.

```bash
# Compilation command
g++ -std=c++11 -o webserver *.cpp -lpthread -I.

# Run
./webserver
