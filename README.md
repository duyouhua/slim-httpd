# SimpleHttp (version 0.0.1)

一个采用多路复用技术的简单的静态文件服务器（目前已实现 `GET` 和 `POST` 方法）

构建 
```SHELL
$cmake .
$make
./slim-httpd -d
```
`cmake` 可根据构建平台选择多路服务技术（select、kqueue 或 epoll）

命令行参数

--d  以守护进程方式执行

--addr [端口] 设置服务器监听地址（默认为 0.0.0.0）

--port [端口] 设置服务器监听端口（默认为 80）

--path [路径] 设置服务器工作目录
