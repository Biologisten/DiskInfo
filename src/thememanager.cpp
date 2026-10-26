#include "thememanager.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

ThemeManager::ThemeManager(QStringList themeRoots, QString writableRoot)
    : m_themeRoots(themeRoots), m_writableRoot(writableRoot)
{
    if (m_themeRoots.isEmpty()) m_themeRoots = defaultThemeRoots();
    if (m_writableRoot.isEmpty()) m_writableRoot = defaultWritableRoot();
}

QStringList ThemeManager::defaultThemeRoots()
{
    QStringList roots;
    for (const QString &base : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        roots.append(base + "/qdiskinfo/themes");
    }
    roots.removeDuplicates();
    QStringList cleaned;
    for (const QString &r : roots) cleaned.append(QFileInfo(r).absoluteFilePath());
    cleaned.removeDuplicates();
    return cleaned;
}

QString ThemeManager::defaultWritableRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/qdiskinfo/themes";
}

void ThemeManager::reload()
{
    m_themes.clear();
    for (const QString &root : m_themeRoots) {
        QDir dir(root);
        if (!dir.exists()) continue;
        QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        // case-insensitive sort with case-sensitive tiebreak (approximate)
        entries.sort();
        for (const QString &entry : entries) {
            QString path = dir.absoluteFilePath(entry);
            QString ini = path + "/theme.ini";
            if (!QFileInfo(ini).exists()) continue;
            Theme t = Theme::parse(path);
            if (!t.isValid()) continue;
            QString name = t.name();
            if (m_themes.contains(name)) continue; // user shadows system
            m_themes.insert(name, t);
        }
    }
    for (Theme &t : m_themes) {
        t.resolve(const_cast<const QHash<QString, Theme>&>(m_themes));
    }
}

const QHash<QString, Theme>& ThemeManager::themes() const { return m_themes; }
QStringList ThemeManager::themeNames() const
{
    QStringList names = m_themes.keys();
    names.sort(Qt::CaseInsensitive);
    return names;
}
const Theme* ThemeManager::theme(const QString &name) const { auto it = m_themes.constFind(name); return (it == m_themes.constEnd()) ? nullptr : &(*it); }
QString ThemeManager::writableRoot() const { return m_writableRoot; }
