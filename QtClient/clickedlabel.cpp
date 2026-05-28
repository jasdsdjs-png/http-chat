#include "clickedlabel.h"
#include <QMouseEvent>

ClickedLabel::ClickedLabel(QWidget* parent)
    : QLabel(parent), _curstate(ClickLbState::Normal)
{
}

void ClickedLabel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        _curstate = (_curstate == ClickLbState::Normal)
                        ? ClickLbState::Selected
                        : ClickLbState::Normal;
        emit clicked();
    }
    QLabel::mousePressEvent(event);
}

ClickLbState ClickedLabel::GetCurState()
{
    return _curstate;
}
