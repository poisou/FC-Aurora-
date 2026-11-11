#ifndef HEXTEXTEDIT_H
#define HEXTEXTEDIT_H

#include <QTextEdit>
#include <QByteArray>
#include <QKeyEvent>
#include <QTextCursor>

namespace Ui {
class HexTextEdit;
}

class HexTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit HexTextEdit(QWidget *parent = nullptr);
    ~HexTextEdit();
    //获取16进制字节数据
    QByteArray toBytes() const;
    void setMaxBytes(int max);
    const char* toCharPointer() const;
    bool isValid()const;
signals:
    void byteCountChange(int count);
protected:
    void keyPressEvent(QKeyEvent* event)override;
private slots:
    void fromHexText();
private:
    int m_maxBytes = 0;
private:
    Ui::HexTextEdit *ui;
};

#endif // HEXTEXTEDIT_H
