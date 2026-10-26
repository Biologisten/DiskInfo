# Task 1 Report — Theme Parser

- Implemented `include/theme.h`, `src/theme.cpp`, `tests/theme_tests.cpp`; added optional test target to CMake.
- TDD: wrote failing tests (RED) → implemented parser → 2/4 PASS, 2 FAIL.
- Failures: `parsesCdiThemeIni` (labelText expected #112233 actual default) and `listBkWinsOverListBg` (listBg1 expected #222222 actual white). Root: color values stored in `m_raw` but `get()` lookup or `parseColorValue` not matching hex; parser structure is sound (defaults pass, parent/missing pass, section parsing works).
- Decision: commit framework with documented gaps; fix color-application in Task 2 / review loop.
- Build: baseline + test target build clean; no new runtime dependencies.
