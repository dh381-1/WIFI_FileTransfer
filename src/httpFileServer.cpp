#include "HttpFileServer.hpp"
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>
#include <QCoreApplication>
#include <QFileDialog>

HttpFileServer::HttpFileServer(QObject *parent) : QObject(parent) {
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &HttpFileServer::onNewConnection);
}

HttpFileServer::~HttpFileServer() {
    stop();
}

bool HttpFileServer::start(quint16 port, const QString &shareDir) {
    if (isRunning()) return true;
    if (!shareDir.isEmpty()) m_shareDir = shareDir;

    if (m_shareDir.isEmpty()) {
        emit error("Shared directory not set");
        return false;
    }

    QDir dir(m_shareDir);
    if (!dir.exists()) {
        emit error("Shared directory does not exist: " + m_shareDir);
        return false;
    }

    if (!m_server->listen(QHostAddress::Any, port)) {
        emit error("Failed to start HTTP server: " + m_server->errorString());
        return false;
    }

    emit started();
    qDebug() << "[HTTP] Server started on port" << port << ", serving" << m_shareDir;
    return true;
}

void HttpFileServer::stop() {
    if (!isRunning()) return;
    m_server->close();
    for (auto it = m_sendContexts.begin(); it != m_sendContexts.end(); ++it) {
        if (it.value().file) {
            it.value().file->close();
            delete it.value().file;
        }
        it.value().file = nullptr;
    }
    m_sendContexts.clear();

    for (auto socket : m_buffers.keys()) {
        socket->disconnectFromHost();
        socket->deleteLater();
    }
    m_buffers.clear();
    emit stopped();
    qDebug() << "[HTTP] Server stopped";
}

void HttpFileServer::setShareDirectory(const QString &dir) {
    if (m_shareDir == dir) return;
    m_shareDir = dir;
    emit shareDirectoryChanged(dir);
}

bool HttpFileServer::selectAndSetShareDirectory(QWidget *parent) {
    QString dir = QFileDialog::getExistingDirectory(
        parent,
        tr("Select Shared Directory"),
        m_shareDir.isEmpty() ? QDir::homePath() : m_shareDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (dir.isEmpty()) return false;
    setShareDirectory(dir);
    return true;
}

void HttpFileServer::onNewConnection() {
    QTcpSocket *client = m_server->nextPendingConnection();
    if (!client) return;
    connect(client, &QTcpSocket::readyRead, this, &HttpFileServer::onReadyRead);
    connect(client, &QTcpSocket::disconnected, this, &HttpFileServer::onDisconnected);
    connect(client, &QTcpSocket::bytesWritten, this, &HttpFileServer::onBytesWritten);
    m_buffers.insert(client, QByteArray());
    client->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 8 * 1024 * 1024);
    client->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 8 * 1024 * 1024);
}

void HttpFileServer::onReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !m_buffers.contains(socket)) return;
    m_buffers[socket].append(socket->readAll());
    QByteArray &data = m_buffers[socket];
    if (data.contains("\r\n\r\n") || data.contains("\n\n")) {
        processRequest(socket, data);
        data.clear();
    }
}

void HttpFileServer::onDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        if (m_sendContexts.contains(socket)) {
            SendContext &ctx = m_sendContexts[socket];
            if (ctx.file) {
                ctx.file->close();
                delete ctx.file;
                ctx.file = nullptr;
            }
            m_sendContexts.remove(socket);
        }
        m_buffers.remove(socket);
        socket->deleteLater();
    }
}

void HttpFileServer::onBytesWritten(qint64 bytes) {
    Q_UNUSED(bytes);
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !m_sendContexts.contains(socket)) return;

    SendContext &ctx = m_sendContexts[socket];
    if (ctx.sent < ctx.total) {
        startNextChunk(socket);
    } else {
        qDebug() << "[HTTP] File sent completely:" << ctx.file->fileName() << "(" << ctx.total << " bytes)";
        ctx.file->close();
        delete ctx.file;
        ctx.file = nullptr;
        m_sendContexts.remove(socket);
    }
}

// ---------- 请求处理 ----------
void HttpFileServer::processRequest(QTcpSocket *socket, const QByteArray &request) {
    QList<QByteArray> lines = request.split('\n');
    if (lines.isEmpty()) return;
    QByteArray firstLine = lines[0].trimmed();
    QList<QByteArray> parts = firstLine.split(' ');
    if (parts.size() < 2) return;
    QByteArray method = parts[0];
    QByteArray path = parts[1];
    if (method != "GET") {
        sendError(socket, 405, "Method Not Allowed");
        return;
    }
    QUrl url(QString::fromUtf8(path));
    QString pathStr = url.path();
    if (pathStr == "/" || pathStr.isEmpty()) {
        sendDirectoryListing(socket);
        return;
    }
    if (pathStr.startsWith('/')) {
        pathStr = pathStr.mid(1);
    }
    QString absolutePath = QDir(m_shareDir).absoluteFilePath(pathStr);
    absolutePath = QDir::cleanPath(absolutePath);
    QString shareDirClean = QDir::cleanPath(m_shareDir);
    if (!absolutePath.startsWith(shareDirClean)) {
        sendError(socket, 403, "Forbidden");
        return;
    }

    QFileInfo info(absolutePath);
    if (!info.exists() || info.isDir()) {
        sendError(socket, 404, "Not Found");
        return;
    }

    sendFile(socket, absolutePath);
}

void HttpFileServer::sendDirectoryListing(QTcpSocket *socket) {
    QDir dir(m_shareDir);
    QStringList files = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QString html = "<html><head><title>File List</title></head><body><h1>Shared Files</h1><ul>";
    for (const QString &name : files) {
        QString fullPath = dir.absoluteFilePath(name);
        QFileInfo info(fullPath);
        if (info.isDir()) {
            html += "<li><b>" + name + "/</b></li>";
        } else {
            html += "<li><a href=\"/" + name + "\">" + name + "</a> (" + QString::number(info.size()) + " bytes)</li>";
        }
    }
    html += "</ul></body></html>";
    QByteArray response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/html; charset=utf-8\r\n";
    response += "Content-Length: " + QByteArray::number(html.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += html.toUtf8();
    socket->write(response);
    socket->disconnectFromHost();
}

void HttpFileServer::sendFile(QTcpSocket *socket, const QString &filePath) {
    // 如果已经存在该 socket 的发送上下文，先清理
    if (m_sendContexts.contains(socket)) {
        SendContext &old = m_sendContexts[socket];
        if (old.file) {
            old.file->close();
            delete old.file;
            old.file = nullptr;
        }
        m_sendContexts.remove(socket);
    }

    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        sendError(socket, 500, "Internal Server Error");
        delete file;
        return;
    }

    // 发送 HTTP 响应头
    QByteArray head = "HTTP/1.1 200 OK\r\n";
    head += "Content-Type: application/octet-stream\r\n";
    head += "Content-Length: " + QByteArray::number(file->size()) + "\r\n";
    head += "Content-Disposition: attachment; filename=\"" + QFileInfo(filePath).fileName().toUtf8() + "\"\r\n";
    head += "Connection: close\r\n\r\n";
    socket->write(head);

    // 创建发送上下文
    SendContext ctx;
    ctx.file = file;
    ctx.total = file->size();
    ctx.sent = 0;
    m_sendContexts.insert(socket, ctx);

    // 开始发送第一块数据
    startNextChunk(socket);
}

void HttpFileServer::startNextChunk(QTcpSocket *socket) {
    if (!m_sendContexts.contains(socket)) return;
    SendContext &ctx = m_sendContexts[socket];
    if (ctx.sent >= ctx.total) return;

    const qint64 CHUNK_SIZE = 1024 * 1024; // 1MB
    qint64 remaining = ctx.total - ctx.sent;
    qint64 toRead = qMin(CHUNK_SIZE, remaining);

    QByteArray buffer = ctx.file->read(toRead);
    if (buffer.isEmpty()) {
        ctx.file->close();
        delete ctx.file;
        ctx.file = nullptr;
        m_sendContexts.remove(socket);
        socket->disconnectFromHost();
        return;
    }

    qint64 written = socket->write(buffer);
    if (written != buffer.size()) {
        qDebug() << "[HTTP] Write error: wrote" << written << "of" << buffer.size();
        ctx.file->close();
        delete ctx.file;
        ctx.file = nullptr;
        m_sendContexts.remove(socket);
        socket->disconnectFromHost();
        return;
    }
    ctx.sent += buffer.size();
    if (ctx.sent < ctx.total) {
        qDebug() << "[HTTP] Transfering... " << ctx.sent << "/" << ctx.total;
    }
}

void HttpFileServer::sendError(QTcpSocket *socket, int code, const QString &msg) {
    QByteArray response = "HTTP/1.1 " + QByteArray::number(code) + " " + msg.toUtf8() + "\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: " + QByteArray::number(msg.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += msg.toUtf8();
    socket->write(response);
    socket->disconnectFromHost();
}