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
  tt_comma,
  tt_err,
  tt_ident,
  tt_lbracket,
  tt_lparen,
  tt_rbracket,
  tt_rparen,
  tt_semicolon,
  tt_string,
} tok_type_t;
const char * tok_names[] = {
  [tt_nil]       = "nil",
  [tt_comma]     = "comma",
  [tt_err]       = "err",
  [tt_ident]     = "ident",
  [tt_lparen]    = "lparen",
  [tt_lbracket]  = "lbracket",
  [tt_rparen]    = "rparen",
  [tt_rbracket]  = "rbracket",
  [tt_semicolon] = "semicolon",
  [tt_string]    = "string",
};

typedef struct tok_t {
  tok_type_t type;
  const char * pos;
  unsigned len;
} tok_t;
tok_t g_toks[102400] = {};

int cc_alpha(char c) { c |= 0x20; return c >= 'a' && c <= 'z'; }
int cc_digit(char c) { return c >= '0' && c <= '9'; }
int cc_ident(char c) { return cc_alpha(c) || cc_digit(c) || c == '_'; }
int cc_ident_start(char c) { return cc_alpha(c) || c == '_'; }
int cc_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

const char * g_f = g_file_buf;
tok_t * g_t = g_toks;

void t_comment() {
  while (*g_f && *g_f != '\n') g_f++;
}

void t_err(const char * f, const char * msg) {
  warn(f, msg);
  *g_t++ = (tok_t) {
    .type = tt_err,
    .pos = f,
    .len = g_f - f,
  };
}

void t_identifier() {
  const char * start = g_f;
  while (cc_ident(*g_f)) g_f++;
  *g_t++ = (tok_t) {
    .type = tt_ident,
    .pos = start,
    .len = g_f - start,
  };
}

void t_punct(tok_type_t tt) {
  *g_t++ = (tok_t) {
    .type = tt,
    .pos = g_f,
    .len = 1,
  };
  g_f++;
}

void t_space() { while (cc_space(*g_f)) g_f++; }

void t_string() {
  const char * start = g_f;
  while (*++g_f != '"') {
    if (!*g_f) return t_err(start, "end-of-file before closing string");
    if (*g_f == '\n') return t_err(start, "end-of-line before closing string");
    if (*g_f == '\\') {
      switch (g_f[1]) {
        case '\\':
        case 'n':
        case '"':
          break;
        default:
          return t_err(g_f, "invalid escape code");
      }
      g_f++;
    }
  }
  g_f++;
  *g_t++ = (tok_t) {
    .type = tt_string,
    .pos = start,
    .len = g_f - start,
  };
}

void t_next() {
  if (cc_ident_start(*g_f)) return t_identifier();
  if (cc_space(*g_f)) return t_space();
  if (*g_f == '/' && g_f[1] == '/') return t_comment();
  if (*g_f == '"') return t_string();
  if (*g_f == '(') return t_punct(tt_lparen);
  if (*g_f == ')') return t_punct(tt_rparen);
  if (*g_f == '{') return t_punct(tt_lbracket);
  if (*g_f == '}') return t_punct(tt_rbracket);
  if (*g_f == ';') return t_punct(tt_semicolon);
  if (*g_f == ',') return t_punct(tt_comma);

  t_err(g_f++, "invalid character");
}

void tokenise() { while (*g_f) t_next(); }

/////////////////////////////////////////////////////////////

typedef enum var_type_t {
  vt_nil,
  vt_int,
  vt_size_t,
  vt_void,
} var_type_t;
typedef struct ast_t {
  const char * pos;
} ast_t;
ast_t g_asts[102400];
ast_t * g_a = g_asts;

void restore(const char * msg) {
  warn(g_t->pos, msg);
  while (g_t->type && g_t->type != tt_semicolon) g_t++;
  g_t++;
}

int take_ident(const char * txt) {
  if (g_t->type != tt_ident) return 0;
  if (g_t->len != len(txt)) return 0;
  for (int i = 0; i < g_t->len; i++) {
    if (txt[i] != g_t->pos[i]) return 0;
  }
  g_t++;
  return 1;
}
var_type_t take_var_type() {
  if (take_ident("int"))    return vt_int;
  if (take_ident("size_t")) return vt_size_t;
  return vt_nil;
}
var_type_t take_ret_type() {
  if (take_ident("void")) return vt_void;
  return take_var_type();
}

void a_fn_def() {
  var_type_t ret_type = take_ret_type();
  if (!ret_type) return restore("invalid return type");

  if (g_t->type != tt_ident) return restore("invalid export name");
  tok_t name = *g_t++;

  if (g_t->type != tt_lparen) return restore("expecting left-parenthesis");
  g_t++;

  while (g_t && g_t->type != tt_rparen) {
    // TODO: support for parameters
    g_t++;
  }
  if (g_t->type != tt_rparen) return restore("eof expecting right-parenthesis");
  g_t++;
}

void a_extern() {
  if (!take_ident("fn")) return restore("only 'extern fn' is supported at the moment");

  a_fn_def();

  if (g_t->type != tt_semicolon) return restore("expecting semi-colon");
  g_t++;
}

void a_block() {
  // TODO: support for single-line blocks
  if (g_t->type != tt_lbracket) return restore("expecting left bracket");
  g_t++;

  while (g_t && g_t->type != tt_rbracket) {
    // TODO: support for statements
    g_t++;
  }
  if (g_t->type != tt_rbracket) return restore("eof expecting right bracket");
  g_t++;
}
void a_fn() {
  a_fn_def();
  a_block();
}

void a_top_level() {
  if (take_ident("extern")) return a_extern();
  if (take_ident("fn")) return a_fn();
  // TODO: support for statements

  restore("invalid top-level element");
}

void astify() {
  g_t = g_toks;
  while (g_t->type) a_top_level();
}

/////////////////////////////////////////////////////////////

int main(int argc, char ** argv) {
  if (argc != 2) usage(argv[0]);
  slurp(argv[1]);
  tokenise();
  astify();

  // TODO: build the AST
  for (ast_t * t = g_asts; t != g_a; t++) {
    write_str("\n");
  }
}
