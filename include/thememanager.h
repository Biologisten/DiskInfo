#pragma once
#include <QStringList>
#include <QHash>
#include "theme.h"

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
    static QStringList defaultThemeRoots();
    static QString defaultWritableRoot();

private:
    QStringList m_themeRoots;
    QString m_writableRoot;
    QHash<QString, Theme> m_themes;
};
