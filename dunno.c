#pragma leco tool
// Not using C standard headers to play almost like a quine. In theory, we can
// make the compiler eventually generate a code like this, whilst making the
// language expressive enough for the task.

typedef long size_t;

typedef enum token_t {
  t_nil,
  t_idkw,
  t_ignore,
  t_punct,
  t_string,
} token_t;

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
token_t g_tok_t;
int g_line = 1;
int g_col = 0;

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

void write_num(int n) {
  if (n > 10) write_num(n / 10);
  char b = '0' + (n % 10);
  write(2, &b, 1);
}
[[noreturn]] void fail(const char * msg) {
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
  return c == '{' || c == '}' || c == '(' || c == ')' || c == ';' || c == '*' || c == '/';
}

token_t eof() {
  g_tok[0] = 0;
  return g_tok_t = t_nil;
}
token_t ignore() {
  g_tok[0] = 0;
  return g_tok_t = t_ignore;
}

token_t take_id_or_kw_token() {
  char * c = g_tok;
  while (g_took) {
    *c++ = g_took;
    if (!is_idkw(peek_char())) break;
    take_char();
  }
  *c = 0;
  g_tok_t = t_idkw;
  return t_idkw;
}

token_t take_string() {
  char * c = g_tok;
  while (take_char() != '"' && g_took) {
    // TODO: actual escaping;
    if (g_took == '\\') take_char();
    *c++ = g_took;
  }
  if (!g_took) fail("unclosed string");

  *c = 0;
  g_tok_t = t_string;
  return t_string;
}

token_t take_space() {
  while (is_space(peek_char()) && g_took) take_char();
  return ignore();
}

token_t take_punct() {
  g_tok[0] = g_took;
  g_tok[1] = 0;
  g_tok_t = t_punct;
  return t_punct;
}

token_t take_comment() {
  while (take_char() != '\n') {}
  return ignore();
}

token_t take_raw_token() {
  if (take_char() == 0) return eof();
  if (g_took == '/' && peek_char() == '/') return take_comment();
  if (is_idkw_start(g_took)) return take_id_or_kw_token();
  if (is_space(g_took)) return take_space();
  if (g_took == ';') return ignore();
  if (is_punct(g_took)) return take_punct();
  if (g_took == '"') return take_string();

  fail("invalid token");
}
token_t take_token() {
  while (take_raw_token() == t_ignore) {}
  return g_tok_t;
}

int check(token_t t, const char * txt) {
  if (t != g_tok_t) return 0;
  return eq(txt, g_tok);
}
int take(token_t t, const char * txt) {
  take_token();
  return check(t, txt);
}

void check_type() {
  if (g_tok_t != t_idkw) fail("expecting a type");

  if (eq(g_tok, "any")) return;
  if (eq(g_tok, "int")) return;
  if (eq(g_tok, "void")) fail("void is only valid as a return type");

  fail("unknown type");
}
void check_ret_type() {
  if (g_tok_t != t_idkw) fail("expecting a type");

  if (eq(g_tok, "any")) return;
  if (eq(g_tok, "int")) return;
  if (eq(g_tok, "void")) return;

  fail("unknown return type");
}

int d_statement() {
  if (check(t_punct, "{")) {
    while (!take(t_punct, "}")) {
      if (!d_statement()) return 0;
    }
    return 1;
  }

  if (g_tok_t != t_idkw) fail("statement should start with an identifier");

  if (take(t_punct, "(")) {
    while (!take(t_punct, ")")) {
    }
  }

  fail("here");
}

int d_fn() {
  take_token(); check_ret_type();

  if (take_token() != t_idkw) fail("expecting function name");
  if (!take(t_punct, "(")) fail("expecting left parenthesis");

  while (!take(t_punct, ")")) {
    check_type();
  }

  return 1;
}
int d_extern() {
  if (take(t_idkw, "fn")) return d_fn();
  fail("we can only declare extern of functions");
}
int d_top_level() {
  take_token();
  if (!g_tok_t) return 0;

  if (g_tok_t == t_idkw && eq(g_tok, "extern")) return d_extern();
  if (g_tok_t == t_idkw && eq(g_tok, "fn")) {
    d_fn();
    take_token();
    d_statement();
  }

  fail("invalid top-level element");
}

int main() {
  g_fd = open(g_file, 0);
  if (g_fd < 0) fail("file not found");

  while (d_top_level()) {}

  close(g_fd);
}
