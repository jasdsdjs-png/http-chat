#include "mainwindow.h"
#include"global.h"
#include <QApplication>
#include<QSettings>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFile qss(":/stytle/systle.qss");
        if(qss.open(QFile::ReadOnly)){
        qDebug()<<"打开成功";
            QString style=QLatin1String(qss.readAll());//转换格式
        a.setStyleSheet(style);
            qss.close();
    }
        else{
            qDebug()<<"打开失败";
    }
    // 获取当前应用程序的路径
    QString app_path = QCoreApplication::applicationDirPath();
    // 拼接文件名
    QString fileName = "config.ini";
    QString config_path = QDir::toNativeSeparators(app_path +
                                                   QDir::separator() + fileName);

    QSettings settings(config_path, QSettings::IniFormat);//读取config的配置
    QString gate_host = settings.value("GateServer/host").toString();
    QString gate_port = settings.value("GateServer/port").toString();
    gate_url_prefix = "http://"+gate_host+":"+gate_port;
    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
