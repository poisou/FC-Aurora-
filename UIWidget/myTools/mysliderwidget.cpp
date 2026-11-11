#include "mysliderwidget.h"

MySliderWidget::MySliderWidget(QWidget* parent)
    : QWidget(parent)
{
    slider = new QSlider(Qt::Horizontal,this);
    slider->setRange(0,100);
    slider->setValue(0);
    connect(slider,&QSlider::valueChanged,this,&MySliderWidget::snapToPresetValue);
}


MySliderWidget::~MySliderWidget()
{

}

void MySliderWidget::setValue(QList<int> list)
{
    presetValues = list;
}

void MySliderWidget::snapToPresetValue(int currentValue)
{
    if(presetValues.isEmpty())
        return;
    int closestValue = presetValues.first();
    int minDiff = std::abs(currentValue - closestValue);

    for(int val : presetValues)
    {
        int diff = std::abs(currentValue - val);
        if(diff < minDiff)
        {
            minDiff = diff;
            closestValue = val;
        }
    }

    if(closestValue != currentValue)
    {
        slider->setValue(closestValue);
    }
}
