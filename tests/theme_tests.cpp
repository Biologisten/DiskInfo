#include "theme.h"
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
