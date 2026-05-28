#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tcpmgr.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icon/chat.ico"));
    logdia = new LoginDialog();
    setCentralWidget(logdia);
    connect(logdia, &LoginDialog::switchRegister, this, &MainWindow::slotSwitchReg);
    connect(logdia, &LoginDialog::switchForgetPassword, this, &MainWindow::slotSwitchReset);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_swich_chatdlg,
            this, &MainWindow::slotSwitchChat);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slotSwitchReg()
{
    QWidget *old = takeCentralWidget();
    if (old) {
        delete old;
    }
    regdia = new RegisterDialog();
    setCentralWidget(regdia);
    connect(regdia, &RegisterDialog::sigSwitchLogin, this, &MainWindow::slotSwitchLogin);
}

void MainWindow::slotSwitchLogin()
{
    QWidget *old = takeCentralWidget();
    if (old) {
        delete old;
    }
    logdia = new LoginDialog();
    setCentralWidget(logdia);
    connect(logdia, &LoginDialog::switchRegister, this, &MainWindow::slotSwitchReg);
    connect(logdia, &LoginDialog::switchForgetPassword, this, &MainWindow::slotSwitchReset);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_swich_chatdlg,
            this, &MainWindow::slotSwitchChat);
}

void MainWindow::slotSwitchReset()
{
    QWidget *old = takeCentralWidget();
    if (old) {
        delete old;
    }
    resetdia = new ResetPasswordDialog();
    setCentralWidget(resetdia);
    connect(resetdia, &ResetPasswordDialog::sigSwitchLogin, this, &MainWindow::slotSwitchLogin);
}

void MainWindow::slotSwitchChat()
{
    qDebug() << "chat login success, chat dialog will be added in later lessons";
}
