#include <QSpinBox>

class QUnevenIntSpinBox : public QSpinBox
{
    QValidator::State validate(QString &text, int &pos) const;
};