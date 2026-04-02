#include "logwidget.h"
#include "ui_logwidget.h"

#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QTextCursor>
#include <QPainter>
#include <QDebug>
#include <QScrollBar>
#include "loguimanager.h"
#include <QCheckBox>
#include <thememanager.h>
// ==================== LogListModel ====================
LogListModel::LogListModel(QObject *parent)

    : QAbstractListModel(parent)
    , m_levelFilter("Все")
    , m_categoryFilter("Все")
    , m_showTech(false)
{
    qDebug() << "Constructor of LogListModel";
    m_batchTimer = new QTimer(this);
    m_batchTimer->setInterval(50); // 50 мс буферизации
    connect(m_batchTimer, &QTimer::timeout, this, &LogListModel::onBatchTimeout);
}

void LogListModel::addEntry(const LogEntry &entry)
{
    beginInsertRows(QModelIndex(), m_allEntries.size(), m_allEntries.size());
    m_allEntries.append(entry);
    m_cachedHtml.append(entry.toHtml()); // кэшируем HTML
    endInsertRows();

    if (!entry.category.isEmpty()) {
        m_uniqueCategories.insert(entry.category);
        emit categoriesChanged(m_uniqueCategories.values());
    }

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
    m_cachedHtml.clear(); // очищаем кэш
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

    // Скрыть технические логи, если флаг выключен
    if (!m_showTech) {
        if (entry.category.startsWith("TECH_") || entry.category == "UI_DATA")
            return false;
    }

    // Текстовый поиск
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

    int originalIndex = m_filteredIndices[index.row()];
    const LogEntry &entry = m_allEntries[originalIndex];

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
        if (originalIndex < m_cachedHtml.size())
            return m_cachedHtml[originalIndex];
        else
            return entry.toHtml(); // fallback
    default:
        return QVariant();
    }
}

void LogListModel::addPendingEntry(const LogEntry &entry)
{
    m_pendingEntries.append(entry);
    if (!m_batchTimer->isActive()) {
        m_batchTimer->start();
    }
}

void LogListModel::onBatchTimeout()
{
    flushPending();
}

void LogListModel::flushPending()
{
    if (m_pendingEntries.isEmpty())
        return;

    m_batchTimer->stop();

    // Добавляем все накопленные записи одним блоком
    int first = m_allEntries.size();
    int last = first + m_pendingEntries.size() - 1;
    beginInsertRows(QModelIndex(), first, last);

    for (const LogEntry& entry : m_pendingEntries) {
        m_allEntries.append(entry);
        // Кэшируем HTML
        m_cachedHtml.append(entry.toHtml());
        // Уникальные категории
        if (!entry.category.isEmpty()) {
            m_uniqueCategories.insert(entry.category);
        }
    }
    endInsertRows();

    m_pendingEntries.clear();

    // Пересчитываем фильтр (включая новые записи)
    applyFilter();

    // Обновляем комбобокс категорий один раз
    emit categoriesChanged(m_uniqueCategories.values());
}

// ==================== LogDelegate ====================
LogDelegate::LogDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , m_doc(new QTextDocument)
{

    updateStyleSheet(); // загрузим начальный CSS
    qDebug() << "Constructor of LogDelegate";
}

LogDelegate::~LogDelegate()
{
    delete m_doc;
}

void LogDelegate::updateStyleSheet()
{
    m_css = ThemeManager::instance().getHtmlLogStyle();
    m_doc->setDefaultStyleSheet(m_css);
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

    // Для поддержки Ctrl+C
    QAction* copyAction = new QAction("Копировать", this);
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this]() {
        QModelIndexList selected = ui->listView->selectionModel()->selectedIndexes();
        if (selected.isEmpty()) return;

        // Собираем текст выбранных строк
        QStringList lines;
        for (const QModelIndex& idx : selected) {
            QString time = idx.data(LogListModel::TimeRole).toString();
            QString level = idx.data(LogListModel::LevelRole).toString();
            QString cat = idx.data(LogListModel::CategoryRole).toString();
            QString msg = idx.data(LogListModel::MessageRole).toString();
            lines << QString("[%1] [%2] [%3] %4").arg(time, level, cat, msg);
        }
        QGuiApplication::clipboard()->setText(lines.join("\n"));
    });
    addAction(copyAction);

    // Настройка комбобокса уровней
    setupLevelComboBox();

    //===============================================================================================//
    // Добавление чекбокса для технических логов
    QCheckBox* chkShowTech = new QCheckBox("Показывать технические логи", this);
    chkShowTech->setChecked(false); // по умолчанию скрыты
    connect(chkShowTech, &QCheckBox::toggled, m_model, &LogListModel::setShowTech);

    QHBoxLayout* filterLayout = qobject_cast<QHBoxLayout*>(ui->comboLevel->parentWidget()->layout());
    if (filterLayout) {
        filterLayout->insertWidget(2, chkShowTech); // вставить между comboCategory и editSearch
    } else {
        // fallback: добавить в верхний layout (например, verticalLayout)
        ui->verticalLayout->insertWidget(1, chkShowTech);
    }
    //=================================================================================================//

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
    //Подключение к тем менеджеру
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &LogWidget::onThemeChanged);

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
    // ограничение на количество записей
    if (m_model->rowCount() > 10000) {
        static bool warned = false;
        if (!warned) {
            qWarning() << "Достигнут лимит лога (10000), новые записи игнорируются";
            warned = true;
        }
        return;
    }

    m_model->addPendingEntry(entry); // теперь пакетное добавление

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

void LogListModel::setShowTech(bool show)
{
    if (m_showTech == show) return;
    m_showTech = show;
    applyFilter();
}

void LogWidget::onThemeChanged()
{
    m_delegate->updateStyleSheet();
    // перерисовать все элементы
    ui->listView->viewport()->update();
}
