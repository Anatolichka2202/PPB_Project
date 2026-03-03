/**
 * @file dataviewwidget.h
 * @brief Виджет переключения режима отображения данных.
 *
 * Позволяет выбрать, как отображать значения: в физических величинах
 * (Вт, °C, коэффициент) или в шестнадцатеричных кодах (0x...).
 */
#ifndef DATAVIEWWIDGET_H
#define DATAVIEWWIDGET_H

#include <QWidget>

namespace Ui {
class DataViewWidget;
}
/**
 * @class DataViewWidget
 * @brief Переключатель режима отображения.
 *
 * Содержит два радиокнопки: «Физические величины» и «Коды».
 * При изменении выбора испускается сигнал displayModeChanged(bool).
 */
class DataViewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataViewWidget(QWidget *parent = nullptr);
    ~DataViewWidget();

    /**
 * @brief Возвращает текущий режим отображения.
 * @return true – отображать коды, false – физические величины.
 */
    bool showCodes() const;   // true – коды, false – физические величины

signals:
    /**
 * @fn void displayModeChanged(bool codes)
 * @brief Сигнал изменения режима отображения.
 * @param codes true – коды, false – физические величины.
 */
    void displayModeChanged(bool codes);

private slots:
    void onPhysicalToggled(bool checked);
    void onCodesToggled(bool checked);

private:
    Ui::DataViewWidget *ui;
};

#endif
