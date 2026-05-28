#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include<logindialog.h>
#include<registerdialog.h>
#include<resetpassworddialog.h>
#include <QIcon>
#include <QPointer>
#include<QFile>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    QPointer<LoginDialog> logdia;
    QPointer<RegisterDialog> regdia;
    QPointer<ResetPasswordDialog> resetdia;

public slots:
   void  slotSwitchReg();
   void  slotSwitchLogin();
   void  slotSwitchReset();
   void  slotSwitchChat();
};
#endif // MAINWINDOW_H
