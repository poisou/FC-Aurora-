#include "hextextedit.h"
#include "ui_hextextedit.h"

HexTextEdit::HexTextEdit(QWidget *parent) :
    QTextEdit(parent),
    ui(new Ui::HexTextEdit)
{
    ui->setupUi(this);
    setLineWrapMode(NoWrap);
    connect(this,&HexTextEdit::textChanged,this,&HexTextEdit::fromHexText);
}

HexTextEdit::~HexTextEdit()
{
    delete ui;
}

void HexTextEdit::fromHexText()
{
    QString text = toPlainText();
    QString clean = text;
    clean.remove(' ');

    //只允许0-9 A-F
    QString valid;
    for(QChar c : clean)
    {
        if(c.isDigit() || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
        {
            valid.append(c.toUpper());
        }
    }
    //限制最大字节数
    if(m_maxBytes > 0 && valid.length() > m_maxBytes * 2)
    {
        valid.truncate(m_maxBytes * 2);
    }

    //格式化：每2位加一个空格
    QString formatted;
    for(int i = 0;i < valid.length();i+=2)
    {
        formatted += valid.mid(i,2);
        if(i + 2 < valid.length())
        {
            formatted += ' ';
        }
    }
    if(formatted != text)
    {
        blockSignals(true);
        setPlainText(formatted);
        moveCursor(QTextCursor::End);
        blockSignals(false);
    }

    emit byteCountChange(valid.length() / 2);
}

//获取数据
QByteArray HexTextEdit::toBytes()const
{
    QString text = toPlainText();
    text.remove(' ');
    return QByteArray::fromHex(text.toUtf8());
}

const char* HexTextEdit::toCharPointer()const
{
    return toBytes().constData();
}

bool HexTextEdit::isValid() const
{
    QString text = toPlainText();
    text.remove(' ');
    return (text.length() % 2 == 0);
}

void HexTextEdit::setMaxBytes(int max)
{
    m_maxBytes = max;
}

//键盘过滤
void HexTextEdit::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    bool isHex = (key >= Qt::Key_0 && key <= Qt::Key_9) ||
            (key >= Qt::Key_A && key <= Qt::Key_F);
    if(isHex || key == Qt::Key_Backspace ||
            key == Qt::Key_Delete || key == Qt::Key_Space ||
            event->modifiers().testFlag(Qt::ControlModifier))
    {
        QTextEdit::keyPressEvent(event);
    }
}
