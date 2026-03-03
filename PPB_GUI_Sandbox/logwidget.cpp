#include "logwidget.h"
#include "ui_logwidget.h"

#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QTextCursor>
#include <QPainter>
#include <QDebug>
#include <QScrollBar>
#include "loguimanager.h"
// ==================== LogListModel ====================
LogListModel::LogListModel(QObject *parent)

    : QAbstractListModel(parent)
    , m_levelFilter("Все")
    , m_categoryFilter("Все")
{
    qDebug() << "Constructor of LogListModel";
}

void LogListModel::addEntry(const LogEntry &entry)
{
    beginInsertRows(QModelIndex(), m_allEntries.size(), m_allEntries.size());
    m_allEntries.append(entry);
    endInsertRows();

    // Обновляем уникальные категории
    if (!entry.category.isEmpty()) {
        m_uniqueCategories.insert(entry.category);
        emit categoriesChanged(m_uniqueCategories.values());
    }

    // Если запись проходит текущий фильтр, добавляем её индекс в filtered
    if (entryMatchesFilter(entry)) {
        int newRow = m_filteredIndices.size();
        beginInsertRows(QModelIndex(), newRow, newRow);
        m_filteredIndices.append(m_allEntries.size() - 1);
        endInsertRows();
    }
}

void LogListModel::clear()
{
    beginResetModel();
    m_allEntries.clear();
    m_filteredIndices.clear();
    m_uniqueCategories.clear();
    endResetModel();
    emit categoriesChanged(QStringList());
}

void LogListModel::setLevelFilter(const QString &level)
{
    m_levelFilter = level;
    applyFilter();
}

void LogListModel::setCategoryFilter(const QString &category)
{
    m_categoryFilter = category;
    applyFilter();
}

void LogListModel::setTextFilter(const QString &text)
{
    m_textFilter = text;
    applyFilter();
}

void LogListModel::applyFilter()
{
    beginResetModel();
    m_filteredIndices.clear();
    for (int i = 0; i < m_allEntries.size(); ++i) {
        if (entryMatchesFilter(m_allEntries[i]))
            m_filteredIndices.append(i);
    }
    endResetModel();
}

bool LogListModel::entryMatchesFilter(const LogEntry &entry) const
{
    // Фильтр по уровню
    if (m_levelFilter != "Все" && entry.level != m_levelFilter)
        return false;

    // Фильтр по категории
    if (m_categoryFilter != "Все" && entry.category != m_categoryFilter)
        return false;

    // Текстовый поиск (регистронезависимый)
    if (!m_textFilter.isEmpty() &&
        !entry.message.contains(m_textFilter, Qt::CaseInsensitive))
        return false;

    return true;
}

int LogListModel::rowCount(const QModelIndex &) const
{
    return m_filteredIndices.size();
}

QVariant LogListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_filteredIndices.size())
        return QVariant();

    const LogEntry &entry = m_allEntries[m_filteredIndices[index.row()]];

    switch (role) {
    case TimeRole:
        return entry.timestamp.toString("hh:mm:ss.zzz");
    case LevelRole:
        return entry.level;
    case CategoryRole:
        return entry.category;
    case MessageRole:
        return entry.message;
    case FullHtmlRole:
        return entry.toHtml();   // используем существующий метод
    default:
        return QVariant();
    }
}

// ==================== LogDelegate ====================
LogDelegate::LogDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , m_doc(new QTextDocument)
{
    // Загружаем стили из ресурсов
    QFile cssFile(":/assets/logstyles.css");
    if (cssFile.open(QIODevice::ReadOnly)) {
        QString css = QString::fromUtf8(cssFile.readAll());
        m_doc->setDefaultStyleSheet(css);
        cssFile.close();
    }
    qDebug() << "Constructor of LogDelegate";
}

LogDelegate::~LogDelegate()
{
    delete m_doc;
}

void LogDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    painter->save();

    // Получаем HTML
    QString html = index.data(LogListModel::FullHtmlRole).toString();

    // Настраиваем документ
    m_doc->setTextWidth(option.rect.width());
    m_doc->setHtml(html);

    // Фон выделения
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    }

    // Рисуем документ
    painter->translate(option.rect.topLeft());
    m_doc->drawContents(painter);
    painter->restore();
}

QSize LogDelegate::sizeHint(const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    if (!index.isValid())
        return QSize();

    QString html = index.data(LogListModel::FullHtmlRole).toString();

    // Определяем ширину: если option.rect имеет разумную ширину, используем её,
    // иначе берём ширину представления (но её тут нет), поэтому используем фиксированную
    int width = option.rect.width();
    if (width <= 0) width = 800; // запасной вариант

    m_doc->setTextWidth(width);
    m_doc->setHtml(html);
    return QSize(width, m_doc->size().toSize().height());
}

// ==================== LogWidget ====================
LogWidget::LogWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LogWidget)
    , m_model(new LogListModel(this))
    , m_delegate(new LogDelegate(this))
{
    ui->setupUi(this);

    qDebug() << "Constructor of LogWidget setupui";


    // Настройка представления
    ui->listView->setModel(m_model);
    ui->listView->setItemDelegate(m_delegate);
    ui->listView->setUniformItemSizes(false); // элементы могут быть разной высоты
    ui->listView->setWordWrap(true);
    ui->listView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Настройка комбобокса уровней
    setupLevelComboBox();

    // Подключение фильтров
    connect(ui->comboLevel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                QString level = ui->comboLevel->itemText(idx);
                onLevelFilterChanged(level);
            });
    connect(ui->comboCategory, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                QString cat = ui->comboCategory->itemText(idx);
                onCategoryFilterChanged(cat);
            });
    connect(ui->editSearch, &QLineEdit::textChanged,
            this, &LogWidget::onTextFilterChanged);

    // Кнопки
    connect(ui->btnClear, &QPushButton::clicked,
            this, &LogWidget::onClearClicked);
    connect(ui->btnExport, &QPushButton::clicked,
            this, &LogWidget::onExportClicked);

    // Подключение к глобальному логгеру
    connect(LogWrapper::instance(), &LogWrapper::logEntryReceived,
            this, &LogWidget::onLogEntryReceived);

    // Обновление комбобокса категорий при изменении уникальных категорий
    connect(m_model, &LogListModel::categoriesChanged,
            this, &LogWidget::updateCategoryComboBox);


    qDebug() << "connecting ui manager...";
    //подклбчение ui менеджера
    m_uiManager.setupLevelComboBox(ui->comboLevel);

    qDebug() << "connect uimanager";
    qDebug() << "Constructor of LogWidget finished";
}

LogWidget::~LogWidget()
{
    delete ui;
}

void LogWidget::setupLevelComboBox()
{
    ui->comboLevel->addItem("Все");
    ui->comboLevel->addItem("Debug");
    ui->comboLevel->addItem("Info");
    ui->comboLevel->addItem("Warning");
    ui->comboLevel->addItem("Error");
    ui->comboLevel->setCurrentIndex(1); // Info по умолчанию
}

void LogWidget::updateCategoryComboBox(const QStringList &categories)
{
    ui->comboCategory->clear();
    ui->comboCategory->addItem("Все");
    ui->comboCategory->addItems(categories);
    ui->comboCategory->setCurrentIndex(0);
}

void LogWidget::onLogEntryReceived(const LogEntry &entry)
{
    m_model->addEntry(entry);

    // Автопрокрутка вниз, если пользователь не скроллил вверх
    if (ui->listView->verticalScrollBar()->value() ==
        ui->listView->verticalScrollBar()->maximum()) {
        ui->listView->scrollToBottom();
    }
}

void LogWidget::onLevelFilterChanged(const QString &level)
{
    m_model->setLevelFilter(level);
}

void LogWidget::onCategoryFilterChanged(const QString &category)
{
    m_model->setCategoryFilter(category);
}

void LogWidget::onTextFilterChanged(const QString &text)
{
    m_model->setTextFilter(text);
}

void LogWidget::onClearClicked()
{
    m_model->clear();
   // LOG_UI_OPERATION("Logs cleared");
}

void LogWidget::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Экспорт лога", "",
                                                    "Текстовые файлы (*.txt);;HTML файлы (*.html)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
       // LOG_UI_ALERT("Не удалось открыть файл для записи: " + fileName);
        return;
    }

    QTextStream stream(&file);

    // Определяем, какие записи экспортировать: все или только отфильтрованные?
    // По умолчанию экспортируем отфильтрованные (что видит пользователь)
    int rowCount = m_model->rowCount();

    if (fileName.endsWith(".html", Qt::CaseInsensitive)) {
        stream << "<!DOCTYPE html>\n<html><head>\n<meta charset=\"UTF-8\">\n";
        stream << "<title>Лог ППБ</title>\n<style>\n";
        // Загружаем тот же CSS
        QFile cssFile(":/logging/styles/logstyles.css");
        if (cssFile.open(QIODevice::ReadOnly)) {
            stream << QString::fromUtf8(cssFile.readAll());
            cssFile.close();
        }
        stream << "\n</style>\n</head><body>\n";

        for (int i = 0; i < rowCount; ++i) {
            QModelIndex idx = m_model->index(i);
            QString html = idx.data(LogListModel::FullHtmlRole).toString();
            stream << html << "\n";
        }
        stream << "</body></html>\n";
    } else {
        stream << "=== Лог ППБ ===\n";
        stream << "Экспортировано: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
        stream << "Количество записей: " << rowCount << "\n\n";

        for (int i = 0; i < rowCount; ++i) {
            QModelIndex idx = m_model->index(i);
            QString time = idx.data(LogListModel::TimeRole).toString();
            QString level = idx.data(LogListModel::LevelRole).toString();
            QString cat = idx.data(LogListModel::CategoryRole).toString();
            QString msg = idx.data(LogListModel::MessageRole).toString();
            stream << QString("[%1] [%2] [%3] %4\n").arg(time, level, cat, msg);
        }
    }

    file.close();
   // LOG_UI_RESULT(QString("Log exported: %1 (%2 entries)").arg(fileName).arg(rowCount));
}
