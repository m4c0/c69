#pragma leco tool
// Avoiding C standard headers to play almost like a quine. In theory, we can
// make the compiler eventually generate a code like this, whilst making the
// language expressive enough for the task.

#include <stddef.h>  // size_t

extern int    close(int);
extern void   exit (int);
extern int    open (const char *, int);
extern size_t read (int fd, void *, size_t);
extern size_t write(int fd, const void *, size_t);

const char * g_file = 0;
char g_file_buf[102400];
int g_file_len = 0;

int len(const char * msg) {
  int res = 0;
  while (msg[res]) res++;
  return res;
}
int eq(const char * a, const char * b) {
  while (*a && *b) {
    if (*a != *b) return 0;
    a++;
    b++;
  }
  return *a == *b;
}

void write_str(const char * msg) {
  write(2, msg, len(msg));
}
void write_num(int n) {
  if (n < 0) {
    write_str("-");
    n = -n;
  }
  if (n >= 10) write_num(n / 10);
  char b = '0' + (n % 10);
  write(2, &b, 1);
}
void warn(const char * j, const char * msg) {
  int line = 1;
  int col = 1;

  write_str(g_file);
  if (j != 0) {
    for (char * i = g_file_buf; i < j; i++) {
      if (*i == '\n') {
        line++;
        col = 1;
      } else col++;
    }
    write_str(":");
    write_num(line);
    write_str(":");
    write_num(col);
  }
  write_str(": ");
  write_str(msg);
  write_str("\n");
}
[[noreturn]] void fail(const char * j, const char * msg) {
  warn(j, msg);
  exit(1);
}

void slurp(const char * file) {
  g_file = file;
  int fd = open(g_file, 0);
  if (fd < 0) fail(0, "file not found");

  g_file_len = read(fd, g_file_buf, sizeof(g_file_buf) - 1);
  if (g_file_len <= 0) fail(0, "failed to read file");
  g_file_buf[g_file_len] = 0;

  close(fd);
}
void usage(const char * argv0) {
  write_str("usage: ");
  write_str(argv0);
  write_str(" <file.c69>");
  exit(1);
}

/////////////////////////////////////////////////////////////

typedef enum tok_type_t {
  tt_nil,
  tt_ident,
} tok_type_t;
const char * tok_names[] = {
  [tt_nil] = "nil",
  [tt_ident] = "ident",
};

typedef struct tok_t {
  tok_type_t type;
  const char * str;
  unsigned len;
} tok_t;
tok_t g_toks[102400] = {};

int cc_alpha(char c) { c |= 0x20; return c >= 'a' && c <= 'z'; }
int cc_digit(char c) { return c >= '0' && c <= '9'; }
int cc_ident(char c) { return cc_alpha(c) || cc_digit(c) || c == '_'; }
int cc_ident_start(char c) { return cc_alpha(c) || c == '_'; }

const char * g_f = g_file_buf;
tok_t * g_t = g_toks;

void t_identifier() {
  const char * start = g_f;
  while (cc_ident(*g_f)) g_f++;
  *g_t++ = (tok_t) {
    .type = tt_ident,
    .str = start,
    .len = g_f - start,
  };
}

void t_next() {
  if (cc_ident_start(*g_f)) return t_identifier();

  warn(g_f, "invalid character");
  g_f++;
}

void tokenise() { while (*g_f) t_next(); }

/////////////////////////////////////////////////////////////

int main(int argc, char ** argv) {
  if (argc != 2) usage(argv[0]);
  slurp(argv[1]);
  tokenise();

  for (tok_t * t = g_toks; t != g_t; t++) {
    write_str(tok_names[t->type]);
    write_str(" ");
    if (t->str) write(2, t->str, t->len);
    write_str("\n");
  }
}
