#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

struct ThemeColors {
    QColor labelText = QColor(Qt::black);
    QColor buttonText = QColor(Qt::black);
    QColor listText1 = QColor(Qt::black);
    QColor listText2 = QColor(Qt::black);
    QColor listBg1 = QColor(Qt::white);
    QColor listBg2 = QColor(0xF8, 0xF8, 0xF8);
    QColor listLine1 = QColor(Qt::black);
    QColor listLine2 = QColor(Qt::black);
    QColor glass = QColor(Qt::white);
    int glassAlpha = 128;
};

class Theme
{
public:
    Theme();

    static Theme parse(const QString &directory);
    static QColor parseColor(const QString &value, bool *ok = nullptr);

    bool isValid() const;
    QString name() const;
    QString directory() const;
    QStringList parentNames() const;
    int characterPosition() const;
    const ThemeColors &colors() const;
    QColor backgroundColor() const;

    void resolve(const QHash<QString, Theme> &available);
    QStringList searchDirectories() const;
    QString assetPath(const QString &baseName, int zoomPercent) const;
    QString backgroundPath(int zoomPercent) const;
    bool hasOwnBackground() const;

private:
    void applyValues(const QVector<const Theme *> &chain);
    void collectChain(const QHash<QString, Theme> &available,
                      QVector<const Theme *> &chain,
                      QStringList &visited) const;

    bool m_valid = false;
    QString m_directory;
    QStringList m_parents;
    int m_characterPosition = 0;
    ThemeColors m_colors;
    QColor m_backgroundColor;
    QHash<QString, QString> m_raw;
    QStringList m_searchDirectories;
};

QString buildThemeStyleSheet(const ThemeColors &colors);
