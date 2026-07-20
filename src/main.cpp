#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QNetworkInterface>
#include <QCloseEvent>
#include <QDebug>
#include <QFileDialog>
#include <QCoreApplication>
#include <QDir>

#include "FileTransfer.hpp"
#include "HttpFileServer.hpp"

const quint16 DEFAULT_PORT = 8888;        // 默认端口
const int DEFAULT_MAX_CONNECTIONS = 4;    // 默认最大连接数

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr) : QWidget(parent), ft(nullptr), httpServer(nullptr), isServerMode(false) {
        setWindowTitle("Wi-Fi File Transfer");
        setFixedSize(600, 250);

        QGroupBox *modeGroup = new QGroupBox("Mode", this);
        QLabel *portLabel = new QLabel("Port:", this);
        portEdit = new QLineEdit(QString::number(DEFAULT_PORT), this);
        portEdit->setMaximumWidth(70);

        QLabel *maxConnLabel = new QLabel("Max Connections:", this);
        maxConnEdit = new QLineEdit(QString::number(DEFAULT_MAX_CONNECTIONS), this);
        maxConnEdit->setMaximumWidth(50);

        serverBtn = new QPushButton("Start Server", this);
        clientBtn = new QPushButton("Connect as Client", this);
        httpBtn = new QPushButton("Start HTTP Server", this);

        QHBoxLayout *modeLayout = new QHBoxLayout;
        modeLayout->addWidget(serverBtn);
        modeLayout->addWidget(clientBtn);
        modeLayout->addWidget(portLabel);
        modeLayout->addWidget(portEdit);
        modeLayout->addWidget(maxConnLabel);
        modeLayout->addWidget(maxConnEdit);
        modeLayout->addWidget(httpBtn);
        modeLayout->addStretch();
        modeGroup->setLayout(modeLayout);

        QGroupBox *funcGroup = new QGroupBox("Actions", this);
        sendBtn = new QPushButton("Select & Send File", this);
        quitBtn = new QPushButton("&Quit", this);   

        QHBoxLayout *funcLayout = new QHBoxLayout;
        funcLayout->addWidget(sendBtn);
        funcLayout->addWidget(quitBtn);
        funcLayout->addStretch();
        funcGroup->setLayout(funcLayout);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(modeGroup);
        mainLayout->addWidget(funcGroup);
        mainLayout->addStretch();

        connect(serverBtn, &QPushButton::clicked, this, &MainWindow::onStartServer);
        connect(clientBtn, &QPushButton::clicked, this, &MainWindow::onStartClient);
        connect(sendBtn, &QPushButton::clicked, this, &MainWindow::onSendFile);
        connect(quitBtn, &QPushButton::clicked, this, &MainWindow::onQuit);
        connect(httpBtn, &QPushButton::clicked, this, &MainWindow::onToggleHttp);

        sendBtn->setEnabled(false);
        httpBtn->setEnabled(false);
        quitBtn->setEnabled(true);

        httpServer = new HttpFileServer(this);
        connect(httpServer, &HttpFileServer::started, this, &MainWindow::onHttpStarted);
        connect(httpServer, &HttpFileServer::stopped, this, &MainWindow::onHttpStopped);
        connect(httpServer, &HttpFileServer::error, this, &MainWindow::onHttpError);
    }

    ~MainWindow()
    {
        if (ft) delete ft;
    }

private slots:
    void onStartServer() {
        if (httpServer->isRunning()) {
            QMessageBox::warning(this, "Conflict", "HTTP server is running. Please stop it before starting TCP server.");
            return;
        }
        if (ft) {
            delete ft;
            ft = nullptr;
        }
        bool ok;
        quint16 port = static_cast<quint16>(portEdit->text().toUInt(&ok));
        if (!ok || port == 0) {
            QMessageBox::warning(this, "Invalid Port", "Please enter a valid port number.");
            return;
        }
        int maxConn = maxConnEdit->text().toInt(&ok);
        if (!ok || maxConn <= 0) {
            QMessageBox::warning(this, "Invalid Max Connections", "Please enter a positive integer.");
            return;
        }
        ft = new FileTransfer(this, port, maxConn, true);
        isServerMode = true;
        sendBtn->setEnabled(true);
        httpBtn->setEnabled(true);
        if (!httpServer->isRunning()) {
            httpBtn->setText("Start HTTP Server");
        }
        serverBtn->setEnabled(false);
        clientBtn->setEnabled(false);
        quitBtn->setText("&Disconnect");

        QMessageBox::information(this, "Server", "Server started on port " + QString::number(port) + "\nMax connections: " + QString::number(maxConn) + "\nCheck console for IP addresses.");
    }

    void onStartClient() {
        if (httpServer->isRunning()) {
            QMessageBox::warning(this, "Conflict", "HTTP server is running. Please stop it before connecting as client.");
            return;
        }

        if (ft) {
            delete ft;
            ft = nullptr;
        }
        bool ok;
        QString ip = QInputDialog::getText(this, "Connect to Server", "Enter server IP address:", QLineEdit::Normal, "192.168.1.100", &ok);
        if (!ok || ip.isEmpty()) {
            return;
        }
        quint16 port = static_cast<quint16>(QInputDialog::getInt(this, "Connect to Server", "Enter port:", DEFAULT_PORT, 1, 65535, 1, &ok));
        if (!ok) return;

        ft = new FileTransfer(this, port, DEFAULT_MAX_CONNECTIONS, false);
        if (ft->connectToServer(ip, port)) {
            isServerMode = false;
            sendBtn->setEnabled(true);
            httpBtn->setEnabled(false);
            httpBtn->setText("HTTP (Server only)");
            serverBtn->setEnabled(false);
            clientBtn->setEnabled(false);
            quitBtn->setText("&Disconnect");
            QMessageBox::information(this, "Client", "Connected to server successfully.");
        } else {
            delete ft;
            ft = nullptr;
            isServerMode = false;
            sendBtn->setEnabled(false);
            httpBtn->setEnabled(false);
            httpBtn->setText("HTTP (Server only)");
            serverBtn->setEnabled(true);
            clientBtn->setEnabled(true);
            quitBtn->setText("&Quit");
            QMessageBox::critical(this, "Error", "Connection failed.\nSee console for details.");
        }
    }

    void onSendFile() {
        if (httpServer->isRunning()) {
            if (httpServer->selectAndSetShareDirectory(this)) {
                QMessageBox::information(this, "HTTP Share", "Shared directory updated to: " + httpServer->shareDirectory());
            }
            return;
        }
        if (!ft) {
            QMessageBox::information(this, "Not Ready", "Please start server or connect first.");
            return;
        }
        QString filePath = ft->OpenFileDialog();
        if (filePath.isEmpty()) {
            return;
        }
        ft->send(filePath);
        QMessageBox::information(this, "Send", "File sent (check console for progress).");
    }

    void onToggleHttp() {
        if (!isServerMode) {
            QMessageBox::warning(this, "Error", "HTTP server can only be enabled in Server mode.");
            return;
        }
        if (httpServer->isRunning()) {
            httpServer->stop();
            return;
        }
        if (ft) {
            delete ft;
            ft = nullptr;
        }

        bool ok;
        quint16 port = static_cast<quint16>(portEdit->text().toUInt(&ok));
        if (!ok || port == 0) {
            QMessageBox::warning(this, "Invalid Port", "Please enter a valid port number for HTTP.");
            restoreTcpServer();
            return;
        }

        QString shareDir = httpServer->shareDirectory();
        if (shareDir.isEmpty()) {
            shareDir = QCoreApplication::applicationDirPath() + "/share";
            QDir dir(shareDir);
            if (!dir.exists()) dir.mkpath(".");
            httpServer->setShareDirectory(shareDir);
        }

        if (!httpServer->start(port)) {
            restoreTcpServer();
        } else {
            sendBtn->setEnabled(true);
        }
    }

    void onHttpStarted() {
        httpBtn->setText("Stop HTTP Server");
        serverBtn->setEnabled(false);
        clientBtn->setEnabled(false);
        quitBtn->setText("&Disconnect"); 
        QMessageBox::information(this, "HTTP Server", "HTTP server started on port " + QString::number(httpServer->serverPort()) + "\nShared directory: " + httpServer->shareDirectory() + "\nAccess from browser via http://<this-IP>:" + QString::number(httpServer->serverPort()) + "/");
        sendBtn->setEnabled(true);
    }

    void onHttpStopped() {
        httpBtn->setText("Start HTTP Server");
        QMessageBox::information(this, "HTTP Server", "HTTP server stopped.");
        if (isServerMode && ft == nullptr) {
            restoreTcpServer();
        } else {
            if (ft) {
                sendBtn->setEnabled(true);
                serverBtn->setEnabled(false);
                clientBtn->setEnabled(false);
                quitBtn->setText("&Disconnect");
            } else {
                sendBtn->setEnabled(false);
                serverBtn->setEnabled(true);
                clientBtn->setEnabled(true);
                quitBtn->setText("&Quit");
                httpBtn->setEnabled(isServerMode); 
            }
        }
    }

    void onHttpError(const QString &msg) {
        QMessageBox::critical(this, "HTTP Server Error", msg);
        httpBtn->setText("Start HTTP Server");
        if (isServerMode && ft == nullptr) {
            restoreTcpServer();
        } else {
            if (ft) {
                sendBtn->setEnabled(true);
                serverBtn->setEnabled(false);
                clientBtn->setEnabled(false);
                quitBtn->setText("&Disconnect");
            } else {
                sendBtn->setEnabled(false);
                serverBtn->setEnabled(true);
                clientBtn->setEnabled(true);
                quitBtn->setText("&Quit");
                httpBtn->setEnabled(isServerMode);
            }
        }
    }

    void onQuit() {
        if (ft != nullptr || httpServer->isRunning()) {
            if (httpServer->isRunning()) {
                httpServer->stop();  
                if (ft) {
                    delete ft;
                    ft = nullptr;
                }
                if (httpServer->isRunning()) {
                    httpServer->stop();
                }
                isServerMode = false; 
                serverBtn->setEnabled(true);
                clientBtn->setEnabled(true);
                httpBtn->setEnabled(false);
                httpBtn->setText("Start HTTP Server");
                sendBtn->setEnabled(false);
                quitBtn->setText("&Quit");
                QMessageBox::information(this, "Disconnected", "Connection stopped.");
                return;
            } else {
                if (ft) {
                    delete ft;
                    ft = nullptr;
                    qDebug() << "[INFO] TCP Server stopped.";
                }
                isServerMode = false;
                serverBtn->setEnabled(true);
                clientBtn->setEnabled(true);
                httpBtn->setEnabled(false);
                httpBtn->setText("Start HTTP Server");
                sendBtn->setEnabled(false);
                quitBtn->setText("&Quit");
                QMessageBox::information(this, "Disconnected", "Connection stopped.");
                return;
            }
        } else {
            if (QMessageBox::question(this, "Quit", "Are you sure you want to quit?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                close();
            }
        }
    }

    void closeEvent(QCloseEvent *event) override {
        if (QMessageBox::question(this, "Quit", "Are you sure you want to quit?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            if (httpServer && httpServer->isRunning()) {
                httpServer->stop();
            }
            if (ft) delete ft;
            event->accept();
            qApp->quit();
        } else {
            event->ignore();
        }
    }

private:
    void restoreTcpServer() {
        if (ft != nullptr) return;
        if (!isServerMode) return;
        bool ok;
        quint16 port = static_cast<quint16>(portEdit->text().toUInt(&ok));
        if (!ok || port == 0) port = DEFAULT_PORT;
        int maxConn = maxConnEdit->text().toInt(&ok);
        if (!ok || maxConn <= 0) maxConn = DEFAULT_MAX_CONNECTIONS;

        ft = new FileTransfer(this, port, maxConn, true);
        sendBtn->setEnabled(true);
        httpBtn->setEnabled(true);
        httpBtn->setText("Start HTTP Server");
        serverBtn->setEnabled(false);
        clientBtn->setEnabled(false);
        quitBtn->setText("&Disconnect");
        QMessageBox::information(this, "Server", "TCP Server restarted on port " + QString::number(port));
    }

private:
    FileTransfer *ft;
    HttpFileServer *httpServer;
    bool isServerMode;

    QLineEdit *portEdit;
    QLineEdit *maxConnEdit;
    QPushButton *serverBtn;
    QPushButton *clientBtn;
    QPushButton *httpBtn;
    QPushButton *sendBtn;
    QPushButton *quitBtn;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

#include "main.moc"