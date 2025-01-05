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
  a_ident,
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

ast_t alt(int j, parser_t * p) {
  ast_t r = { -1 };
  while (*p) {
    ast_t r2 = (*p++)(j);
    if (r2.j >= 0 && r.j >= 0) fail(j, "grammar is ambiguous for this");
    if (r2.j >= 0) r = r2;
  }
  if (r.j == -1) return err(j, "no valid alternative");
  return r;
}
ast_t seq(int j, parser_t * p) {
  ast_t r = { j };
  while (*p && r.j >= 0) r = (*p++)(r.j);
  return r;
}

ast_t term(int j, int (*fn)(char)) {
  return fn(g_buf[j]) ? (ast_t) { j + 1, j } : err(j, "mismatched char");
}
ast_t whilst(int j, int (*fn)(char)) {
  ast_t res = { .start_j = j };
  while (fn(g_buf[j])) j++;
  res.j = j;
  return res;
}

int cc_alpha(char c) { c |= 0x20; return c >= 'a' && c <= 'z'; }
int cc_digit(char c) { return c >= '0' && c <= '9'; }
int cc_eol(char c) { return c == 0 || c == '\n'; }
int cc_ident(char c) { return cc_alpha(c) || cc_digit(c) || c == '_'; }
int cc_non_eol(char c) { return !cc_eol(c); }
int cc_non_ident(char c) { return !cc_ident(c); }
int cc_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

ast_t str(int j, const char * str) {
  ast_t res = {
    .start_j = j,
    .type = a_ident,
    .str = str,
  };
  while (g_buf[j] && *str && *str == g_buf[j]) {
    j++;
    str++;
  }
  if (*str) return err(res.start_j, "mismatched string");
  res.j = j;
  return res;
}
ast_t ident(int j, const char * s) {
  ast_t res = str(j, s);
  if (cc_ident(g_buf[res.j])) return err(j, "mismatched identifier");
  return res;
}
ast_t i_int (int j) { return ident(j, "int" ); }
ast_t i_void(int j) { return ident(j, "void"); }

ast_t space(int j) { return term(j, cc_space); }
ast_t comment_start(int j) { return str(j, "//"); }
ast_t until_eol(int j) { return whilst(j, cc_non_eol); }
ast_t comment(int j) { return seq(j, (parser_t[]) { comment_start, until_eol, 0 }); }
ast_t space_or_comment(int j) { return alt(j, (parser_t[]) { comment, space, 0 }); }
ast_t sp(int j) {
  while (1) {
    ast_t res = space_or_comment(j);
    if (res.j == -1) return empty(j);
    j = res.j;
  }
}

////////////////////////////////////////////////////////////////////////

ast_t run(parser_t p, const char * code) {
  char * b = g_buf;
  while (*code) *b++ = *code++;
  g_len = b - g_buf;

  return p(0);
}

ast_t test(int j) {
  return sp(j);
}

int main() {
  ast_t res = run(test, "i64");
  if (res.j == -1) warn(res.start_j, res.str);
  else warn(res.j, "done here");
  return res.j >= 0;
}
