#include "theme.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QVector>

Theme::Theme() = default;

QColor Theme::parseColor(const QString &value, bool *ok)
{
    auto setOk = [&](bool success) {
        if (ok) {
            *ok = success;
        }
    };

    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        setOk(false);
        return {};
    }

    bool convOk = false;
    quint32 num = 0;

    if (trimmed.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
        num = trimmed.mid(2).toUInt(&convOk, 16);
    } else if (trimmed.startsWith(QLatin1Char('#'))) {
        num = trimmed.mid(1).toUInt(&convOk, 16);
    } else if (trimmed.length() == 6 && [&]() {
        for (const QChar &ch : trimmed) {
            const char latin = ch.toLatin1();
            if (!((latin >= '0' && latin <= '9') ||
                  (latin >= 'a' && latin <= 'f') ||
                  (latin >= 'A' && latin <= 'F'))) {
                return false;
            }
        }
        return true;
    }()) {
        num = trimmed.toUInt(&convOk, 16);
    } else {
        num = trimmed.toUInt(&convOk, 10);
    }

    if (!convOk || num > 0xFFFFFF) {
        setOk(false);
        return {};
    }

    setOk(true);
    const int r = static_cast<int>((num >> 16) & 0xFF);
    const int g = static_cast<int>((num >> 8) & 0xFF);
    const int b = static_cast<int>(num & 0xFF);
    return QColor(r, g, b);
}

Theme Theme::parse(const QString &directory)
{
    Theme theme;

    const QFileInfo dirInfo(QDir::cleanPath(directory));
    QFile iniFile(dirInfo.filePath() + QLatin1String("/theme.ini"));
    if (!iniFile.exists() || !iniFile.open(QIODevice::ReadOnly)) {
        return theme;
    }

    const QByteArray data = iniFile.readAll();
    iniFile.close();

    QString content;
    if (data.startsWith("\xFF\xFE")) {
        const qsizetype charCount = (data.size() - 2) / 2;
        content.resize(charCount);
        for (qsizetype i = 0; i < charCount; ++i) {
            const uchar b1 = static_cast<uchar>(data[2 + i * 2]);
            const uchar b2 = static_cast<uchar>(data[2 + i * 2 + 1]);
            content[i] = QChar(static_cast<ushort>(static_cast<ushort>(b1) | static_cast<ushort>(b2 << 8)));
        }
    } else if (data.startsWith("\xFE\xFF")) {
        const qsizetype charCount = (data.size() - 2) / 2;
        content.resize(charCount);
        for (qsizetype i = 0; i < charCount; ++i) {
            const uchar b1 = static_cast<uchar>(data[2 + i * 2]);
            const uchar b2 = static_cast<uchar>(data[2 + i * 2 + 1]);
            content[i] = QChar(static_cast<ushort>(static_cast<ushort>(b1 << 8) | static_cast<ushort>(b2)));
        }
    } else if (data.startsWith("\xEF\xBB\xBF")) {
        content = QString::fromUtf8(data.constData() + 3, data.size() - 3);
    } else {
        content = QString::fromUtf8(data);
    }

    if (!content.isEmpty() && content.at(0) == QChar(0xFEFF)) {
        content.remove(0, 1);
    }

    QString currentSection;
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (line.startsWith(QLatin1Char('['))) {
            const qsizetype closeIdx = line.indexOf(QLatin1Char(']'));
            if (closeIdx > 1) {
                currentSection = line.mid(1, closeIdx - 1).trimmed().toLower();
                continue;
            }
        }

        const qsizetype eqIdx = line.indexOf(QLatin1Char('='));
        if (eqIdx == -1) {
            continue;
        }

        const QString rawKey = line.left(eqIdx).trimmed().toLower();
        QString rawVal = line.mid(eqIdx + 1);
        const qsizetype semiIdx = rawVal.indexOf(QLatin1Char(';'));
        if (semiIdx != -1) {
            rawVal = rawVal.left(semiIdx);
        }
        const QString val = rawVal.trimmed();

        if (currentSection == QLatin1String("info") && rawKey.startsWith(QLatin1String("parenttheme"))) {
            if (!val.isEmpty() &&
                !val.contains(QLatin1Char('/')) &&
                !val.contains(QLatin1Char('\\')) &&
                val != QLatin1String(".") &&
                val != QLatin1String("..")) {
                theme.m_parents.append(val);
            }
        }

        const QString lookupKey = currentSection + QLatin1Char('/') + rawKey;
        theme.m_values.insert(lookupKey, val);
    }

    theme.m_directory = dirInfo.absoluteFilePath();
    theme.m_valid = true;
    theme.m_searchDirectories = QStringList({theme.m_directory});

    if (theme.m_values.contains(QLatin1String("character/position"))) {
        bool posOk = false;
        const int pos = theme.m_values.value(QLatin1String("character/position")).toInt(&posOk);
        if (posOk) {
            theme.m_characterPosition = pos;
        }
    }

    if (theme.m_values.contains(QLatin1String("alpha/glassalpha"))) {
        bool alphaOk = false;
        const int alpha = theme.m_values.value(QLatin1String("alpha/glassalpha")).toInt(&alphaOk);
        if (alphaOk) {
            theme.m_colors.glassAlpha = alpha;
        }
    }

    auto readColor = [&](const QString &key, const QColor &fallback) -> QColor {
        if (!theme.m_values.contains(key)) {
            return fallback;
        }
        bool colorOk = false;
        const QColor col = parseColor(theme.m_values.value(key), &colorOk);
        return colorOk ? col : fallback;
    };

    theme.m_colors.labelText = readColor(QLatin1String("color/labeltext"), theme.m_colors.labelText);
    theme.m_colors.buttonText = readColor(QLatin1String("color/buttontext"), theme.m_colors.buttonText);
    theme.m_colors.listText1 = readColor(QLatin1String("color/listtext1"), theme.m_colors.listText1);
    theme.m_colors.listText2 = readColor(QLatin1String("color/listtext2"), theme.m_colors.listText2);
    theme.m_colors.listLine1 = readColor(QLatin1String("color/listline1"), theme.m_colors.listLine1);
    theme.m_colors.listLine2 = readColor(QLatin1String("color/listline2"), theme.m_colors.listLine2);
    theme.m_colors.glass = readColor(QLatin1String("color/glass"), theme.m_colors.glass);

    if (theme.m_values.contains(QLatin1String("color/listbk1"))) {
        bool bk1Ok = false;
        const QColor col = parseColor(theme.m_values.value(QLatin1String("color/listbk1")), &bk1Ok);
        if (bk1Ok) {
            theme.m_colors.listBg1 = col;
        } else {
            theme.m_colors.listBg1 = readColor(QLatin1String("color/listbg1"), theme.m_colors.listBg1);
        }
    } else {
        theme.m_colors.listBg1 = readColor(QLatin1String("color/listbg1"), theme.m_colors.listBg1);
    }

    if (theme.m_values.contains(QLatin1String("color/listbk2"))) {
        bool bk2Ok = false;
        const QColor col = parseColor(theme.m_values.value(QLatin1String("color/listbk2")), &bk2Ok);
        if (bk2Ok) {
            theme.m_colors.listBg2 = col;
        } else {
            theme.m_colors.listBg2 = readColor(QLatin1String("color/listbg2"), theme.m_colors.listBg2);
        }
    } else {
        theme.m_colors.listBg2 = readColor(QLatin1String("color/listbg2"), theme.m_colors.listBg2);
    }

    if (theme.m_values.contains(QLatin1String("color/background"))) {
        bool bgOk = false;
        const QColor col = parseColor(theme.m_values.value(QLatin1String("color/background")), &bgOk);
        if (bgOk) {
            theme.m_backgroundColor = col;
        }
    }

    return theme;
}

bool Theme::isValid() const
{
    return m_valid;
}

QString Theme::name() const
{
    return QFileInfo(m_directory).fileName();
}

QString Theme::directory() const
{
    return m_directory;
}

QStringList Theme::parentNames() const
{
    return m_parents;
}

int Theme::characterPosition() const
{
    return m_characterPosition;
}

const ThemeColors &Theme::colors() const
{
    return m_colors;
}

bool Theme::isResolved() const
{
    return m_resolved;
}

QStringList Theme::searchDirectories() const
{
    return m_searchDirectories;
}

void Theme::resolve(const QHash<QString, Theme> &available)
{
    m_searchDirectories.clear();
    QSet<QString> visitedNames;
    QVector<const Theme *> chain;

    visitedNames.insert(name());
    m_searchDirectories.append(m_directory);
    chain.append(this);

    std::function<void(const QStringList &)> visitParents = [&](const QStringList &parentList) {
        for (const QString &pName : parentList) {
            if (visitedNames.contains(pName)) {
                continue;
            }
            const auto it = available.constFind(pName);
            if (it == available.constEnd()) {
                continue;
            }
            visitedNames.insert(pName);
            const Theme *parentTheme = &it.value();
            m_searchDirectories.append(parentTheme->m_directory);
            chain.append(parentTheme);
            visitParents(parentTheme->m_parents);
        }
    };

    visitParents(m_parents);

    auto findColor = [&](const QString &key, const QColor &fallback) -> QColor {
        for (const Theme *t : chain) {
            if (t->m_values.contains(key)) {
                bool colorOk = false;
                const QColor c = parseColor(t->m_values.value(key), &colorOk);
                if (colorOk) {
                    return c;
                }
            }
        }
        return fallback;
    };

    auto findListBg = [&](const QString &bkKey, const QString &bgKey, const QColor &fallback) -> QColor {
        for (const Theme *t : chain) {
            if (t->m_values.contains(bkKey)) {
                bool bkOk = false;
                const QColor c = parseColor(t->m_values.value(bkKey), &bkOk);
                if (bkOk) {
                    return c;
                }
            }
            if (t->m_values.contains(bgKey)) {
                bool bgOk = false;
                const QColor c = parseColor(t->m_values.value(bgKey), &bgOk);
                if (bgOk) {
                    return c;
                }
            }
        }
        return fallback;
    };

    m_colors.labelText = findColor(QLatin1String("color/labeltext"), QColor(Qt::black));
    m_colors.buttonText = findColor(QLatin1String("color/buttontext"), QColor(Qt::black));
    m_colors.listText1 = findColor(QLatin1String("color/listtext1"), QColor(Qt::black));
    m_colors.listText2 = findColor(QLatin1String("color/listtext2"), QColor(Qt::black));
    m_colors.listLine1 = findColor(QLatin1String("color/listline1"), QColor(Qt::black));
    m_colors.listLine2 = findColor(QLatin1String("color/listline2"), QColor(Qt::black));
    m_colors.glass = findColor(QLatin1String("color/glass"), QColor(Qt::white));

    m_colors.listBg1 = findListBg(QLatin1String("color/listbk1"), QLatin1String("color/listbg1"), QColor(Qt::white));
    m_colors.listBg2 = findListBg(QLatin1String("color/listbk2"), QLatin1String("color/listbg2"), QColor(0xF8, 0xF8, 0xF8));

    m_colors.glassAlpha = 128;
    for (const Theme *t : chain) {
        if (t->m_values.contains(QLatin1String("alpha/glassalpha"))) {
            bool alphaOk = false;
            const int a = t->m_values.value(QLatin1String("alpha/glassalpha")).toInt(&alphaOk);
            if (alphaOk) {
                m_colors.glassAlpha = a;
                break;
            }
        }
    }

    m_characterPosition = 0;
    for (const Theme *t : chain) {
        if (t->m_values.contains(QLatin1String("character/position"))) {
            bool posOk = false;
            const int pos = t->m_values.value(QLatin1String("character/position")).toInt(&posOk);
            if (posOk) {
                m_characterPosition = pos;
                break;
            }
        }
    }

    m_backgroundColor = QColor();
    for (const Theme *t : chain) {
        if (t->m_values.contains(QLatin1String("color/background"))) {
            bool bgOk = false;
            const QColor c = parseColor(t->m_values.value(QLatin1String("color/background")), &bgOk);
            if (bgOk) {
                m_backgroundColor = c;
                break;
            }
        }
    }

    m_resolved = true;
}

QString Theme::assetPath(const QString &baseName, int zoomPercent) const
{
    const QString prefix = baseName + QLatin1Char('-');
    const QString ext = QLatin1String(".png");

    struct Candidate {
        int suffix;
        QString filePath;
    };

    for (const QString &dirPath : m_searchDirectories) {
        const QDir dir(dirPath);
        const QFileInfoList files = dir.entryInfoList(
            QStringList({prefix + QLatin1Char('*') + ext}),
            QDir::Files);

        QVector<Candidate> candidates;
        for (const QFileInfo &fi : files) {
            const QString fn = fi.fileName();
            if (fn.startsWith(prefix, Qt::CaseInsensitive) && fn.endsWith(ext, Qt::CaseInsensitive)) {
                const QString numStr = fn.mid(prefix.length(), fn.length() - prefix.length() - ext.length());
                bool ok = false;
                const int s = numStr.toInt(&ok);
                if (ok && s > 0) {
                    candidates.append({s, fi.filePath()});
                }
            }
        }

        if (!candidates.isEmpty()) {
            std::sort(candidates.begin(), candidates.end(), [zoomPercent](const Candidate &a, const Candidate &b) {
                const int distA = std::abs(a.suffix - zoomPercent);
                const int distB = std::abs(b.suffix - zoomPercent);
                if (distA != distB) {
                    return distA < distB;
                }
                return a.suffix > b.suffix;
            });
            return candidates.first().filePath;
        }
    }

    return {};
}

QString Theme::backgroundPath(int zoomPercent) const
{
    const QRegularExpression regex(QStringLiteral("^(.+Background)-\\d+\\.png$"), QRegularExpression::CaseInsensitiveOption);
    for (const QString &dirPath : m_searchDirectories) {
        const QFileInfoList files = QDir(dirPath).entryInfoList(QDir::Files);
        for (const QFileInfo &fi : files) {
            const QRegularExpressionMatch match = regex.match(fi.fileName());
            if (match.hasMatch()) {
                const QString baseName = match.captured(1);
                return assetPath(baseName, zoomPercent);
            }
        }
    }
    return {};
}

bool Theme::hasOwnBackground() const
{
    const QRegularExpression regex(QStringLiteral("^.+Background-\\d+\\.png$"), QRegularExpression::CaseInsensitiveOption);
    const QFileInfoList files = QDir(m_directory).entryInfoList(QDir::Files);
    for (const QFileInfo &fi : files) {
        if (regex.match(fi.fileName()).hasMatch()) {
            return true;
        }
    }
    return false;
}

QColor Theme::backgroundColor() const
{
    return m_backgroundColor;
}
