#include "thememanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace {

constexpr qsizetype kMaxArchiveEntries = 5000;
constexpr qint64 kMaxTotalUncompressed = 256LL * 1024 * 1024;
constexpr qint64 kMaxFileUncompressed = 64LL * 1024 * 1024;

bool isValidThemeName(const QString &name)
{
    return !name.isEmpty()
        && name != QLatin1String(".")
        && name != QLatin1String("..")
        && !name.contains(QLatin1Char('/'))
        && !name.contains(QLatin1Char('\\'))
        && !name.contains(QChar(0));
}

bool isAllowedThemeAsset(const QString &relativePath)
{
    const QString suffix = QFileInfo(relativePath).suffix().toLower();
    return suffix == QLatin1String("ini")
        || suffix == QLatin1String("png")
        || suffix == QLatin1String("jpg")
        || suffix == QLatin1String("jpeg")
        || suffix == QLatin1String("bmp");
}

QString rootDirectoryOfEntry(const QString &entryName)
{
    QString rootDir = QFileInfo(entryName).path();
    if (rootDir == QLatin1String(".")) {
        rootDir.clear();
    }
    return rootDir;
}

QString themeNameForRoot(const QString &rootDir, const QString &zipPath)
{
    if (rootDir.isEmpty()) {
        return QFileInfo(zipPath).completeBaseName();
    }
    return QFileInfo(rootDir).fileName();
}

QHash<QString, QString> themeRootsFromEntries(const QVector<ZipEntry> &entries, const QString &zipPath)
{
    QHash<QString, QString> roots;
    for (const ZipEntry &entry : entries) {
        if (entry.isDirectory) {
            continue;
        }
        if (QFileInfo(entry.name).fileName().compare(QLatin1String("theme.ini"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        const QString rootDir = rootDirectoryOfEntry(entry.name);
        const QString name = themeNameForRoot(rootDir, zipPath);
        if (!isValidThemeName(name)) {
            continue;
        }
        if (!roots.contains(name)) {
            roots.insert(name, rootDir);
        }
    }
    return roots;
}

void sortThemeNames(QStringList &names)
{
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        const int insensitive = QString::compare(a, b, Qt::CaseInsensitive);
        if (insensitive != 0) {
            return insensitive < 0;
        }
        return QString::compare(a, b, Qt::CaseSensitive) < 0;
    });
}

} // namespace

ThemeManager::ThemeManager(QStringList themeRoots, QString writableRoot)
    : m_themeRoots(std::move(themeRoots))
    , m_writableRoot(std::move(writableRoot))
{
}

QStringList ThemeManager::defaultThemeRoots()
{
    QStringList roots;
    const QStringList bases = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &base : bases) {
        const QString root = QFileInfo(base + QLatin1String("/qdiskinfo/themes")).absoluteFilePath();
        if (!roots.contains(root)) {
            roots.append(root);
        }
    }
    return roots;
}

QString ThemeManager::defaultWritableRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QLatin1String("/qdiskinfo/themes");
}

void ThemeManager::reload()
{
    m_themes.clear();
    for (const QString &root : std::as_const(m_themeRoots)) {
        QDir dir(root);
        if (!dir.exists()) {
            continue;
        }
        const QStringList entries = dir.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        for (const QString &entry : entries) {
            const QString path = dir.absoluteFilePath(entry);
            if (!QFileInfo::exists(path + QLatin1String("/theme.ini"))) {
                continue;
            }
            Theme theme = Theme::parse(path);
            if (!theme.isValid()) {
                continue;
            }
            const QString name = theme.name();
            if (m_themes.contains(name)) {
                continue;
            }
            m_themes.insert(name, theme);
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
    sortThemeNames(names);
    return names;
}

const Theme *ThemeManager::theme(const QString &name) const
{
    const auto it = m_themes.constFind(name);
    if (it == m_themes.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

QString ThemeManager::writableRoot() const
{
    return m_writableRoot;
}

ThemeZipInfo ThemeManager::inspectZip(const QString &zipPath) const
{
    ThemeZipInfo info;
    ZipArchive zip;
    QString error;
    if (!zip.open(zipPath, &error)) {
        info.error = error;
        return info;
    }

    const QVector<ZipEntry> entries = zip.entries();
    if (entries.size() > kMaxArchiveEntries) {
        info.error = QStringLiteral("Archive contains too many entries.");
        return info;
    }

    const QHash<QString, QString> roots = themeRootsFromEntries(entries, zipPath);
    info.names = roots.keys();
    sortThemeNames(info.names);

    for (const QString &name : std::as_const(info.names)) {
        if (QFileInfo::exists(m_writableRoot + QLatin1Char('/') + name)) {
            info.existing.append(name);
        }
    }

    if (info.names.isEmpty()) {
        info.error = QStringLiteral("No CDI themes found in archive.");
    }
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
    QString error;
    if (!zip.open(zipPath, &error)) {
        result.error = error;
        return result;
    }
    const QVector<ZipEntry> entries = zip.entries();
    if (entries.size() > kMaxArchiveEntries) {
        result.error = QStringLiteral("Archive contains too many entries.");
        return result;
    }

    const QHash<QString, QString> roots = themeRootsFromEntries(entries, zipPath);

    struct ThemeExtraction {
        QString name;
        QString rootDir;
        QStringList files;
        bool skip = false;
    };
    QVector<ThemeExtraction> extractions;
    for (const QString &name : info.names) {
        ThemeExtraction extraction;
        extraction.name = name;
        extraction.rootDir = roots.value(name);
        if (QFileInfo::exists(m_writableRoot + QLatin1Char('/') + name) && !overwriteExisting) {
            extraction.skip = true;
        }
        extractions.append(extraction);
    }

    // Pass 1: validate every selected entry before anything is written.
    qint64 totalUncompressed = 0;
    for (const ZipEntry &entry : entries) {
        if (entry.isDirectory) {
            continue;
        }
        bool selected = false;
        for (const ThemeExtraction &extraction : extractions) {
            if (extraction.skip) {
                continue;
            }
            if (extraction.rootDir.isEmpty()
                || entry.name.startsWith(extraction.rootDir + QLatin1Char('/'))) {
                selected = true;
                break;
            }
        }
        if (!selected) {
            continue;
        }
        if (!ZipArchive::isSafePath(entry.name)) {
            result.error = QStringLiteral("Archive contains an unsafe path: %1").arg(entry.name);
            return result;
        }
        if (entry.isSymlink) {
            result.error = QStringLiteral("Archive contains a symbolic link entry: %1").arg(entry.name);
            return result;
        }
        if (entry.uncompressedSize > quint64(kMaxFileUncompressed)) {
            result.error = QStringLiteral("Archive entry is too large: %1").arg(entry.name);
            return result;
        }
        totalUncompressed += qint64(entry.uncompressedSize);
        if (totalUncompressed > kMaxTotalUncompressed) {
            result.error = QStringLiteral("Archive contains too much uncompressed data.");
            return result;
        }
    }

    // Pass 2: extract every theme into a temporary directory.
    const QString tempRoot = m_writableRoot + QLatin1String("/.import-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkpath(tempRoot)) {
        result.error = QStringLiteral("Could not create the temporary theme directory.");
        return result;
    }

    QVector<ThemeExtraction> pending;
    for (ThemeExtraction &extraction : extractions) {
        if (extraction.skip) {
            continue;
        }
        const QString tempThemeDir = tempRoot + QLatin1Char('/') + extraction.name;
        if (!QDir().mkpath(tempThemeDir)) {
            QDir(tempRoot).removeRecursively();
            result.error = QStringLiteral("Could not create the temporary theme directory.");
            return result;
        }
        const QString prefix = extraction.rootDir.isEmpty()
            ? QString()
            : extraction.rootDir + QLatin1Char('/');
        bool hasThemeIni = false;
        for (const ZipEntry &entry : entries) {
            if (entry.isDirectory) {
                continue;
            }
            QString relative = entry.name;
            if (!prefix.isEmpty()) {
                if (!entry.name.startsWith(prefix)) {
                    continue;
                }
                relative = entry.name.mid(prefix.length());
            }
            if (relative.isEmpty()) {
                continue;
            }
            if (relative.compare(QLatin1String("theme.ini"), Qt::CaseInsensitive) == 0) {
                hasThemeIni = true;
            }
            if (!isAllowedThemeAsset(relative)) {
                continue;
            }
            QByteArray data;
            QString readError;
            if (!zip.read(entry, &data, &readError)) {
                QDir(tempRoot).removeRecursively();
                result.error = QStringLiteral("Could not read %1: %2").arg(entry.name, readError);
                return result;
            }
            const QString target = tempThemeDir + QLatin1Char('/') + relative;
            const QFileInfo targetInfo(target);
            if (!QDir().mkpath(targetInfo.absolutePath())) {
                QDir(tempRoot).removeRecursively();
                result.error = QStringLiteral("Could not create theme files.");
                return result;
            }
            QFile out(target);
            if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()) {
                QDir(tempRoot).removeRecursively();
                result.error = QStringLiteral("Could not write theme file %1.").arg(target);
                return result;
            }
            out.close();
            extraction.files.append(relative);
        }
        if (!hasThemeIni) {
            result.failed.append(extraction.name);
            extraction.files.clear();
        }
        pending.append(extraction);
    }

    for (const ThemeExtraction &extraction : extractions) {
        if (extraction.skip) {
            result.skipped.append(extraction.name);
        }
    }

    // Pass 3: rename each extracted theme into place.
    for (ThemeExtraction &extraction : pending) {
        if (extraction.files.isEmpty()) {
            continue;
        }
        const QString target = m_writableRoot + QLatin1Char('/') + extraction.name;
        QString backup;
        if (QFileInfo::exists(target)) {
            backup = target + QLatin1String(".old-")
                + QUuid::createUuid().toString(QUuid::WithoutBraces);
            if (!QDir().rename(target, backup)) {
                result.failed.append(extraction.name);
                continue;
            }
        }
        if (!QDir().rename(tempRoot + QLatin1Char('/') + extraction.name, target)) {
            if (!backup.isEmpty()) {
                QDir().rename(backup, target);
            }
            result.failed.append(extraction.name);
            continue;
        }
        if (!backup.isEmpty()) {
            QDir(backup).removeRecursively();
        }
        result.imported.append(extraction.name);
    }

    QDir(tempRoot).removeRecursively();
    reload();
    return result;
}
