#include <shobjidl.h>
#include <algorithm>

#include <QFileInfo>
#include <QDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>
#include <QNetworkInterface>
#include <QtEndian>
#include <QTimer>

#include "FileTransfer.hpp"
#include "WFTheader.hpp"

const int LOCAL_HEADER_SIZE = 144; // 缓存文件头大小

FileTransfer::FileTransfer(QObject* parent, const quint16 port, const int Connections, bool isServer)
    : QObject(parent), maxConnections(Connections), m_isClient(!isServer)
{
    if (isServer) {
        ts = new QTcpServer(this);
        if (!ts->listen(QHostAddress::Any, port)) {
            qDebug() << "[ERROR] TCP Server failed: " << ts->errorString();
        } else {
            connect(ts, &QTcpServer::newConnection, this, &FileTransfer::onNewConnection);
            qDebug() << "[INFO] Server started, maxConnections:" << maxConnections;
            qDebug() << "[INFO] Listening addresses:";
            for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
                if (!iface.flags().testFlag(QNetworkInterface::IsRunning) ||
                    iface.flags().testFlag(QNetworkInterface::IsLoopBack))
                    continue;
                for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                    QHostAddress addr = entry.ip();
                    if (addr.protocol() == QAbstractSocket::IPv4Protocol)
                        qDebug() << "  -" << addr.toString() << ":" << port;
                }
            }
        }
    } else {
        qDebug() << "[INFO] Client mode initialized.";
    }
}

bool FileTransfer::connectToServer(const QString& ip, quint16 port) {
    if (!m_isClient) return false;
    if (m_clientSocket) {
        delete m_clientSocket;
        m_clientSocket = nullptr;
    }
    m_clientSocket = new QTcpSocket(this);
    connect(m_clientSocket, &QTcpSocket::readyRead, this, &FileTransfer::onReadyRead);
    connect(m_clientSocket, &QTcpSocket::disconnected, this, &FileTransfer::onClientDisconnected);
    m_clientSocket->connectToHost(ip, port);
    if (m_clientSocket->waitForConnected(3000)) {
        qDebug() << "[INFO] Connected to server" << ip << ":" << port;
        return true;
    } else {
        qDebug() << "[ERROR] Connection failed:" << m_clientSocket->errorString();
        delete m_clientSocket;
        m_clientSocket = nullptr;
        return false;
    }
}

void FileTransfer::save_file(QTcpSocket* client) {
    auto it = clientTemp.find(client);
    if (it == clientTemp.end()) return;
    ClientFileContext& ctx = it.value();
    if (ctx.isCompleted) return;
    QString tempPath = ctx.tempFilePath;
    QString finalPath = ctx.finalFileName;
    if(ctx.file) {
        ctx.file->close();
        delete ctx.file;
        ctx.file = nullptr;
    }
    if(ctx.metafile) {
        ctx.metafile->close();
        delete ctx.metafile;
        ctx.metafile = nullptr;
    }
    if (QFile::exists(finalPath)) QFile::remove(finalPath);
    if(!QFile::remove(ctx.metaFilePath)) {
        qDebug() << "[WARN] delete META file failed:" << ctx.metaFilePath;
    }
    if(!QFile::rename(tempPath, finalPath)) {
        qDebug() << "[ERROR] Rename failed:" << tempPath << "->" << finalPath;
    }
    else {
        qDebug() << "[INFO] File received completely:" << finalPath;
    }
    ctx.tempFilePath.clear();
    ctx.finalFileName.clear();
    ctx.metaFilePath.clear();
    ctx.totalSize = 0;
    ctx.receivedBytes = 0;
    ctx.isCompleted = false;
}

void FileTransfer::onNewConnection() {
    if (m_isClient) return;
    QTcpSocket* client = ts->nextPendingConnection();
    if (!client) return;
    if (connections >= maxConnections) {
        qDebug() << "[ERROR] connect refused: full connections";
        client->disconnectFromHost();
        client->deleteLater();
        return;
    }
    client->setParent(this);
    clients.append(client);
    connections++;
    connect(client, &QTcpSocket::disconnected, this, &FileTransfer::onClientDisconnected);
    connect(client, &QTcpSocket::readyRead, this, &FileTransfer::onReadyRead);
    qDebug() << "[INFO] New client connected from " << client->peerAddress().toString() << ":" << client->peerPort() << ". Total:" << connections;
}

void FileTransfer::onClientDisconnected() {
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;
    auto it = clientTemp.find(client);
    if (it != clientTemp.end()) {
        ClientFileContext& ctx = it.value();
        if (ctx.file) {
            ctx.file->close();
            delete ctx.file;
            ctx.file = nullptr;
        }
        if(!ctx.isCompleted) {
            qDebug() << "[WARN] Incomplete file: " << ctx.tempFilePath;
        }
        clientTemp.erase(it);
    }
    if (m_isClient) {
        if (m_clientSocket == client) {
            m_clientSocket = nullptr;
            qDebug() << "[INFO] Disconnected from server.";
        }
    } else {
        clients.removeAll(client);
        connections--;
        qDebug() << "[INFO] Client disconnected. Remaining:" << connections;
    }
    client->deleteLater();
}

void FileTransfer::send(const QString file_path) {
    if (m_sending) {
        qDebug() << "[ERROR] Another file transfer is in progress.";
        return;
    }
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[ERROR] open file failed.";  
        return;
    }
    QFileInfo fileInfo(file_path);
    QByteArray baseName = fileInfo.baseName().toUtf8();
    QByteArray suffix = fileInfo.suffix().toUtf8();
    quint64 totalSize = file.size();
    file.close();   
    QVector<QTcpSocket*> targets;
    if (m_isClient) {
        if (m_clientSocket && m_clientSocket->state() == QAbstractSocket::ConnectedState) {
            targets.append(m_clientSocket);
        } else {
            qDebug() << "[ERROR] Not connected to server."; 
            return;
        }
    } else {
        for (QTcpSocket* client : clients) {
            if (client->state() == QAbstractSocket::ConnectedState) {
                targets.append(client);
            }
        }
        if (targets.isEmpty()) {
            qDebug() << "[ERROR] No connected clients."; 
            return;
        }
    }
    m_sendFile = new QFile(file_path, this);
    if (!m_sendFile->open(QIODevice::ReadOnly)) {
        qDebug() << "[ERROR] Cannot reopen file for sending.";
        delete m_sendFile;
        m_sendFile = nullptr;
        return;
    }
    m_sendOffset = 0;
    m_sendTotal = totalSize;
    m_sendSeq = 0;
    m_sendBaseName = baseName;
    m_sendExt = suffix;
    m_sendTargets = targets;
    m_sending = true;
    qDebug() << "[INFO] File transfer started:" << file_path << "(" << totalSize << "bytes)";
    QTimer::singleShot(0, this, &FileTransfer::sendNextBlock);
}

void FileTransfer::sendNextBlock() {
    if (!m_sending || !m_sendFile) {
        return;
    }
    if (m_sendFile->atEnd() || m_sendOffset >= m_sendTotal) {
        m_sendFile->close();
        delete m_sendFile;
        m_sendFile = nullptr;
        m_sending = false;
        m_sendTargets.clear();
        qDebug() << "[INFO] File transfer completed.";  
        return;
    }
    const quint64 maxBlockSize = 128 * 1024;
    QByteArray data = m_sendFile->read(maxBlockSize);
    if (data.isEmpty() && !m_sendFile->atEnd()) {
        qDebug() << "[ERROR] Read file error at offset " << m_sendOffset;
        m_sendFile->close();
        delete m_sendFile;
        m_sendFile = nullptr;
        m_sending = false;
        m_sendTargets.clear();
        return;
    }
    WFTheader header(m_sendOffset, m_sendSeq, m_sendBaseName.constData(), m_sendExt.constData());
    header.setRealSize(data.size());
    header.setTotalSize(m_sendTotal);
    QByteArray send_buffer;
    send_buffer.reserve(WFTheader::HEADERSIZE + data.size());

    int be_magic = qToBigEndian(WFTheader::MAGIC);
    send_buffer.append(reinterpret_cast<const char*>(&be_magic), 4);
    send_buffer.append(header.getName(), 32);

    quint64 be_offset = qToBigEndian(header.getOffset());
    quint64 be_data_size = qToBigEndian(header.getSize());
    quint64 be_real_size = qToBigEndian(header.getRealSize());
    quint64 be_total_size = qToBigEndian(header.getTotalSize());
    int be_seq = qToBigEndian(header.seq);

    send_buffer.append(reinterpret_cast<const char*>(&be_offset), 8);
    send_buffer.append(reinterpret_cast<const char*>(&be_data_size), 8);
    send_buffer.append(reinterpret_cast<const char*>(&be_real_size), 8);
    send_buffer.append(reinterpret_cast<const char*>(&be_seq), 4);
    send_buffer.append(header.file_name, 128);
    send_buffer.append(header.ext, 8);
    send_buffer.append(reinterpret_cast<const char*>(&be_total_size), 8);
    send_buffer.append(data);

    bool anyFailed = false;
    for (QTcpSocket* target : m_sendTargets) {
        if (target->state() == QAbstractSocket::ConnectedState) {
            qint64 written = target->write(send_buffer);
            if (written != send_buffer.size()) {
                qDebug() << "[WARN] Partial write to" << target->peerAddress().toString() << ", wrote" << written << "of" << send_buffer.size();
            }
        } else {
            anyFailed = true;
            qDebug() << "[WARN] Target disconnected, will skip in next blocks.";
        }
    }
    if (anyFailed) {
        m_sendTargets.erase(
            std::remove_if(m_sendTargets.begin(), m_sendTargets.end(),
                           [](QTcpSocket* sock) { return sock->state() != QAbstractSocket::ConnectedState; }),
            m_sendTargets.end()
        );
        if (m_sendTargets.isEmpty()) {
            qDebug() << "[ERROR] All targets disconnected, aborting send.";
            m_sendFile->close();
            delete m_sendFile;
            m_sendFile = nullptr;
            m_sending = false;
            return;
        }
    }
    m_sendOffset += data.size();
    m_sendSeq++;
    qDebug() << "[INFO] Transfering... " << m_sendOffset << "/" << m_sendTotal;
    if (m_sendOffset < m_sendTotal) {
        QTimer::singleShot(0, this, &FileTransfer::sendNextBlock);
    } else {
        m_sendFile->close();
        delete m_sendFile;
        m_sendFile = nullptr;
        m_sending = false;
        m_sendTargets.clear();
        qDebug() << "[INFO] File transfer completed.";
    }
}

void FileTransfer::onReadyRead() {
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;
    auto it = clientTemp.find(client);
    if (it == clientTemp.end()) {
        ClientFileContext ctx;
        ctx.isCompleted = false;
        ctx.file = nullptr;
        ctx.recvBuffer.clear();
        it = clientTemp.insert(client, ctx);
    }
    ClientFileContext& ctx = it.value();
    ctx.recvBuffer.append(client->readAll());
    while (true) {
        if (ctx.recvBuffer.size() < 4) break;
        int net_magic;
        memcpy(&net_magic, ctx.recvBuffer.constData(), 4);
        int host_magic = qFromBigEndian(net_magic);
        if(host_magic != WFTheader::MAGIC) {
            qDebug() << "[WARN] Magic error,skipping...";
            ctx.recvBuffer.remove(0,1);
            continue;
        }

        if(ctx.recvBuffer.size() < WFTheader::HEADERSIZE) break;
        char protocol_name[32];
        quint64 net_offset, net_data_size, net_real_size, net_total_size;
        int net_seq, magic;
        char filename[128], ext[8];

        memcpy(protocol_name, ctx.recvBuffer.constData() + 4, 32);
        memcpy(&net_offset, ctx.recvBuffer.constData() + 36, 8);
        memcpy(&net_data_size, ctx.recvBuffer.constData() + 44, 8);
        memcpy(&net_real_size, ctx.recvBuffer.constData() + 52, 8);
        memcpy(&net_seq, ctx.recvBuffer.constData() + 60, 4);
        memcpy(filename, ctx.recvBuffer.constData() + 64, 128);
        memcpy(ext, ctx.recvBuffer.constData() + 192, 8);
        memcpy(&net_total_size, ctx.recvBuffer.constData() + 200, 8);

        quint64 real_size = qFromBigEndian<quint64>(net_real_size);
        quint64 partSize = WFTheader::HEADERSIZE + real_size;
        if(real_size > 128 * 1024) {
            qDebug() << "[WARN] Invalid packet,skipping...";
            ctx.recvBuffer.remove(0,4);
            continue;
        }
        if (static_cast<quint64>(ctx.recvBuffer.size()) < partSize) break;  
        QByteArray fileData = ctx.recvBuffer.mid(WFTheader::HEADERSIZE, real_size);
        if (ctx.file == nullptr) {
            QString baseName = QString::fromUtf8(filename);
            QString suffix = QString::fromUtf8(ext);
            ctx.finalFileName = baseName + "." + suffix;
            ctx.tempFilePath = baseName + ".WFTMP";
            ctx.metaFilePath = baseName + ".META";
            ctx.totalSize = qFromBigEndian<quint64>(net_total_size);
            ctx.receivedBytes = 0;

            ctx.file = new QFile(ctx.tempFilePath, this);
            if (!ctx.file->open(QIODevice::WriteOnly | QIODevice::Append)) {
                qDebug() << "[ERROR] Cannot open temp file:" << ctx.tempFilePath;
                delete ctx.file;
                ctx.file = nullptr;
                ctx.recvBuffer.remove(0, partSize);
                continue;
            }
            ctx.metafile = new QFile(ctx.metaFilePath, this);
            if(!ctx.metafile->open(QIODevice::WriteOnly | QIODevice::Append)) {
                qDebug() << "[ERROR] Cannot open meta file:" << ctx.metaFilePath;
                delete ctx.metafile;
                ctx.metafile = nullptr;
                ctx.recvBuffer.remove(0, partSize);
                continue;
            }
            QByteArray headerData;
            headerData.resize(LOCAL_HEADER_SIZE);
            memcpy(headerData.data(), filename, 128);
            memcpy(headerData.data() + 128, ext, 8);
            quint64 be_total = qToBigEndian(ctx.totalSize);
            memcpy(headerData.data() + 136, &be_total, 8);
            if (ctx.metafile->write(headerData) != LOCAL_HEADER_SIZE) {
                qDebug() << "[ERROR] Failed to write local file header.";
                ctx.file->close();
                delete ctx.file;
                ctx.file = nullptr;
                ctx.recvBuffer.remove(0, partSize);
                continue;
            }
        }
        quint64 offset = qFromBigEndian<quint64>(net_offset);
        quint64 writePos = offset;
        if (ctx.file->pos() != writePos && !ctx.file->seek(writePos)) {
            qDebug() << "[ERROR] Seek to offset " << offset << " failed";
            ctx.recvBuffer.remove(0, partSize);
            continue;
        }
        qint64 written = ctx.file->write(fileData);
        ctx.recvBuffer.remove(0, partSize);
        if (written != fileData.size()) {
            qDebug() << "[ERROR] Write error, written" << written << "of" << fileData.size();
        } else {
            ctx.receivedBytes += real_size;
            qDebug() << "[INFO] Receiving... " << ctx.receivedBytes << "/" << ctx.totalSize;
            if (ctx.receivedBytes >= ctx.totalSize) {
                qDebug() << "[INFO] Saving file...";
                save_file(client);
                continue;
            }
        }
    }
}
QString FileTransfer::OpenFileDialog() {
    std::wstring file_path;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return QString();
    IFileOpenDialog* fileopen = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                          IID_IFileOpenDialog, reinterpret_cast<void**>(&fileopen));
    if (SUCCEEDED(hr)) {
        FILEOPENDIALOGOPTIONS fos;
        fileopen->GetOptions(&fos);
        fileopen->SetOptions(fos | FOS_FILEMUSTEXIST);
        hr = fileopen->Show(NULL);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = fileopen->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    file_path = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        fileopen->Release();
    }
    CoUninitialize();
    return QString::fromStdWString(file_path);
}

int FileTransfer::getConnections() { 
    return connections;
}