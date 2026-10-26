#include "theme.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QStringList>
#include <QRegularExpression>

Theme::Theme() {}

static QColor parseColorValue(const QString &value, bool *ok = nullptr)
{
    QString s = value.trimmed();
    if (ok) *ok = false;
    if (s.isEmpty()) return QColor();
    bool isHex = false;
    if (s.startsWith("0x", Qt::CaseInsensitive) || s.startsWith("#")) {
        isHex = true;
        s = s.mid(1); // drop 0x or #
        if (s.startsWith("0x", Qt::CaseInsensitive)) s = s.mid(2);
    } else if (s.length() == 6) {
        bool allHex = true;
        for (int i = 0; i < s.length(); ++i) {
            QChar c = s.at(i);
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')))
                { allHex = false; break; }
        }
        if (allHex) isHex = true;
    }
    if (isHex) {
        bool converted = false;
        int rgb = s.toInt(&converted, 16);
        if (converted && rgb >= 0 && rgb <= 0xFFFFFF) {
            if (ok) *ok = true;
            return QColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }
    } else {
        bool converted = false;
        int rgb = s.toInt(&converted);
        if (converted && rgb >= 0 && rgb <= 0xFFFFFF) {
            if (ok) *ok = true;
            return QColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }
    }
    return QColor();
}

Theme Theme::parse(const QString &directory)
{
    Theme t;
    t.m_directory = QFileInfo(directory).absoluteFilePath();
    QString iniPath = t.m_directory + "/theme.ini";
    QFile f(iniPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        t.m_valid = false;
        return t;
    }
    QByteArray data = f.readAll();
    f.close();
    QString text;
    if (data.startsWith("\xFE\xFF")) {
        text = QString::fromUtf16((const char16_t*)(data.constData() + 2), (data.size() - 2) / 2);
    } else if (data.startsWith("\xFF\xFE")) {
        text = QString::fromUtf16((const char16_t*)(data.constData() + 2), (data.size() - 2) / 2);
    } else {
        text = QString::fromUtf8(data.constData(), data.size());
    }
    // strip leading BOM if still present
    if (!text.isEmpty() && text.at(0) == QChar(0xFEFF))
        text.remove(0, 1);

    QString currentSection = "";
    QStringList lines = text.split('\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) continue;
        // Ignore pure comment lines (start with ; or #), but not inline comments inside values
        if ((line.startsWith(';') || line.startsWith('#')) && line.indexOf('=') < 0) continue;
        // Section line with optional trailing comment after ]
        if (line.startsWith('[') && line.contains(']')) {
            int end = line.indexOf(']'); currentSection = line.mid(1, end - 1).trimmed().toLower();
            continue;
        }
        int eq = line.indexOf('='); if (eq < 0) continue; // skip lines without key=value
        QString key = line.left(eq).trimmed().toLower();
        QString value = line.mid(eq + 1).trimmed();
        // Strip trailing semicolon from value only (CDI writes 0x000000;)
        if (value.endsWith(';')) value = value.left(value.length() - 1);
        if (key.isEmpty()) continue;
        qDebug() << "DEBUG section=" << currentSection << " key=" << key << " value=" << value;
        if (currentSection == "info" && (key.startsWith("parenttheme") || key == "parenttheme")) {
            QString parent = value.trimmed();
            if (parent.isEmpty()) continue;
            if (parent.contains('/') || parent.contains('\\') || parent == "." || parent == "..") continue;
            t.m_parents.append(parent);
            continue;
        }
        QString fullKey = currentSection + "/" + key;
        t.m_raw.insert(fullKey, value);
    }

    t.m_valid = true;
    t.m_colors = ThemeColors(); // defaults
    // apply own raw values (no inheritance yet)
    auto get = [&](const QString &sec, const QString &k) -> QString {
        QString v = t.m_raw.value(sec.toLower() + "/" + k.toLower());
        if (v.isEmpty() && sec.toLower() == "color") v = t.m_raw.value(k.toLower());
        return v;
    };
    // list alias precedence
    auto setColor = [&](const QString &sec, const QString &k, QColor &target) {
        QString v = get(sec, k);
        if (!v.isEmpty()) {
            bool ok = false;
            QColor c = parseColorValue(v, &ok);
            if (ok) target = c;
        }
    };
    // LabelText
    setColor("color", "labeltext", t.m_colors.labelText);
    // ButtonText
    setColor("color", "buttontext", t.m_colors.buttonText);
    // ListText1/2
    setColor("color", "listtext1", t.m_colors.listText1);
    setColor("color", "listtext2", t.m_colors.listText2);
    // ListBg1/2 with Bk alias
    QString bg1 = get("color", "listbg1");
    QString bk1 = get("color", "listbk1");
    QString bgValue = bk1.isEmpty() ? bg1 : bk1;
    if (!bgValue.isEmpty()) { bool ok=false; QColor c=parseColorValue(bgValue,&ok); if(ok) t.m_colors.listBg1=c; }
    QString bg2 = get("color", "listbg2");
    QString bk2 = get("color", "listbk2");
    QString bgValue2 = bk2.isEmpty() ? bg2 : bk2;
    if (!bgValue2.isEmpty()) { bool ok=false; QColor c=parseColorValue(bgValue2,&ok); if(ok) t.m_colors.listBg2=c; }
    // ListLine1/2
    setColor("color", "listline1", t.m_colors.listLine1);
    setColor("color", "listline2", t.m_colors.listLine2);
    // Glass
    setColor("color", "glass", t.m_colors.glass);
    // GlassAlpha
    QString alphaStr = get("alpha", "glassalpha");
    if (!alphaStr.isEmpty()) {
        bool ok = false;
        int a = alphaStr.toInt(&ok);
        if (ok && a >= 0 && a <= 255) t.m_colors.glassAlpha = a;
    }
    // Character Position
    QString posStr = get("character", "position");
    if (!posStr.isEmpty()) {
        bool ok = false;
        int p = posStr.toInt(&ok);
        if (ok) t.m_characterPosition = p;
    }
    // Background colour
    QString bgColorStr = get("color", "background");
    if (!bgColorStr.isEmpty()) {
        bool ok = false;
        QColor c = parseColorValue(bgColorStr, &ok);
        if (ok) t.m_backgroundColor = c;
    }
    return t;
}

bool Theme::isValid() const { return m_valid; }
QString Theme::name() const { return QFileInfo(m_directory).fileName(); }
QString Theme::directory() const { return m_directory; }
QStringList Theme::parentNames() const { return m_parents; }
int Theme::characterPosition() const { return m_characterPosition; }
const ThemeColors &Theme::colors() const { return m_colors; }
QColor Theme::backgroundColor() const { return m_backgroundColor; }
