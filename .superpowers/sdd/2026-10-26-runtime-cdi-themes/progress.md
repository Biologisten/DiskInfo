# SDD ledger — plan: /home/admin/QDiskInfo/.worktrees/runtime-cdi-themes-impl/docs/superpowers/plans/2026-10-26-runtime-cdi-themes.md

Spec: /home/admin/QDiskInfo/.worktrees/runtime-cdi-themes-impl/docs/superpowers/specs/2026-10-26-runtime-cdi-themes-design.md
Worktree: /home/admin/QDiskInfo/.worktrees/runtime-cdi-themes-impl
Branch: feature/runtime-cdi-themes-impl (from feature/runtime-cdi-themes @ d23ae80)
Baseline: cmake -S . -B build -DQT_VERSION_MAJOR=6 -DENABLE_TRANSLATIONS=OFF && cmake --build build -j$(nproc) — passed (no tests exist yet)

## Task checklist

- [x] Task 1: Theme INI parser and own colours
- [x] Task 2: Parent inheritance and DPI asset resolution
- [x] Task 3: Theme discovery
- [x] Task 4: Vendored ZIP reader and path safety
- [x] Task 5: CDI ZIP inspection and import
- [x] Task 6: Theme menu, persistence, artwork and colours
- [ ] Task 7: Reversible themed layout and drive illustration
- [ ] Task 8: Legacy compiled theme, Clear Settings, docs and final verification

## Pre-flight conflict scan

| Tasks | Shared file / interface | Produced vs consumed | Finding |
|---|---|---|---|
| 1 → 2 | `include/theme.h`, `src/theme.cpp`, `tests/theme_tests.cpp` | Task 1 produces `Theme::parse`, `ThemeColors`; Task 2 consumes and extends | Clean |
| 2 → 3 | `include/theme.h`, `src/theme.cpp`, `include/thememanager.h`, `src/thememanager.cpp` | Task 2 produces `resolve`/`assetPath`; Task 3 consumes | Clean |
| 3 → 5 | `include/thememanager.h`, `src/thememanager.cpp` | Task 3 produces discovery; Task 5 adds inspection/import | Clean |
| 4 → 5 | `include/ziparchive.h`, `src/ziparchive.cpp` | Task 4 produces ZIP wrapper; Task 5 consumes it | Clean |
| 1 → 6 | `include/theme.h`, `src/theme.cpp`, `tests/theme_tests.cpp` | Task 6 adds `buildThemeStyleSheet` and its test | Clean |
| 3/5 → 6 | `ThemeManager` interface | Task 6 consumes discovery/import | Clean |
| 6 → 7 | `include/mainwindow.h`, `src/mainwindow.cpp` | Task 6 produces menu/apply/clear/background; Task 7 adds layout/status | Intentional staged extension: Task 6 has a partial `initializeDefaultThemeState`; Task 7 replaces it with the full reversible implementation |
| 6 → 8 | `include/mainwindow.h`, `src/mainwindow.cpp` | Task 6 produces runtime apply/clear; Task 8 adds built-in and startup routing | Intentional staged extension: Task 8 adds `__builtin__` handling and the Built-in menu action |
| 7 → 8 | `include/mainwindow.h`, `src/mainwindow.cpp` | Task 7 produces `applyThemeLayout`/`updateStatusIllustration`; Task 8 consumes them | Clean |
| 8 → docs | `README.md` | Task 8 replaces compile-time instructions | Clean |

Self-consistency scan (one row per task):

| Task | Own text check | Finding |
|---|---|---|
| 1 | Parser tests match parser rules; CMake test target includes `src/theme.cpp` and links Core/Gui/Test | Clean |
| 2 | Inheritance/asset tests match `resolve`/`assetPath`/`backgroundPath` rules | Clean |
| 3 | Discovery tests match user-shadow/system scan order; test target add is specified | Clean |
| 4 | ZIP tests match `ZipArchive` interface, path rules and symlink bit test; CMake integration specified | Clean |
| 5 | Import tests match inspect/import semantics, fatal traversal handling and overwrite/skip | Clean |
| 6 | Stylesheet test matches QSS contract; manual step is explicit; layout intentionally deferred | Clean |
| 7 | Layout/status manual checks match reversible spacer/widget rules; no automated test expected | Clean |
| 8 | Built-in/startup/Clear Settings/build matrix agree with spec | Clean |

Rulings:
- Ruling: Task 6 implements a partial `initializeDefaultThemeState`, and Task 7 replaces it with the full spacer/status implementation — because the task split keeps Task 6 independently buildable — cost if wrong: one extra refactor pass in Task 7, already anticipated.
- Ruling: Task 8, not Task 6, routes an empty saved key to the compiled built-in theme — because the built-in mode is introduced in Task 8 — cost if wrong: startup behaviour needs one small relocation.
- Ruling: Use `GenericDataLocation + "/qdiskinfo/themes"` rather than `AppDataLocation` — because QSettings is explicitly constructed as `QSettings("qdiskinfo", "qdiskinfo")` and the user path is already `~/.local/share/qdiskinfo/themes` — cost if wrong: theme roots would need a one-line change.
- Ruling: Ordinary buttons/fields use `LabelText`; `ButtonText` is reserved for health/temperature pills — because CDI uses ButtonText for those pills and AoiNight would otherwise render black text on dark glass — cost if wrong: one QSS colour change.

## Progress

- 2026-10-26: Worktree created at `.worktrees/runtime-cdi-themes-impl`; baseline build passed.
- Task 1 BASE: d23ae80
Task 1: complete (commits d23ae80..b1cc8f7, tests: ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure → Total Test time (real) =   0.02 sec)
Task 2: complete (commits b1cc8f7..35a643e, tests: ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure → Total Test time (real) =   0.02 sec)
Task 3: complete (commits 35a643e..7d9103c, tests: ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure → Total Test time (real) =   0.02 sec)
Task 4: complete (commits 7d9103c..e4b27d5, tests: ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure → Total Test time (real) =   0.02 sec)
Task 5: complete (commits e4b27d5..f9a9c5e, tests: ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure → Total Test time (real) =   0.02 sec)
Task 6: complete (commits f9a9c5e..16ebefe, tests: ctest --test-dir build -R qdiskinfo_theme_tests --output-on-failure → Total Test time (real) =   0.03 sec)
