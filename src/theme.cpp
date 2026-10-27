#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

Theme::Theme() = default;

namespace {

QString decodeUtf16(const QByteArray &data, bool littleEndian)
{
    QString out;
    out.reserve(data.size() / 2);
    for (int i = 0; i + 1 < data.size(); i += 2) {
        const uchar lo = uchar(data.at(i));
        const uchar hi = uchar(data.at(i + 1));
        const ushort unit = littleEndian ? ushort(lo | (hi << 8)) : ushort((lo << 8) | hi);
        out.append(QChar(unit));
    }
    return out;
}

} // namespace

QColor Theme::parseColor(const QString &value, bool *ok)
{
    if (ok) {
        *ok = false;
    }

    const QString s = value.trimmed();
    if (s.isEmpty()) {
        return QColor();
    }

    QString hex = s;
    bool hexPrefixed = false;
    if (hex.startsWith(QLatin1Char('#'))) {
        hex = hex.mid(1);
        hexPrefixed = true;
    } else if (hex.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
        hex = hex.mid(2);
        hexPrefixed = true;
    }

    if (hexPrefixed || hex.length() == 6) {
        bool converted = false;
        const uint rgb = hex.toUInt(&converted, 16);
        if (converted && rgb <= 0xFFFFFFu) {
            if (ok) {
                *ok = true;
            }
            return QColor(int((rgb >> 16) & 0xFFu), int((rgb >> 8) & 0xFFu), int(rgb & 0xFFu));
        }
        return QColor();
    }

    bool converted = false;
    const int rgb = s.toInt(&converted);
    if (converted && rgb >= 0 && rgb <= 0xFFFFFF) {
        if (ok) {
            *ok = true;
        }
        return QColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }
    return QColor();
}

Theme Theme::parse(const QString &directory)
{
    Theme theme;
    theme.m_directory = QFileInfo(directory).absoluteFilePath();

    QFile file(theme.m_directory + QLatin1String("/theme.ini"));
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return theme;
    }
    const QByteArray data = file.readAll();
    file.close();

    QString text;
    if (data.size() >= 2 && uchar(data.at(0)) == 0xFE && uchar(data.at(1)) == 0xFF) {
        text = decodeUtf16(data.mid(2), false);
    } else if (data.size() >= 2 && uchar(data.at(0)) == 0xFF && uchar(data.at(1)) == 0xFE) {
        text = decodeUtf16(data.mid(2), true);
    } else {
        text = QString::fromUtf8(data);
    }
    if (!text.isEmpty() && text.at(0) == QChar(0xFEFF)) {
        text.remove(0, 1);
    }

    QString section;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('['))) {
            const qsizetype close = line.indexOf(QLatin1Char(']'));
            if (close > 0) {
                section = line.mid(1, close - 1).trimmed().toLower();
                continue;
            }
        }
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq < 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed().toLower();
        if (key.isEmpty()) {
            continue;
        }
        QString value = line.mid(eq + 1);
        const qsizetype semicolon = value.indexOf(QLatin1Char(';'));
        if (semicolon >= 0) {
            value = value.left(semicolon);
        }
        value = value.trimmed();

        if (section == QLatin1String("info") && key.startsWith(QLatin1String("parenttheme"))) {
            if (value.isEmpty()) {
                continue;
            }
            if (value.contains(QLatin1Char('/')) || value.contains(QLatin1Char('\\'))
                || value == QLatin1String(".") || value == QLatin1String("..")) {
                continue;
            }
            theme.m_parents.append(value);
            continue;
        }

        theme.m_raw.insert(section + QLatin1Char('/') + key, value);
    }

    theme.m_valid = true;
    theme.applyValues(QVector<const Theme *>{&theme});
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

QColor Theme::backgroundColor() const
{
    return m_backgroundColor;
}

void Theme::applyValues(const QVector<const Theme *> &chain)
{
    auto firstRaw = [&chain](const QString &section, const QString &key) -> QString {
        for (const Theme *theme : chain) {
            const QString value = theme->m_raw.value(section + QLatin1Char('/') + key);
            if (!value.isEmpty()) {
                return value;
            }
        }
        return QString();
    };
    auto firstRawAlias = [&chain](const QString &preferred, const QString &fallback) -> QString {
        for (const Theme *theme : chain) {
            QString value = theme->m_raw.value(QLatin1String("color/") + preferred);
            if (value.isEmpty()) {
                value = theme->m_raw.value(QLatin1String("color/") + fallback);
            }
            if (!value.isEmpty()) {
                return value;
            }
        }
        return QString();
    };
    auto applyColor = [](const QString &value, QColor &target) {
        if (value.isEmpty()) {
            return;
        }
        bool ok = false;
        const QColor parsed = parseColor(value, &ok);
        if (ok) {
            target = parsed;
        }
    };

    ThemeColors colors;
    applyColor(firstRaw("color", "labeltext"), colors.labelText);
    applyColor(firstRaw("color", "buttontext"), colors.buttonText);
    applyColor(firstRaw("color", "listtext1"), colors.listText1);
    applyColor(firstRaw("color", "listtext2"), colors.listText2);
    applyColor(firstRawAlias("listbk1", "listbg1"), colors.listBg1);
    applyColor(firstRawAlias("listbk2", "listbg2"), colors.listBg2);
    applyColor(firstRaw("color", "listline1"), colors.listLine1);
    applyColor(firstRaw("color", "listline2"), colors.listLine2);
    applyColor(firstRaw("color", "glass"), colors.glass);

    const QString alphaValue = firstRaw("alpha", "glassalpha");
    if (!alphaValue.isEmpty()) {
        bool ok = false;
        const int alpha = alphaValue.toInt(&ok);
        if (ok && alpha >= 0 && alpha <= 255) {
            colors.glassAlpha = alpha;
        }
    }

    const QString positionValue = firstRaw("character", "position");
    int characterPosition = 0;
    if (!positionValue.isEmpty()) {
        bool ok = false;
        const int position = positionValue.toInt(&ok);
        if (ok) {
            characterPosition = position;
        }
    }

    m_colors = colors;
    m_characterPosition = characterPosition;
    m_backgroundColor = QColor();
    applyColor(firstRaw("color", "background"), m_backgroundColor);
}

void Theme::collectChain(const QHash<QString, Theme> &available,
                         QVector<const Theme *> &chain,
                         QStringList &visited) const
{
    chain.append(this);
    visited.append(name());
    for (const QString &parentName : m_parents) {
        if (visited.contains(parentName)) {
            continue;
        }
        const auto it = available.constFind(parentName);
        if (it == available.constEnd()) {
            continue;
        }
        it.value().collectChain(available, chain, visited);
    }
}

void Theme::resolve(const QHash<QString, Theme> &available)
{
    QVector<const Theme *> chain;
    QStringList visited;
    collectChain(available, chain, visited);

    applyValues(chain);

    m_searchDirectories.clear();
    for (const Theme *theme : chain) {
        m_searchDirectories.append(theme->m_directory);
    }
}

QStringList Theme::searchDirectories() const
{
    return m_searchDirectories;
}

QString Theme::assetPath(const QString &baseName, int zoomPercent) const
{
    const int requested = qBound(100, zoomPercent, 300);
    QList<int> remaining;
    for (int suffix = 100; suffix <= 300; suffix += 100) {
        if (suffix != requested) {
            remaining.append(suffix);
        }
    }
    std::sort(remaining.begin(), remaining.end(), [requested](int a, int b) {
        const int distanceA = qAbs(a - requested);
        const int distanceB = qAbs(b - requested);
        if (distanceA != distanceB) {
            return distanceA < distanceB;
        }
        return a > b;
    });
    QList<int> suffixes;
    suffixes.append(requested);
    for (int suffix : remaining) {
        suffixes.append(suffix);
    }

    for (const QString &dir : m_searchDirectories) {
        for (int suffix : suffixes) {
            const QString candidate = dir + QLatin1Char('/') + baseName + QLatin1Char('-')
                + QString::number(suffix) + QLatin1String(".png");
            if (QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
    }
    return QString();
}

QString Theme::backgroundPath(int zoomPercent) const
{
    static const QRegularExpression re(QStringLiteral("^(.+Background)-\\d+\\.png$"));
    for (const QString &dir : m_searchDirectories) {
        const QStringList files = QDir(dir).entryList(
            QStringList{QLatin1String("*Background-*.png")},
            QDir::Files | QDir::NoDotAndDotDot,
            QDir::Name);
        for (const QString &fileName : files) {
            const QRegularExpressionMatch match = re.match(fileName);
            if (!match.hasMatch()) {
                continue;
            }
            const QString path = assetPath(match.captured(1), zoomPercent);
            if (!path.isEmpty()) {
                return path;
            }
        }
    }
    return QString();
}

bool Theme::hasOwnBackground() const
{
    const QStringList files = QDir(m_directory).entryList(
        QStringList{QLatin1String("*Background-*.png")},
        QDir::Files | QDir::NoDotAndDotDot);
    return !files.isEmpty();
}

QString buildThemeStyleSheet(const ThemeColors &colors)
{
    auto rgb = [](const QColor &color) {
        return color.name(QColor::HexRgb);
    };
    auto rgba = [](const QColor &color, int alpha) {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(qBound(0, alpha, 255));
    };

    const double luminance = 0.299 * colors.glass.red()
        + 0.587 * colors.glass.green()
        + 0.114 * colors.glass.blue();
    const QColor selectionText = (luminance > 128.0) ? QColor(Qt::black) : QColor(Qt::white);

    QString qss = QStringLiteral(
        "QWidget#centralwidget { color: @label@; }"
        "QLabel { color: @label@; }"
        "QLineEdit { background-color: @glass@; color: @label@; border: 1px solid @line@; border-radius: 3px; padding: 2px; }"
        "QPushButton { color: @label@; background-color: @glass@; border: 1px solid @line@; border-radius: 4px; }"
        "QPushButton:hover { background-color: @glassHover@; }"
        "QPushButton:pressed { background-color: @glassPressed@; }"
        "QPushButton:checked { background-color: @glassChecked@; border: 1px solid @text1@; }"
        "QHeaderView::section { background-color: @glass@; color: @text1@; border: 0; border-right: 1px solid @line@; border-bottom: 1px solid @line@; padding: 4px; }"
        "QTableWidget { background-color: @bg1@; alternate-background-color: @bg2@; gridline-color: @line@; }"
        "QTableWidget::item { color: @text1@; }"
        "QTableWidget::item:selected { color: @selection@; }"
        "QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }"
        "QTableCornerButton::section { background-color: @glass@; }");

    qss.replace(QLatin1String("@label@"), rgb(colors.labelText));
    qss.replace(QLatin1String("@glass@"), rgba(colors.glass, colors.glassAlpha));
    qss.replace(QLatin1String("@glassHover@"), rgba(colors.glass, colors.glassAlpha + 24));
    qss.replace(QLatin1String("@glassPressed@"), rgba(colors.glass, colors.glassAlpha - 24));
    qss.replace(QLatin1String("@glassChecked@"), rgba(colors.glass, colors.glassAlpha + 40));
    qss.replace(QLatin1String("@line@"), rgb(colors.listLine1));
    qss.replace(QLatin1String("@text1@"), rgb(colors.listText1));
    qss.replace(QLatin1String("@bg1@"), rgba(colors.listBg1, colors.glassAlpha));
    qss.replace(QLatin1String("@bg2@"), rgba(colors.listBg2, colors.glassAlpha));
    qss.replace(QLatin1String("@selection@"), rgb(selectionText));
    return qss;
}
