#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>

struct ThemeColors
{
    QColor labelText{Qt::black};
    QColor buttonText{Qt::black};
    QColor listText1{Qt::black};
    QColor listText2{Qt::black};
    QColor listBg1{Qt::white};
    QColor listBg2{0xF8, 0xF8, 0xF8};
    QColor listLine1{Qt::black};
    QColor listLine2{Qt::black};
    QColor glass{Qt::white};
    int glassAlpha = 128;
};

class Theme
{
public:
    Theme();

    static Theme parse(const QString &directory);
    static QColor parseColor(const QString &value, bool *ok = nullptr);

    bool isValid() const;
    bool isResolved() const;
    QString name() const;
    QString directory() const;
    QStringList parentNames() const;
    QStringList searchDirectories() const;
    int characterPosition() const;
    const ThemeColors &colors() const;
    QColor backgroundColor() const;

    void resolve(const QHash<QString, Theme> &available);
    QString assetPath(const QString &baseName, int zoomPercent) const;
    QString backgroundPath(int zoomPercent) const;
    bool hasOwnBackground() const;

private:
    bool m_valid = false;
    bool m_resolved = false;
    QString m_directory;
    QStringList m_parents;
    int m_characterPosition = 0;
    QColor m_backgroundColor;
    ThemeColors m_colors;
    QHash<QString, QString> m_values;
    QStringList m_searchDirectories;
};
