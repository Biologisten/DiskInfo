#include "thememanager.h"
#include "ziparchive.h"

#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

ThemeManager::ThemeManager(QStringList themeRoots, QString writableRoot)
    : m_themeRoots(std::move(themeRoots))
    , m_writableRoot(std::move(writableRoot))
{
}

QStringList ThemeManager::defaultThemeRoots()
{
    const QStringList locations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    QStringList roots;
    for (const QString &loc : locations) {
        const QString cleaned = QDir::cleanPath(loc + QLatin1String("/qdiskinfo/themes"));
        if (!roots.contains(cleaned)) {
            roots.append(cleaned);
        }
    }
    return roots;
}

QString ThemeManager::defaultWritableRoot()
{
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/qdiskinfo/themes"));
}

void ThemeManager::reload()
{
    m_themes.clear();

    for (const QString &root : m_themeRoots) {
        const QDir rootDir(root);
        if (!rootDir.exists()) {
            continue;
        }

        const QFileInfoList entries = rootDir.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name | QDir::IgnoreCase);

        for (const QFileInfo &entry : entries) {
            const QString iniPath = entry.filePath() + QLatin1String("/theme.ini");
            if (!QFileInfo::exists(iniPath)) {
                continue;
            }

            const Theme t = Theme::parse(entry.filePath());
            if (!t.isValid()) {
                continue;
            }

            const QString tName = t.name();
            if (!m_themes.contains(tName)) {
                m_themes.insert(tName, t);
            }
        }
    }

    for (auto it = m_themes.begin(); it != m_themes.end(); ++it) {
        it.value().resolve(m_themes);
    }
}

const QHash<QString, Theme> &ThemeManager::themes() const
{
    return m_themes;
}

QStringList ThemeManager::themeNames() const
{
    QStringList names = m_themes.keys();
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        const int cmp = a.compare(b, Qt::CaseInsensitive);
        if (cmp != 0) {
            return cmp < 0;
        }
        return a < b;
    });
    return names;
}

const Theme *ThemeManager::theme(const QString &name) const
{
    const auto it = m_themes.constFind(name);
    if (it != m_themes.constEnd()) {
        return &it.value();
    }
    return nullptr;
}

QString ThemeManager::writableRoot() const
{
    return m_writableRoot;
}

namespace {

bool isAllowedExtension(const QString &fileName)
{
    const QString lower = fileName.toLower();
    return lower.endsWith(QLatin1String(".ini")) ||
           lower.endsWith(QLatin1String(".png")) ||
           lower.endsWith(QLatin1String(".jpg")) ||
           lower.endsWith(QLatin1String(".jpeg")) ||
           lower.endsWith(QLatin1String(".bmp"));
}

} // namespace

ThemeZipInfo ThemeManager::inspectZip(const QString &zipPath) const
{
    ThemeZipInfo info;
    ZipArchive zip;
    QString err;
    if (!zip.open(zipPath, &err)) {
        info.error = err;
        return info;
    }

    const QVector<ZipEntry> entries = zip.entries();
    if (entries.size() > 5000) {
        info.error = QStringLiteral("Archive exceeds entry limit (max 5000).");
        return info;
    }

    QSet<QString> seenNames;
    QStringList foundNames;

    for (const ZipEntry &entry : entries) {
        if (entry.isDirectory) {
            continue;
        }

        QString normName = entry.name;
        normName.replace(QLatin1Char('\\'), QLatin1Char('/'));

        bool isThemeIni = false;
        QString rootDir;
        QString themeName;

        if (normName.compare(QLatin1String("theme.ini"), Qt::CaseInsensitive) == 0) {
            isThemeIni = true;
            rootDir = QString();
            themeName = QFileInfo(zipPath).completeBaseName();
        } else if (normName.endsWith(QLatin1String("/theme.ini"), Qt::CaseInsensitive)) {
            isThemeIni = true;
            rootDir = normName.left(normName.lastIndexOf(QLatin1Char('/')));
            const qsizetype slashIdx = rootDir.lastIndexOf(QLatin1Char('/'));
            themeName = (slashIdx == -1) ? rootDir : rootDir.mid(slashIdx + 1);
        }

        if (isThemeIni) {
            if (themeName.isEmpty() ||
                themeName == QLatin1String(".") ||
                themeName == QLatin1String("..") ||
                themeName.contains(QLatin1Char('/')) ||
                themeName.contains(QLatin1Char('\\')) ||
                themeName.contains(QLatin1Char('\0'))) {
                continue;
            }

            if (!seenNames.contains(themeName)) {
                seenNames.insert(themeName);
                foundNames.append(themeName);
            }
        }
    }

    if (foundNames.isEmpty()) {
        info.error = QStringLiteral("No CDI themes found in archive.");
        return info;
    }

    foundNames.sort(Qt::CaseInsensitive);
    info.names = foundNames;

    for (const QString &name : foundNames) {
        const QString targetPath = QDir(m_writableRoot).filePath(name);
        if (QFileInfo::exists(targetPath)) {
            info.existing.append(name);
        }
    }
    info.existing.sort(Qt::CaseInsensitive);

    return info;
}

ThemeImportResult ThemeManager::importZip(const QString &zipPath, bool overwriteExisting)
{
    ThemeImportResult result;

    const ThemeZipInfo info = inspectZip(zipPath);
    if (!info.ok()) {
        result.error = info.error;
        return result;
    }

    ZipArchive zip;
    QString openErr;
    if (!zip.open(zipPath, &openErr)) {
        result.error = openErr;
        return result;
    }

    const QVector<ZipEntry> entries = zip.entries();

    for (const ZipEntry &entry : entries) {
        if (!ZipArchive::isSafePath(entry.name)) {
            result.error = QStringLiteral("Archive contains unsafe path: ") + entry.name;
            return result;
        }
    }

    struct ThemeRoot {
        QString themeName;
        QString rootDir;
    };
    QVector<ThemeRoot> themeRoots;
    QSet<QString> seenNames;

    for (const ZipEntry &entry : entries) {
        if (entry.isDirectory) {
            continue;
        }

        QString normName = entry.name;
        normName.replace(QLatin1Char('\\'), QLatin1Char('/'));

        bool isThemeIni = false;
        QString rootDir;
        QString themeName;

        if (normName.compare(QLatin1String("theme.ini"), Qt::CaseInsensitive) == 0) {
            isThemeIni = true;
            rootDir = QString();
            themeName = QFileInfo(zipPath).completeBaseName();
        } else if (normName.endsWith(QLatin1String("/theme.ini"), Qt::CaseInsensitive)) {
            isThemeIni = true;
            rootDir = normName.left(normName.lastIndexOf(QLatin1Char('/')));
            const qsizetype slashIdx = rootDir.lastIndexOf(QLatin1Char('/'));
            themeName = (slashIdx == -1) ? rootDir : rootDir.mid(slashIdx + 1);
        }

        if (isThemeIni) {
            if (themeName.isEmpty() ||
                themeName == QLatin1String(".") ||
                themeName == QLatin1String("..") ||
                themeName.contains(QLatin1Char('/')) ||
                themeName.contains(QLatin1Char('\\')) ||
                themeName.contains(QLatin1Char('\0'))) {
                continue;
            }

            if (!seenNames.contains(themeName)) {
                seenNames.insert(themeName);
                themeRoots.append({themeName, rootDir});
            }
        }
    }

    QVector<ThemeRoot> toImport;
    for (const ThemeRoot &tr : themeRoots) {
        const QString targetPath = QDir(m_writableRoot).filePath(tr.themeName);
        if (QFileInfo::exists(targetPath) && !overwriteExisting) {
            result.skipped.append(tr.themeName);
        } else {
            toImport.append(tr);
        }
    }

    quint64 totalUncompressed = 0;
    const quint64 maxSingleFile = 64ULL * 1024ULL * 1024ULL;
    const quint64 maxTotal = 256ULL * 1024ULL * 1024ULL;

    for (const ThemeRoot &tr : toImport) {
        const QString prefix = tr.rootDir.isEmpty() ? QString() : tr.rootDir + QLatin1Char('/');
        for (const ZipEntry &entry : entries) {
            if (entry.isDirectory) {
                continue;
            }
            QString normName = entry.name;
            normName.replace(QLatin1Char('\\'), QLatin1Char('/'));
            if (!prefix.isEmpty() && !normName.startsWith(prefix, Qt::CaseInsensitive)) {
                continue;
            }
            if (prefix.isEmpty() && normName.contains(QLatin1Char('/'))) {
                continue;
            }

            const QString relPath = prefix.isEmpty() ? normName : normName.mid(prefix.length());
            if (!isAllowedExtension(relPath)) {
                continue;
            }

            if (entry.isSymlink) {
                result.error = QStringLiteral("Archive contains symlink: ") + entry.name;
                return result;
            }

            if (entry.uncompressedSize > maxSingleFile) {
                result.error = QStringLiteral("File exceeds size limit (64MB): ") + entry.name;
                return result;
            }

            totalUncompressed += entry.uncompressedSize;
            if (totalUncompressed > maxTotal) {
                result.error = QStringLiteral("Total uncompressed size exceeds limit (256MB).");
                return result;
            }
        }
    }

    if (toImport.isEmpty()) {
        result.skipped.sort(Qt::CaseInsensitive);
        return result;
    }

    QDir().mkpath(m_writableRoot);
    const QString importUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString importTempRoot = QDir(m_writableRoot).filePath(QStringLiteral(".import-") + importUuid);
    if (!QDir().mkpath(importTempRoot)) {
        result.error = QStringLiteral("Failed to create temporary directory for extraction.");
        return result;
    }

    for (const ThemeRoot &tr : toImport) {
        const QString tempThemeDir = QDir(importTempRoot).filePath(tr.themeName);
        if (!QDir().mkpath(tempThemeDir)) {
            result.failed.append(tr.themeName);
            continue;
        }

        const QString prefix = tr.rootDir.isEmpty() ? QString() : tr.rootDir + QLatin1Char('/');
        bool themeExtractOk = true;

        for (const ZipEntry &entry : entries) {
            if (entry.isDirectory) {
                continue;
            }
            QString normName = entry.name;
            normName.replace(QLatin1Char('\\'), QLatin1Char('/'));
            if (!prefix.isEmpty() && !normName.startsWith(prefix, Qt::CaseInsensitive)) {
                continue;
            }
            if (prefix.isEmpty() && normName.contains(QLatin1Char('/'))) {
                continue;
            }

            const QString relPath = prefix.isEmpty() ? normName : normName.mid(prefix.length());
            if (!isAllowedExtension(relPath)) {
                continue;
            }

            QByteArray data;
            QString readErr;
            if (!zip.read(entry, &data, &readErr)) {
                result.error = QStringLiteral("Failed to read ") + entry.name + QStringLiteral(": ") + readErr;
                QDir(importTempRoot).removeRecursively();
                return result;
            }

            const QString destFilePath = QDir(tempThemeDir).filePath(relPath);
            QDir().mkpath(QFileInfo(destFilePath).absolutePath());
            QFile outFile(destFilePath);
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                themeExtractOk = false;
                break;
            }
            outFile.write(data);
            outFile.close();
        }

        if (!themeExtractOk || !QFileInfo::exists(tempThemeDir + QLatin1String("/theme.ini"))) {
            result.failed.append(tr.themeName);
            continue;
        }

        const QString targetPath = QDir(m_writableRoot).filePath(tr.themeName);
        if (QFileInfo::exists(targetPath)) {
            const QString backupPath = targetPath + QStringLiteral(".old-") + importUuid;
            if (!QDir().rename(targetPath, backupPath)) {
                result.failed.append(tr.themeName);
                continue;
            }
            if (!QDir().rename(tempThemeDir, targetPath)) {
                QDir().rename(backupPath, targetPath);
                result.failed.append(tr.themeName);
                continue;
            }
            QDir(backupPath).removeRecursively();
            result.imported.append(tr.themeName);
        } else {
            if (QDir().rename(tempThemeDir, targetPath)) {
                result.imported.append(tr.themeName);
            } else {
                result.failed.append(tr.themeName);
            }
        }
    }

    QDir(importTempRoot).removeRecursively();

    reload();

    result.imported.sort(Qt::CaseInsensitive);
    result.skipped.sort(Qt::CaseInsensitive);
    result.failed.sort(Qt::CaseInsensitive);

    return result;
}
