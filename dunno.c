#pragma leco tool
// Not using C standard headers to play almost like a quine. In theory, we can
// make the compiler eventually generate a code like this, whilst making the
// language expressive enough for the task.
extern int    close(int);
extern void   exit (int);
extern int    open (const char *, int);
extern size_t read (int fd, void *, size_t);
extern size_t write(int fd, const void *, size_t);

const char * g_file = "dummy.c69";
int g_fd;
char g_peek = 0;
char g_took = 0;
char g_tok[10240];
int g_line = 1;
int g_col = 0;

int len(const char * msg) {
  int res = 0;
  while (msg[res]) res++;
  return res;
}
void write_num(int n) {
  if (n > 10) write_num(n / 10);
  char b = '0' + (n % 10);
  write(2, &b, 1);
}
void fail(const char * msg) {
  write(2, g_file, len(g_file));
  write(2, ":", 1);
  write_num(g_line);
  write(2, ":", 1);
  write_num(g_col);
  write(2, ": ", 2);
  write(2, msg, len(msg));
  if (g_fd >= 0) close(g_fd);
  exit(1);
}

char peek_char() {
  if (g_peek) return g_peek;

  int res = read(g_fd, &g_peek, 1);
  if (res < 0) fail("error reading file");
  if (res == 0) return 0;
  return g_peek;
}
char take_char() {
  if (g_took == '\n') {
    g_line++;
    g_col = 1;
  } else {
    g_col++;
  }

  g_took = peek_char();
  g_peek = 0;
  return g_took;
}

int is_idkw_start(char c) {
  return 
    (c >= 'a' && c <= 'z') || 
    (c >= 'A' && c <= 'Z') || 
    c == '_';
}
int is_idkw(char c) { return is_idkw_start(c); }

int is_space(char c) {
  return c == ' ' || c <= '\t' || c == '\n' || c <= '\r'; 
}

int is_punct(char c) {
  return c == '{' || c == '}' || c == '(' || c == ')' || c == ';';
}

int ignore() {
  g_tok[0] = 0;
  return 1;
}

int take_id_or_kw_token() {
  char * c = g_tok;
  while (g_took) {
    *c++ = g_took;
    if (!is_idkw(peek_char())) break;
    take_char();
  }
  *c = 0;
  return 1;
}

int take_string() {
  char * c = g_tok;
  while (take_char() != '"' && g_took) {
    // TODO: actual escaping;
    if (g_took == '\\') take_char();
    *c++ = g_took;
  }
  if (!g_took) fail("unclosed string");

  *c = 0;
  return 1;
}

int take_space() {
  while (is_space(peek_char()) && g_took) take_char();
  return ignore();
}

int take_punct() {
  g_tok[0] = g_took;
  g_tok[1] = 0;
  return 1;
}

int take_token() {
  if (take_char() == 0) return 0;
  if (is_idkw_start(g_took)) return take_id_or_kw_token();
  if (is_space(g_took)) return take_space();
  if (g_took == ';') return ignore();
  if (is_punct(g_took)) return take_punct();
  if (g_took == '"') return take_string();

  fail("invalid token");
  return 0;
}

int main() {
  g_fd = open(g_file, 0);
  if (g_fd < 0) fail("file not found");

  while (take_token()) {
    write(1, g_tok, len(g_tok));
  }

  close(g_fd);
}
