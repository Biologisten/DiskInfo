#include "thememanager.h"

#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

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
