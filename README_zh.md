# Wi-Fi File Transfer

一个基于 Qt6 的轻量级 Wi-Fi 文件传输工具，支持在局域网内快速发送和接收文件。  
它既可作为服务端等待连接，也可作为客户端主动连接，适合在无互联网环境下（如会议室、实验室）使用。
一次发送或接收一个文件，不限大小，不限格式。


## 特性

### 1.WFT双向传输模式

- 支持 **服务端模式** 和 **客户端模式**
- 默认开启，可双向传输
- 图形化界面，操作简单
- 支持多客户端同时连接（可配置最大连接数）
- 文件传输进度实时显示在控制台
- **服务端发送**：文件会广播给所有已连接的客户端
- **客户端发送**：文件单独发送给服务端（点对点）
- META 文件预留用于后续断点续传功能（当前版本暂未实现）
- 基于 Qt 网络模块
- 使用 Windows 原生文件对话框（COM 接口）

### 2.HTTP单向传输模式

- 仅需要服务端开启
- 图形化界面，操作简单
- 可选择共享文件夹
- 浏览器输入控制台显示的 **服务端ip + 端口号** 访问文件夹，例如 **192.168.x.x:8888**
- 点击链接直接下载文件
- 基于 Qt 网络模块

---

## 系统要求与依赖

- **操作系统**：Windows 10 及以上版本（使用了 Windows 原生 API）
- **编译器**：仅支持 **MinGW**（本项目未配置 MSVC 工具链）
- **开发环境**：
  - [CMake](https://cmake.org/) 3.16 或更高版本
  - [Qt 6.11.1](https://www.qt.io/download) 或更高（需包含 `Core`、`Widgets`、`Network` 模块）
  - MinGW 11.2 或更高版本

---

## 快速编译与运行

### 1. 克隆仓库
```bash
git clone https://github.com/dh381-1/WIFI_FileTransfer.git
cd WIFI_FileTransfer
```

### 2. 配置 Qt 环境
确保你的系统已安装 Qt 6.11.1（MinGW 版本），并记住其安装路径（例如 `D:/Qt/6.11.1/mingw_64`）。  
在命令行中设置 `CMAKE_PREFIX_PATH`：

```bash
set CMAKE_PREFIX_PATH=D:/Qt/6.11.1/mingw_64
```

> ⚠️ 请将路径替换为你电脑上实际的 Qt 安装目录。

### 3. 使用 CMake 构建
```bash
cmake -B build -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%"
cmake --build build --config Release
```

构建完成后，可执行文件位于 `build/bin/WIFI_FileTransfer.exe`。  
**所有依赖的 DLL（Qt 核心库、MinGW 运行时、`platforms/qwindows.dll` 等）会自动复制到 exe 所在目录**，可直接双击运行。

---

## 使用说明

### 启动服务端
- 打开程序，点击 `Start Server`。  
- 自由配置端口（默认 8888）和**最大连接人数**（默认 4）。  
- 控制台会打印本机的所有 IPv4 地址，供客户端连接时参考。
- 默认WFT模式，可点击 `Start HTTP Server` 开启HTTP模式。

### 连接为客户端
- 点击 `Connect as Client`，输入**服务端的 IP 地址**和端口（可自由配置）。  
- 连接成功后，`Select & Send File` 按钮变为可用。
- 仅WFT模式可用。

### 发送文件

#### 1.WFT模式

- 点击 `Select & Send File`，选择要发送的文件。
- **服务端发送**：文件会广播给**所有**已连接的客户端。
- **客户端发送**：文件只会发送给服务端（点对点）。
- 传输进度在控制台显示。
- 文件发送完毕后才可以继续发送，连续发送将会拒绝请求。

#### 2.HTTP模式

- 点击 `Select & Send File`，选择共享文件目录。

### 接收文件

#### 1.WFT模式

- 服务端和客户端均可自动接收文件。
- 接收到的文件保存在可执行文件所在目录，文件名为 `原文件名`（若存在同名文件会被覆盖）。
- 传输过程中会生成 `.WFTMP` 临时文件和 `.META` 元数据文件：
  - `.WFTMP`：存储接收到的原始数据块。
  - `.META`：预留用于后续实现断点续传（当前版本仅记录文件头信息，传输完成后自动删除）。
- 传输完成后，`.WFTMP` 自动重命名为目标文件，`.META` 自动删除。

#### 2.HTTP模式

- 通过浏览器访问控制台显示的 **服务端ip + 端口号** 获取文件列表。
- 点击文件名链接开始下载。
- 接收到的文件由浏览器设置决定，文件名不变。

---

## 常见问题

- **启动时提示“无法找到 Qt 平台插件”**  
  请确保 `platforms/qwindows.dll` 与 `exe` 同级目录。CMake 已配置 `windeployqt` 自动部署，正常情况下不会出现此问题。

- **编译时提示“找不到 Qt6”**  
  请检查 `CMAKE_PREFIX_PATH` 是否指向 Qt 的顶层目录（即包含 `bin`、`lib`、`include` 的文件夹），且该 Qt 版本为 MinGW 编译版本。

- **MinGW 运行时 DLL 缺失**  
  `CMakeLists.txt` 已配置自动复制 `libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`，若仍缺失，请检查 MinGW 安装。

- **为什么服务端发送文件，所有客户端都能收到？**  
  这是本项目的设计行为：服务端作为广播中心，向所有已连接的客户端分发文件。客户端之间不互通。

- **能否使用 MSVC 编译器编译？**  
  暂不支持。本项目 `CMakeLists.txt` 未配置 MSVC 工具链，且依赖的 DLL 复制脚本仅针对 MinGW。如需使用 MSVC，请自行修改构建配置。

- **为什么要求 Windows 10 及以上？**  
  因为文件打开功能使用了 Windows COM 接口（`IFileOpenDialog`），并且 Qt6 明确仅支持 Windows 10 及以上系统。

---

## 许可证

本项目采用 **GNU Lesser General Public License v3** 授权。  
您可以在 [`LICENSE`](LICENSE) 文件中查看完整条款。  
本程序依赖的 **Qt 框架** 同样遵循 LGPL v3 协议，其源码可从 [Qt 官网](https://www.qt.io/download) 获取。

---

## 贡献与反馈

欢迎提交 Issue 或 Pull Request。  
如果你有好的建议或发现 Bug，请随时告诉我们！

---

**Happy Transfer!** 🚀
```

