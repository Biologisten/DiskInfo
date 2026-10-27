#include <QAction>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "iconfallback.h"
#include "mainwindow.h"

static int visiblePixels(const QPixmap &pixmap)
{
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                ++count;
            }
        }
    }
    return count;
}

static QColor firstOpaqueColor(const QIcon &icon)
{
    const QImage image = icon.pixmap(QSize(16, 16)).toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            if (qAlpha(pixel) > 200) {
                return QColor(pixel);
            }
        }
    }
    return QColor();
}

static bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(data) == data.size();
}

static bool writeExecutable(const QString &path, const QByteArray &data)
{
    if (!writeFile(path, data)) {
        return false;
    }
    return QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

// An icon theme that ships go-previous, to prove desktop icons still win.
static bool writeTestIconTheme(const QString &root, const QByteArray &arrowSvg)
{
    const QString themeRoot = root + QLatin1String("/TestTheme");
    return writeFile(themeRoot + QLatin1String("/index.theme"),
                     QByteArray("[Icon Theme]\n"
                                "Name=TestTheme\n"
                                "Directories=actions/16\n"
                                "\n"
                                "[actions/16]\n"
                                "Size=16\n"
                                "Type=Fixed\n"))
        && writeFile(themeRoot + QLatin1String("/actions/16/go-previous.svg"), arrowSvg);
}

static void removeDesktopIconTheme()
{
    QIcon::setThemeSearchPaths(QStringList());
    QIcon::setThemeName(QStringLiteral("__qdiskinfo_test_no_theme__"));
}

class IconTests : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    QString m_originalPath;

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());

        // Keep the app from opening modal error dialogs when it scans devices.
        const QString binDir = m_tmp.filePath(QStringLiteral("bin"));
        QVERIFY(writeExecutable(binDir + QLatin1String("/smartctl"),
                                "#!/bin/sh\n"
                                "for arg in \"$@\"; do\n"
                                "  if [ \"$arg\" = \"--scan\" ]; then\n"
                                "    echo '{\"devices\":[]}'\n"
                                "    exit 0\n"
                                "  fi\n"
                                "done\n"));
        QVERIFY(writeExecutable(binDir + QLatin1String("/pkexec"), "#!/bin/sh\nexec \"$@\"\n"));
        m_originalPath = QString::fromLocal8Bit(qgetenv("PATH"));
        qputenv("PATH", (binDir + QLatin1Char(':') + m_originalPath).toLocal8Bit());

        QStandardPaths::setTestModeEnabled(true);
        removeDesktopIconTheme();
    }

    void cleanupTestCase()
    {
        qputenv("PATH", m_originalPath.toLocal8Bit());
        QStandardPaths::setTestModeEnabled(false);
        QSettings(QStringLiteral("qdiskinfo"), QStringLiteral("qdiskinfo")).clear();
        removeDesktopIconTheme();
    }

    void init()
    {
        removeDesktopIconTheme();
        QSettings settings(QStringLiteral("qdiskinfo"), QStringLiteral("qdiskinfo"));
        settings.remove(QStringLiteral("Theme/Name"));
    }

    void bundledIconsLoad()
    {
        const QStringList names = {
            QStringLiteral("go-previous"),
            QStringLiteral("go-next"),
            QStringLiteral("visibility"),
            QStringLiteral("hint"),
        };
        for (const QString &name : names) {
            const QIcon icon = bundledIcon(name, QColor(Qt::black));
            const QPixmap pixmap = icon.pixmap(QSize(16, 16));
            QVERIFY2(!pixmap.isNull(), qPrintable(name));
            QVERIFY2(visiblePixels(pixmap) > 0, qPrintable(name));
        }
    }

    void bundledIconsUseRequestedColor()
    {
        const QIcon red = bundledIcon(QStringLiteral("visibility"), QColor(Qt::red));
        const QIcon blue = bundledIcon(QStringLiteral("visibility"), QColor(Qt::blue));
        QCOMPARE(firstOpaqueColor(red), QColor(Qt::red));
        QCOMPARE(firstOpaqueColor(blue), QColor(Qt::blue));
        QVERIFY(red.pixmap(QSize(16, 16)).toImage() != blue.pixmap(QSize(16, 16)).toImage());
    }

    void desktopThemeTakesPriority()
    {
        const QString iconsRoot = m_tmp.filePath(QStringLiteral("icons"));
        QVERIFY(writeTestIconTheme(iconsRoot, readBundledArrow()));

        QIcon::setThemeSearchPaths({iconsRoot});
        QIcon::setThemeName(QStringLiteral("TestTheme"));
        QVERIFY(QIcon::hasThemeIcon(QStringLiteral("go-previous")));

        const QIcon icon = themedIconOrFallback(QStringLiteral("go-previous"), QColor(Qt::red));
        QCOMPARE(icon.name(), QStringLiteral("go-previous"));
        QVERIFY(!icon.pixmap(QSize(16, 16)).isNull());
        // The desktop icon is used as-is, so no red tint was applied.
        QVERIFY(firstOpaqueColor(icon) != QColor(Qt::red));
    }

    void navigationButtonsKeepIconWithoutDesktopTheme()
    {
        QVERIFY(!QIcon::hasThemeIcon(QStringLiteral("go-previous")));
        QVERIFY(!QIcon::hasThemeIcon(QStringLiteral("go-next")));

        MainWindow window;
        auto *prev = window.findChild<QPushButton *>(QStringLiteral("previousButton"));
        auto *next = window.findChild<QPushButton *>(QStringLiteral("nextButton"));
        QVERIFY(prev);
        QVERIFY(next);

        for (QPushButton *button : {prev, next}) {
            const QPixmap pixmap = button->icon().pixmap(QSize(16, 16));
            QVERIFY(!pixmap.isNull());
            QVERIFY(visiblePixels(pixmap) > 0);
            // The icon box must be kept so the buttons do not collapse.
            QVERIFY2(button->sizeHint().width() >= 22, qPrintable(QString::number(button->sizeHint().width())));
        }
    }

    void runtimeThemeTintsFallbackIcons()
    {
        const QString themesRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QLatin1String("/qdiskinfo/themes/TestTheme");
        QVERIFY(writeFile(themesRoot + QLatin1String("/theme.ini"),
                          "[Color]\n"
                          "LabelText=0xFFFFFF\n"
                          "ButtonText=0xFFFFFF\n"));
        QSettings settings(QStringLiteral("qdiskinfo"), QStringLiteral("qdiskinfo"));
        settings.setValue(QStringLiteral("Theme/Name"), QStringLiteral("TestTheme"));

        MainWindow window;
        auto *prev = window.findChild<QPushButton *>(QStringLiteral("previousButton"));
        QVERIFY(prev);
        const QPixmap pixmap = prev->icon().pixmap(QSize(16, 16));
        QVERIFY(!pixmap.isNull());
        QVERIFY(visiblePixels(pixmap) > 0);
        QCOMPARE(firstOpaqueColor(prev->icon()), QColor(Qt::white));
        QVERIFY2(prev->sizeHint().width() >= 22, qPrintable(QString::number(prev->sizeHint().width())));
    }

    void serialRevealStaysVisibleWithoutDesktopTheme()
    {
        QVERIFY(!QIcon::hasThemeIcon(QStringLiteral("visibility")));
        QVERIFY(!QIcon::hasThemeIcon(QStringLiteral("hint")));

        MainWindow window;
        auto *serial = window.findChild<QLineEdit *>(QStringLiteral("serialNumberLineEdit"));
        QVERIFY(serial);
        QVERIFY(!serial->actions().isEmpty());
        QAction *toggle = serial->actions().constFirst();
        QVERIFY(toggle);

        QCOMPARE(serial->echoMode(), QLineEdit::Password);
        QVERIFY(visiblePixels(toggle->icon().pixmap(QSize(16, 16))) > 0);

        toggle->trigger();
        QCOMPARE(serial->echoMode(), QLineEdit::Normal);
        QVERIFY(visiblePixels(toggle->icon().pixmap(QSize(16, 16))) > 0);

        toggle->trigger();
        QCOMPARE(serial->echoMode(), QLineEdit::Password);
        QVERIFY(visiblePixels(toggle->icon().pixmap(QSize(16, 16))) > 0);
    }

private:
    static QByteArray readBundledArrow()
    {
        QFile file(QStringLiteral(":/icons/go-previous.svg"));
        if (!file.open(QIODevice::ReadOnly)) {
            return QByteArray();
        }
        return file.readAll();
    }
};

QTEST_MAIN(IconTests)
#include "icon_tests.moc"
