# Contributing to Axiom

Thanks for taking the time to contribute. This document covers everything you need to get started.

---

## Table of Contents

- [Reporting Bugs](#reporting-bugs)
- [Suggesting Features](#suggesting-features)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Pull Request Guidelines](#pull-request-guidelines)
- [Code Style](#code-style)
- [Commit Messages](#commit-messages)

---

## Reporting Bugs

Before opening a bug report, check the [existing issues](https://github.com/ovenpickled/axiom/issues) to avoid duplicates.

When filing a bug, include:

- Your OS and terminal emulator (e.g. Arch Linux, Alacritty)
- The Axiom version (`axiom --version`)
- Steps to reproduce
- What you expected to happen
- What actually happened
- A screenshot or terminal output if relevant

Use the **Bug Report** issue template when available.

---

## Suggesting Features

Open a [GitHub Issue](https://github.com/ovenpickled/axiom/issues) with the label `enhancement`. Describe:

- What you want Axiom to do
- Why it would be useful
- Any prior art (how other editors handle it)

Features that add external dependencies will not be accepted. Axiom is deliberately dependency-free and that constraint is non-negotiable.

---

## Development Setup

You need a C99-compatible compiler (`gcc` or `clang`) and `make`. No other tools required.

```bash
git clone https://github.com/ovenpickled/axiom.git
cd axiom
make
./axiom yourfile.c
```

To build with a specific version string:

```bash
make VERSION=0.2.0
```

To install system-wide:

```bash
sudo make install
```

To uninstall:

```bash
sudo make uninstall
```

---

## Making Changes

Follow this workflow for every change, no matter how small:

```bash
# 1. Make sure main is up to date
git checkout main
git pull

# 2. Create a branch
git checkout -b fix/your-bug       # for bug fixes
git checkout -b feature/your-idea  # for new features
git checkout -b docs/your-change   # for documentation

# 3. Make your changes, then commit
git add axiom.c
git commit -m "fix: describe what you fixed"

# 4. Push and open a PR
git push origin fix/your-bug
```

Never commit directly to `main`.

---

## Pull Request Guidelines

- One PR per change. Don't bundle unrelated fixes.
- Link the relevant issue in the PR description using `Closes #123`.
- Make sure `make` compiles cleanly with no warnings before opening a PR.
- If your PR changes behaviour visible to the user, update the man page (`axiom.1`) and README accordingly.
- Keep PRs small and focused. Large PRs are harder to review and slower to merge.

---

## Code Style

Axiom is written in C99. The entire codebase lives in a single file (`axiom.c`) organized into clearly labelled sections. Keep it that way.

**Formatting:**
- 2-space indentation
- No trailing whitespace
- Opening braces on the same line as the statement
- No line longer than 100 characters where avoidable

**Memory:**
- Every `malloc` and `realloc` result must be checked or guarded
- Every allocation must have a corresponding `free` path
- Don't introduce memory leaks — the editor runs for long sessions

**Naming:**
- Functions: `editorCamelCase`
- Structs: `camelCase` or `UPPER_CASE` for defines
- Keep names descriptive but concise

**No external dependencies.** If your feature requires linking against an external library, it will not be merged.

---

## Commit Messages

Use the conventional commit format:

```
type: short description in imperative mood
```

Types:
- `feat:` — new feature
- `fix:` — bug fix
- `docs:` — documentation only
- `refactor:` — code restructure without behaviour change
- `ci:` — CI/CD changes
- `chore:` — maintenance, version bumps

Examples:
```
feat: add syntax highlighting for Zig
fix: restore ~ chars below welcome banner
docs: update keybindings in man page
refactor: optimize abAppend with doubling strategy
```

---

## Questions

If you're unsure about anything before opening a PR, open an issue and ask. It's better to align early than to do a lot of work in the wrong direction.
