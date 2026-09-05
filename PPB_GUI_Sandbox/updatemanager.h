#pragma once

#include <QObject>
#include <QPointer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QProgressDialog;

class UpdateManager final : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(QObject* parent = nullptr);

    void checkForUpdates(bool interactive = false);

private:
    struct Candidate {
        QString version;
        QUrl installerUrl;
        QByteArray sha256;
    };

    static int compareSemVer(const QString& left, const QString& right);
    static bool isValidSemVer(const QString& value);
    static QByteArray parseSha256Digest(const QString& digest);

    void handleReleaseList(QNetworkReply* reply, bool interactive);
    void offerUpdate(const Candidate& candidate);
    void downloadAndInstall(const Candidate& candidate);
    void failDownload(const QString& message);

    QNetworkAccessManager* m_network = nullptr;
    QPointer<QNetworkReply> m_downloadReply;
    QPointer<QProgressDialog> m_progress;
    QString m_downloadPath;
    Candidate m_downloadCandidate;
};
