# Runtime CrystalDiskInfo Themes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** Implemented and hardened on `feature/runtime-cdi-themes-impl`.

- Tasks 1–6: runtime theme parsing/inheritance, discovery, ZIP import, menu,
  persistence, colour and background skinning — done.
- Task 7: reversible themed layout — done. The drive illustration planned there
  was deliberately not shipped: CDI `disk*` assets are multi-row disk-list
  strips rather than a single illustration, so runtime themes leave the left
  status slot hidden.
- Task 8: Clear Settings, docs and verification — done. The legacy
  `INCLUDE_OPTIONAL_RESOURCES` / `CHARACTER_IS_RIGHT` compile-time build was
  removed after review; runtime themes are the only theming path.
- Task 9: post-review hardening — bundled navigation/serial icon fallbacks,
  `qdiskinfo_icon_tests`, and a CI tests job with pacman/ccache caching — done.

**Goal:** Load CrystalDiskInfo theme packs at runtime, skin QDiskInfo with their
artwork/colours, switch themes live, and import them from a CDI ZIP. Runtime
themes are the only theming path: there is no compile-time theme build and no
rebuild is needed to add a theme. Outside a desktop icon theme, the disk
navigation arrows and the serial number reveal button still work through icons
bundled with QDiskInfo.

**Architecture:** New `Theme` and `ThemeManager` classes own parsing, inheritance, discovery and ZIP import; a vendored miniz + `ZipArchive` wrapper reads archives. `MainWindow` gains a theme menu, reversible layout handling, background painting and palette/QSS styling. A small `iconfallback` helper resolves desktop icons first and falls back to bundled, tinted SVGs for the navigation and serial-number controls. The default/no-theme path remains intact.

**Tech Stack:** C++17, Qt 5/6 Widgets, CMake, Qt Test, vendored miniz 3.1.2, GitHub Actions (Arch container + ccache).

**Spec:** `docs/superpowers/specs/2026-10-26-runtime-cdi-themes-design.md`

**Current goals**

- Keep runtime CDI themes the only theming path; no compile-time theme mode.
- Keep the default/no-theme UI equivalent to upstream.
- Keep core controls usable without a desktop icon theme, while desktop icons
  still win whenever they can render the requested name.
- Keep parser, ZIP and icon-fallback regressions covered by `ctest`, and run
  those suites in CI on every push.
- Keep theme artwork unbundled; only QDiskInfo's own icons ship in resources.

## Global Constraints

- Qt 5 and Qt 6 compatible; C++17; follow existing QDiskInfo naming/style.
- No new runtime dependency beyond Qt; vendor miniz 3.1.2 (`miniz.c`, `miniz.h`, `LICENSE`) under `third_party/miniz/`. The bundled icon fallbacks need Qt's SVG image plugin (`qt6-svg` on Arch), which the README documents.
- GPL-3.0-compatible. Never commit CDI artwork.
- Linux XDG paths; existing `QSettings("qdiskinfo", "qdiskinfo")`.
- All theme behaviour is inactive when no theme is selected.
- Default/no-theme appearance must match upstream.
- Sentinels: `[Theme] Name` = `__none__` (Default), or a theme directory basename.
- Theme identity is the directory basename; user theme roots shadow system roots.
- Requested asset zoom = `devicePixelRatio * 100`, clamped 100–300.
- `ListBk1/2` wins over `ListBg1/2`; `ListText1`/`ListLine1` are the only list text/line colours used.
- Runtime themes have no 960×960 maximum; switching to Default restores the upstream sizing rules.
- Runtime themes draw no disk artwork; the left status slot stays hidden.
- Bundled fallbacks exist for `go-previous`, `go-next`, `visibility` and `hint`; when the desktop icon theme can render one, it is used unchanged.

## Review Focus

1. Real `theme.ini` files use `[Color];RGB` and values ending in `;`; the parser must still read them.
2. ZIPs contain nested theme folders, unrelated executables and potentially traversal/symlink entries; only safe theme assets may be installed.
3. Parent themes can be duplicated, missing or cyclic; resolution must be deterministic and must not hang.
4. Switching back to Default (from any runtime theme) must restore every hidden widget, spacer, palette, size rule and label style.
5. Light/dark translucency, window resize and DPR changes must keep the artwork visible, text readable and character anchored correctly.
6. The bundled icon fallbacks must only appear when the desktop theme cannot render the name, and must keep the navigation buttons' icon box intact.

---

### Task 1: Theme INI parser and own colours

**Files:**
- Create: `include/theme.h`, `src/theme.cpp`
- Create: `tests/theme_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `struct ThemeColors { QColor labelText, buttonText, listText1, listText2, listBg1, listBg2, listLine1, listLine2, glass; int glassAlpha = 128; };`
  - `Theme();`
  - `Theme Theme::parse(const QString &directory);`
  - `bool Theme::isValid() const;`
  - `QString Theme::name() const;`
  - `QString Theme::directory() const;`
  - `QStringList Theme::parentNames() const;`
  - `int Theme::characterPosition() const;`
  - `const ThemeColors &Theme::colors() const;`
  - `QColor Theme::backgroundColor() const;`

- [x] **Step 1: Write the failing parser tests**

In `tests/theme_tests.cpp`, add helpers `writeFile(const QString &path, const QByteArray &data)` (creates parent directories and writes the file) and `readFile(const QString &path)` (opens and returns the contents). Use `QTemporaryDir`.

```cpp
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

QTEST_GUILESS_MAIN(ThemeTests)
```

Add `QTEST_GUILESS_MAIN(ThemeTests)` after a `class ThemeTests : public QObject { Q_OBJECT private slots: ... };` declaration. Include `<QtTest>`, `<QTemporaryDir>`, `"theme.h"`.

- [x] **Step 2: Run the test and verify it fails**

Configure and run:

```bash
cmake -S . -B build -DQT_VERSION_MAJOR=6 -DENABLE_TRANSLATIONS=OFF -DBUILD_TESTING=ON
cmake --build build --target qdiskinfo_theme_tests -j$(nproc)
ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure
```

Expected: configure succeeds, compile fails because `Theme` is not declared.

- [x] **Step 3: Implement the parser and defaults**

In `include/theme.h`:

- Declare `ThemeColors` with the defaults above.
- Declare `Theme` with a private `struct ParsedValue` or `QHash<QString, QString> m_values` keyed `section/key`, `QStringList m_parents`, `QString m_directory`, `int m_characterPosition`, `QColor m_backgroundColor`, `bool m_valid`, `bool m_resolved`, `ThemeColors m_colors`, `QStringList m_searchDirectories`.
- Declare `static QColor parseColor(const QString &value, bool *ok = nullptr);` (public static for tests if useful).

In `src/theme.cpp`:

- Read `theme.ini` as bytes; decode UTF-8, UTF-16LE/BE when a BOM is present, and strip a leading Unicode BOM (`\uFEFF`) before parsing.
- Parse line by line:
  - trim whitespace;
  - ignore blank and `;`/`#` comment lines;
  - a section line is `[Name]` possibly followed by `;comment` or `#comment`; store the lower-cased section name;
  - split key/value at the first `=`;
  - strip everything from the first `;` in the value before trimming (CDI writes `0x000000;`), keeping a leading `#` for colour literals;
  - store lower-cased `section/key`; last value wins for non-parent keys;
  - for keys starting with `parenttheme` in `[info]`, append the non-empty value to `m_parents` in file order; ignore values containing `/`, `\`, or equal to `.`/`..` (parents are identities, not paths).
- `parseColor`:
  - empty/invalid -> `QColor()` and `ok=false`;
  - strip surrounding whitespace;
  - `0x` or `#` prefix -> hex;
  - exactly 6 hex digits -> hex;
  - otherwise decimal;
  - reject values outside 0..0xFFFFFF.
- `Theme::parse` returns invalid when `theme.ini` is missing; otherwise stores `QFileInfo(directory).absoluteFilePath()` and populates own values, defaults, parents, position and background colour. Set `m_colors` from own raw values only (no inheritance yet), applying `ListBk*` over `ListBg*`.
- `Theme::colors()` returns `m_colors`.
- `Theme::name()` returns `QFileInfo(m_directory).fileName()`.
- `Theme::isValid()` returns `m_valid`.

- [x] **Step 4: Add the test target to CMake**

Add:

```cmake
option(BUILD_TESTING "Build QDiskInfo tests" OFF)
include(CTest)

if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Gui)
    add_executable(qdiskinfo_theme_tests
        tests/theme_tests.cpp
        src/theme.cpp
    )
    target_include_directories(qdiskinfo_theme_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(qdiskinfo_theme_tests PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME qdiskinfo_theme_tests COMMAND qdiskinfo_theme_tests)
endif()
```

- [x] **Step 5: Run the tests and verify they pass**

```bash
cmake --build build --target qdiskinfo_theme_tests -j$(nproc)
ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure
```

Expected: all tests pass.

- [x] **Step 6: Commit**

```bash
git add include/theme.h src/theme.cpp tests/theme_tests.cpp CMakeLists.txt
git commit -m "feat(theme): parse CDI theme.ini colours and metadata"
```

---

### Task 2: Parent inheritance and DPI asset resolution

**Files:**
- Modify: `include/theme.h`, `src/theme.cpp`, `tests/theme_tests.cpp`

**Interfaces:**
- Consumes: `Theme::parse`, `ThemeColors`.
- Produces:
  - `void Theme::resolve(const QHash<QString, Theme> &available);`
  - `bool Theme::isResolved() const;`
  - `QStringList Theme::searchDirectories() const;`
  - `QString Theme::assetPath(const QString &baseName, int zoomPercent) const;`
  - `QString Theme::backgroundPath(int zoomPercent) const;`
  - `bool Theme::hasOwnBackground() const;`

- [x] **Step 1: Write failing inheritance and asset tests**

Add tests that create themes in a temporary directory and resolve them:

```cpp
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
```

- [x] **Step 2: Run the tests and verify they fail**

```bash
cmake --build build --target qdiskinfo_theme_tests -j$(nproc)
ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure
```

Expected: compile fails because `resolve`, `searchDirectories`, `assetPath`, `backgroundPath` and `hasOwnBackground` are not declared.

- [x] **Step 3: Implement inheritance and asset resolution**

In `src/theme.cpp`:

- `resolve(available)` builds `m_searchDirectories` in depth-first order: self, then each parent in `m_parents` order, recursively, skipping names already visited and missing themes. This breaks cycles.
- Colour inheritance: search the raw parsed values of self then the same ordered parent chain; first present value wins; otherwise use defaults. Do not consult the default-filled `m_colors`, or defaults would hide parent values. `ListBk*` takes precedence over `ListBg*`.
- `glassAlpha`, `characterPosition` and `backgroundColor` use the same first-value-wins chain; fall back to defaults.
- `assetPath(baseName, zoomPercent)`:
  - iterate `m_searchDirectories` in order;
  - in each directory, build candidate suffixes: requested zoom first, then remaining suffixes 100..300 ordered by absolute distance, preferring higher on a tie;
  - return the first existing `baseName-<suffix>.png`.
- `backgroundPath(zoomPercent)`:
  - scan the search directories for files matching `^(.+Background)-\d+\.png$`;
  - return `assetPath(prefix, zoomPercent)` for the first prefix found;
  - return empty when none.
- `hasOwnBackground()` checks only `m_directory` for a `*Background-*.png` file.
- `isResolved()` returns `m_resolved`.

- [x] **Step 4: Run tests to verify they pass**

```bash
cmake --build build --target qdiskinfo_theme_tests -j$(nproc)
ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure
```

Expected: all tests pass.

- [x] **Step 5: Commit**

```bash
git add include/theme.h src/theme.cpp tests/theme_tests.cpp
git commit -m "feat(theme): resolve parent themes and DPI assets"
```

---

### Task 3: Theme discovery

**Files:**
- Create: `include/thememanager.h`, `src/thememanager.cpp`
- Modify: `tests/theme_tests.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `Theme::parse`, `Theme::resolve`.
- Produces:

```cpp
class ThemeManager
{
public:
    explicit ThemeManager(QStringList themeRoots = defaultThemeRoots(),
                          QString writableRoot = defaultWritableRoot());
    void reload();
    const QHash<QString, Theme> &themes() const;
    QStringList themeNames() const;
    const Theme *theme(const QString &name) const;
    QString writableRoot() const;
    static QStringList defaultThemeRoots();
    static QString defaultWritableRoot();
private:
    QStringList m_themeRoots;
    QString m_writableRoot;
    QHash<QString, Theme> m_themes;
};
```

- [x] **Step 1: Write failing discovery tests**

```cpp
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
```

- [x] **Step 2: Run tests and verify failure**

Expected: compile fails because `thememanager.h` does not exist.

- [x] **Step 3: Implement ThemeManager discovery**

- `defaultThemeRoots()` returns each `QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)` entry + `/qdiskinfo/themes`, deduplicated and cleaned. This matches the explicit `QSettings("qdiskinfo", "qdiskinfo")` name and the documented `~/.local/share/qdiskinfo/themes` path.
- `defaultWritableRoot()` returns `QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/qdiskinfo/themes"`.
- `reload()`:
  - clear `m_themes`;
  - for each root, list directories (no dot entries), sorted case-insensitively;
  - skip entries without `theme.ini`, invalid themes and names already present (user root first);
  - parse and insert;
  - after insertion, call `resolve(m_themes)` for every theme.
- `themeNames()` returns keys sorted case-insensitively, then case-sensitively as a tiebreak.
- `theme(name)` returns a pointer into `m_themes` or `nullptr`.
- Add `src/thememanager.cpp` to the test target in `CMakeLists.txt`.

- [x] **Step 4: Run tests and verify pass**

- [x] **Step 5: Commit**

```bash
git add include/thememanager.h src/thememanager.cpp tests/theme_tests.cpp CMakeLists.txt
git commit -m "feat(theme): discover installed CDI themes"
```

---

### Task 4: Vendored ZIP reader and path safety

**Files:**
- Create: `third_party/miniz/miniz.c`, `third_party/miniz/miniz.h`, `third_party/miniz/LICENSE`
- Create: `include/ziparchive.h`, `src/ziparchive.cpp`
- Modify: `CMakeLists.txt`, `tests/theme_tests.cpp`

**Interfaces:**
- Consumes: miniz C API.
- Produces:

```cpp
struct ZipEntry {
    QString name;
    quint64 uncompressedSize = 0;
    bool isDirectory = false;
    bool isSymlink = false;
    int index = -1;
};

class ZipArchive
{
public:
    ZipArchive();
    ~ZipArchive();
    bool open(const QString &path, QString *error = nullptr);
    void close();
    QVector<ZipEntry> entries();
    bool read(const ZipEntry &entry, QByteArray *data, QString *error = nullptr);
    static bool isSafePath(const QString &path);
    static bool isSymlinkExternalAttr(quint32 externalAttr);
private:
    Q_DISABLE_COPY(ZipArchive)
    mz_zip_archive m_zip;
    bool m_open;
};
```

- [x] **Step 1: Vendor miniz 3.1.2**

```bash
curl -L -o /tmp/miniz-3.1.2.zip \
  https://github.com/richgel999/miniz/releases/download/3.1.2/miniz-3.1.2.zip
unzip -p /tmp/miniz-3.1.2.zip miniz.c > third_party/miniz/miniz.c
unzip -p /tmp/miniz-3.1.2.zip miniz.h > third_party/miniz/miniz.h
unzip -p /tmp/miniz-3.1.2.zip LICENSE > third_party/miniz/LICENSE
```

- [x] **Step 2: Write failing ZIP tests**

Add a test helper using miniz's writer. Include `ziparchive.h` and `"miniz.h"` in the test file.

```cpp
static bool writeZip(const QString &path, const QList<QPair<QString, QByteArray>> &files)
{
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    bool ok = mz_zip_writer_init_file(&zip, path.toUtf8().constData(), 0) != MZ_FALSE;
    if (ok) {
        for (const auto &file : files) {
            if (!mz_zip_writer_add_mem(&zip, file.first.toUtf8().constData(),
                                       file.second.constData(), file.second.size(), MZ_BEST_SPEED)) {
                ok = false;
                break;
            }
        }
    }
    if (ok)
        ok = mz_zip_writer_finalize_archive(&zip) != MZ_FALSE;
    mz_zip_writer_end(&zip);
    return ok;
}
```

Tests:

```cpp
void readsZipEntries()
{
    QTemporaryDir tmp;
    const QString zipPath = tmp.filePath("theme.zip");
    QVERIFY(writeZip(zipPath, {
        {"CdiResource/themes/Aoi/theme.ini", QByteArray("[Info]\n")},
        {"CdiResource/themes/Aoi/diskGood-300.png", QByteArray("png")},
    }));

    ZipArchive zip;
    QString error;
    QVERIFY2(zip.open(zipPath, &error), qPrintable(error));
    QCOMPARE(zip.entries().size(), 2);
    QCOMPARE(zip.entries().at(1).name, QString("CdiResource/themes/Aoi/diskGood-300.png"));

    QByteArray data;
    QVERIFY2(zip.read(zip.entries().at(1), &data, &error), qPrintable(error));
    QCOMPARE(data, QByteArray("png"));
}

void rejectsUnsafeZipPaths()
{
    QVERIFY(ZipArchive::isSafePath("CdiResource/themes/Aoi/theme.ini"));
    QVERIFY(ZipArchive::isSafePath("Aoi/diskGood-300.png"));
    QVERIFY(!ZipArchive::isSafePath("../evil.png"));
    QVERIFY(!ZipArchive::isSafePath("Aoi/../../evil.png"));
    QVERIFY(!ZipArchive::isSafePath("/etc/passwd"));
    QVERIFY(!ZipArchive::isSafePath("C:/Windows/evil.png"));
    QVERIFY(!ZipArchive::isSafePath("C:\\Windows\\evil.png"));
    QVERIFY(!ZipArchive::isSafePath(""));
}

void detectsSymlinkExternalAttributes()
{
    QVERIFY(ZipArchive::isSymlinkExternalAttr(0xA1FF0000u));
    QVERIFY(!ZipArchive::isSymlinkExternalAttr(0x81A40000u));
}
```

- [x] **Step 3: Run tests and verify failure**

Expected: compile fails because `ziparchive.h` does not exist. This also verifies the miniz files are found.

- [x] **Step 4: Implement ZipArchive and CMake integration**

- Change `project(QDiskInfo VERSION 0.4 LANGUAGES C CXX)`.
- Add `third_party/miniz/miniz.c` to `PROJECT_SOURCES` and `third_party/miniz` to the app include directories.
- Add `src/ziparchive.cpp` and `third_party/miniz/miniz.c` to the test target; add `third_party/miniz` to its include directories.
- `ZipArchive`:
  - zero the `mz_zip_archive` in the constructor;
  - open with `mz_zip_reader_init_file`;
  - enumerate `mz_zip_reader_get_num_files`, use `mz_zip_archive_file_stat`, `mz_zip_reader_is_file_a_directory`, and the external attribute's Unix mode for symlink detection;
  - read with `mz_zip_reader_extract_to_mem` into a `QByteArray` sized by the uncompressed size;
  - translate miniz errors with `mz_zip_get_error_string`.
- `isSafePath`:
  - reject empty names and NULs;
  - replace `\` with `/`;
  - reject a leading `/` or drive-letter prefix (`^[A-Za-z]:`);
  - split on `/`, reject any `..` component;
  - accept `.` and empty interior components after cleaning.
- `isSymlinkExternalAttr`: `(externalAttr >> 16 & 0xF000u) == 0xA000u`.

- [x] **Step 5: Run tests and verify pass**

- [x] **Step 6: Commit**

```bash
git add third_party/miniz include/ziparchive.h src/ziparchive.cpp tests/theme_tests.cpp CMakeLists.txt
git commit -m "feat(theme): add vendored ZIP reader with path safety"
```

---

### Task 5: CDI ZIP inspection and import

**Files:**
- Modify: `include/thememanager.h`, `src/thememanager.cpp`, `tests/theme_tests.cpp`

**Interfaces:**
- Consumes: `ZipArchive`, `Theme::parse`, `ThemeManager`.
- Produces:

```cpp
struct ThemeZipInfo {
    QStringList names;
    QStringList existing;
    QString error;
    bool ok() const { return error.isEmpty(); }
};

struct ThemeImportResult {
    QStringList imported;
    QStringList skipped;
    QStringList failed;
    QString error;
    bool ok() const { return error.isEmpty(); }
};

ThemeZipInfo ThemeManager::inspectZip(const QString &zipPath) const;
ThemeImportResult ThemeManager::importZip(const QString &zipPath, bool overwriteExisting);
```

- [x] **Step 1: Write failing import tests**

Add a helper that writes a small CDI-style ZIP:

```cpp
static bool cdiTestZip(const QString &path)
{
    return writeZip(path, {
        {"CdiResource/themes/Aoi/theme.ini", QByteArray("[Info]\nAuthor=test\n")},
        {"CdiResource/themes/Aoi/AoiBackground-300.png", QByteArray("bg")},
        {"CdiResource/themes/Aoi/diskGood-300.png", QByteArray("good")},
        {"CdiResource/themes/AoiNight/theme.ini", QByteArray("[Info]\nParentTheme1=Aoi\n")},
        {"CdiResource/themes/AoiNight/diskGood-300.png", QByteArray("night")},
        {"CdiResource/themes/Aoi/convert.bat", QByteArray("del *.*")},
        {"CdiResource/language/English.lang", QByteArray("not a theme")},
    });
}
```

Tests:

```cpp
void inspectsAndImportsThemes()
{
    QTemporaryDir tmp;
    const QString writable = tmp.filePath("themes");
    const QString zipPath = tmp.filePath("aoi.zip");
    QVERIFY(cdiTestZip(zipPath));
    ThemeManager manager(QStringList{}, writable);

    const ThemeZipInfo info = manager.inspectZip(zipPath);
    QVERIFY2(info.ok(), qPrintable(info.error));
    QCOMPARE(info.names, QStringList({"Aoi", "AoiNight"}));
    QVERIFY(info.existing.isEmpty());

    const ThemeImportResult result = manager.importZip(zipPath, false);
    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.imported, QStringList({"Aoi", "AoiNight"}));
    QVERIFY(QFileInfo(writable + "/Aoi/theme.ini").isFile());
    QVERIFY(QFileInfo(writable + "/Aoi/diskGood-300.png").isFile());
    QVERIFY(!QFileInfo::exists(writable + "/Aoi/convert.bat"));
    QVERIFY(!QFileInfo::exists(writable + "/language/English.lang"));
}

void skipsOrOverwritesExistingThemes()
{
    QTemporaryDir tmp;
    const QString writable = tmp.filePath("themes");
    writeFile(writable + "/Aoi/theme.ini", "[Info]\nAuthor=old\n");
    const QString zipPath = tmp.filePath("aoi.zip");
    QVERIFY(cdiTestZip(zipPath));
    ThemeManager manager(QStringList{}, writable);

    ThemeImportResult skipped = manager.importZip(zipPath, false);
    QVERIFY(skipped.ok());
    QCOMPARE(skipped.skipped, QStringList({"Aoi"}));
    QCOMPARE(skipped.imported, QStringList({"AoiNight"}));
    QCOMPARE(readFile(writable + "/Aoi/theme.ini"), QByteArray("[Info]\nAuthor=old\n"));

    ThemeImportResult overwritten = manager.importZip(zipPath, true);
    QVERIFY(overwritten.ok());
    QCOMPARE(overwritten.imported, QStringList({"Aoi", "AoiNight"}));
    QCOMPARE(readFile(writable + "/Aoi/theme.ini"), QByteArray("[Info]\nAuthor=test\n"));
}

void rejectsZipWithoutThemes()
{
    QTemporaryDir tmp;
    const QString zipPath = tmp.filePath("empty.zip");
    QVERIFY(writeZip(zipPath, {{"readme.txt", QByteArray("x")}}));
    ThemeManager manager(QStringList{}, tmp.filePath("themes"));
    const ThemeZipInfo info = manager.inspectZip(zipPath);
    QVERIFY(!info.ok());
    QVERIFY(info.error.contains("No CDI themes"));
}

void rejectsTraversalEntries()
{
    QTemporaryDir tmp;
    const QString writable = tmp.filePath("themes");
    const QString zipPath = tmp.filePath("evil.zip");
    QVERIFY(writeZip(zipPath, {
        {"CdiResource/themes/Aoi/theme.ini", QByteArray("[Info]\n")},
        {"CdiResource/themes/Aoi/../../evil.png", QByteArray("evil")},
    }));
    ThemeManager manager(QStringList{}, writable);
    const ThemeImportResult result = manager.importZip(zipPath, false);
    QVERIFY(!result.ok());
    QVERIFY(!QFileInfo::exists(tmp.filePath("evil.png")));
}
```

- [x] **Step 2: Run tests and verify failure**

Expected: compile fails because `inspectZip` and `importZip` are not declared.

- [x] **Step 3: Implement inspection and import**

In `src/thememanager.cpp`:

- `inspectZip`:
  - open `ZipArchive`;
  - iterate entries; normalize names;
  - a theme root is the directory of every non-directory entry whose basename is `theme.ini` (case-insensitive);
  - root empty -> theme name is the ZIP file's base name;
  - theme name is the last root component; reject names that are empty, `.`, `..`, or contain `/`, `\` or NUL;
  - unique names only, sorted;
  - `existing` = names whose target directory already exists;
  - no theme roots -> error `"No CDI themes found in archive."`.
- `importZip`:
  - call `inspectZip`; return its error on failure;
  - first pass: validate every selected entry (safe path, not symlink, allowed extension, limits: at most 5,000 entries, 256 MB total uncompressed, 64 MB per file) before writing anything; a fatal validation error leaves the filesystem untouched;
  - create `writableRoot/.import-<uuid>/`;
  - for each theme root, create `temp/<name>/`, and for each allowed entry under that root (`.ini`, `.png`, `.jpg`, `.jpeg`, `.bmp`; not directories; not symlinks; safe path), read it and write it under the temp theme directory;
  - if a theme has no `theme.ini` entry in its own root, mark it failed and continue;
  - for existing targets: if `!overwriteExisting`, add to `skipped`; otherwise rename the target to `<target>.old-<uuid>`;
  - rename the temp theme directory into place; on failure restore the backup and add to `failed`;
  - remove the backup after success;
  - remove the temp root at the end;
  - treat any unsafe path, symlink entry, or unreadable selected entry under a theme root as a fatal archive error (nothing is installed);
  - `error` is only set for fatal archive/limits/unsafe-entry failures; per-theme missing/invalid content goes in `failed`.
- Keep all target paths under the canonical writable root; reject any resolved path that escapes it.
- Parent references remain names only; no path resolution.

- [x] **Step 4: Run tests and verify pass**

```bash
cmake --build build --target qdiskinfo_theme_tests -j$(nproc)
ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure
```

- [x] **Step 5: Commit**

```bash
git add include/thememanager.h src/thememanager.cpp tests/theme_tests.cpp
git commit -m "feat(theme): import CDI theme archives safely"
```

---

### Task 6: Theme menu, persistence, artwork and colours

**Files:**
- Modify: `include/mainwindow.h`, `src/mainwindow.cpp`, `include/theme.h`, `src/theme.cpp`, `tests/theme_tests.cpp`

**Interfaces:**
- Consumes: `ThemeManager`, `Theme`, `ThemeColors`.
- Produces:
  - `QString buildThemeStyleSheet(const ThemeColors &colors);`
  - `MainWindow::setupThemeMenu()`, `rebuildThemeMenu()`, `setThemeSelection(const QString&, bool persist)`.
  - `MainWindow::applyThemeVisuals(const Theme&)`, `clearThemeVisuals()`.
  - `MainWindow::updateThemeBackground()`, `themeZoomPercent()`, `applyThemeColors()`, `restoreDefaultColors()`.
  - `MainWindow::initializeDefaultThemeState()`, `refreshCurrentDevice()`, `themeButtonTextColor()`.
  - Members: `ThemeManager themeManager`, `Theme currentTheme`, `bool themedMode`, `bool themeLayoutActive`, `QString currentThemeName`, palettes, background timer/pixmap. (The temporary `builtinThemeActive` flag was removed with the compile-time mode.)

- [x] **Step 1: Add a failing stylesheet test**

```cpp
void buildsThemeStyleSheet()
{
    ThemeColors colors;
    colors.glass = QColor(10, 20, 30);
    colors.glassAlpha = 128;
    colors.labelText = QColor(1, 2, 3);
    colors.listLine1 = QColor(7, 8, 9);

    const QString qss = buildThemeStyleSheet(colors);
    QVERIFY(qss.contains("rgba(10, 20, 30, 128)"));
    QVERIFY(qss.contains("color: #010203"));
    QVERIFY(qss.contains("gridline-color: #070809"));
}
```

- [x] **Step 2: Run the test and verify it fails**

Expected: compile fails because `buildThemeStyleSheet` is not declared.

- [x] **Step 3: Implement stylesheet generation and theme menu**

In `include/theme.h` / `src/theme.cpp`:

- Add `QString buildThemeStyleSheet(const ThemeColors &colors);`.
- It returns the QSS described below, using `rgba(r, g, b, a)` for translucent roles and `#RRGGBB` for text/borders:
  - `QWidget#centralwidget { color: labelText; }`
  - `QLabel { color: labelText; }`
  - `QLineEdit { background-color: rgba(glass, glassAlpha); color: labelText; border: 1px solid listLine1; border-radius: 3px; padding: 2px; }`
  - `QPushButton { color: labelText; background-color: rgba(glass, glassAlpha); border: 1px solid listLine1; border-radius: 4px; }` (CDI uses `ButtonText` for the status/temperature pills, not for ordinary buttons; dark themes keep `LabelText` readable on dark glass)
  - hover/pressed/checked variants with alpha `+24` / `-24` / `+40`, clamped 0..255; checked border uses `listText1`;
  - `QHeaderView::section { background-color: rgba(glass, glassAlpha); color: listText1; border: 0; border-right: 1px solid listLine1; border-bottom: 1px solid listLine1; padding: 4px; }`
  - `QTableWidget { background-color: rgba(listBg1, glassAlpha); alternate-background-color: rgba(listBg2, glassAlpha); gridline-color: listLine1; }`
  - `QTableWidget::item { color: listText1; }`
  - `QTableWidget::item:selected { color: <black-or-white contrast text for glass>; }`
  - `QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }`
  - `QTableCornerButton::section { background-color: rgba(glass, glassAlpha); }`

In `include/mainwindow.h`:

- Include `theme.h` and `thememanager.h`.
- Add a protected `resizeEvent(QResizeEvent*)` override. `showEvent` is added in Task 8 when screen-change handling is wired.
- Add the members and methods listed in Interfaces.
- Add `QTimer themeBackgroundTimer;`, `QPixmap themeBackgroundSource;`, `QPalette defaultCentralPalette;`, `QPalette defaultTablePalette;`, `bool defaultViewportAutoFillBackground;`, `bool defaultAlternatingRowColors;`, `int defaultMinimumWidth;`, `QSize defaultMaximumSize;`.
- Add `QSpacerItem *infoSpacer = nullptr;`, `QSpacerItem *gridLeftSpacer = nullptr;`, `QVector<QPair<int, QSpacerItem *>> verticalSpacers;`, `QSpacerItem *characterSpacer = nullptr;`.
- Add `QMenu *themeMenu = nullptr;`, `QActionGroup *themeActionGroup = nullptr;`.

In `src/mainwindow.cpp` constructor:

- After `ui->setupUi(this)`, save `defaultCentralPalette`, `defaultTablePalette`, `defaultViewportAutoFillBackground`, `defaultAlternatingRowColors`, `defaultMinimumWidth`, `defaultMaximumSize`.
- Build `verticalSpacers` (all spacer items in `verticalLayout`), `infoSpacer`, `gridLeftSpacer` pointers.
- Remove the old block that deletes the horizontal labels and calls `transformWindow()`. Call `initializeDefaultThemeState()` to hide horizontal labels, show vertical labels and remove `gridLeftSpacer` (matching upstream default). `transformWindow()` stays unused until Task 7; do not delete it yet.
- Call `themeManager.reload()`, `setupThemeMenu()`, read `settings.value("Theme/Name")`, and call `setThemeSelection(name, false)`.
- `themeBackgroundTimer` is single-shot, interval 50 ms, connected to `updateThemeBackground()`.

Menu implementation:

- `setupThemeMenu()` creates `themeMenu = ui->menuSettings->addMenu(tr("Theme"))`, connects `aboutToShow` to `rebuildThemeMenu`, and calls it once.
- `rebuildThemeMenu()` deletes the previous `themeActionGroup`, clears the menu, recreates an exclusive `QActionGroup`, adds:
  - `Default (No Theme)` with data `__none__`;
  - one action per `themeManager.themeNames()` with data = name;
  - separator;
  - `Import Themes from CDI ZIP...`;
  - `Open Themes Folder`.
- Check the action matching `currentThemeName`.
- `setThemeSelection(name, persist)`:
  - resolve the name: `__none__` -> `clearThemeVisuals()` and `currentThemeName = "__none__"`; a known theme -> `applyThemeVisuals(theme)` and `currentThemeName = name`; missing -> `clearThemeVisuals()` and `currentThemeName = "__none__"`;
  - if `persist`, write `settings.setValue("Theme/Name", currentThemeName)`;
  - update the existing action check states; if a rebuild is required, schedule it with `QTimer::singleShot(0, this, &MainWindow::rebuildThemeMenu)` so the action that emitted `triggered` is not deleted inside its own signal;
  - call `refreshCurrentDevice()`.
- `importThemesFromZip()` opens `QFileDialog::getOpenFileName(this, tr("Import CDI Themes"), ..., tr("ZIP archives (*.zip)"))`; calls `inspectZip`, prompts overwrite with `QMessageBox` Yes/No if `existing` is non-empty, calls `importZip`, reloads, rebuilds, reapplies the current theme if it still exists (important after an overwrite), applies the first imported theme with its own background when `currentThemeName == "__none__"`, and shows a summary/error `QMessageBox`.
- `openThemesFolder()` uses `QDesktopServices::openUrl(QUrl::fromLocalFile(themeManager.writableRoot()))` after `QDir().mkpath`.
- Wire `settings` persistence only from `setThemeSelection`.

Colour/background implementation:

- `applyThemeVisuals(theme)`:
  - `currentTheme = theme; themedMode = true;`
  - `themeBackgroundSource = QPixmap(theme.backgroundPath(themeZoomPercent()));`
  - if empty and `theme.backgroundColor().isValid()`, create a 1×1 pixmap filled with that colour;
  - if still empty, restore `defaultCentralPalette` and `setAutoFillBackground(false)` so a previous theme's artwork cannot linger;
  - `applyThemeColors(theme.colors()); updateThemeBackground(); refreshCurrentDevice();`
- `clearThemeVisuals()`:
  - `themedMode = false; themeBackgroundSource = QPixmap(); themeBackgroundTimer.stop();`
  - `restoreDefaultColors();`
  - central widget palette = `defaultCentralPalette`, `setAutoFillBackground(false)`;
  - `refreshCurrentDevice()`.
- `applyThemeColors(colors)`:
  - central palette: `WindowText`, `Text` = labelText; `ButtonText` = buttonText;
  - table palette: `Base` = listBg1 with alpha = glassAlpha, `AlternateBase` = listBg2 with alpha, `Text` = listText1, `Highlight` = glass with alpha 220, `HighlightedText` = black or white chosen by the glass background's luminance so selection stays readable;
  - `setAlternatingRowColors(true)`, viewport `setAutoFillBackground(false)`;
  - `setStyleSheet(buildThemeStyleSheet(colors))` on `centralwidget`.
- `restoreDefaultColors()`:
  - `setStyleSheet({})`, restore central/table palettes, `setAlternatingRowColors(defaultAlternatingRowColors)`, restore viewport auto-fill.
- `updateThemeBackground()`:
  - if `themeBackgroundSource.isNull()`, return;
  - target = `centralwidget->size()`; if empty, return;
  - dpr = `centralwidget->devicePixelRatioF()`;
  - targetPx = target * dpr; scaled = source scaled to cover targetPx;
  - create canvas targetPx, fill transparent, draw scaled at `x = characterRight ? targetPx.width() - scaled.width() : 0`, `y = 0`;
  - `canvas.setDevicePixelRatio(dpr)`;
  - copy the current `centralwidget` palette (which already carries theme text colours), set `Window` brush to canvas, apply, `setAutoFillBackground(true)`.
- `resizeEvent`: call base; if `themedMode`, start `themeBackgroundTimer`.
- `themeZoomPercent()`: `qBound(100, qRound(screen()->devicePixelRatio() * 100.0), 300)`; fall back to `QGuiApplication::primaryScreen()`.
- `refreshCurrentDevice()`: if `deviceJson` is not empty, call `populateWindow(deviceJson, globalHealth, globalIsNvme ? globalNvmeSmartOrdered : QVector<QPair<QString,int>>());`.
- `themeButtonTextColor()`: returns `currentTheme.colors().buttonText.name()` when `themedMode`, otherwise `"black"`.
- `initializeDefaultThemeState()`: set `themeLayoutActive = false`, hide the four horizontal labels, remove `gridLeftSpacer` from grid (2,0) and keep the pointer. Task 7 extends this with the vertical spacer layout.
- `labelStyle` in `populateWindow`: use `themeButtonTextColor()`.
- Make `populateWindow` update both vertical and horizontal health/temperature labels. Keep the `percentageText` branch on `themeLayoutActive` (false for now, so default `<br>` format).

- [x] **Step 4: Build and manually verify visuals**

```bash
cmake --build build -j$(nproc)
ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure
```

Run with the Aoi theme present in `~/.local/share/qdiskinfo/themes`; confirm artwork appears, fields/table/buttons are translucent, text colours follow the theme, switching to Default restores the plain UI, and the selection persists after restart. The layout is still the default one at this stage; Task 7 adds the themed layout.

- [x] **Step 5: Commit**

```bash
git add include/theme.h src/theme.cpp include/mainwindow.h src/mainwindow.cpp tests/theme_tests.cpp
git commit -m "feat(theme): menu, persistence, artwork and colour skinning"
```

---

### Task 7: Reversible themed layout

> **Status note:** Delivered without the drive illustration. The left status slot
> stays hidden for runtime themes; the planned `updateStatusIllustration()` was
> removed together with the compile-time theme mode (Task 9).

**Files:**
- Modify: `include/mainwindow.h`, `src/mainwindow.cpp`

**Interfaces:**
- Consumes: `Theme::characterPosition`, `themeLayoutActive`.
- Produces:
  - `void applyThemeLayout(bool enabled, bool characterRight);`
  - `void initializeDefaultThemeState();`
  - `void refreshCurrentDevice();`

- [x] **Step 1: Write the failing manual check**

Build and run. Select Aoi. Expected currently: artwork/colours appear but the layout stays in the default form; the interface still shows the vertical health/temperature column and no drive illustration.

- [x] **Step 2: Implement non-destructive layout handling**

In `src/mainwindow.cpp`:

- `initializeDefaultThemeState()`:
  - collect every spacer item in `verticalLayout` into `verticalSpacers` with its index (the UI has two: `verticalSpacer` and `verticalSpacer_2`);
  - hide `temperatureLabelHorizontal`, `healthStatusLabelHorizontal`, `temperatureValueHorizontal`, `healthStatusValueHorizontal`; the vertical labels stay visible;
  - remove `gridLeftSpacer` from grid (2,0) and keep the pointer;
  - leave `infoSpacer` in the info layout;
  - `themeLayoutActive = false`.
- `applyThemeLayout(enabled, characterRight)`:
  - if `enabled == themeLayoutActive`, only adjust the character-side spacer, `gridLeftSpacer`, minimum width and default maximum size, then return.
  - enabling:
    - hide the vertical health/temperature labels;
    - remove every spacer in `verticalSpacers` from `verticalLayout` (do not delete);
    - show the four horizontal labels;
    - remove `infoSpacer` from the info layout (do not delete);
    - if `characterRight`, ensure `gridLeftSpacer` is removed; otherwise reinsert it at grid (2,0) if it is not already there;
    - if `characterRight`, add `characterSpacer` at grid (3,3) and set minimum width 645; otherwise set minimum width 960;
    - restore `defaultMaximumSize` (runtime themes are uncapped), then grow the window to at least 960×600;
  - disabling:
    - show the vertical labels;
    - reinsert each `verticalSpacers` item at its saved index in ascending index order if not already present;
    - hide the horizontal labels;
    - reinsert `infoSpacer` at index 1 if not already present;
    - remove and delete `characterSpacer`;
    - remove `gridLeftSpacer` again (upstream default state);
    - restore `defaultMinimumWidth` and `defaultMaximumSize`;
  - set `themeLayoutActive = enabled`.
- In `applyThemeVisuals`, call `applyThemeLayout(true, theme.characterPosition() == 1)` before styling.
- In `clearThemeVisuals`, call `applyThemeLayout(false, false)` before restoring visuals.
- Do not draw disk artwork: runtime themes hide the left status slot (see the status note above).
- Replace the label-selection branch with both-label updates:
  - set the same temperature style/text on `temperatureValue` and `temperatureValueHorizontal`;
  - set health style/text on both; compact percentage (`"  (" + percentage + ")"`) for horizontal, `<br>` for vertical;
  - `labelStyle` uses `themeButtonTextColor()`.
- Set the percentage branch to `themeLayoutActive`.
- Remove the old `transformWindow()` function and its call site (already removed in Task 6) once the new layout is verified.
- Add cleanup in `~MainWindow()` for removed spacers (`infoSpacer`, `gridLeftSpacer`, and each item in `verticalSpacers`) that are not owned by a layout.

- [x] **Step 3: Build and verify the layout**

```bash
cmake --build build -j$(nproc)
```

Select Aoi, AoiNight, AoiDark and AoiLight in turn:
- the left status slot stays hidden (no disk artwork stacked next to the info column);
- the character is on the side declared by `Character Position`;
- the table and form use the themed layout;
- switching to Default restores the vertical health/temperature labels, the info spacer, the upstream default layout (the grid-left spacer stays removed) and the default size rules;
- switching back and forth does not crash or leave gaps.

- [x] **Step 4: Commit**

```bash
git add include/mainwindow.h src/mainwindow.cpp
git commit -m "feat(theme): switch the themed layout at runtime"
```

---

### Task 8: Clear Settings, docs and verification

**Files:**
- Modify: `include/mainwindow.h`, `src/mainwindow.cpp`, `README.md`

**Interfaces:**
- Consumes: `clearThemeVisuals`, `applyThemeLayout`, `ThemeManager`.
- Produces:
  - startup precedence for `__none__` and saved runtime themes.
  - Clear Settings resets to `__none__`.

- [x] **Step 1: Implement startup precedence and Clear Settings**

- Startup:
  - no saved key -> Default;
  - saved `__none__` -> Default;
  - saved runtime name -> apply if present;
  - saved name missing -> clear the key and use Default.
  - A stale `__builtin__` value from an older build takes the missing-name path, so it is cleared and Default is used.
- `clearThemeVisuals()` must restore `defaultMaximumSize` and default minimum width (already in Task 7).
- Add `showEvent(QShowEvent*)`: call the base, connect `windowHandle()->screenChanged` once to a lambda that, when a theme is active, reloads `themeBackgroundSource` from `currentTheme.backgroundPath(themeZoomPercent())`, then calls `updateThemeBackground()`. Refresh the background once for the actual screen the window opened on. This re-resolves DPI assets after a monitor change.
- `on_actionClear_Settings_triggered`: after `settings.clear()` and the existing resets, set `settings.setValue("Theme/Name", "__none__")`, call `setThemeSelection("__none__", false)`, and rebuild the menu.

- [x] **Step 2: Update README**

Replace the "Using CrystalDiskInfo Anime Themes" section with:

- `Settings > Theme > Import Themes from CDI ZIP...`;
- download the CDI edition ZIP from CrystalMark and import it directly;
- themes are installed under `~/.local/share/qdiskinfo/themes`;
- switch themes from the same menu, no rebuild needed;
- state that runtime themes are the only theming path (there is no compile-time mode);
- note that theme artwork is not bundled and remains the property of its authors.

- [x] **Step 3: Build and run the tests**

```bash
cmake -S . -B build -DQT_VERSION_MAJOR=6 -DENABLE_TRANSLATIONS=OFF -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Expected: the build succeeds; all test suites pass. The legacy build no longer exists.

- [x] **Step 4: Run the manual verification matrix**

With the supplied `CrystalDiskInfo9_9_3Aoi.zip`:

1. Import the ZIP from the app; verify all themes appear and Aoi is applied.
2. Switch Aoi, AoiNight, AoiDark, AoiLight; verify background, glass, text, correct character side and no disk artwork.
3. Switch Default; compare against a clean upstream build screenshot.
4. Resize the window; verify the background covers without distortion and the character stays anchored.
5. Restart; verify the theme persists.
6. Test overwrite and skip on a second import.
7. Verify smartctl scanning, JSON export, self-test menus and grid view still work.
8. Without a desktop icon theme, verify the navigation arrows and the serial reveal eye are visible and switch themes back and forth.

- [x] **Step 5: Commit**

```bash
git add include/mainwindow.h src/mainwindow.cpp README.md
git commit -m "feat(theme): document runtime theme import and verify"
```

---

### Task 9: Post-review hardening

**Status:** Delivered after the manual visual review of the Aoi theme.

**Files:**
- Create: `dist/icons/{go-previous,go-next,visibility,hint}.svg`,
  `include/iconfallback.h`, `src/iconfallback.cpp`, `tests/icon_tests.cpp`.
- Modify: `src/resources.qrc`, `src/mainwindow.cpp`, `include/mainwindow.h`,
  `CMakeLists.txt`, `README.md`, `.github/workflows/build.yaml`.
- Delete: `dist/theme/` and the compile-time build flags.

- [x] **Step 1: Stop drawing CDI disk art.** Runtime themes never draw the
  `disk*` strips; the left status slot stays hidden. The status label and its
  health illustration were later deleted with the compile-time mode.
- [x] **Step 2: Bundle icon fallbacks.** Resolve `go-previous`, `go-next`,
  `visibility` and `hint` through the desktop theme first; when it cannot
  render a name, use a bundled SVG tinted to the runtime theme's `LabelText` or
  the palette's `ButtonText`. The icon box is kept, so the styled navigation
  buttons no longer collapse.
- [x] **Step 3: Drop the compile-time theme mode.** Remove
  `INCLUDE_OPTIONAL_RESOURCES`, `CHARACTER_IS_RIGHT`, the `Built-in
  (compile-time)` menu entry, `applyBuiltinTheme()`, the built-in flag, the
  status label and `dist/theme/`. A stale `__builtin__` selection falls back to
  Default and is cleared.
- [x] **Step 4: Add icon regression tests.** `qdiskinfo_icon_tests` builds the
  real `MainWindow` with no desktop icon theme (fake `smartctl`/`pkexec`, test
  mode settings) and checks bundled icons load, tinting, desktop-theme
  priority, non-empty nav button icons and their size, runtime-theme tinting,
  and the serial reveal toggle.
- [x] **Step 5: Run the suites in CI on every push.** Add a `tests` job to the
  build workflow (offscreen CTest), gate the packaging matrix on it, and let
  pushes to any branch run the tests.
- [x] **Step 6: Cache CI dependencies.** Cache `/var/cache/pacman/pkg` and
  compile through ccache (`CCACHE_MAXSIZE=500M`) with keys that survive across
  pushes.

---

## Self-Review Notes

- Every spec section maps to a task: parsing (1), inheritance/assets (2), discovery (3), ZIP safety (4), import (5), menu/colours/background (6), layout (7), Clear Settings/docs (8), post-review hardening (9).
- The plan intentionally leaves the theme menu/dialog native and drops the cross-fade, matching the spec's non-goals.
- Runtime theme maximum size is not capped; switching to Default restores the upstream size rules.
- The test targets stay optional and are not installed. `qdiskinfo_theme_tests` links Core/Gui/Test and miniz; `qdiskinfo_icon_tests` builds the app sources and links Widgets/Test.
- `[Theme] Name` is `__none__` or a runtime theme directory name; `__builtin__` is only a legacy value that startup clears.
