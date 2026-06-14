# Changelog

All notable changes to Axiom are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.2.0] - 2025-06-14

### Added
- Multiple buffers with persistent tab bar (Ctrl-T, Ctrl-W, Ctrl-←, Ctrl-→)
- Visual line selection with Ctrl-K and multi-line copy/paste
- Line numbers with dynamic gutter width
- Soft indent with tab stop snapping and smart backspace
- Auto indent on newline
- Mouse scroll support
- Syntax highlighting for Python, Go, and Rust (C/C++ was already supported)
- ASCII banner on welcome screen
- `~/.axiomrc` config file with remappable buffer keybindings
- `--version` flag
- Man page (`man axiom` after install)
- GitHub Pages website at https://ovenpickled.github.io/axiom
- Cross-platform release binaries (Linux x86_64, Linux ARM64, macOS x86_64, macOS ARM64)
- Docker image published to Docker Hub (`awwyan/axiom`)
- `CONTRIBUTING.md` and `CODE_OF_CONDUCT.md`
- GitHub Issue templates for bug reports and feature requests

### Changed
- `make install` now also installs the man page
- Status bar shows current buffer index (`[1/3]`)
- Welcome screen centered correctly accounting for gutter width

### Fixed
- Cursor positioning offset after line number gutter was introduced
- `~` characters below welcome screen banner
- Quit warning now checks all buffers for unsaved changes, not just current
- Empty new buffers no longer show dirty indicator in tab bar
- Mouse reporting correctly disabled on all exit paths including signals
- Keybinding config settings were silently ignored in `~/.axiomrc`
- Status bar width incorrectly used content width instead of full terminal width

### Optimized
- `abAppend` now uses a doubling strategy reducing `realloc` calls from O(n) to O(log n) per frame
- `editorUpdateLinenumWidth` exits early when digit count hasn't changed
- `editorDrawTabBar` no longer saves buffer state on every frame
- Syntax highlighting is now lazy - only propagates when multi-line comment state changes

## [0.1.2] - 2025-05-XX

### Added
- GitHub Actions release pipeline with cross-platform builds
- Docker workflow publishing to Docker Hub
- Smoke tests for Linux and macOS binaries
- GitHub Pages deployment workflow

## [0.1.1] - 2025-05-10

### Added
- CI release pipeline setup

## [0.1.0] - 2025-05-10

### Added
- Initial release
- Core editor functionality based on raw terminal I/O
- Syntax highlighting for C and C++
- Incremental search with match highlighting
- `make install` / `make uninstall`
