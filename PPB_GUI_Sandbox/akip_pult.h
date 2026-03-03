
/**
 * @file akip_pult.h
 * @brief Виджет управления АКИП (не реализован).
 *
 * @deprecated Данный виджет не задействован в текущей версии интерфейса.
 * Вместо него используется AkipWidget. Пульт будет являться расширенной версией.
 */

#ifndef AKIP_PULT_H
#define AKIP_PULT_H

#include <QWidget>

namespace Ui {
class akip_pult;
}

class akip_pult : public QWidget
{
    Q_OBJECT

public:
    explicit akip_pult(QWidget *parent = nullptr);
    ~akip_pult();

private:
    Ui::akip_pult *ui;
};

#endif // AKIP_PULT_H
