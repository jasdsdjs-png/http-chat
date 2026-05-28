#include"global.h"
QString gate_url_prefix = "";

std::function<void(QWidget*)> repolish=[](QWidget* w){
    w->style()->unpolish(w);//去除原来的样式
    w->style()->polish(w);//刷新

};