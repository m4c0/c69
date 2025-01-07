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

typedef enum link {
  l_nil,
  l_c,
} link_t;
typedef enum type {
  a_nil,
  a_err,
  a_int,
  a_size_t,
  a_void,
} type_t;
typedef struct var {
  type_t type;
  const char * name;
  struct var * next;
} var_t;
typedef struct stm {
} stmt_t;
typedef struct ast {
  int j;
  int start_j;
  type_t type;
  const char * str;
  var_t * var;
  stmt_t * stmt;
  link_t linkage;
} ast_t;
typedef ast_t (*parser_t)(int j);

char g_ast_buf[102400] = { 0 };
char * g_ast_ptr = g_ast_buf;

var_t g_var_buf[102400] = { 0 };
var_t * g_var_ptr = g_var_buf;

stmt_t g_stmt_buf[102400] = { };
stmt_t * g_stmt_ptr = g_stmt_buf;

const char * type_name(type_t t) {
  switch (t) {
    case a_err:    return "err";
    case a_int:    return "int";
    case a_nil:    return "nil";
    case a_size_t: return "size_t";
    case a_void:   return "void";
  }
}

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
ast_t seq(int j, ast_t * rb, parser_t * p) {
  ast_t r = { j };
  while (*p && r.j >= 0) {
    r = (*p++)(r.j);
    if (rb) *rb++ = r;
  }
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
int cc_comma(char c) { return c == ','; }
int cc_digit(char c) { return c >= '0' && c <= '9'; }
int cc_eol(char c) { return c == 0 || c == '\n'; }
int cc_ident(char c) { return cc_alpha(c) || cc_digit(c) || c == '_'; }
int cc_ident_start(char c) { return cc_alpha(c) || c == '_'; }
int cc_lbracket(char c) { return c == '{'; }
int cc_lparen(char c) { return c == '('; }
int cc_non_eol(char c) { return !cc_eol(c); }
int cc_non_ident(char c) { return !cc_ident(c); }
int cc_rbracket(char c) { return c == '}'; }
int cc_rparen(char c) { return c == ')'; }
int cc_semicolon(char c) { return c == ';'; }
int cc_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

ast_t c_comma    (int j) { return term(j, cc_comma    ); }
ast_t c_lbracket (int j) { return term(j, cc_lbracket ); }
ast_t c_lparen   (int j) { return term(j, cc_lparen   ); }
ast_t c_rbracket (int j) { return term(j, cc_rbracket ); }
ast_t c_rparen   (int j) { return term(j, cc_rparen   ); }
ast_t c_semicolon(int j) { return term(j, cc_semicolon); }

ast_t str(int j, const char * str, type_t t) {
  ast_t res = {
    .start_j = j,
    .type = t,
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
ast_t ident(int j, const char * s, type_t t) {
  ast_t res = str(j, s, t);
  if (cc_ident(g_buf[res.j])) return err(j, "mismatched identifier");
  res.type = t;
  return res;
}
ast_t i_extern(int j) { return ident(j, "extern", 0); }
ast_t i_fn    (int j) { return ident(j, "fn",     0); }
ast_t i_int   (int j) { return ident(j, "int",    a_int   ); }
ast_t i_size_t(int j) { return ident(j, "size_t", a_size_t); }
ast_t i_void  (int j) { return ident(j, "void",   a_void  ); }

ast_t space(int j) { return term(j, cc_space); }
ast_t comment_start(int j) { return str(j, "//", 0); }
ast_t until_eol(int j) { return whilst(j, cc_non_eol); }
ast_t comment(int j) { return seq(j, 0, (parser_t[]) { comment_start, until_eol, 0 }); }
ast_t space_or_comment(int j) { return alt(j, (parser_t[]) { comment, space, 0 }); }
ast_t sp(int j) {
  while (1) {
    ast_t res = space_or_comment(j);
    if (res.j == -1) return empty(j);
    j = res.j;
  }
}

ast_t comma    (int j) { return seq(j, 0, (parser_t[]) { sp, c_comma,      0 }); }
ast_t lbracket (int j) { return seq(j, 0, (parser_t[]) { sp, c_lbracket,   0 }); }
ast_t lparen   (int j) { return seq(j, 0, (parser_t[]) { sp, c_lparen,     0 }); }
ast_t rbracket (int j) { return seq(j, 0, (parser_t[]) { sp, c_rbracket,   0 }); }
ast_t rparen   (int j) { return seq(j, 0, (parser_t[]) { sp, c_rparen,     0 }); }
ast_t semicolon(int j) { return seq(j, 0, (parser_t[]) { sp, c_semicolon,  0 }); }

ast_t ret_type(int j) { return alt(j, (parser_t[]) { i_int, i_size_t, 0 }); }
ast_t var_type(int j) {
  ast_t r = alt(j, (parser_t[]) { i_int, i_size_t, 0 });
  if (r.j < 0) r.str = "invalid type";
  return r;
}
ast_t var_start(int j) { return term(j, cc_ident_start); }
ast_t var_rest(int j) { return whilst(j, cc_ident); }
ast_t var_name(int j) {
  ast_t a[4] = { 0 };
  ast_t res = seq(j, a, (parser_t[]) { sp, var_start, var_rest, sp, 0 });
  if (res.j < 0) return res;
  res.str = g_ast_ptr;
  for (int i = a[0].j; i < res.j; i++, g_ast_ptr++) *g_ast_ptr = g_buf[i];
  *g_ast_ptr++ = 0;
  return res;
}
ast_t var(int j) {
  ast_t a[3] = { 0 };
  ast_t res = seq(j, a, (parser_t[]) { sp, var_type, var_name, 0 });
  if (res.j >= 0) {
    res.type = a_nil;
    res.var = g_var_ptr++;
    res.var->name = a[2].str;
    res.var->type = a[1].type;
  } else if (a[1].j >= 0) {
    fail(a[1].j, "expecting variable name");
  }
  return res;
}
ast_t next_var(int j) { return seq(j, 0, (parser_t[]) { comma, var, 0 }); }

ast_t more_vars_or_end(int j);
ast_t more_vars(int j) {
  ast_t a[2] = { 0 };
  ast_t r = seq(j, a, (parser_t[]) { next_var, more_vars_or_end, 0 });
  if (r.j < 0) return r;
  r.var = a[0].var;
  r.var->next = a[1].var;
  return r;
}
ast_t more_vars_or_end(int j) { return alt(j, (parser_t[]) { more_vars, rparen, 0 }); }
ast_t first_var(int j) {
  ast_t a[2] = { 0 };
  ast_t r = seq(j, a, (parser_t[]) { var, more_vars_or_end, 0 });
  if (r.j < 0) return r;
  r.var = a[0].var;
  r.var->next = a[1].var;
  return r;
}
ast_t vars(int j) {
  ast_t r = alt(j, (parser_t[]) { first_var, rparen, 0 });
  if (r.j < 0) r.str = "expecting right-parenthesis or a valid type name";
  return r;
}

ast_t fn_signature(int j) {
  ast_t a[9] = { 0 };
  ast_t r = seq(j, a, (parser_t[]) { sp, i_fn, sp, ret_type, var_name, lparen, sp, vars, 0 });
  if (r.j >= 0) {
    r.type = a[3].type;
    r.str = a[4].str;
    r.var = a[7].var;
  }
  return r;
}

ast_t extern_fn(int j) {
  ast_t a[4] = { 0 };
  ast_t r = seq(j, a, (parser_t[]) { sp, i_extern, fn_signature, semicolon, 0 });
  if (r.j >= 0) {
    r.type = a[2].type;
    r.str = a[2].str;
    r.var = a[2].var;
    r.linkage = l_c;
  } else if (a[1].j >= 0) {
    fail(r.start_j, r.str);
  }
  return r;
}

ast_t block(int j) {
  ast_t a[2] = { 0 };
  ast_t r =  seq(j, a, (parser_t[]) { lbracket, rbracket, 0 });
  if (a[0].j >= 0 && a[1].j < 0) {
    fail(j, "bracket without a corresponding close");
  }
  return r;
}
ast_t stmt(int j) {
  ast_t res = alt(j, (parser_t[]) { block, semicolon, 0 });
  if (res.j <= 0) res.str = "expecting statement";
  return res;
}
ast_t fn(int j) {
  ast_t a[4] = { 0 };
  ast_t r = seq(j, a, (parser_t[]) { sp, fn_signature, stmt, 0 });
  if (r.j >= 0) {
    r.type = a[1].type;
    r.str = a[1].str;
    r.var = a[1].var;
    r.stmt = a[2].stmt;
  }
  return r;
}

ast_t top_level(int j) {
  return alt(j, (parser_t[]) { extern_fn, fn, stmt, 0 });
}

////////////////////////////////////////////////////////////////////////

ast_t run(parser_t p, const char * code) {
  char * b = g_buf;
  while (*code) *b++ = *code++;
  *b = 0;
  g_len = b - g_buf;

  return p(0);
}

ast_t test(int j) {
  return top_level(j);
}

int print_result(ast_t res) {
  if (res.j == -1) warn(res.start_j, res.str);
  else {
    warn(res.j, "done here");
    if (res.linkage) write_str("extern\n");
    if (res.type) { write_str("type: "); write_str(type_name(res.type)); write_str("\n"); }
    if (res.str) { write_str("value: "); write_str(res.str); write_str("\n");}

    var_t * v = res.var;
    while (v) {
      write_str("arg: ");
      write_str(type_name(v->type));
      write_str(" ");
      write_str(v->name);
      write_str("\n");
      v = v->next;
    }
  }
  return res.j >= 0;
}
int test_case(const char * txt) {
  ast_t res = run(test, txt);
  print_result(run(test, txt));
  return res.j >= 0;
}
int main(int argc, char ** argv) {
  if (argc == 2) {
    slurp(argv[1]);
    return print_result(test(0));
  }

  int a = test_case("fn int sum(int a, int b) {}\n");
  int b = test_case("extern fn int puts();\n");
  int c = test_case("{}\n ");
  return (a && b && c) ? 0 : 1;
}
