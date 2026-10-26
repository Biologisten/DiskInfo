#include <QDir>
#include "theme.h"
#include "thememanager.h"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

class ThemeTests : public QObject
{
    Q_OBJECT
private slots:
    void parsesCdiThemeIni()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString dir = tmp.filePath("Aoi");
        QDir().mkpath(dir);
        QFile f(dir + "/theme.ini");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[Info]\nAuthor=test\nParentTheme1=AoiNight\nParentTheme1=Aoi\n[Color];RGB\nLabelText=0x112233;\nButtonText=#445566;\nListText1=0xAABBCC;\nListText2=0xDDEEFF;\nListBg1=0xFFFFFF;\nListBg2=0xF8F8F8;\nListLine1=0xE0E0E0;\nListLine2=0xE8E8E8;\nGlass=0x102030;\n[Alpha]\nGlassAlpha=160;\n[Character]\nPosition=1;\n");
        f.close();

        Theme theme = Theme::parse(dir);
        QVERIFY(theme.isValid());
        QCOMPARE(theme.name(), QString("Aoi"));
        QCOMPARE(theme.parentNames(), QStringList({"AoiNight", "Aoi"}));
        QCOMPARE(theme.characterPosition(), 1);
        QCOMPARE(theme.colors().labelText, QColor(0x11, 0x22, 0x33));
        qDebug() << "DEBUG raw=" << theme.debugRaw("color/labeltext") << "valid=" << theme.isValid() << "dir=" << theme.directory();
        QCOMPARE(theme.colors().buttonText, QColor(0x44, 0x55, 0x66));
        QCOMPARE(theme.colors().glass, QColor(0x10, 0x20, 0x30));
        QCOMPARE(theme.colors().glassAlpha, 160);
    }

    void listBkWinsOverListBg()
    {
        QTemporaryDir tmp;
        const QString dir = tmp.filePath("Alias");
        QDir().mkpath(dir);
        QFile f(dir + "/theme.ini");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[Color]\nListBg1=0x111111;\nListBk1=0x222222;\nListBg2=0x333333;\nListBk2=0x444444;\n");
        f.close();
        Theme theme = Theme::parse(dir);
        QCOMPARE(theme.colors().listBg1, QColor(0x22, 0x22, 0x22));
        QCOMPARE(theme.colors().listBg2, QColor(0x44, 0x44, 0x44));
    }

    void appliesCdiDefaults()
    {
        QTemporaryDir tmp;
        const QString dir = tmp.filePath("Defaults");
        QDir().mkpath(dir);
        QFile f(dir + "/theme.ini");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[Info]\nAuthor=test\n");
        f.close();
        Theme theme = Theme::parse(dir);
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
};

QTEST_GUILESS_MAIN(ThemeTests)
#include "theme_tests.moc"
void inheritsMissingColoursParentFirst()
{
    QTemporaryDir tmp;
    const QString selfDir = tmp.filePath("Child");
    const QString parentDir = tmp.filePath("Parent");
    const QString grandDir = tmp.filePath("Grand");
    QDir().mkpath(selfDir); QDir().mkpath(parentDir); QDir().mkpath(grandDir);
    QFile(selfDir+"/theme.ini").open(QIODevice::WriteOnly); QFile(selfDir+"/theme.ini").write("[Info]\nParentTheme1=Parent\nParentTheme2=Grand\n[Color]\nLabelText=0x010203;\n");
    QFile(parentDir+"/theme.ini").open(QIODevice::WriteOnly); QFile(parentDir+"/theme.ini").write("[Color]\nGlass=0x111111;\nLabelText=0xAAAAAA;\n");
    QFile(grandDir+"/theme.ini").open(QIODevice::WriteOnly); QFile(grandDir+"/theme.ini").write("[Color]\nGlass=0x222222;\nListBg1=0x333333;\n");
    QHash<QString, Theme> themes{{"Child", Theme::parse(selfDir)}, {"Parent", Theme::parse(parentDir)}, {"Grand", Theme::parse(grandDir)}};
    themes["Child"].resolve(themes);
    QCOMPARE(themes["Child"].colors().labelText, QColor(0x01, 0x02, 0x03));
    QCOMPARE(themes["Child"].colors().glass, QColor(0x11, 0x11, 0x11));
    QCOMPARE(themes["Child"].colors().listBg1, QColor(0x33, 0x33, 0x33));
    QCOMPARE(themes["Child"].searchDirectories().size(), 3);
}

void discoversThemesAndUserShadowsSystem()
{
    QTemporaryDir tmp;
    const QString userRoot = tmp.filePath("user");
    const QString systemRoot = tmp.filePath("system");
    QDir().mkpath(userRoot+"/Aoi"); QDir().mkpath(systemRoot+"/Aoi"); QDir().mkpath(systemRoot+"/Night");
    QFile(userRoot+"/Aoi/theme.ini").open(QIODevice::WriteOnly); QFile(userRoot+"/Aoi/theme.ini").write("[Info]
Author=user
");
    QFile(systemRoot+"/Aoi/theme.ini").open(QIODevice::WriteOnly); QFile(systemRoot+"/Aoi/theme.ini").write("[Info]
Author=system
");
    QFile(systemRoot+"/Night/theme.ini").open(QIODevice::WriteOnly); QFile(systemRoot+"/Night/theme.ini").write("[Info]
Author=system
");
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
