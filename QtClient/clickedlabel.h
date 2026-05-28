#ifndef CLICKEDLABEL_H
#define CLICKEDLABEL_H
#include <QLabel>
#include "global.h"

class ClickedLabel : public QLabel
{
    Q_OBJECT
public:
    ClickedLabel(QWidget* parent);
    virtual void mousePressEvent(QMouseEvent* ev) override;
    ClickLbState GetCurState();

private:
    ClickLbState _curstate;

signals:
    void clicked(void);
};

#endif // CLICKEDLABEL_H
