#include "./QUnevenIntSpinBox.h"
#include <QString>
#include <iostream>

/*
 * 
 * thanks to https://stackoverflow.com/questions/40416074/how-to-implement-a-validator-for-a-spin-box-to-check-if-n-mod-k-0
 * 
 */
QValidator::State QUnevenIntSpinBox::validate(QString &text, int &pos) const
{
    QIntValidator validator;
    validator.setBottom(minimum());
    validator.setTop(maximum());
    QValidator::State state = validator.validate(text, pos);
    if (state == QValidator::Invalid)
    {
        return state;
    }

    int value = text.toInt();
    if (value % 2 == 0)
    {
        state = QValidator::Invalid;
    }
    else
    {
        state = QValidator::Acceptable;
    }
    return state;
}