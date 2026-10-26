#include "theme.h"
#include "thememanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

namespace {

void writeFile(const QString &path, const QByteArray &data)
{
    const QFileInfo info(path);
    QDir().mkpath(info.absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(data);
    file.close();
}

[[maybe_unused]] QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    return file.readAll();
}

} // namespace

class ThemeTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesCdiThemeIni()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString dir = tmp.filePath("Aoi");
        writeFile(dir + "/theme.ini",
            "[Info]\n"
            "Author=test\n"
            "ParentTheme1=AoiNight\n"
            "ParentTheme1=Aoi\n"
            "[Color];RGB\n"
            "LabelText=0x112233;\n"
            "ButtonText=#445566;\n"
            "ListText1=0xAABBCC;\n"
            "ListText2=0xDDEEFF;\n"
            "ListBg1=0xFFFFFF;\n"
            "ListBg2=0xF8F8F8;\n"
            "ListLine1=0xE0E0E0;\n"
            "ListLine2=0xE8E8E8;\n"
            "Glass=0x102030;\n"
            "[Alpha]\n"
            "GlassAlpha=160;\n"
            "[Character]\n"
            "Position=1;\n");

        const Theme theme = Theme::parse(dir);
        QVERIFY(theme.isValid());
        QCOMPARE(theme.name(), QString("Aoi"));
        QCOMPARE(theme.parentNames(), QStringList({"AoiNight", "Aoi"}));
        QCOMPARE(theme.characterPosition(), 1);
        QCOMPARE(theme.colors().labelText, QColor(0x11, 0x22, 0x33));
        QCOMPARE(theme.colors().buttonText, QColor(0x44, 0x55, 0x66));
        QCOMPARE(theme.colors().glass, QColor(0x10, 0x20, 0x30));
        QCOMPARE(theme.colors().glassAlpha, 160);
    }

    void listBkWinsOverListBg()
    {
        QTemporaryDir tmp;
        const QString dir = tmp.filePath("Alias");
        writeFile(dir + "/theme.ini",
            "[Color]\nListBg1=0x111111;\nListBk1=0x222222;\nListBg2=0x333333;\nListBk2=0x444444;\n");

        const Theme theme = Theme::parse(dir);
        QCOMPARE(theme.colors().listBg1, QColor(0x22, 0x22, 0x22));
        QCOMPARE(theme.colors().listBg2, QColor(0x44, 0x44, 0x44));
    }

    void appliesCdiDefaults()
    {
        QTemporaryDir tmp;
        const QString dir = tmp.filePath("Defaults");
        writeFile(dir + "/theme.ini", "[Info]\nAuthor=test\n");

        const Theme theme = Theme::parse(dir);
        QCOMPARE(theme.colors().labelText, QColor(Qt::black));
        QCOMPARE(theme.colors().listBg1, QColor(Qt::white));
        QCOMPARE(theme.colors().listBg2, QColor(0xF8, 0xF8, 0xF8));
        QCOMPARE(theme.colors().glass, QColor(Qt::white));
        QCOMPARE(theme.colors().glassAlpha, 128);
        QCOMPARE(theme.characterPosition(), 0);
    }

    void rejectsMissingThemeIni()
    {
        QTemporaryDir tmp;
        QVERIFY(!Theme::parse(tmp.filePath("Missing")).isValid());
    }

    void decodesUtf16Bom()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString dir = tmp.filePath("Utf16");
        QByteArray data("\xFF\xFE", 2);
        const QString text = "[Info]\nParentTheme1=Utf16Parent\n[Color]\nLabelText=#010203;\n";
        for (const QChar ch : text) {
            const ushort code = ch.unicode();
            data.append(static_cast<char>(code & 0xFF));
            data.append(static_cast<char>((code >> 8) & 0xFF));
        }
        writeFile(dir + "/theme.ini", data);

        const Theme theme = Theme::parse(dir);
        QVERIFY(theme.isValid());
        QCOMPARE(theme.parentNames(), QStringList({"Utf16Parent"}));
        QCOMPARE(theme.colors().labelText, QColor(0x01, 0x02, 0x03));
    }

    void parsesColourFormats()
    {
        QVERIFY(!Theme::parseColor("").isValid());
        QVERIFY(!Theme::parseColor("not-a-colour").isValid());
        QVERIFY(!Theme::parseColor("0x1000000").isValid());
        QCOMPARE(Theme::parseColor("0x112233"), QColor(0x11, 0x22, 0x33));
        QCOMPARE(Theme::parseColor(" #445566 "), QColor(0x44, 0x55, 0x66));
        QCOMPARE(Theme::parseColor("AABBCC"), QColor(0xAA, 0xBB, 0xCC));
        QCOMPARE(Theme::parseColor("1193046"), QColor(0x12, 0x34, 0x56));
    }

    void inheritsMissingColoursParentFirst()
    {
        QTemporaryDir tmp;
        const QString selfDir = tmp.filePath("Child");
        const QString parentDir = tmp.filePath("Parent");
        const QString grandDir = tmp.filePath("Grand");
        writeFile(selfDir + "/theme.ini", "[Info]\nParentTheme1=Parent\nParentTheme2=Grand\n[Color]\nLabelText=0x010203;\n");
        writeFile(parentDir + "/theme.ini", "[Color]\nGlass=0x111111;\nLabelText=0xAAAAAA;\n");
        writeFile(grandDir + "/theme.ini", "[Color]\nGlass=0x222222;\nListBg1=0x333333;\n");

        QHash<QString, Theme> themes{
            {"Child", Theme::parse(selfDir)},
            {"Parent", Theme::parse(parentDir)},
            {"Grand", Theme::parse(grandDir)},
        };
        themes["Child"].resolve(themes);

        QCOMPARE(themes["Child"].colors().labelText, QColor(0x01, 0x02, 0x03));
        QCOMPARE(themes["Child"].colors().glass, QColor(0x11, 0x11, 0x11));
        QCOMPARE(themes["Child"].colors().listBg1, QColor(0x33, 0x33, 0x33));
        QCOMPARE(themes["Child"].searchDirectories().size(), 3);
    }

    void breaksParentCycles()
    {
        QTemporaryDir tmp;
        const QString aDir = tmp.filePath("A");
        const QString bDir = tmp.filePath("B");
        writeFile(aDir + "/theme.ini", "[Info]\nParentTheme1=B\n[Color]\nLabelText=0x010101;\n");
        writeFile(bDir + "/theme.ini", "[Info]\nParentTheme1=A\n[Color]\nGlass=0x020202;\n");

        QHash<QString, Theme> themes{{"A", Theme::parse(aDir)}, {"B", Theme::parse(bDir)}};
        themes["A"].resolve(themes);
        QCOMPARE(themes["A"].colors().labelText, QColor(0x01, 0x01, 0x01));
        QCOMPARE(themes["A"].colors().glass, QColor(0x02, 0x02, 0x02));
        QCOMPARE(themes["A"].searchDirectories(), QStringList({aDir, bDir}));
    }

    void picksNearestDpiAsset()
    {
        QTemporaryDir tmp;
        const QString dir = tmp.filePath("Aoi");
        writeFile(dir + "/theme.ini", "[Info]\nAuthor=test\n");
        writeFile(dir + "/diskGood-100.png", "");
        writeFile(dir + "/diskGood-200.png", "");
        writeFile(dir + "/diskGood-300.png", "");

        QHash<QString, Theme> themes{{"Aoi", Theme::parse(dir)}};
        themes["Aoi"].resolve(themes);
        QCOMPARE(themes["Aoi"].assetPath("diskGood", 150), dir + "/diskGood-200.png");
        QCOMPARE(themes["Aoi"].assetPath("diskGood", 400), dir + "/diskGood-300.png");
        QCOMPARE(themes["Aoi"].assetPath("diskGood", 100), dir + "/diskGood-100.png");
        QVERIFY(themes["Aoi"].assetPath("missing", 100).isEmpty());
    }

    void childAssetWinsOverParentAsset()
    {
        QTemporaryDir tmp;
        const QString childDir = tmp.filePath("Child");
        const QString parentDir = tmp.filePath("Parent");
        writeFile(childDir + "/theme.ini", "[Info]\nParentTheme1=Parent\n");
        writeFile(parentDir + "/theme.ini", "[Info]\nAuthor=test\n");
        writeFile(childDir + "/diskGood-100.png", "");
        writeFile(parentDir + "/diskGood-200.png", "");

        QHash<QString, Theme> themes{{"Child", Theme::parse(childDir)}, {"Parent", Theme::parse(parentDir)}};
        themes["Child"].resolve(themes);
        QCOMPARE(themes["Child"].assetPath("diskGood", 150), childDir + "/diskGood-100.png");
    }

    void resolvesBackgroundThroughParents()
    {
        QTemporaryDir tmp;
        const QString childDir = tmp.filePath("AoiDark");
        const QString parentDir = tmp.filePath("AoiNight");
        writeFile(childDir + "/theme.ini", "[Info]\nParentTheme1=AoiNight\n");
        writeFile(parentDir + "/theme.ini", "[Info]\nAuthor=test\n");
        writeFile(parentDir + "/AoiBackground-300.png", "");

        QHash<QString, Theme> themes{{"AoiDark", Theme::parse(childDir)}, {"AoiNight", Theme::parse(parentDir)}};
        themes["AoiDark"].resolve(themes);
        QCOMPARE(themes["AoiDark"].backgroundPath(100), parentDir + "/AoiBackground-300.png");
        QVERIFY(!themes["AoiDark"].hasOwnBackground());
        QVERIFY(themes["AoiNight"].hasOwnBackground());
    }

    void discoversThemesAndUserShadowsSystem()
    {
        QTemporaryDir tmp;
        const QString userRoot = tmp.filePath("user");
        const QString systemRoot = tmp.filePath("system");
        writeFile(userRoot + "/Aoi/theme.ini", "[Info]\nAuthor=user\n");
        writeFile(systemRoot + "/Aoi/theme.ini", "[Info]\nAuthor=system\n");
        writeFile(systemRoot + "/Night/theme.ini", "[Info]\nAuthor=system\n");
        writeFile(systemRoot + "/Broken/readme.txt", "not a theme");

        ThemeManager manager(QStringList{userRoot, systemRoot}, userRoot);
        manager.reload();

        QCOMPARE(manager.themeNames(), QStringList({"Aoi", "Night"}));
        QVERIFY(manager.theme("Aoi") != nullptr);
        QCOMPARE(manager.theme("Aoi")->directory(), userRoot + "/Aoi");
        QVERIFY(manager.theme("Missing") == nullptr);
    }

    void ignoresMissingRoots()
    {
        QTemporaryDir tmp;
        ThemeManager manager(QStringList{tmp.filePath("missing")}, tmp.filePath("writable"));
        manager.reload();
        QVERIFY(manager.themeNames().isEmpty());
    }
};

QTEST_GUILESS_MAIN(ThemeTests)

#include "theme_tests.moc"
