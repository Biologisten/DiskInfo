# Changelog

This file records notable changes in this fork relative to upstream
[QDiskInfo](https://github.com/edisionnano/QDiskInfo). Upstream's release
history lives in its own repository. The fork starts from upstream QDiskInfo
0.4.

## Unreleased

### Added

- Runtime CrystalDiskInfo theme support. Themes can be imported from
  `Settings > Theme > Import Themes from CDI ZIP...`, switched at runtime and
  are remembered across restarts. Themes are installed under
  `~/.local/share/qdiskinfo/themes`.
- A CDI theme engine that reads `theme.ini` colour and metadata sections,
  including UTF-16 encoded files, parent-theme inheritance, `ListBk` colour
  aliases, glass transparency and the character position.
- DPI-aware artwork lookup for 100%, 125%, 150%, 200%, 250% and 300% variants,
  with a fallback to the nearest available asset. Theme backgrounds are scaled
  to the window and support left- or right-side characters.
- A safe ZIP importer built on a vendored miniz reader. It rejects path
  traversal and symlink entries, caps entry count and uncompressed size,
  prompts before overwriting installed themes and reports duplicate theme
  folders.
- Bundled monochrome SVG icons for `go-previous`, `go-next`, `visibility` and
  `hint`. They are used when the desktop icon theme has no matching icon and
  are tinted to the active theme's text colour.
- `qdiskinfo_theme_tests` and `qdiskinfo_icon_tests` CTest suites, covering
  theme parsing and inheritance, ZIP import and safety limits, bundled icons,
  desktop icon priority and the themed `MainWindow` on Qt's offscreen platform.
- A GitHub Actions test job that builds and runs the suites on pushes and pull
  requests, with caches for pacman packages and compiler output.

### Changed

- Theming is now runtime-only and applies immediately; the
  `Default (No Theme)` entry restores the stock look.
- Themed windows move the health/temperature values to the horizontal strip and
  make room for the character on the side requested by the theme.
- CDI's `disk*` images are multi-row disk-list strips rather than a single
  status illustration, so they are deliberately not drawn. The themed window
  keeps the disk selector's default appearance and uses the theme background,
  colours and character instead.
- The README now documents this fork, its runtime theming workflow, the bundled
  icons, current build dependencies and test instructions.

### Removed

- The compile-time theme mode and its `INCLUDE_OPTIONAL_RESOURCES` and
  `CHARACTER_IS_RIGHT` options.
- The placeholder `dist/theme` resources that were only used by that mode.
- The AppImage packaging script and the GitHub Actions job that built and
  uploaded AppImages; the fork publishes nothing and is built from source.
- The unreferenced `dist/QDiskInfo.png` and `dist/QDiskInfo-256x256.png`
  icons, the unused upstream `crowdin.yml` and the plan and design documents
  under `docs/superpowers/` for the finished runtime theme work.

### Fixed

- Navigation arrows and the serial-number reveal button no longer disappear
  when the desktop icon theme does not provide `go-previous`, `go-next`,
  `visibility` or `hint`.
- Importing a theme archive can no longer write outside the themes directory.
- Existing theme folders are detected and reported instead of being silently
  duplicated or skipped without explanation.
