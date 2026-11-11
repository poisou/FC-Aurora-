#ifndef CDEVSTATUSFORM_H
#define CDEVSTATUSFORM_H

#include <QWidget>

namespace Ui {
class CDevStatusForm;
}

class CDevStatusForm : public QWidget
{
    Q_OBJECT

public:
    explicit CDevStatusForm(QWidget *parent = nullptr);
    ~CDevStatusForm();
    void SetStatus(bool bstatus);
    void SetText(QString stext);
    void SetPressStatus(bool bflg);
    bool bPress;
Q_SIGNALS:
    void MousePress(QString);
private:
    Ui::CDevStatusForm *ui;
    void mousePressEvent(QMouseEvent *e);
    void focusInEvent(QFocusEvent *event);
    void focusOutEvent(QFocusEvent *event);
};

#endif // CDEVSTATUSFORM_H
