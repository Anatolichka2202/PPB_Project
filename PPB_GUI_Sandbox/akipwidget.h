#ifndef AKIPWIDGET_H
#define AKIPWIDGET_H

#include <QWidget>

namespace Ui {
class AkipWidget;
}

class AkipWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AkipWidget(QWidget *parent = nullptr);
    ~AkipWidget();

    // Геттеры
    double frequencyA() const;   // МГц
    double powerA() const;       // дБм
    bool outputA() const;

    double frequencyB() const;   // МГц
    double amplitudeB() const;   // Впп
    double dutyCycleB() const;   // %
    bool outputB() const;

signals:
    void applyClicked();

private:
    Ui::AkipWidget *ui;
};

#endif
