#pragma leco tool
// Avoiding C standard headers to play almost like a quine. In theory, we can
// make the compiler eventually generate a code like this, whilst making the
// language expressive enough for the task.

#include <stdarg.h>  // va_list, etc
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
// Assumes grammar to have no ambiguity
////////////////////////////////////////////////////////////////////////

int empty(int j) { return j; }
int term(int j, char c) {
  if (j >= g_len) return -1;
  if (g_buf[j] == c) return j + 1;
  return -1;
}
int alt(int j, ...) {
  int res = -1;
  int (*a)(int j) = 0;

  va_list ap;
  va_start(ap, j);

  while ((a = va_arg(ap, int (*)(int)))) {
    int aa = a(j);
    if (res != -1 && aa != -1) fail(j, "grammar is ambiguos about this");
    if (aa != -1) res = aa;
  }

  va_end(ap);
  return res;
}
int seq(int j, ...) {
  int (*a)(int j) = 0;

  va_list ap;
  va_start(ap, j);

  while (j >= 0 && (a = va_arg(ap, int (*)(int)))) j = a(j);

  va_end(ap);
  return j;
}

// TODO: is it possible to do this without a list of outcomes for alt/seq?
int one_or_more(int j, int (*a)(int)) {
  int vj = j = a(j);
  while (vj >= 0) {
    vj = a(j);
    if (vj >= 0) j = vj;
  }
  return j;
}
// TODO: is it possible to do this without a list of outcomes for alt/seq?
int zero_or_more(int j, int (*a)(int)) {
  // equivalent: one_or_more | empty
  int vj = one_or_more(j, a);
  return vj >= 0 ? vj : j;
}

int expect(int j, int (*f)(int), const char * msg) {
  int vj = f(j);
  if (vj == -1) fail(j, msg);
  return vj;
}
// TODO: remove this
int logme(int j) {
  write_num(j);
  return j;
}

int c_lparen   (int j) { return term(j,  '('); }
int c_newline  (int j) { return term(j, '\n'); }
int c_return   (int j) { return term(j, '\r'); }
int c_rparen   (int j) { return term(j,  ')'); }
int c_slash    (int j) { return term(j,  '/'); }
int c_space    (int j) { return term(j,  ' '); }
int c_semicolon(int j) { return term(j,  ';'); }
int c_tab      (int j) { return term(j, '\t'); }

int non_nl(int j) {
  if (j >= g_len) return -1;
  if (g_buf[j] != '\n') return j + 1;
  return -1;
}

int comment_0(int j) { return zero_or_more(j, non_nl); }
int comment(int j) { return seq(j, c_slash, c_slash, comment_0, c_newline, 0); }

int space(int j) { return alt(j, c_newline, c_return, c_space, c_tab, comment, 0); }
int spaces(int j) { return one_or_more(j, space); }
int opt_spaces(int j) { return zero_or_more(j, space); }

int kw(int j, const char * s) {
  while (j >= 0 && *s) j = term(j, *s++);
  return j;
}
int kw_any(int j)    { return kw(j, "any");    }
int kw_extern(int j) { return kw(j, "extern"); }
int kw_fn(int j)     { return kw(j, "fn");     }
int kw_int(int j)    { return kw(j, "int");    }
int kw_void(int j)   { return kw(j, "void");   }

int var_type(int j) { return alt(j, kw_any, kw_int, 0); }
int ret_type(int j) { return alt(j, var_type, kw_void, 0); }

int ident_start(int j) {
  // shortcut for alt(term(a), term(b), etc)
  if (j >= g_len) return -1;
  char c = g_buf[j];
  if (c == '_') return j + 1;
  if (c >= 'a' && c <= 'z') return j + 1;
  if (c >= 'A' && c <= 'Z') return j + 1;
  return -1;
}
int ident_mid(int j) {
  // shortcut for alt(term(a), term(b), etc)
  if (j >= g_len) return -1;
  char c = g_buf[j];
  if (c == '_') return j + 1;
  if (c >= '0' && c <= '9') return j + 1;
  if (c >= 'a' && c <= 'z') return j + 1;
  if (c >= 'A' && c <= 'Z') return j + 1;
  return -1;
}
int ident_chars(int j) { return one_or_more(j, ident_mid); }
int ident(int j) { return seq(j, ident_start, ident_chars, 0); }

int spc_lparen(int j) { return seq(j, spaces, c_lparen, 0); }
int exp_lparen(int j) { return alt(j, spc_lparen, c_lparen, 0); }
int lparen(int j) { return expect(j, exp_lparen, "expecting left parenthesis"); }

int spc_rparen(int j) { return seq(j, spaces, c_rparen, 0); }
int exp_rparen(int j) { return alt(j, spc_rparen, c_rparen, 0); }
int rparen(int j) { return expect(j, exp_rparen, "expecting right parenthesis"); }

int exp_semicolon(int j) { return seq(j, opt_spaces, c_semicolon, 0); }
int semicolon(int j) { return expect(j, exp_semicolon, "expecting semi-colon"); }

int arg_0(int j) { return seq(j, var_type, ident, 0); }
int arg(int j) { return alt(j, var_type, arg_0, 0); }
int args(int j) { return zero_or_more(j, arg); }

int fn_signature(int j) {
  return seq(j, kw_fn, spaces, ret_type, spaces, ident, lparen, args, rparen, semicolon, 0);
}

int stmt(int j) {
  return -1;
}
int fn(int j) {
  return -1;
}

int extern_after_kw(int j) {
  return expect(j, fn_signature, "only 'extern fn' supported at the moment");
}
int extern_fn(int j) {
  return seq(j, kw_extern, spaces, extern_after_kw, 0);
}

int toplevel_1(int j) { return alt(j, extern_fn, fn, stmt, 0); }
int toplevel_0(int j) { return expect(j, toplevel_1, "unknown top-level construct"); }
int toplevel(int j) { return seq(j, opt_spaces, toplevel_0, 0); }

int module(int j) {
  return seq(j, toplevel, module, 0);
}

////////////////////////////////////////////////////////////////////////

int main(int argc, char ** argv) {
  if (argc != 2) usage(argv[0]);

  slurp(argv[1]);

  return module(0) == -1 ? 1 : 0;
}
