# Runtime CrystalDiskInfo Themes for QDiskInfo — Design

**Date:** 2026-10-26

**Status:** Approved

## Intent

Add runtime loading of CrystalDiskInfo (CDI) theme packs to QDiskInfo while
preserving QDiskInfo's existing layout and default appearance, so the change is
practical to merge upstream.

While a theme is active, QDiskInfo should gain the visual effects that make CDI
themes recognisable:

- full-window background artwork;
- translucent "glass" panels, so the artwork remains visible through the table,
  fields and buttons;
- colours read from the theme's `theme.ini`.

Themes must be switchable at runtime. A user must also be able to import a CDI
ZIP from inside QDiskInfo, without manually extracting it.

## Goals

1. Discover theme packs in XDG app-data directories: the user's
   `~/.local/share/qdiskinfo/themes` and system equivalents. Each subdirectory
   containing `theme.ini` is a theme.
2. Parse `theme.ini`: label/button/list text colours, list backgrounds and
   lines, glass colour, `GlassAlpha`, character position, and parent themes.
3. Resolve parent themes for missing colours and images, with cycle protection.
4. Resolve CDI image names, chiefly `*Background-<zoom>.png`; the other assets
   (including any `disk*` art) are indexed but not drawn.
5. Draw the background scaled to cover the window without distortion, anchored
   to the character side and top, and redraw on resize and DPR changes.
6. Apply theme colours to labels, fields, buttons, table rows, table headers,
   grid lines and selection, with glass translucency.
7. Leave the left status slot empty; runtime themes draw no disk artwork.
8. Add `Settings > Theme`: Default, each discovered theme, Import Themes from
   CDI ZIP…, and Open Themes Folder.
9. Persist the selection in the existing `[Theme] Name` key.
10. Import every theme from a CDI ZIP with one action, safely and atomically.
11. Leave the default/no-theme UI equivalent in appearance to current upstream.
12. Drop the compile-time theme mode entirely; runtime themes are the only
    theming path.
13. Keep the patch modular: new Theme/ThemeManager classes, a vendored ZIP
    reader, and thin MainWindow hooks.

## Non-goals

- Reshaping QDiskInfo to match CDI's layout.
- Theming menus, dialogs, tray, grid view, or About artwork.
- Bundling CDI artwork in the repository.
- A theme editor, network downloads, Windows support, animated characters, or
  sound effects.
- A cross-fade between themes. It would need a separate background layer or a
  widget-tree change, which works against easy upstreaming.
- Perfect fidelity for `ListText2` / `ListLine2`. The Aoi packs keep them equal
  to their `1` counterparts; the UI uses `ListText1` and `ListLine1`.

## Constraints

- Qt 5 and Qt 6 compatible, C++17, existing code style.
- No new runtime dependency: vendor miniz 3.1.2 (MIT).
- GPL-3.0-compatible.
- Linux XDG paths.
- Use the existing `QSettings("qdiskinfo", "qdiskinfo")` configuration.
- Existing configs may already contain `[Theme] Name=Aoi`.
- Theme files are user-supplied; never commit or redistribute CDI artwork.

## Chosen approaches

- **Separate Theme / ThemeManager module plus thin MainWindow hooks.** Chosen
  over inline MainWindow code (too tangled and hard to review) and over an
  application-wide `QProxyStyle`/palette (too invasive and breaks native
  styling).
- **Vendored miniz 3.1.2.** Chosen over Qt's private `QZipReader` (private API,
  missing headers on some distros) and over shelling out to `unzip` (new runtime
  dependency, harder to make safe).

## Theme identity and precedence

- Identity is the **directory basename**. It is used for parent references,
  menu labels, import destinations and `[Theme] Name`.
- User themes shadow system themes with the same name. Theme roots are
  `QStandardPaths::standardLocations(GenericDataLocation) + "/qdiskinfo/themes"`,
  scanned in order. This matches the existing `QSettings("qdiskinfo",
  "qdiskinfo")` name and the documented `~/.local/share/qdiskinfo/themes` path.
- Startup precedence:
  1. saved runtime theme name;
  2. default/no theme.
- Sentinels in `[Theme] Name`:
  - `__none__` — Default (No Theme).
  - any other value — runtime theme directory name.
- If a saved theme disappears, startup falls back to Default and clears the key.
- Selecting Default writes `__none__`, so the choice survives restarts.
- `Clear Settings` resets the theme selection to Default and restores the
  default UI immediately.

## Theme parsing

- A small INI reader is used instead of `QSettings`, because real CDI themes
  contain duplicate `ParentTheme1` keys and section lines such as
  `[Color];RGB`.
- The parser accepts:
  - section headers with trailing comments: `[Color];RGB`;
  - values with trailing semicolons: `LabelText=0x000000;`;
  - `0xRRGGBB`, `#RRGGBB`, `RRGGBB` and decimal colours;
  - UTF-8 with or without BOM; UTF-16 BOMs are also decoded.
- Sections and keys are matched case-insensitively.
- `[Color]` keys: `LabelText`, `ButtonText`, `ListText1`, `ListText2`,
  `ListBg1`, `ListBg2`, `ListLine1`, `ListLine2`, `Glass`, optional
  `Background`.
- `ListBk1` / `ListBk2` are accepted as aliases. `ListBk*` wins over `ListBg*`
  if both are present.
- `[Alpha] GlassAlpha` defaults to 128.
- `[Character] Position` defaults to 0 (left).
- `[Info] ParentTheme*` values are collected in file order. This preserves the
  intent of AoiDark's duplicate `ParentTheme1` lines (AoiNight, then Aoi).
- Colour defaults when no theme and no parent supplies a value:
  - `LabelText`, `ButtonText`, `ListText1/2`, `ListLine1/2`: black;
  - `ListBg1`: white;
  - `ListBg2`: `#F8F8F8`;
  - `Glass`: white;
  - `GlassAlpha`: 128.
- Missing colours are inherited parent-first (depth-first, file order). This is
  a superset of CDI, which falls straight to defaults, and makes thin variants
  such as AoiLight and AoiDark behave predictably.
- Parent cycles are broken and ignored; the theme still loads.
- Background prefix is detected from any `<prefix>Background-<zoom>.png` file in
  the theme or its parents.

## Asset and DPI selection

- The requested zoom is the window's `devicePixelRatio * 100`, clamped to
  100–300.
- For each image, choose the nearest available suffix, preferring the next
  higher suffix. Asset resolution happens after parent inheritance.
- A theme may request a 150-DPI asset while its parent only has 200; the parent
  asset is used.
- A screen or DPR change re-resolves assets and repaints.
- The background is loaded once per theme and rescaled for the window size.
- Background scaling uses `KeepAspectRatioByExpanding`, then crops:
  - horizontal anchor: left for `Character Position=0`, right for `1`;
  - vertical anchor: top.

## Visual application

- Background: a pixmap exactly the size of the central widget, filled with the
  cover-scaled artwork, installed as `QPalette::Window`; `autoFillBackground`
  is enabled. The original palette is restored when the theme is cleared.
- Table:
  - `Base` / `AlternateBase` from `ListBg1/2`, with alpha = `GlassAlpha`;
  - text from `ListText1`;
  - grid from `ListLine1`;
  - header background from `Glass` at `GlassAlpha`; header text from
    `ListText1`;
  - selection from `Glass` at high alpha with contrasting text;
  - alternating rows enabled;
  - the viewport is transparent so the artwork shows through.
- Fields and buttons: background `Glass` at `GlassAlpha`, text `LabelText`,
  border `ListLine1`. CDI uses `ButtonText` for the status/temperature pills,
  not for ordinary buttons, which keeps dark themes readable.
- Plain labels use `LabelText`.
- Health and temperature pills keep their semantic green/yellow/red/grey
  backgrounds. Their HTML text uses `ButtonText`.
- Table status dots keep their semantic green/yellow/red/grey colours.
- Drive illustration: none. The left status slot stays hidden. CDI's `disk*`
  assets (`diskGood`, `diskCaution`, `diskBad`, `diskUnknown` and their
  `Mini`/`diskStatus*` variants) are multi-row strips of disk-list cards rather
  than a single illustration, so they are not drawn and the disk selectors keep
  their default look.
- Menus and dialogs stay native.
- All theme styling is applied to the central widget only and removed when the
  theme is cleared.

## Reversible themed layout

The current `transformWindow()` deletes sidebar widgets, which makes runtime
switching impossible. It is replaced by non-destructive handling:

- Both the vertical and horizontal health/temperature label sets stay alive and
  are kept up to date; one set is hidden depending on the mode.
- The vertical spacer is removed while themed and restored afterwards.
- `horizontalSpacer_3` (inside the `info` layout) is removed and reinserted; it
  is not deleted.
- `horizontalSpacer_4` (grid row 2, column 0) is removed by default, matching
  upstream. It is reinserted only for a theme with `Character Position=0`.
- The character-side grid spacer is added at (3, 3) when the character is on the
  right, and removed otherwise.
- Minimum width follows the existing rules: 645 for a right-side character, 960
  for a left-side character.
- Runtime themes do not impose the old 960×960 maximum.
- First activation of a runtime theme grows the window to at least 960×600.
- Switching to Default restores the upstream layout, palette and sizing rules
  without a restart.

## Theme menu

`Settings > Theme`:

- `Default (No Theme)` — checkable.
- One checkable action per discovered theme, sorted case-insensitively.
- Separator.
- `Import Themes from CDI ZIP…`.
- `Open Themes Folder`.

The menu rebuilds on `aboutToShow` and after import.

## ZIP import

- One file picker. The selected ZIP is inspected for every directory containing
  `theme.ini`; nested theme folders are flattened to their basename.
- Import all such themes. Duplicate basenames within the archive keep the first
  and report the rest.
- Extract only `.ini` and image assets (`.png`, `.jpg`, `.jpeg`, `.bmp`).
  Ignore executables, DLLs, BAT files, language packs and HTML.
- Reject:
  - absolute paths;
  - drive letters;
  - `..` path components;
  - symlink entries;
  - NUL bytes in names.
- Limits: 5,000 archive entries, 256 MB total selected uncompressed data, 64 MB
  per file.
- Extract each theme to a temporary directory under the themes root. On success,
  rename into place. Existing themes prompt once:
  - **Yes** — overwrite existing themes;
  - **No** — skip existing themes, import the rest.
- Each theme's own directory is replaced only after its temporary extraction has
  succeeded. Backups are removed after a successful rename; a failed rename
  restores the backup.
- Parent references are theme identities only. Paths and separators are not
  accepted.
- After import: reload, rebuild the menu, reapply the active theme if it was
  overwritten, otherwise auto-apply the first imported theme that has its own
  background when the current selection is Default.
- Show a summary or error dialog. No network access, no root privileges.

## Persistence and Clear Settings

- `[Theme] Name` stores the selection:
  - `__none__`;
  - a theme directory name.
- `Clear Settings` resets the theme to `__none__` and applies Default
  immediately, alongside the existing settings resets.

## File structure

**New**

- `include/theme.h`, `src/theme.cpp` — parsing, inheritance, colour defaults,
  asset resolution, QSS generation.
- `include/thememanager.h`, `src/thememanager.cpp` — discovery, ZIP inspection
  and import.
- `include/ziparchive.h`, `src/ziparchive.cpp` — miniz C++ wrapper.
- `include/iconfallback.h`, `src/iconfallback.cpp` — desktop icon theme lookup
  with bundled, tinted SVG fallbacks for `go-previous`, `go-next`,
  `visibility` and `hint`.
- `dist/icons/{go-previous,go-next,visibility,hint}.svg` — QDiskInfo's own
  fallback artwork (not CDI artwork), registered in `src/resources.qrc`.
- `third_party/miniz/miniz.c`, `miniz.h`, `LICENSE`.
- `tests/theme_tests.cpp` — parser, inheritance, asset and ZIP tests
  (`qdiskinfo_theme_tests`).
- `tests/icon_tests.cpp` — bundled icons, fallback priority and control-level
  tests (`qdiskinfo_icon_tests`).
- `docs/superpowers/specs/2026-10-26-runtime-cdi-themes-design.md` — this file.
- `docs/superpowers/plans/2026-10-26-runtime-cdi-themes.md` — implementation
  plan.

**Modified**

- `include/mainwindow.h`, `src/mainwindow.cpp` — menu, apply/clear, reversible
  layout, background, styling, icon-fallback refresh on theme changes, display
  refresh.
- `src/resources.qrc` — registers the four fallback SVGs.
- `CMakeLists.txt` — C language, miniz, `qdiskinfo_theme_tests` and
  `qdiskinfo_icon_tests` behind `BUILD_TESTING`; the legacy
  `INCLUDE_OPTIONAL_RESOURCES` / `CHARACTER_IS_RIGHT` paths are gone.
- `README.md` — runtime theme/import instructions, bundled-icon note and how to
  run the tests.
- `.github/workflows/build.yaml` — `tests` job that runs `ctest` on every push
  and caches pacman packages and ccache output.

## Verification

### Automated

- Colour parsing: `0x`, `#`, bare hex, decimal, trailing semicolons, section
  comments.
- `ListBg`/`ListBk` alias and precedence.
- Duplicate parent keys, parent cycles, missing parents.
- Colour and asset inheritance order.
- DPI suffix selection.
- Background prefix detection.
- ZIP open/read, unsafe path rejection, symlink rejection, limits, import,
  overwrite/skip, nested theme roots and unsupported entries.
- Bundled icons: all four SVGs load to a non-empty pixmap and take the
  requested tint colour.
- Fallback selection: with no desktop icon theme the bundled icon is returned;
  a temporary theme that ships `go-previous` wins untouched.
- Real `MainWindow` without an icon theme (fake `smartctl`/`pkexec`, test-mode
  settings, offscreen platform): the navigation buttons and the serial action
  keep visible icons, the button size hint keeps its icon box; a runtime theme
  tints the fallback to `LabelText`, and the serial reveal toggle stays visible
  in both states.

### Manual

- Build with default flags.
- Aoi, AoiNight, AoiDark and AoiLight: artwork visible, text readable, glass
  panels translucent, correct character side, no disk artwork in the info
  column.
- Live switching between themes and back to Default; resize; simulated DPR
  change; restart persistence.
- Import the supplied 67 MB Aoi ZIP; verify all themes appear; test overwrite
  and skip.
- Compare no-theme appearance against upstream.
- Verify disk scanning, JSON export, self-tests and the grid view still work.
- Without a desktop icon theme (e.g. `hicolor`), verify the navigation arrows
  and the serial reveal eye are visible in Default and themed modes; with a
  working theme (Breeze/Adwaita/Papirus), verify the native icons are still
  used.
- Run both CTest suites; the icon suite runs headless with
  `QT_QPA_PLATFORM=offscreen`.
