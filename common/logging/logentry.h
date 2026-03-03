#ifndef LOGENTRY_H
#define LOGENTRY_H

#include <QDateTime>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QMap>
#include <QColor>
#include <QMetaType>

// ==================== СТРУКТУРИРОВАННЫЕ ДАННЫЕ ====================

// Типы структурированных данных
enum class DataType {
    Text,           // Простой текст
    Table,          // Таблица
    Card,           // Карточка
    Progress,       // Прогресс-бар
    KeyValue,       // Ключ-значение
    Chart,          // Диаграмма
    RawData         // Сырые данные (hex и т.д.)
};

// Структура для табличных данных
struct TableData {
    QString id;                     // Уникальный ID для обновления
    QString title;                  // Заголовок таблицы
    QStringList headers;            // Заголовки столбцов
    QVector<QStringList> rows;      // Данные строк
    QMap<int, QString> columnFormats;  // Форматы столбцов: "hex", "dec", "float", "percent"
    QMap<int, QColor> columnColors;    // Цвета столбцов
    bool sortable = false;          // Можно ли сортировать
    bool compact = false;           // Компактный режим

    TableData() = default;

    // Добавление строки
    void addRow(const QStringList& row) {
        rows.append(row);
    }

    // Очистка данных
    void clear() {
        rows.clear();
        headers.clear();
        columnFormats.clear();
        columnColors.clear();
    }

    // Проверка на пустоту
    bool isEmpty() const {
        return rows.isEmpty();
    }

    // Получение HTML представления
    QString toHtml() const;

    // Получение CSV представления
    QString toCsv() const;
};

// Структура для карточки
struct CardData {
    QString id;                     // Уникальный ID
    QString title;                  // Заголовок карточки
    QString icon;                   // Иконка (эмодзи или HTML)
    QColor backgroundColor;         // Цвет фона
    QColor textColor;               // Цвет текста
    QMap<QString, QString> fields;  // Поля: ключ-значение
    QString footer;                 // Текст внизу
    bool closable = false;          // Можно закрыть
    bool sticky = false;            // Остаётся наверху

    CardData() = default;

    // Добавление поля
    void addField(const QString& key, const QString& value) {
        fields[key] = value;
    }

    // Получение HTML представления
    QString toHtml() const;
};

// Структура для прогресс-бара
struct ProgressData {
    QString id;                     // Уникальный ID
    QString title;                  // Заголовок
    int current = 0;                // Текущее значение
    int total = 100;                // Всего
    QString units;                  // Единицы измерения
    QColor barColor;                // Цвет прогресс-бара
    QColor backgroundColor;         // Цвет фона
    bool showPercentage = true;     // Показывать процент
    bool showText = true;           // Показывать текст (123/456)
    bool animated = false;          // Анимация

    ProgressData() = default;

    // Обновление прогресса
    void update(int newCurrent, int newTotal = -1) {
        current = newCurrent;
        if (newTotal > 0) {
            total = newTotal;
        }
    }

    // Получение процента
    int percentage() const {
        if (total == 0) return 0;
        return (current * 100) / total;
    }

    // Проверка завершения
    bool isComplete() const {
        return current >= total;
    }

    // Получение HTML представления
    QString toHtml() const;
};

// Структура для данных ключ-значение
struct KeyValueData {
    QString id;                     // Уникальный ID
    QString title;                  // Заголовок
    QMap<QString, QString> data;   // Данные
    bool horizontal = false;        // Горизонтальное расположение
    bool showBorder = true;         // Показывать границы

    KeyValueData() = default;

    // Добавление данных
    void add(const QString& key, const QString& value) {
        data[key] = value;
    }

    // Получение HTML представления
    QString toHtml() const;
};

// Структура для сырых данных (hex дамп и т.д.)
struct RawData {
    QString id;                     // Уникальный ID
    QString title;                  // Заголовок
    QByteArray data;                // Сырые данные
    QString format;                 // Формат: "hex", "ascii", "binary"
    bool showOffset = true;         // Показывать смещение
    bool showAscii = true;          // Показывать ASCII представление
    int bytesPerLine = 16;          // Байт на строку

    RawData() = default;

    // Получение HEX дампа
    QString toHexDump() const;

    // Получение HTML представления
    QString toHtml() const;
};

// ==================== ОСНОВНАЯ СТРУКТУРА LOGENTRY ====================

struct LogEntry {
    QDateTime timestamp;
    QString level;                  // "DEBUG", "INFO", "WARNING", "ERROR"
    QString category;               // Категория (UI_STATUS, TECH_DEBUG и т.д.)
    QString message;                // Сообщение
    QVariant data;                  // Дополнительные данные

    // Новые поля для структурированных данных
    DataType dataType = DataType::Text;    // Тип данных
    QString structuredDataId;              // ID структурированных данных (для обновления)
    QVariant structuredData;               // Структурированные данные

    // Метаданные
    QString sourceFile;                    // Исходный файл
    QString sourceFunction;                // Функция
    int sourceLine = 0;                    // Номер строки
    qint64 threadId = 0;                   // ID потока
    qint64 elapsedMs = 0;                  // Время выполнения (если применимо)

    // Конструкторы для обратной совместимости
    LogEntry() = default;

    LogEntry(const QString& lvl, const QString& cat, const QString& msg)
        : timestamp(QDateTime::currentDateTime())
        , level(lvl)
        , category(cat)
        , message(msg)
        , dataType(DataType::Text) {}

    // Конструктор для структурированных данных
    LogEntry(const QString& lvl, const QString& cat, DataType type, const QVariant& data)
        : timestamp(QDateTime::currentDateTime())
        , level(lvl)
        , category(cat)
        , dataType(type)
        , structuredData(data) {}

    // Фабричные методы для создания структурированных записей

    // Таблица
    static LogEntry createTable(const QString& category, const TableData& table) {
        LogEntry entry("INFO", category, DataType::Table, QVariant::fromValue(table));
        entry.structuredDataId = table.id;
        entry.message = table.title;
        return entry;
    }

    // Карточка
    static LogEntry createCard(const QString& category, const CardData& card) {
        LogEntry entry("INFO", category, DataType::Card, QVariant::fromValue(card));
        entry.structuredDataId = card.id;
        entry.message = card.title;
        return entry;
    }

    // Прогресс
    static LogEntry createProgress(const QString& category, const ProgressData& progress) {
        LogEntry entry("INFO", category, DataType::Progress, QVariant::fromValue(progress));
        entry.structuredDataId = progress.id;
        entry.message = progress.title;
        return entry;
    }

    // Ключ-значение
    static LogEntry createKeyValue(const QString& category, const KeyValueData& kv) {
        LogEntry entry("INFO", category, DataType::KeyValue, QVariant::fromValue(kv));
        entry.structuredDataId = kv.id;
        entry.message = kv.title;
        return entry;
    }

    // Сырые данные
    static LogEntry createRawData(const QString& category, const RawData& raw) {
        LogEntry entry("DEBUG", category, DataType::RawData, QVariant::fromValue(raw));
        entry.structuredDataId = raw.id;
        entry.message = raw.title;
        return entry;
    }

    // Методы для проверки типа данных
    bool isText() const { return dataType == DataType::Text; }
    bool isTable() const { return dataType == DataType::Table; }
    bool isCard() const { return dataType == DataType::Card; }
    bool isProgress() const { return dataType == DataType::Progress; }
    bool isKeyValue() const { return dataType == DataType::KeyValue; }
    bool isRawData() const { return dataType == DataType::RawData; }

    // Методы для получения структурированных данных
    TableData getTable() const { return structuredData.value<TableData>(); }
    CardData getCard() const { return structuredData.value<CardData>(); }
    ProgressData getProgress() const { return structuredData.value<ProgressData>(); }
    KeyValueData getKeyValue() const { return structuredData.value<KeyValueData>(); }
    RawData getRawData() const { return structuredData.value<RawData>(); }

    // Методы для преобразования
    QString toString() const;
    QString toHtml() const;
    QString toJson() const;

    QString toLegacyString() const;
    QString toLegacyFormat() const;
    // Для сортировки по времени
    bool operator<(const LogEntry& other) const {
        return timestamp < other.timestamp;
    }

    bool operator==(const LogEntry& other) const {
        return timestamp == other.timestamp &&
               level == other.level &&
               category == other.category &&
               message == other.message &&
               structuredDataId == other.structuredDataId;
    }
};

// Регистрация типов для QVariant
Q_DECLARE_METATYPE(TableData)
Q_DECLARE_METATYPE(CardData)
Q_DECLARE_METATYPE(ProgressData)
Q_DECLARE_METATYPE(KeyValueData)
Q_DECLARE_METATYPE(RawData)
Q_DECLARE_METATYPE(LogEntry)

#endif // LOGENTRY_H
