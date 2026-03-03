/**
 * @file logwidget.h
 * @brief Виджет отображения логов с возможностью фильтрации и экспорта.
 *
 * Использует собственную модель LogListModel и делегат LogDelegate для
 * форматированного вывода (HTML). Логи поступают через глобальный объект
 * LogWrapper (синглтон).
 */
#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QSet>
#include "dependencies.h"
#include "loguimanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LogWidget; }
QT_END_NAMESPACE

// ==================== МОДЕЛЬ ====================
/**
 * @class LogListModel
 * @brief Модель списка логов для QListView.
 *
 * Хранит все записи (LogEntry) и поддерживает фильтрацию по уровню,
 * категории и текстовому поиску. Отображает только отфильтрованные записи.
 */

class LogListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    /**
 * @enum Roles
 * @brief Кастомные роли для доступа к данным записи.
 */
    enum Roles {
        TimeRole = Qt::UserRole + 1,
        LevelRole,
        CategoryRole,
        MessageRole,
        FullHtmlRole
    };

    explicit LogListModel(QObject *parent = nullptr);

    // Управление данными
    /**
 * @brief Добавляет новую запись в модель.
 * @param entry Запись лога.
 *
 * Запись добавляется в общий массив. Если она проходит текущий фильтр,
 * то также появляется в отфильтрованном представлении.
 */
    void addEntry(const LogEntry &entry);

    /**
 * @brief Очищает все записи и сбрасывает фильтры.
 */
    void clear();

    // Фильтры
    /**
 * @brief Устанавливает фильтр по уровню.
 * @param level Уровень (например, "Info") или "Все".
 */
    void setLevelFilter(const QString &level);

    /**
 * @brief Устанавливает фильтр по категории.
 * @param category Категория или "Все".
 */
    void setCategoryFilter(const QString &category);

    /**
 * @brief Устанавливает текстовый фильтр (поиск по сообщению).
 * @param text Подстрока для поиска.
 */
    void setTextFilter(const QString &text);

    // Получение уникальных категорий для комбобокса
    /**
 * @brief Возвращает множество уникальных категорий среди всех записей.
 * @return QSet<QString> категорий.
 */
    QSet<QString> uniqueCategories() const { return m_uniqueCategories; }

    // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

signals:
    /**
 * @fn void categoriesChanged(const QStringList &categories)
 * @brief Сигнал об изменении списка уникальных категорий.
 * @param categories Новый список (включая "Все").
 */
    void categoriesChanged(const QStringList &categories); // для обновления комбобокса

private:
    void applyFilter(); ///< Применяет текущие фильтры к модели.
    bool entryMatchesFilter(const LogEntry &entry) const; ///< Проверяет, проходит ли запись фильтры.

    QVector<LogEntry> m_allEntries;          // все записи
    QVector<int>      m_filteredIndices;     // индексы записей, проходящих фильтр
    QSet<QString>     m_uniqueCategories;    // уникальные категории для комбобокса



    QString m_levelFilter;
    QString m_categoryFilter;
    QString m_textFilter;
};

// ==================== ДЕЛЕГАТ ====================
/**
 * @class LogDelegate
 * @brief Делегат для рисования записей лога с использованием HTML.
 *
 * Использует QTextDocument для форматированного вывода и поддержки стилей.
 */
class LogDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit LogDelegate(QObject *parent = nullptr);
    ~LogDelegate();

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;///< Рисует HTML-содержимое.
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;///< Вычисляет размер элемента.

private:
    QTextDocument *m_doc;
};

// ==================== ВИДЖЕТ ====================
/**
 * @class LogWidget
 * @brief Виджет логов с элементами управления (фильтры, кнопки очистки/экспорта).
 *
 * Содержит QListView, комбобоксы выбора уровня и категории, поле поиска,
 * кнопки "Очистить" и "Экспорт". Подключается к сигналу LogWrapper::logEntryReceived.
 */
class LogWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LogWidget(QWidget *parent = nullptr);
    ~LogWidget();

private slots:
    /**
 * @brief Обработчик поступления новой записи лога.
 * @param entry Запись лога.
 */
    void onLogEntryReceived(const LogEntry &entry);

    /**
 * @brief Обработчик изменения фильтра уровня.
 * @param level Выбранный уровень.
 */
    void onLevelFilterChanged(const QString &level);

    /**
 * @brief Обработчик изменения фильтра категории.
 * @param category Выбранная категория.
 */
    void onCategoryFilterChanged(const QString &category);

    /**
 * @brief Обработчик изменения текстового фильтра.
 * @param text Текст для поиска.
 */
    void onTextFilterChanged(const QString &text);

    /**
 * @brief Очищает все логи.
 */
    void onClearClicked();

    /**
 * @brief Экспортирует текущее (отфильтрованное) представление в файл.
 * Поддерживает форматы .txt и .html.
 */
    void onExportClicked();

private:
    void setupLevelComboBox();
    void updateCategoryComboBox(const QStringList &categories);

     LogUIManager m_uiManager;
    Ui::LogWidget *ui;
    LogListModel   *m_model;
    LogDelegate    *m_delegate;
};

#endif // LOGWIDGET_H
