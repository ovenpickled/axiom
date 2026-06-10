<div align="center">

```
 █████╗ ██╗  ██╗██╗ ██████╗ ███╗   ███╗
██╔══██╗╚██╗██╔╝██║██╔═══██╗████╗ ████║
███████║ ╚███╔╝ ██║██║   ██║██╔████╔██║
██╔══██║ ██╔██╗ ██║██║   ██║██║╚██╔╝██║
██║  ██║██╔╝ ██╗██║╚██████╔╝██║ ╚═╝ ██║
╚═╝  ╚═╝╚═╝  ╚═╝╚═╝ ╚═════╝ ╚═╝     ╚═╝
```

**A minimal terminal text editor. No dependencies. Just C.**

</div>

---

Axiom is a terminal text editor written in a single C file with no external dependencies. It runs entirely on raw terminal I/O using POSIX APIs - no ncurses, no Readline, no third-party libraries. If your system can compile C99, Axiom will build and run.

It supports syntax highlighting for C, Python, Go, and Rust; multiple open buffers with a tab bar; text selection, copy, and paste; incremental search with match highlighting; mouse scroll; line numbers; and a `~/.axiomrc` file for configuring tab width, scroll speed, and keybindings.

## Screenshots

> Screenshots coming soon.

<!-- Add screenshots:
![Axiom editing a C file](./screenshots/axiom-c-highlight.png)
![Axiom with multiple buffers](./screenshots/axiom-buffers.png)
-->

## Installation

Clone the repo and build with `make`:

```bash
git clone https://github.com/awwwyan/axiom.git
cd axiom
make
```

Then install to `/usr/local/bin`:

```bash
make install
```

To install to a custom location, pass a `PREFIX`:

```bash
make install PREFIX=~/.local
```

To uninstall:

```bash
make uninstall
# or, if you used a custom prefix:
make uninstall PREFIX=~/.local
```

To remove the compiled binary without uninstalling:

```bash
make clean
```

**Configuration** — Copy [`.axiomrc.example`](./.axiomrc.example) to `~/.axiomrc` and uncomment the options you want to change. Axiom uses its built-in defaults for anything not specified.

## Interesting Techniques

- **[Raw terminal mode](https://man7.org/linux/man-pages/man3/termios.3.html)** - Axiom puts the terminal into raw mode using `tcgetattr`/`tcsetattr` from `termios.h`, disabling canonical line buffering and echo so it can read input one byte at a time. The original terminal state is saved at startup and restored on exit via `atexit()`.

- **[ANSI/VT100 escape sequences](https://www.ecma-international.org/publications-and-standards/standards/ecma-48/)** for rendering - The entire screen is rendered using raw escape sequences for cursor movement, colour, and line-erasing. No ncurses. Sequences like `\x1b[2J` (clear screen), `\x1b[7m` (reverse video), and `\x1b[%dm` (foreground colour) are written directly to stdout.

- **Single-write rendering via append buffer** - All screen output is assembled into a `struct abuf` (a growable heap buffer using `realloc`) and flushed to stdout in a single `write()` call per frame. This eliminates the flicker that would occur from many individual writes.

- **[X10 mouse protocol](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)** - Mouse scroll events are enabled by writing `\x1b[?1000h` to the terminal and decoded from raw byte sequences in the escape sequence parser — no helper library involved.

- **[`ioctl(TIOCGWINSZ)`](https://man7.org/linux/man-pages/man4/tty_ioctl.4.html)** for terminal dimensions - Window size is queried directly via `ioctl`. When that fails or returns zero columns, Axiom falls back to moving the cursor to position 999,999 and querying its actual position with `\x1b[6n`.

- **Dual coordinate system** - Each row stores two representations: `chars` (raw character data) and `render` (display-ready, with tabs expanded to spaces). The editor maintains `cx`/`cy` for the logical cursor and `rx` for the rendered column, with bidirectional conversion via `editorRowCxToRx` / `editorRowRxToCx`.

- **Cascading multiline comment highlighting** - Each row tracks an `hl_open_comment` flag. When a row's open-comment state changes (e.g. a `/*` is typed), `editorUpdateSyntax` recursively updates the next row, keeping multiline comment highlighting correct as you edit.

- **[`getline()`](https://man7.org/linux/man-pages/man3/getline.3.html) for file reading** - Lines are read with POSIX `getline()`, which handles allocation automatically. The `_GNU_SOURCE` / `_DEFAULT_SOURCE` / `_BSD_SOURCE` defines at the top of [`axiom.c`](./axiom.c) unlock this and other POSIX extensions while compiling under `-std=c99`.

- **[`ftruncate()`](https://man7.org/linux/man-pages/man2/ftruncate.2.html) for saving** - Files are saved by opening with `O_RDWR | O_CREAT`, truncating to the exact new length with `ftruncate()`, then writing the full content. This avoids stale bytes at the end of a file if it gets shorter.

- **Auto-indentation** - When you press Enter, `editorInsertNewLine` counts the leading spaces on the current line and prepends them to the new row, keeping indentation in sync without any language server.

- **Smart backspace** - Backspace checks whether the characters to the left of the cursor are all spaces filling a complete tab stop. If so, it deletes the whole tab stop's worth in one keystroke.

- **[Signal handling](https://man7.org/linux/man-pages/man7/signal.7.html)** - `SIGTERM` and `SIGHUP` are caught so mouse reporting (`\x1b[?1000l`) is disabled before the process exits, leaving the terminal in a clean state.

- **Runtime-configurable keybindings** - `~/.axiomrc` is parsed at startup. Buffer navigation keys are stored as integers in `editorConfig` and compared directly against the key code from `editorReadKey()`, so there's no hardcoded keymap.

## Technologies and Libraries

Axiom uses only the C standard library and POSIX interfaces. There are no package dependencies, but a few of the headers and syscalls are worth knowing about if you're not already familiar with low-level terminal programming:

- **[`termios.h`](https://man7.org/linux/man-pages/man3/termios.3.html)** - POSIX terminal attributes API. Used to enter and exit raw mode.
- **[`sys/ioctl.h` + `TIOCGWINSZ`](https://man7.org/linux/man-pages/man4/tty_ioctl.4.html)** - Kernel interface for querying terminal window size.
- **[`fcntl.h`](https://man7.org/linux/man-pages/man2/fcntl.2.html)** - POSIX file control. Used with `O_RDWR | O_CREAT` and `ftruncate()` for saves.
- **[`signal.h`](https://man7.org/linux/man-pages/man7/signal.7.html)** - POSIX signal handling. `SIGTERM` and `SIGHUP` handlers clean up mouse mode before exit.
- **[`stdarg.h`](https://en.cppreference.com/w/c/variadic)** - C variadic arguments. Used in `editorSetStatusMessage` for `printf`-style format strings.
- **[Feature test macros](https://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html)** (`_DEFAULT_SOURCE`, `_BSD_SOURCE`, `_GNU_SOURCE`) - Required to expose POSIX/GNU extensions like `getline()` under `-std=c99`.

## Project Structure

```
ovenpickled-axiom/
├── axiom.c
├── Makefile
├── LICENSE
├── README.md
└── .axiomrc.example
```

The entire editor lives in [`axiom.c`](./axiom.c). [`.axiomrc.example`](./.axiomrc.example) is the reference config — copy it to `~/.axiomrc` to override defaults for tab width, scroll speed, and buffer keybindings.

The [`Makefile`](./Makefile) compiles with `-Wall -Wextra -pedantic -std=c99` and provides `install`, `uninstall`, and `clean` targets. The default install prefix is `/usr/local`.

## License

[MIT](./LICENSE)
