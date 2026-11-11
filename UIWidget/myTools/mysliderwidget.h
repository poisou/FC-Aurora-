#ifndef MYSLIDERWIDGET_H
#define MYSLIDERWIDGET_H

#include <QSlider>
#include <QList>
#include <QApplication>
#include <QWidget>
#include <cmath>

class MySliderWidget :public QWidget
{
    Q_OBJECT
public:
    MySliderWidget(QWidget* parent = nullptr);
    ~MySliderWidget();
    void setValue(QList<int> list);
private slots:
    void snapToPresetValue(int currentValue);
private:
    QSlider* slider;
    QList<int> presetValues;
};

#endif // MYSLIDERWIDGET_H
