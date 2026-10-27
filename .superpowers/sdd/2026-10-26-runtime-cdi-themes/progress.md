# SDD ledger — plan: /home/admin/Dokument/Programmering/QDiskInfo/docs/superpowers/plans/2026-10-26-runtime-cdi-themes.md

Spec: /home/admin/Dokument/Programmering/QDiskInfo/docs/superpowers/specs/2026-10-26-runtime-cdi-themes-design.md
Worktree: /home/admin/Dokument/Programmering/QDiskInfo (repo relocated; the old .worktrees/runtime-cdi-themes-impl link was pruned and the branch is checked out here)
Branch: feature/runtime-cdi-themes-impl (from feature/runtime-cdi-themes @ d23ae80)

## Task checklist

- [x] Task 1: Theme INI parser and own colours — re-implemented
- [x] Task 2: Parent inheritance and DPI asset resolution — re-implemented
- [x] Task 3: Theme discovery — re-implemented
- [x] Task 4: Vendored ZIP reader and path safety — fixed miniz API usage
- [x] Task 5: CDI ZIP inspection and import — implemented for real
- [ ] Task 6: Theme menu, persistence, artwork and colours — under review
- [ ] Task 7: Reversible themed layout and drive illustration
- [ ] Task 8: Legacy compiled theme, Clear Settings, docs and final verification

## Review of the first pass (b297d1a)

Findings, against plan + spec:

- Task 1: `src/theme.cpp` held concatenated fragments and did not parse as C++ (stray `\&`, duplicate definitions, placeholder QSS). The recorded 2/4 test result came from a test file that itself did not compile.
- Task 2: `resolve()` collected parent directories only; no colour inheritance from raw values; the cycle test could not have passed.
- Task 3: discovery partly worked, but the constructor replaced an explicitly empty roots list with the default locations, and the name sort had no case-sensitive tiebreak.
- Task 4: `ZipArchive` used a different miniz API version (`m_uncompr_size`, 5-argument `mz_zip_reader_get_filename`) and lacked `QRegularExpression`; the target did not compile.
- Task 5: `thememanager_import.cpp` re-defined `ThemeZipInfo`/`ThemeImportResult` (compile error) and `importZip()` created directories without extracting any file.
- Task 6: MainWindow code is materially real (menu, persistence, visuals, background) and is reviewed after the repair.

Rulings:

- Task 1-5: Ruling: re-implement from the plan's tests and the spec instead of patching the committed stubs — because the committed code neither compiled nor implemented the required behaviour — cost if wrong: none; tests and spec are the contract.
- Task 5: Ruling: consolidate `thememanager_import.cpp` back into `src/thememanager.cpp` per the plan's file list — because the split file re-declared the shared structs — cost if wrong: file layout only.
- Task 1: Ruling: `ListLine1`/`ListLine2` default to black per the spec (the stub had E0/E8) — cost if wrong: one-line default change.
- Task 3: Ruling: an explicitly empty roots list means "no roots" (default arguments cover the standard locations) — because the plan's tests construct managers with explicit roots — cost if wrong: callers wanting defaults pass no argument.
- Task 6: Ruling: QSS uses token replacement instead of positional `.arg()` — because the stylesheet needs more than nine substitutions and chained `.arg()` corrupts multi-digit placeholders — cost if wrong: cosmetic implementation detail.

## Progress

- Repair: `tests/theme_tests.cpp` rewritten to the plan's full 19-test suite; CMake test target now builds `theme.cpp`, `thememanager.cpp`, `ziparchive.cpp` and `miniz.c`. RED captured: theme.cpp syntax errors, miniz API mismatch, missing `buildThemeStyleSheet` declaration.
- Repair: `theme.h`/`theme.cpp`, `thememanager.h`/`thememanager.cpp`, `ziparchive.cpp` re-implemented to the plan. App and test targets build warning-free; suite green: 19 passed / 0 failed, 0.03 s.
- Task 6: reviewed — menu, persistence, artwork and colours are real; fixed theme switches to start from the default palettes so a second theme cannot inherit the first one's palette.
- Task 7: complete — reversible layout and drive illustration (commit with Task 8).
- Task 8: complete — built-in compile-time theme, startup precedence, Clear Settings, README.
- Ruling, Task 7: `updateThemeBackground` anchors the built-in theme by `CHARACTER_IS_RIGHT` because `currentTheme` is empty for it — cost if wrong: built-in background anchored on the wrong side.
- Ruling, Task 7: health text keeps the mode-based percentage branch (compact while themed, `<br>` otherwise), matching upstream rendering for whichever label is visible — cost if wrong: cosmetic.
- Ruling, Task 8: the Built-in menu entry and built-in startup are compile-time guarded with `#if INCLUDE_OPTIONAL_RESOURCES` — cost if wrong: none.
- Ruling, Task 8: the built-in illustration also calls `setFixedSize` so a fixed size left by a runtime theme cannot leak into the built-in layout — cost if wrong: cosmetic.
- Verification: default and legacy (`INCLUDE_OPTIONAL_RESOURCES=ON CHARACTER_IS_RIGHT=ON`) builds succeed warning-free; 19/19 tests pass; offscreen startup smoke tests (runtime theme with parent/background/illustration, `__none__`, built-in, missing saved name) all stay running without a crash.
- Remaining: final whole-branch self-review. The manual visual matrix (artwork, readability, anchoring on real screens) is not executable headless and stays with the user.
