#include "updatemanager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVersionNumber>

namespace {
constexpr auto kReleasesUrl = "https://api.github.com/repos/Anatolichka2202/PPB_Project/releases?per_page=30";
constexpr auto kInstallerSuffix = "-Windows-x86_64.exe";

struct ParsedSemVer {
    QVersionNumber core;
    QStringList prerelease;
    bool valid = false;
};

ParsedSemVer parseSemVer(const QString& raw)
{
    static const QRegularExpression re(
        QStringLiteral(R"(^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?(?:\+[0-9A-Za-z.-]+)?$)"));

    const auto match = re.match(raw.trimmed());
    if (!match.hasMatch()) {
        return {};
    }

    ParsedSemVer result;
    result.core = QVersionNumber(match.captured(1).toInt(),
                                 match.captured(2).toInt(),
                                 match.captured(3).toInt());
    if (!match.captured(4).isEmpty()) {
        result.prerelease = match.captured(4).split('.');
    }
    result.valid = true;
    return result;
}

int comparePrereleaseIdentifier(const QString& left, const QString& right)
{
    bool leftNumeric = false;
    bool rightNumeric = false;
    const qlonglong leftNumber = left.toLongLong(&leftNumeric);
    const qlonglong rightNumber = right.toLongLong(&rightNumeric);

    if (leftNumeric && rightNumeric) {
        if (leftNumber < rightNumber) return -1;
        if (leftNumber > rightNumber) return 1;
        return 0;
    }
    if (leftNumeric != rightNumeric) {
        return leftNumeric ? -1 : 1;
    }
    return QString::compare(left, right, Qt::CaseSensitive);
}
}

UpdateManager::UpdateManager(QObject* parent)
    : QObject(parent),
      m_network(new QNetworkAccessManager(this))
{
}

void UpdateManager::checkForUpdates(bool interactive)
{
    QNetworkRequest request(QUrl(QString::fromLatin1(kReleasesUrl)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "PPB-Updater/1");
    request.setRawHeader("X-GitHub-Api-Version", "2026-03-10");
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#endif

    auto* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, interactive]() {
        handleReleaseList(reply, interactive);
    });
}

int UpdateManager::compareSemVer(const QString& left, const QString& right)
{
    const ParsedSemVer a = parseSemVer(left);
    const ParsedSemVer b = parseSemVer(right);
    if (!a.valid || !b.valid) {
        return 0;
    }

    const int coreCompare = QVersionNumber::compare(a.core, b.core);
    if (coreCompare != 0) {
        return coreCompare;
    }

    if (a.prerelease.isEmpty() && b.prerelease.isEmpty()) return 0;
    if (a.prerelease.isEmpty()) return 1;
    if (b.prerelease.isEmpty()) return -1;

    const int count = qMin(a.prerelease.size(), b.prerelease.size());
    for (int i = 0; i < count; ++i) {
        const int idCompare = comparePrereleaseIdentifier(a.prerelease.at(i), b.prerelease.at(i));
        if (idCompare < 0) return -1;
        if (idCompare > 0) return 1;
    }

    if (a.prerelease.size() < b.prerelease.size()) return -1;
    if (a.prerelease.size() > b.prerelease.size()) return 1;
    return 0;
}

bool UpdateManager::isValidSemVer(const QString& value)
{
    return parseSemVer(value).valid;
}

QByteArray UpdateManager::parseSha256Digest(const QString& digest)
{
    const QString prefix = QStringLiteral("sha256:");
    if (!digest.startsWith(prefix, Qt::CaseInsensitive)) {
        return {};
    }

    const QByteArray value = digest.mid(prefix.size()).trimmed().toLatin1().toLower();
    static const QRegularExpression re(QStringLiteral("^[0-9a-fA-F]{64}$"));
    if (!re.match(QString::fromLatin1(value)).hasMatch()) {
        return {};
    }
    return value;
}

void UpdateManager::handleReleaseList(QNetworkReply* reply, bool interactive)
{
    const auto cleanup = qScopeGuard([reply]() { reply->deleteLater(); });

    if (reply->error() != QNetworkReply::NoError) {
        if (interactive) {
            QMessageBox::warning(nullptr, tr("Проверка обновлений"),
                                 tr("Не удалось проверить обновления:\n%1").arg(reply->errorString()));
        }
        return;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (interactive) {
            QMessageBox::warning(nullptr, tr("Проверка обновлений"),
                                 tr("GitHub вернул некорректный ответ."));
        }
        return;
    }

    const QString currentVersion = QCoreApplication::applicationVersion();
    const bool allowPrerelease = currentVersion.contains('-');
    Candidate best;

    for (const QJsonValue& value : document.array()) {
        const QJsonObject release = value.toObject();
        if (release.value("draft").toBool()) {
            continue;
        }
        if (release.value("prerelease").toBool() && !allowPrerelease) {
            continue;
        }

        QString version = release.value("tag_name").toString().trimmed();
        if (version.startsWith('v')) {
            version.remove(0, 1);
        }
        if (!isValidSemVer(version) || compareSemVer(version, currentVersion) <= 0) {
            continue;
        }

        Candidate candidate;
        candidate.version = version;

        for (const QJsonValue& assetValue : release.value("assets").toArray()) {
            const QJsonObject asset = assetValue.toObject();
            const QString name = asset.value("name").toString();
            if (!name.startsWith(QStringLiteral("PPB-")) || !name.endsWith(QString::fromLatin1(kInstallerSuffix))) {
                continue;
            }

            const QUrl url(asset.value("browser_download_url").toString());
            if (!url.isValid() || url.scheme() != QStringLiteral("https") ||
                url.host().compare(QStringLiteral("github.com"), Qt::CaseInsensitive) != 0) {
                continue;
            }

            candidate.installerUrl = url;
            candidate.sha256 = parseSha256Digest(asset.value("digest").toString());
            break;
        }

        if (!candidate.installerUrl.isValid()) {
            continue;
        }

        if (best.version.isEmpty() || compareSemVer(candidate.version, best.version) > 0) {
            best = candidate;
        }
    }

    if (best.version.isEmpty()) {
        if (interactive) {
            QMessageBox::information(nullptr, tr("Проверка обновлений"),
                                     tr("Установлена актуальная версия %1.").arg(currentVersion));
        }
        return;
    }

    offerUpdate(best);
}

void UpdateManager::offerUpdate(const Candidate& candidate)
{
    const auto answer = QMessageBox::question(
        nullptr,
        tr("Доступно обновление"),
        tr("Доступна версия PPB %1.\n\nТекущая версия: %2\n\nСкачать установщик и начать обновление?")
            .arg(candidate.version, QCoreApplication::applicationVersion()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (answer == QMessageBox::Yes) {
        downloadAndInstall(candidate);
    }
}

void UpdateManager::downloadAndInstall(const Candidate& candidate)
{
    if (m_downloadReply) {
        return;
    }

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDir.isEmpty()) {
        failDownload(tr("Не удалось определить временный каталог."));
        return;
    }

    m_downloadCandidate = candidate;
    m_downloadPath = QDir(tempDir).filePath(QStringLiteral("PPB-%1-Windows-x86_64.exe").arg(candidate.version));
    QFile::remove(m_downloadPath);

    auto* output = new QFile(m_downloadPath, this);
    if (!output->open(QIODevice::WriteOnly)) {
        output->deleteLater();
        failDownload(tr("Не удалось создать файл обновления:\n%1").arg(m_downloadPath));
        return;
    }

    m_progress = new QProgressDialog(tr("Загрузка PPB %1...").arg(candidate.version),
                                     tr("Отмена"), 0, 100, nullptr);
    m_progress->setWindowTitle(tr("Обновление PPB"));
    m_progress->setWindowModality(Qt::ApplicationModal);
    m_progress->setMinimumDuration(0);
    m_progress->setValue(0);

    QNetworkRequest request(candidate.installerUrl);
    request.setRawHeader("User-Agent", "PPB-Updater/1");
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#endif

    m_downloadReply = m_network->get(request);
    auto* reply = m_downloadReply.data();

    connect(reply, &QNetworkReply::readyRead, this, [reply, output]() {
        output->write(reply->readAll());
    });

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        if (!m_progress || total <= 0) return;
        m_progress->setValue(static_cast<int>((received * 100) / total));
    });

    connect(m_progress, &QProgressDialog::canceled, this, [reply]() {
        reply->abort();
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, output]() {
        output->write(reply->readAll());
        output->close();
        output->deleteLater();

        if (m_progress) {
            m_progress->setValue(100);
            m_progress->deleteLater();
            m_progress = nullptr;
        }

        const auto replyError = reply->error();
        const QString replyErrorText = reply->errorString();
        reply->deleteLater();
        m_downloadReply = nullptr;

        if (replyError != QNetworkReply::NoError) {
            QFile::remove(m_downloadPath);
            if (replyError != QNetworkReply::OperationCanceledError) {
                failDownload(tr("Не удалось скачать обновление:\n%1").arg(replyErrorText));
            }
            return;
        }

        if (!m_downloadCandidate.sha256.isEmpty()) {
            QFile installer(m_downloadPath);
            if (!installer.open(QIODevice::ReadOnly)) {
                QFile::remove(m_downloadPath);
                failDownload(tr("Не удалось открыть загруженный установщик для проверки."));
                return;
            }

            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (!hash.addData(&installer)) {
                installer.close();
                QFile::remove(m_downloadPath);
                failDownload(tr("Не удалось вычислить SHA-256 установщика."));
                return;
            }
            installer.close();

            const QByteArray actual = hash.result().toHex().toLower();
            if (actual != m_downloadCandidate.sha256) {
                QFile::remove(m_downloadPath);
                failDownload(tr("SHA-256 загруженного установщика не совпадает с GitHub Release.\nОбновление отменено."));
                return;
            }
        }

        if (!QProcess::startDetached(m_downloadPath, {})) {
            failDownload(tr("Установщик загружен, но Windows не смог его запустить:\n%1").arg(m_downloadPath));
            return;
        }

        QCoreApplication::quit();
    });
}

void UpdateManager::failDownload(const QString& message)
{
    QMessageBox::critical(nullptr, tr("Обновление PPB"), message);
}
