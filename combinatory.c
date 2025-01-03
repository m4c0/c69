#pragma leco tool
// Not using C standard headers to play almost like a quine. In theory, we can
// make the compiler eventually generate a code like this, whilst making the
// language expressive enough for the task.

// TODO: find a C way of the equivalent of C++'s decltype(sizeof(0))
#include <stddef.h>  // required because of size_t

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
  if (n > 10) write_num(n / 10);
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
int term(char c, int j) {
  if (j >= g_len) return -1;
  if (g_buf[j] == c) return j + 1;
  return -1;
}
int alt(int (*a)(int), int (*b)(int), int j) {
  int aa = a(j);
  int bb = b(j);
  if (aa == -1) return bb;
  if (bb == -1) return aa;
  fail(j, "grammar is ambiguos about this");
}
int seq(int (*a)(int), int (*b)(int), int j) {
  int aa = a(j);
  return aa == -1 ? -1 : b(aa);
}

int statement(int j) {
  fail(j, "tbd");
}

int module(int j) {
  return seq(statement, module, j);
}

////////////////////////////////////////////////////////////////////////

int main(int argc, char ** argv) {
  if (argc != 2) usage(argv[0]);

  slurp(argv[1]);

  return module(0) == -1 ? 1 : 0;
}
