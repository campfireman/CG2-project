#include <QSpinBox>

class UnevenIntSpinBox : public QSpinBox
{
    QValidator::State validate(QString &text, int &pos) const;
};