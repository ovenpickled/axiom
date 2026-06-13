/*** includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>

/*** defines ***/
#ifndef AXIOM_VERSION
#define AXIOM_VERSION "dev"
#endif

#define AXIOM_TAB_STOP 8
#define AXIOM_QUIT_TIMES 3
#define AXIOM_SCROLL_SPEED 3
#define MAX_BUFFERS 10

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
  MOUSE_SCROLL_UP,
  MOUSE_SCROLL_DOWN,
  CTRL_ARROW_LEFT,
  CTRL_ARROW_RIGHT
};

enum editorHighlight {
  HL_NORMAL = 0,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_SELECTION
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

/*** data ***/
struct editorSyntax {
  char *filetype;
  char **filematch;
  char **keywords;
  char *singleline_comment_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
  int flags;
};

typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
} erow;

typedef struct {
  int cx, cy, rx;
  int rowoff, coloff;
  int numrows;
  int linenum_width;
  erow *row;
  int dirty;
  char *filename;
  struct editorSyntax *syntax;
} editorBuffer;

struct editorConfig {
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int base_screencols;
  int numrows;
  int linenum_width;
  int tab_stop;
  int scroll_speed;
  int quit_times;
  int quit_times_reset;
  int sel_active;
  int sel_anchor_x;
  int sel_anchor_y;

  editorBuffer bufs[MAX_BUFFERS];
  int cur_buf;
  int num_bufs;

  int key_new_buffer;
  int key_close_buffer;
  int key_next_buffer;
  int key_prev_buffer;

  erow *row;
  int dirty;
  char *filename;
  char statusmsg[120];
  char *clipboard;
  int clipboard_len;
  time_t statusmsg_time;
  struct editorSyntax *syntax;
  struct termios orig_termios;
};

struct editorConfig E;

/*** filetypes ***/
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };
char *C_HL_keywords[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case",

  "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
  "void|", NULL
};

/*** Python ***/
char *PYTHON_HL_extensions[] = { ".py", NULL };
char *PYTHON_HL_keywords[] = {
  "and", "as", "assert", "break", "class", "continue", "def", "del",
  "elif", "else", "except", "finally", "for", "from", "global", "if",
  "import", "in", "is", "lambda", "nonlocal", "not", "or", "pass",
  "raise", "return", "try", "while", "with", "yield",

  "bool|", "bytes|", "dict|", "float|", "int|", "list|", "None|",
  "set|", "str|", "tuple|", NULL
};

/*** Go ***/
char *GO_HL_extensions[] = { ".go", NULL };
char *GO_HL_keywords[] = {
  "break", "case", "chan", "const", "continue", "default", "defer",
  "else", "fallthrough", "for", "func", "go", "goto", "if", "import",
  "interface", "map", "package", "range", "return", "select", "struct",
  "switch", "type", "var",

  "bool|", "byte|", "complex64|", "complex128|", "error|", "float32|",
  "float64|", "int|", "int8|", "int16|", "int32|", "int64|", "rune|",
  "string|", "uint|", "uint8|", "uint16|", "uint32|", "uint64|",
  "uintptr|", NULL
};

/*** Rust ***/
char *RUST_HL_extensions[] = { ".rs", NULL };
char *RUST_HL_keywords[] = {
  "as", "break", "const", "continue", "crate", "else", "enum", "extern",
  "false", "fn", "for", "if", "impl", "in", "let", "loop", "match",
  "mod", "move", "mut", "pub", "ref", "return", "self", "Self", "static",
  "struct", "super", "trait", "true", "type", "unsafe", "use", "where",
  "while",

  "bool|", "char|", "f32|", "f64|", "i8|", "i16|", "i32|", "i64|",
  "i128|", "isize|", "str|", "u8|", "u16|", "u32|", "u64|", "u128|",
  "usize|", "String|", "Vec|", NULL
};

struct editorSyntax HLDB[] = {
  {
    "c",
    C_HL_extensions,
    C_HL_keywords,
    "//", "/*", "*/",
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
  },
  {
    "python",
    PYTHON_HL_extensions,
    PYTHON_HL_keywords,
    "#", NULL, NULL,
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
  },
  {
    "go",
    GO_HL_extensions,
    GO_HL_keywords,
    "//", "/*", "*/",
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
  },
  {
    "rust",
    RUST_HL_extensions,
    RUST_HL_keywords,
    "//", "/*", "*/",
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
  },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** append buffer ***/
struct abuf {
  char *b;
  int len;
  int cap;
};

#define ABUF_INIT {NULL, 0, 0}

/*** prototypes ***/
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void editorUpdateLinenumWidth();
void editorCopy();
void editorPaste();
void loadConfig();
void editorSaveCurrentBuffer();
void editorLoadBuffer(int idx);
void editorSwitchBuffer(int idx);
void editorNewBuffer();
void editorCloseBuffer();
int parseKey(const char *keystr);
void editorDrawTabBar(struct abuf *ab);
int editorUpdateSyntax(erow *row);
void editorPropagateHighlight(int start_row);

/*** terminal ***/
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[?1000l", 8);
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void handleSignal(int sig) {
  (void)sig;
  write(STDOUT_FILENO, "\x1b[?1000l", 8);
  exit(0);
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
  write(STDOUT_FILENO, "\x1b[?1000h", 8);
}

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        } else if (seq[2] == ';') {
          char mod, key;
          if (read(STDIN_FILENO, &mod, 1) != 1) return '\x1b';
          if (read(STDIN_FILENO, &key, 1) != 1) return '\x1b';
          if (mod == '5') {
            switch (key) {
              case 'C': return CTRL_ARROW_RIGHT;
              case 'D': return CTRL_ARROW_LEFT;
            }
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
          case 'M': {
            char mouse[3];
            if (read(STDIN_FILENO, &mouse[0], 1) != 1) return '\x1b';
            if (read(STDIN_FILENO, &mouse[1], 1) != 1) return '\x1b';
            if (read(STDIN_FILENO, &mouse[2], 1) != 1) return '\x1b';
            int button = mouse[0] - 32;
            if (button == 64) return MOUSE_SCROLL_UP;
            if (button == 65) return MOUSE_SCROLL_DOWN;
            return '\x1b';
          }
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** syntax highlighting ***/
int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

int editorUpdateSyntax(erow *row) {
  row -> hl = realloc(row -> hl, row -> rsize);
  memset(row -> hl, HL_NORMAL, row -> rsize);

  if (E.syntax == NULL) return 0;

  char **keywords = E.syntax -> keywords;

  char *scs = E.syntax -> singleline_comment_start;
  char *mcs = E.syntax -> multiline_comment_start;
  char *mce = E.syntax -> multiline_comment_end;

  int scs_len = scs ? strlen(scs) : 0;
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;

  int prev_sep = 1;
  int in_string = 0;
  int in_comment = (row -> idx > 0 && E.row[row -> idx - 1].hl_open_comment);

  int i = 0;
  while (i < row -> rsize) {
    char c = row -> render[i];
    unsigned char prev_hl = (i > 0) ? row -> hl[i - 1] : HL_NORMAL;

    if (scs_len && !in_string && !in_comment) {
      if (!strncmp(&row -> render[i], scs, scs_len)) {
        memset(&row -> hl[i], HL_COMMENT, row -> rsize - i);
        break;
      }
    }

    if (mcs_len && mce_len && !in_string) {
      if (in_comment) {
        row -> hl[i] = HL_MLCOMMENT;
        if (!strncmp(&row -> render[i], mce, mce_len)) {
          memset(&row -> hl[i], HL_MLCOMMENT, mce_len);
          i += mce_len;
          in_comment = 0;
          prev_sep = 1;
          continue;
        } else {
          i++;
          continue;
        }
      } else if (!strncmp(&row -> render[i], mcs, mcs_len)) {
        memset(&row -> hl[i], HL_MLCOMMENT, mcs_len);
        i += mcs_len;
        in_comment = 1;
        continue;
      }
    }

    if (E.syntax -> flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        row -> hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row -> rsize) {
          row -> hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == in_string) in_string = 0;
        i++;
        prev_sep = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row -> hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }

    if (E.syntax -> flags & HL_HIGHLIGHT_NUMBERS) {
      if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER)) {
        row -> hl[i] = HL_NUMBER;
        i++;
        prev_sep = 0;
        continue;
      }
    }

    if (prev_sep) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if (kw2) klen--;

        if (!strncmp(&row -> render[i], keywords[j], klen) && is_separator(row -> render[i + klen])) {
          memset(&row -> hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen;
          break;
        }
      }
      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue;
      }
    }

    prev_sep = is_separator(c);
    i++;
  }

  int old_open = row -> hl_open_comment;
  row -> hl_open_comment = in_comment;
  return (row -> hl_open_comment != old_open);
}

void editorPropagateHighlight(int start_row) {
  int i = start_row;
  while (i < E.numrows) {
    int prev_open = E.row[i].hl_open_comment;
    editorUpdateSyntax(&E.row[i]);
    if (E.row[i].hl_open_comment == prev_open) break;
    i++;
  }
}

int editorSyntaxToColour(int hl) {
  switch (hl) {
    case HL_SELECTION: return 7;
    case HL_COMMENT:
    case HL_MLCOMMENT: return 36;
    case HL_KEYWORD1: return 33;
    case HL_KEYWORD2: return 32;
    case HL_STRING: return 35;
    case HL_NUMBER: return 31;
    case HL_MATCH: return 34;
    default: return 37;
  }
}

void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL) return;

  char *ext = strrchr(E.filename, '.');

  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    struct editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s -> filematch[i]) {
      int is_ext = (s -> filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s -> filematch[i])) || (!is_ext && strstr(E.filename, s -> filematch[i]))) {
        E.syntax = s;

        int filerow;
        for (filerow = 0; filerow < E.numrows; filerow++) {
          editorUpdateSyntax(&E.row[filerow]);
        }
        return;
      }
      i++;
    }
  }
}

/*** row operations ***/
int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row -> chars[j] == '\t')
      rx += (E.tab_stop - 1) - (rx % E.tab_stop);
    rx++;
  }
  return rx;
}

int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;
  for (cx = 0; cx < row -> size; cx++) {
    if (row -> chars[cx] == '\t')
      cur_rx += (E.tab_stop - 1) - (cur_rx % E.tab_stop);
    cur_rx++;

    if (cur_rx > rx) return cx;
  }
  return cx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row -> size; j++)
    if (row -> chars[j] == '\t') tabs++;

  free(row -> render);
  row -> render = malloc(row -> size + tabs * (E.tab_stop - 1) + 1);

  int idx = 0;
  for (j = 0; j < row -> size; j++) {
    if (row -> chars[j] == '\t') {
      row -> render[idx++] = ' ';
      while (idx % E.tab_stop != 0) row -> render[idx++] = ' ';
    }else {
      row -> render[idx++] = row -> chars[j];
    }
  }
  row -> render[idx] = '\0';
  row -> rsize = idx;

  int changed = editorUpdateSyntax(row);
  if (changed) editorPropagateHighlight(row -> idx + 1);
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;

  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
  for (int j = at + 1; j <= E.numrows; j++) E.row[j].idx++;

  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
  editorUpdateLinenumWidth();
}

void editorFreeRow(erow *row) {
  free(row -> render);
  free(row -> chars);
  free(row -> hl);
}

void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  for (int j = at; j < E.numrows - 1; j++) E.row[j].idx--;
  E.numrows--;
  E.dirty++;
  editorUpdateLinenumWidth();
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row -> size) at = row -> size;
  row -> chars = realloc(row -> chars, row -> size + 2);
  memmove(&row -> chars[at + 1], &row -> chars[at], row -> size - at + 1);
  row -> size++;
  row -> chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row -> chars = realloc(row -> chars, row -> size + len + 1);
  memcpy(&row -> chars[row -> size], s, len);
  row -> size += len;
  row -> chars[row -> size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row -> size) return;
  memmove(&row -> chars[at], &row -> chars[at + 1], row -> size - at);
  row -> size--;
  editorUpdateRow(row);
  E.dirty++;
}

/*** editor operations ***/
void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorInsertNewLine() {
  erow *row = (E.cy < E.numrows) ? &E.row[E.cy] : NULL;

  int indent = 0;
  if (row) {
    while (indent < row -> size && row -> chars[indent] == ' ')
      indent++;
  }

  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else if (row) {
    editorInsertRow(E.cy + 1, &row -> chars[E.cx], row -> size - E.cx);
    row = &E.row[E.cy];
    row -> size = E.cx;
    row -> chars[row -> size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = 0;

  if (indent > 0) {
    erow *newrow = &E.row[E.cy];
    newrow -> chars = realloc(newrow -> chars, newrow -> size + indent + 1);
    memmove(&newrow -> chars[indent], newrow -> chars, newrow -> size);
    memset(newrow -> chars, ' ', indent);
    newrow -> size += indent;
    newrow -> chars[newrow -> size] = '\0';
    editorUpdateRow(newrow);
    E.cx = indent;
  }

  if (E.cy < E.numrows)
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
}

void editorDelChar() {
  if (E.cy == E.numrows) return;
  if (E.cx == 0 && E.cy == 0) return;

  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row -> chars, row -> size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

void editorCopy() {
  if (!E.sel_active) {
    if (E.cy >= E.numrows) return;
    erow *row = &E.row[E.cy];
    free(E.clipboard);
    E.clipboard = malloc(row->size + 1);
    memcpy(E.clipboard, row->chars, row->size);
    E.clipboard[row->size] = '\0';
    E.clipboard_len = row->size;
    editorSetStatusMessage("Line copied");
    return;
  }

  int start_y = E.sel_anchor_y;
  int end_y = E.cy;

  if (start_y > end_y) {
    int tmp = start_y;
    start_y = end_y;
    end_y = tmp;
  }

  int total = 0;
  for (int y = start_y; y <= end_y; y++) {
    if (y >= E.numrows) break;
    total += E.row[y].size;
    if (y < end_y) total++;
  }

  free(E.clipboard);
  E.clipboard = malloc(total + 1);
  E.clipboard_len = total;

  int pos = 0;
  for (int y = start_y; y <= end_y; y++) {
    if (y >= E.numrows) break;
    erow *row = &E.row[y];
    memcpy(&E.clipboard[pos], row->chars, row->size);
    pos += row->size;
    if (y < end_y) E.clipboard[pos++] = '\n';
  }
  E.clipboard[pos] = '\0';

  E.sel_active = 0;
  editorSetStatusMessage("Copied %d chars", total);
}

void editorPaste() {
  if (E.clipboard == NULL) {
    editorSetStatusMessage("Nothing to paste");
    return;
  }

  char *p = E.clipboard;
  char *end = E.clipboard + E.clipboard_len;
  int first = 1;

  while (p <= end) {
    char *nl = memchr(p, '\n', end - p);
    int chunk_len = nl ? (nl - p) : (end - p);

    if (first) {
      for (int i = 0; i < chunk_len; i++) {
        editorInsertChar(p[i]);
      }
      first = 0;
    } else {
      editorInsertRow(E.cy + 1, "", 0);
      E.cy++;
      E.cx = 0;
      for (int i = 0; i < chunk_len; i++) {
        editorInsertChar(p[i]);
      }
    }

    if (!nl) break;
    p = nl + 1;
  }

  E.coloff = 0;
  editorSetStatusMessage("Pasted");
}

/*** buffers ***/
void editorSaveCurrentBuffer() {
  editorBuffer *b = &E.bufs[E.cur_buf];
  b->cx = E.cx; b->cy = E.cy; b->rx = E.rx;
  b->rowoff = E.rowoff; b->coloff = E.coloff;
  b->numrows = E.numrows; b->row = E.row;
  b->dirty = E.dirty; b->filename = E.filename;
  b->syntax = E.syntax; b->linenum_width = E.linenum_width;
}

void editorLoadBuffer(int idx) {
  editorBuffer *b = &E.bufs[idx];
  E.cx = b->cx; E.cy = b->cy; E.rx = b->rx;
  E.rowoff = b->rowoff; E.coloff = b->coloff;
  E.numrows = b->numrows; E.row = b->row;
  E.dirty = b->dirty; E.filename = b->filename;
  E.syntax = b->syntax; E.linenum_width = b->linenum_width;
  E.screencols = E.base_screencols - E.linenum_width;
}

void editorSwitchBuffer(int idx) {
  if (idx < 0 || idx >= E.num_bufs) return;
  editorSaveCurrentBuffer();
  E.cur_buf = idx;
  editorLoadBuffer(idx);
  editorSetStatusMessage("Buffer %d/%d: %s", E.cur_buf + 1, E.num_bufs,
    E.filename ? E.filename : "[No Name]");
}

void editorNewBuffer() {
  if (E.num_bufs >= MAX_BUFFERS) {
    editorSetStatusMessage("Maximum buffers reached (%d)", MAX_BUFFERS);
    return;
  }
  editorSaveCurrentBuffer();
  int idx = E.num_bufs++;
  editorBuffer *b = &E.bufs[idx];
  b->cx = 0; b->cy = 0; b->rx = 0;
  b->rowoff = 0; b->coloff = 0;
  b->numrows = 0; b->row = NULL;
  b->dirty = 0;
  b->filename = NULL;
  b->syntax = NULL;
  b->linenum_width = 3;
  E.cur_buf = idx;
  editorLoadBuffer(idx);
  E.dirty = 0;
  editorSetStatusMessage("New buffer %d/%d", E.cur_buf + 1, E.num_bufs);
}

void editorCloseBuffer() {
  if (E.num_bufs == 1) {
    editorSetStatusMessage("Can't close last buffer -- use Ctrl-Q to quit");
    return;
  }
  int effectively_empty = (E.numrows == 0 ||
                          (E.numrows == 1 && E.row[0].size == 0));

  if (E.dirty && !effectively_empty) {
    editorSetStatusMessage("Buffer has unsaved changes -- save first with Ctrl-S");
    return;
  }

  for (int i = 0; i < E.numrows; i++)
    editorFreeRow(&E.row[i]);
  free(E.row);
  free(E.filename);

  for (int i = E.cur_buf; i < E.num_bufs - 1; i++)
    E.bufs[i] = E.bufs[i + 1];
  E.num_bufs--;
  memset(&E.bufs[E.num_bufs], 0, sizeof(editorBuffer));

  if (E.cur_buf >= E.num_bufs) E.cur_buf = E.num_bufs - 1;
  editorLoadBuffer(E.cur_buf);
  editorSetStatusMessage("Buffer closed -- now on buffer %d/%d: %s",
    E.cur_buf + 1, E.num_bufs, E.filename ? E.filename : "[No Name]");
}

/*** file i/o ***/
char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;
  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;
  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  editorSelectSyntaxHighlight();

  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' || 
                           line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
  editorSaveCurrentBuffer();
}

void editorSave() {
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
    editorSelectSyntaxHighlight();
  }

  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSaveCurrentBuffer();
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** find ***/
void editorFindCallback(char *query, int key) {
  static int last_match = -1;
  static int direction = 1;

  static int saved_hl_line;
  static char *saved_hl = NULL;

  if (saved_hl) {
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1) direction = 1;
  int current = last_match;
  int i;
  for (i = 0; i < E.numrows; i++) {
    current += direction;
    if (current == -1) current = E.numrows - 1;
    else if (current == E.numrows) current = 0;

    erow *row = &E.row[current];
    char *match = strstr(row -> render, query);
    if (match) {
      last_match = current;
      E.cy = current;
      E.cx = editorRowRxToCx(row, match - row -> render);
      E.rowoff = E.numrows;

      saved_hl_line = current;
      saved_hl = malloc(row -> rsize);
      memcpy(saved_hl, row -> hl, row -> rsize);
      memset(&row -> hl[match - row -> render], HL_MATCH, strlen(query));
      break;
    }
  }
}

void editorFind() {
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);
  if (query) {
    free(query);
  } else {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
}

void abAppend(struct abuf *ab, const char *s, int len) {
  if (ab -> len + len > ab -> cap) {
    int new_cap = (ab -> cap == 0) ? 4096 : ab -> cap * 2;
    while (new_cap < ab -> len + len) new_cap *= 2;
    char *new_b = realloc(ab -> b, new_cap);
    if (new_b == NULL) return;
    ab -> b = new_b;
    ab -> cap = new_cap;
  }
  memcpy(&ab -> b[ab -> len], s, len);
  ab -> len += len;
}

void abFree(struct abuf *ab) {
  free(ab -> b);
}

/*** output ***/
void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorUpdateLinenumWidth() {
  int digits = 0;
  int n = (E.numrows > 0) ? E.numrows : 1;
  while (n) { digits++; n /= 10; }
  int new_width = digits + 2;

  if (new_width == E.linenum_width) return;

  E.linenum_width = new_width;
  E.screencols = E.base_screencols - E.linenum_width;
}

int editorPosInSelection(int row, int col) {
  if (!E.sel_active) return 0;

  int start_y = E.sel_anchor_y;
  int end_y = E.cy;

  if (start_y > end_y) {
    int tmp = start_y;
    start_y = end_y;
    end_y = tmp;
  }

  if (row < start_y || row > end_y) return 0;
  if (row < E.numrows && col >= E.row[row].size) return 0;

  return 1;
}

void editorDrawTabBar(struct abuf *ab) {
  abAppend(ab, "\x1b[0m", 4);

  int fullwidth = E.screencols + E.linenum_width;
  int written = 0;

  for (int i = 0; i < E.num_bufs; i++) {
    char tab[64];
    const char *name = E.bufs[i].filename ? E.bufs[i].filename : "[No Name]";

    if (i == E.cur_buf && E.filename)
      name = E.filename;

    const char *slash = strrchr(name, '/');
    if (slash) name = slash + 1;

    char shortname[21];
    snprintf(shortname, sizeof(shortname), "%.20s", name);

    int is_current = (i == E.cur_buf);

    int numrows = (i == E.cur_buf) ? E.numrows : E.bufs[i].numrows;
    erow *row0   = (i == E.cur_buf) ? E.row     : E.bufs[i].row;
    int dirty    = (i == E.cur_buf) ? E.dirty    : E.bufs[i].dirty;

    int effectively_empty = (numrows == 0 ||
                            (numrows == 1 &&
                             row0 != NULL &&
                             row0[0].size == 0));
    int is_dirty = dirty && !effectively_empty;

    int tablen;
    if (is_current) {
      abAppend(ab, "\x1b[0m", 4);
      abAppend(ab, "\x1b[7m", 4);
      tablen = snprintf(tab, sizeof(tab), " [%d] %s%s ",
                        i + 1, shortname, is_dirty ? " \xe2\x97\x8f" : "");
    } else {
      abAppend(ab, "\x1b[0m", 4);
      abAppend(ab, "\x1b[2m", 4);
      tablen = snprintf(tab, sizeof(tab), " [%d] %s%s ",
                        i + 1, shortname, is_dirty ? " \xe2\x97\x8f" : "");
    }

    if (written + tablen > fullwidth) break;
    abAppend(ab, tab, tablen);
    written += tablen;
  }

  abAppend(ab, "\x1b[0m", 4);
  while (written < fullwidth) {
    abAppend(ab, " ", 1);
    written++;
  }
  abAppend(ab, "\x1b[0m", 4);
  abAppend(ab, "\r\n", 2);
}

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        const char *banner[] = {
          " \u2588\u2588\u2588\u2588\u2588\u2557 \u2588\u2588\u2557  \u2588\u2588\u2557\u2588\u2588\u2551 \u2588\u2588\u2588\u2588\u2588\u2588\u2557 \u2588\u2588\u2588\u2557   \u2588\u2588\u2588\u2557",
          "\u2588\u2588\u2554\u2550\u2550\u2588\u2588\u2557\u255a\u2588\u2588\u2557\u2588\u2588\u2554\u255d\u2588\u2588\u2551\u2588\u2588\u2554\u2550\u2550\u2550\u2588\u2588\u2557\u2588\u2588\u2588\u2588\u2557 \u2588\u2588\u2588\u2588\u2551",
          "\u2588\u2588\u2588\u2588\u2588\u2588\u2588\u2551 \u255a\u2588\u2588\u2588\u2554\u255d \u2588\u2588\u2551\u2588\u2588\u2551   \u2588\u2588\u2551\u2588\u2588\u2554\u2588\u2588\u2588\u2588\u2554\u2588\u2588\u2551",
          "\u2588\u2588\u2554\u2550\u2550\u2588\u2588\u2551 \u2588\u2588\u2554\u2588\u2588\u2557 \u2588\u2588\u2551\u2588\u2588\u2551   \u2588\u2588\u2551\u2588\u2588\u2551\u255a\u2588\u2588\u2554\u255d\u2588\u2588\u2551",
          "\u2588\u2588\u2551  \u2588\u2588\u2551\u2588\u2588\u2554\u255d \u2588\u2588\u2557\u2588\u2588\u2551\u255a\u2588\u2588\u2588\u2588\u2588\u2588\u2554\u255d\u2588\u2588\u2551 \u255a\u2550\u255d \u2588\u2588\u2551",
          "\u255a\u2550\u255d  \u255a\u2550\u255d\u255a\u2550\u255d  \u255a\u2550\u255d\u255a\u2550\u255d \u255a\u2550\u2550\u2550\u2550\u2550\u255d \u255a\u2550\u255d     \u255a\u2550\u255d",
          "",
          "     v" AXIOM_VERSION " -- A minimal terminal text editor     ",
        };
        int num_lines = 8;
        int fullwidth = E.screencols + E.linenum_width;

        for (int b = 0; b < num_lines && (y + b) < E.screenrows; b++) {
          const char *bline = banner[b];
          int bytelen = strlen(bline);

          int visuallen = 0;
          for (int i = 0; i < bytelen; ) {
            unsigned char ch = (unsigned char)bline[i];
            if (ch >= 0xe0) { visuallen++; i += 3; }
            else { visuallen++; i++; }
          }

          int padding = (fullwidth - visuallen) / 2;
          if (padding < 0) padding = 0;

          for (int p = 0; p < padding; p++)
            abAppend(ab, " ", 1);

          if (b < 6)
            abAppend(ab, "\x1b[35m", 5);
          else
            abAppend(ab, "\x1b[90m", 5);

          abAppend(ab, bline, bytelen);
          abAppend(ab, "\x1b[m", 3);

          if (b < num_lines - 1) {
            abAppend(ab, "\x1b[K", 3);
            abAppend(ab, "\r\n", 2);
          }
        }
        y += num_lines - 1;
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      char linenum[16];
      int digits = E.linenum_width - 2;
      int linelen = snprintf(linenum, sizeof(linenum), " %*d ", digits, filerow + 1);
      abAppend(ab, "\x1b[90m", 5);
      abAppend(ab, linenum, linelen);
      abAppend(ab, "\x1b[39m", 5);

      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols) len = E.screencols;
      char *c = &E.row[filerow].render[E.coloff];
      unsigned char *hl = &E.row[filerow].hl[E.coloff];
      int current_colour = -1;
      int j;
      for (j = 0; j < len; j++) {
        int actual_col = j + E.coloff;
        int in_sel = editorPosInSelection(filerow, actual_col);

        if (iscntrl(c[j])) {
          char sym = (c[j] <= 26) ? '@' + c[j] : '?';
          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &sym, 1);
          abAppend(ab, "\x1b[m", 3);
          if (current_colour != -1) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_colour);
            abAppend(ab, buf, clen);
          }
        } else if (in_sel) {
          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &c[j], 1);
          abAppend(ab, "\x1b[m", 3);
          current_colour = -1;
        } else if (hl[j] == HL_NORMAL) {
          if (current_colour != -1) {
            abAppend(ab, "\x1b[39m", 5);
            current_colour = -1;
          }
          abAppend(ab, &c[j], 1);
        } else {
          int colour = editorSyntaxToColour(hl[j]);
          if (colour != current_colour) {
            current_colour = colour;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", colour);
            abAppend(ab, buf, clen);
          }
          abAppend(ab, &c[j], 1);
        }
      }
      abAppend(ab, "\x1b[39m", 5);
    }

    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  int fullwidth = E.screencols + E.linenum_width;
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "[%d/%d] %.20s - %d lines %s",
  E.cur_buf + 1, E.num_bufs,
  E.filename ? E.filename : "[No Name]",
  E.numrows, E.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax ? E.syntax -> filetype : "no ft", E.cy + 1, E.numrows);
  if (len > fullwidth) len = fullwidth;
  abAppend(ab, status, len);
  while (len < fullwidth) {
    if (fullwidth - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols + E.linenum_width) msglen = E.screencols + E.linenum_width;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawTabBar(&ab);
  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1 + 1, (E.rx - E.coloff) + 1 + E.linenum_width);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** input ***/
char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback) callback(buf, c);
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback) callback(buf, c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
    if (callback) callback(buf, c);
  }
}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row -> size) {
        E.cx++;
      } else if (row && E.cx == row -> size) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows) {
        E.cy++;
      }
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row -> size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorProcessKeypress() {
  int c = editorReadKey();

  if (c == E.key_new_buffer) {
    editorNewBuffer();
    return;
  }
  if (c == E.key_close_buffer) {
    editorCloseBuffer();
    return;
  }
  if (c == E.key_next_buffer) {
    editorSwitchBuffer((E.cur_buf + 1) % E.num_bufs);
    return;
  }
  if (c == E.key_prev_buffer) {
    editorSwitchBuffer((E.cur_buf - 1 + E.num_bufs) % E.num_bufs);
    return;
  }

  switch (c) {
    case '\r':
      editorInsertNewLine();
      break;

    case CTRL_KEY('q'):
      {
        editorSaveCurrentBuffer();

        int any_dirty = 0;
        for (int i = 0; i < E.num_bufs; i++) {
          int empty = (E.bufs[i].numrows == 0 ||
              (E.bufs[i].numrows == 1 &&
               E.bufs[i].row != NULL &&
               E.bufs[i].row[0].size == 0));
          if (E.bufs[i].dirty && !empty) { any_dirty = 1; break; }
        }

        if (any_dirty && E.quit_times > 0) {
          editorSetStatusMessage("WARNING!!! Unsaved changes in one or more buffers. "
                                 "Press Ctrl-Q %d more times to quit.", E.quit_times);
          E.quit_times--;
          return;
        }
        free(E.clipboard);
        write(STDOUT_FILENO, "\x1b[?1000l", 8);
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
      }
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;

    case CTRL_KEY('f'):
      editorFind();
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      if (c == DEL_KEY) {
        editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
      } else {
        if (E.cx > 0 && E.cy < E.numrows) {
          erow *row = &E.row[E.cy];
          int spaces = E.cx % E.tab_stop;
          if (spaces == 0) spaces = E.tab_stop;
          int can_delete = 1;
          for (int i = E.cx - spaces; i < E.cx; i++) {
            if (row->chars[i] != ' ') { can_delete = 0; break; }
          }
          if (can_delete) {
            while (spaces--) editorDelChar();
          } else {
            editorDelChar();
          }
        } else {
          editorDelChar();
        }
      }
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows) E.cy = E.numrows;
        }
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

    case MOUSE_SCROLL_UP:
      {
        int times = E.scroll_speed;
        while (times--) editorMoveCursor(ARROW_UP);
      }
      return;

    case MOUSE_SCROLL_DOWN:
      {
        int times = E.scroll_speed;
        while (times--) editorMoveCursor(ARROW_DOWN);
      }
      return;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case CTRL_KEY('k'):
      if (E.sel_active) {
        E.sel_active = 0;
        editorSetStatusMessage("Selection cancelled");
      } else {
        E.sel_active = 1;
        E.sel_anchor_x = 0;
        E.sel_anchor_y = E.cy;
        editorSetStatusMessage("Selection started -- move cursor to select");
      }
      return;

    case '\x1b':
    case CTRL_KEY('l'):
      E.sel_active = 0;
      return;

    case '\t':
      {
        int spaces = E.tab_stop - (E.cx % E.tab_stop);
        while (spaces--)
          editorInsertChar(' ');
      }
      break;

    case CTRL_KEY('c'):
      editorCopy();
      break;

    case CTRL_KEY('v'):
      editorPaste();
      break;

    default:
      editorInsertChar(c);
      break;
  }
  E.quit_times = E.quit_times_reset;
}

/*** config ***/
int parseKey(const char *keystr) {
  if (strncmp(keystr, "ctrl-", 5) == 0) {
    const char *rest = keystr + 5;
    if (strlen(rest) == 1 && rest[0] >= 'a' && rest[0] <= 'z')
      return CTRL_KEY(rest[0]);
    if (strcmp(rest, "right") == 0) return CTRL_ARROW_RIGHT;
    if (strcmp(rest, "left") == 0) return CTRL_ARROW_LEFT;
  }
  return -1;
}

void loadConfig() {
  char *home = getenv("HOME");
  if (!home) return;

  char path[256];
  snprintf(path, sizeof(path), "%s/.axiomrc", home);

  FILE *fp = fopen(path, "r");
  if (!fp) return;

  char line[256];
  while (fgets(line, sizeof(line), fp)) {
    int len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

    if (line[0] == '#' || line[0] == '\0') continue;

    char key[64], value[64];
    if (sscanf(line, "%63s = %63s", key, value) != 2) continue;

    int val = atoi(value);

    if (strcmp(key, "tab_stop") == 0) {
      if (val > 0) E.tab_stop = val;
    } else if (strcmp(key, "quit_times") == 0) {
      if (val > 0) { E.quit_times = val; E.quit_times_reset = val; }
    } else if (strcmp(key, "scroll_speed") == 0) {
      if (val > 0) E.scroll_speed = val;
    } else if (strcmp(key, "key_new_buffer") == 0) {
      int k = parseKey(value);
      if (k != -1) E.key_new_buffer = k;
    } else if (strcmp(key, "key_close_buffer") == 0) {
      int k = parseKey(value);
      if (k != -1) E.key_close_buffer = k;
    } else if (strcmp(key, "key_next_buffer") == 0) {
      int k = parseKey(value);
      if (k != -1) E.key_next_buffer = k;
    } else if (strcmp(key, "key_prev_buffer") == 0) {
      int k = parseKey(value);
      if (k != -1) E.key_prev_buffer = k;
    }
  }
  fclose(fp);
}

/*** init ***/
void initEditor() {
  signal(SIGTERM, handleSignal);
  signal(SIGHUP, handleSignal);
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.syntax = NULL;
  E.clipboard = NULL;
  E.clipboard_len = 0;
  E.tab_stop = AXIOM_TAB_STOP;
  E.quit_times = AXIOM_QUIT_TIMES;
  E.quit_times_reset = AXIOM_QUIT_TIMES;
  E.scroll_speed = AXIOM_SCROLL_SPEED;
  E.sel_active = 0;
  E.sel_anchor_x = 0;
  E.sel_anchor_y = 0;
  E.cur_buf = 0;
  E.num_bufs = 1;
  memset(E.bufs, 0, sizeof(E.bufs));
  E.key_new_buffer = CTRL_KEY('t');
  E.key_close_buffer = CTRL_KEY('w');
  E.key_next_buffer = CTRL_ARROW_RIGHT;
  E.key_prev_buffer = CTRL_ARROW_LEFT;
  E.base_screencols = 0;
  E.linenum_width = 0;

  if (getWindowSize(&E.screenrows, &E.base_screencols) == -1) die("getWindowSize");
  E.screenrows -= 3;
  E.screencols = E.base_screencols;
  editorUpdateLinenumWidth();
  loadConfig();
}

int main(int argc, char *argv[]) {
  if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
    printf("axiom %s\n", AXIOM_VERSION);
    return 0;
  }

  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find | Ctrl-K = select | Ctrl-C = copy | Ctrl-V = paste | Ctrl-T = new buffer | Ctrl-W = close buffer");

  while(1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
