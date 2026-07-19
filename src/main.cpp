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

#include "FileTransfer.hpp"

const quint16 DEFAULT_PORT = 8888;       // 默认端口
const int DEFAULT_MAX_CONNECTIONS = 4;   // 默认最大连接数

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr) : QWidget(parent), ft(nullptr)
    {
        setWindowTitle("Wi-Fi File Transfer");
        setFixedSize(550, 220);  
        QGroupBox *modeGroup = new QGroupBox("Mode", this);
        QLabel *portLabel = new QLabel("Port:", this);
        portEdit = new QLineEdit(QString::number(DEFAULT_PORT), this);
        portEdit->setMaximumWidth(70);

        QLabel *maxConnLabel = new QLabel("Max Connections:", this);
        maxConnEdit = new QLineEdit(QString::number(DEFAULT_MAX_CONNECTIONS), this);
        maxConnEdit->setMaximumWidth(50);

        serverBtn = new QPushButton("Start Server", this);
        clientBtn = new QPushButton("Connect as Client", this);

        QHBoxLayout *modeLayout = new QHBoxLayout;
        modeLayout->addWidget(serverBtn);
        modeLayout->addWidget(clientBtn);
        modeLayout->addWidget(portLabel);
        modeLayout->addWidget(portEdit);
        modeLayout->addWidget(maxConnLabel);
        modeLayout->addWidget(maxConnEdit);
        modeLayout->addStretch();
        modeGroup->setLayout(modeLayout);
        QGroupBox *funcGroup = new QGroupBox("Actions", this);
        sendBtn = new QPushButton("Select & Send File", this);
        recvBtn = new QPushButton("&Receive / Wait", this);
        quitBtn = new QPushButton("&Quit", this);

        QHBoxLayout *funcLayout = new QHBoxLayout;
        funcLayout->addWidget(sendBtn);
        funcLayout->addWidget(recvBtn);
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
        connect(recvBtn, &QPushButton::clicked, this, &MainWindow::onReceiveFile);
        connect(quitBtn, &QPushButton::clicked, this, &MainWindow::onQuit);

        sendBtn->setEnabled(false);
        recvBtn->setEnabled(false);
    }

    ~MainWindow()
    {
        if (ft) delete ft;
    }

private slots:
    void onStartServer()
    {
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
        sendBtn->setEnabled(true);
        recvBtn->setEnabled(true);
        QMessageBox::information(this, "Server", "Server started on port " + QString::number(port) +
                                 "\nMax connections: " + QString::number(maxConn) +
                                 "\nCheck console for IP addresses.");
    }

    void onStartClient()
    {
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
            sendBtn->setEnabled(true);
            recvBtn->setEnabled(true);
            QMessageBox::information(this, "Client", "Connected to server successfully.");
        } else {
            delete ft;
            ft = nullptr;
            sendBtn->setEnabled(false);
            recvBtn->setEnabled(false);
            QMessageBox::critical(this, "Error", "Connection failed.\nSee console for details.");
        }
    }

    void onSendFile()
    {
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

    void onReceiveFile()
    {
        if (!ft) {
            QMessageBox::information(this, "Not Ready", "Please start server or connect first.");
            return;
        }
        QMessageBox::information(this, "Receive", "Waiting for incoming file.\nCheck console for status.");
    }

    void onQuit()
    {
        if (QMessageBox::question(this, "Quit", "Are you sure?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            close();
        }
    }

    void closeEvent(QCloseEvent *event) override
    {
        if (QMessageBox::question(this, "Quit", "Are you sure you want to quit?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            event->accept();
            qApp->quit();
        } else {
            event->ignore();
        }
    }

private:
    FileTransfer *ft;

    QLineEdit *portEdit;
    QLineEdit *maxConnEdit;   
    QPushButton *serverBtn;
    QPushButton *clientBtn;
    QPushButton *sendBtn;
    QPushButton *recvBtn;
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