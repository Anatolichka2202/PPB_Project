#ifndef LOGUIMANAGER_H
#define LOGUIMANAGER_H

#include <QObject>
#include <QComboBox>
#include "logconfig.h"

class LogUIManager : public QObject
{
    Q_OBJECT

public:
    explicit LogUIManager(QObject* parent = nullptr);

    // Привязка комбобокса выбора уровня логирования
    void setupLevelComboBox(QComboBox* levelCombo);

    // Получение текущего уровня (если нужно)
    LogLevel getCurrentLevel() const;

public slots:
    void onLevelChanged(int index);

private:
    void updateUIConfig();

    QComboBox* m_levelCombo;
};

#endif // LOGUIMANAGER_H
