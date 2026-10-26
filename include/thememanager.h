#pragma once
#include <QStringList>
#include <QHash>
#include <QString>
#include "theme.h"
#include "ziparchive.h"
#include <QUuid>

struct ThemeZipInfo { QStringList names; QStringList existing; QString error; bool ok() const { return error.isEmpty(); } };
struct ThemeImportResult { QStringList imported; QStringList skipped; QStringList failed; QString error; bool ok() const { return error.isEmpty(); } };

class ThemeManager
{
public:
    explicit ThemeManager(QStringList themeRoots = defaultThemeRoots(),
                          QString writableRoot = defaultWritableRoot());
    void reload();
    const QHash<QString, Theme>& themes() const;
    QStringList themeNames() const;
    const Theme* theme(const QString &name) const;
    QString writableRoot() const;
    ThemeZipInfo inspectZip(const QString &zipPath) const;
    ThemeImportResult importZip(const QString &zipPath, bool overwriteExisting = false);
    static QStringList defaultThemeRoots();
    static QString defaultWritableRoot();

private:
    QStringList m_themeRoots;
    QString m_writableRoot;
    QHash<QString, Theme> m_themes;
};
