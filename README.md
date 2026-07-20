# Wi-Fi File Transfer

A lightweight Wi-Fi file transfer tool based on Qt6, supporting fast file sending and receiving over a local area network.  
It can act as a server waiting for connections or as a client initiating connections, suitable for environments without internet access (e.g., meeting rooms, laboratories).  
Transfer one file at a time, with no size or format restrictions.

---

## Features

### 1. WFT Bidirectional Transfer Mode

- Supports **Server Mode** and **Client Mode**
- Enabled by default, supports bidirectional transfer
- Graphical interface, simple to operate
- Supports multiple clients simultaneously (configurable max connections)
- Transfer progress displayed in real‑time on the console
- **Server sending**: file is broadcast to all connected clients
- **Client sending**: file is sent point‑to‑point to the server
- META files are reserved for future resume support (not implemented in current version)
- Based on Qt Network module
- Uses Windows native file dialog (COM interface)

### 2. HTTP Unidirectional Transfer Mode

- Only the server needs to be started
- Graphical interface, simple to operate
- Choose a shared folder
- Enter the **server IP + port** shown in the console into a browser, e.g., `192.168.x.x:8888`
- Click links to download files directly
- Based on Qt Network module

---

## System Requirements & Dependencies

- **Operating System**: Windows 10 and above (uses Windows native APIs)
- **Compiler**: Only **MinGW** is supported (MSVC toolchain is not configured for this project)
- **Development Environment**:
  - [CMake](https://cmake.org/) 3.16 or higher
  - [Qt 6.11.1](https://www.qt.io/download) or higher (requires `Core`, `Widgets`, `Network` modules)
  - MinGW 11.2 or higher

---

## Quick Build & Run

### 1. Clone the repository
```bash
git clone https://github.com/dh381-1/WIFI_FileTransfer.git
cd WIFI_FileTransfer
```
### 2. Configure Qt environment
Make sure you have Qt 6.11.1 (MinGW version) installed, and note its installation path (e.g., D:/Qt/6.11.1/mingw_64).
Set CMAKE_PREFIX_PATH in the command line:

```bash
set CMAKE_PREFIX_PATH=D:/Qt/6.11.1/mingw_64
```
⚠️ Replace the path with your actual Qt installation directory.

### 3. Build with CMake
```bash
cmake -B build -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%"
cmake --build build --config Release
```
After the build is complete, the executable is located at build/bin/WIFI_FileTransfer.exe.
All required DLLs (Qt core libraries, MinGW runtime, platforms/qwindows.dll, etc.) are automatically copied to the same directory as the executable, so you can double‑click to run it directly.

## Usage Instructions
### Start a server

Launch the program and click Start Server.

Open the program and click Start Server.

Freely configure the port (default 8888) and maximum number of connections (default 4).

The console will print all IPv4 addresses of the local machine for clients to connect to.

WFT mode is active by default; click Start HTTP Server to switch to HTTP mode.

### Connect as a client

Click Connect as Client, enter the server's IP address and port (freely configurable).

### Connect as a client
Click Connect as Client, enter the server IP address and port (freely configurable).

After successful connection, the Select & Send File button becomes active.

### Send a file

Click Select & Send File and choose the file you want to send.

Only available in WFT mode.

### Send a file

#### 1. WFT Mode
Click Select & Send File and choose the file to send.

Server sending: the file is broadcast to all connected clients.

Client sending: the file is sent only to the server (point‑to‑point).

Transfer progress is displayed on the console.

Files are sent sequentially; sending again before the previous transfer finishes will be rejected.

#### 2. HTTP Mode
Click Select & Send File and choose the shared folder.

### Receive a file
#### 1. WFT Mode
Both server and clients automatically receive files.

Received files are saved in the executable's directory with the original filename (existing files will be overwritten).

During transfer, .WFTMP temporary files and .META metadata files are generated:

.WFTMP: stores raw data chunks received.

.META: reserved for future resume support (currently only records header info and is automatically deleted after transfer completes).

After completion, .WFTMP is renamed to the final file, and .META is removed.

#### 2. HTTP Mode
Access the server IP + port shown in the console using a browser to view the file list.

Click a file name to start downloading.

The downloaded file is saved according to your browser settings, keeping the original filename.

## Frequently Asked Questions
### Error "Unable to find the Qt platform plugin" when starting
Make sure platforms/qwindows.dll is in the same directory as the executable. CMake is configured to run windeployqt automatically, so this issue should not occur under normal circumstances.

### Build error: "Could not find Qt6"
Check that CMAKE_PREFIX_PATH points to the top‑level Qt directory (the one containing bin, lib, include), and that this Qt version is built for MinGW.

### Missing MinGW runtime DLLs
CMakeLists.txt is configured to automatically copy libgcc_s_seh-1.dll, libstdc++-6.dll, and libwinpthread-1.dll. If they are still missing, check your MinGW installation.

### Why do all clients receive the file when the server sends?
This is by design: the server acts as a broadcast centre, distributing the file to all connected clients. Clients do not communicate with each other.

### Can I compile with MSVC?
Not currently supported. This project's CMakeLists.txt is not configured for the MSVC toolchain, and the DLL copying logic is specific to MinGW. If you need to use MSVC, you would need to modify the build configuration yourself.

### Why is Windows 10 or later required?
The file opening feature uses the Windows COM interface (IFileOpenDialog), and Qt6 explicitly supports only Windows 10 and above.

### License
This project is licensed under the GNU General Public License v3.
You can find the full terms in the LICENSE file.
The Qt Framework used by this program is also licensed under the LGPL v3; its source code is available from the Qt official website.

### Contributions & Feedback
Issues and Pull Requests are welcome.
If you have suggestions or find a bug, please let us know!

Happy Transfer! 🚀
