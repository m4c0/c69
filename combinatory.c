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

const char * g_file = "dummy.c69";
char g_buf[102400];
int g_len;

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
void warn(int j, const char * msg) {
  int line = 1;
  int col = 1;

  write_str(g_file);
  if (j >= 0) {
    for (int i = 0; i < j; i++) {
      if (g_buf[i] == '\n') {
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
[[noreturn]] void fail(int j, const char * msg) {
  warn(j, msg);
  exit(1);
}

void usage(const char * argv0) {
  write_str("usage: ");
  write_str(argv0);
  write_str(" <file.c69>");
  exit(1);
}
void slurp(const char * file) {
  g_file = file;
  int fd = open(g_file, 0);
  if (fd < 0) fail(-1, "file not found");

  g_len = read(fd, g_buf, sizeof(g_buf) - 1);
  if (g_len <= 0) fail(0, "failed to read file");
  g_buf[g_len] = 0;

  close(fd);
}

////////////////////////////////////////////////////////////////////////
// https://en.wikipedia.org/wiki/Parser_combinator
////////////////////////////////////////////////////////////////////////

typedef enum type {
  a_err,
} type_t;
typedef struct ast {
  int j;
  int start_j;
  type_t type;
  const char * str;
} ast_t;
typedef ast_t (*parser_t)(int j);

ast_t err(int j, const char * msg) {
  return (ast_t){ .j = -1, .start_j = j, .type = a_err, .str = msg };
}

ast_t empty(int j) { return (ast_t) { j }; }
ast_t term(int j, char c) {
  if (j >= g_len) return err(j, "unexpected eof");
  if (g_buf[j] == c) return (ast_t) { .j = j + 1 };
  return err(j, "mismatched char");
}

ast_t seq(int j, parser_t * p) {
  ast_t r = { j };
  while (*p && r.j >= 0) r = (*p++)(r.j);
  return r;
}

////////////////////////////////////////////////////////////////////////

ast_t run(parser_t p, const char * code) {
  char * b = g_buf;
  while (*code) *b++ = *code++;
  g_len = b - g_buf;

  return p(0);
}

ast_t c_i(int j) { return term(j, 'i'); }
ast_t c_n(int j) { return term(j, 'n'); }
ast_t c_t(int j) { return term(j, 't'); }

ast_t test(int j) {
  return seq(j, (parser_t[]) { c_i, c_n, c_t, 0 });
}

int main() {
  ast_t res = run(test, "i64");
  if (res.j == -1) warn(res.start_j, res.str);
  else warn(res.j, "done here");
  return res.j >= 0;
}
