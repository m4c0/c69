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
  tt_char,
  tt_comma,
  tt_div,
  tt_dot,
  tt_eq,
  tt_eqeq,
  tt_err,
  tt_gt,
  tt_gteq,
  tt_int,
  tt_ident,
  tt_lbracket,
  tt_lt,
  tt_lteq,
  tt_lparen,
  tt_lsqbr,
  tt_mod,
  tt_minus,
  tt_mult,
  tt_not,
  tt_noteq,
  tt_plus,
  tt_rbracket,
  tt_rparen,
  tt_rsqbr,
  tt_semicolon,
  tt_string,
} tok_type_t;

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

void t_err(const char * f, const char * msg) {
  warn(f, msg);
  *g_t++ = (tok_t) {
    .type = tt_err,
    .pos = f,
    .len = g_f - f,
  };
}

void t_char() {
  const char * start = g_f;
  g_f++;

  if (*g_f == '\\') {
    g_f++;
    switch (*g_f) {
      // TODO: other escape codes
      case 'n':
        break;
      default:
        return t_err(start, "invalid escape code");
    }
  }
  g_f++;

  if (*g_f != '\'') return t_err(start, "missing closing apostrophe");
  g_f++;

  *g_t++ = (tok_t) {
    .type = tt_char,
    .pos = start,
    .len = g_f - start,
  };
}

void t_comment() {
  while (*g_f && *g_f != '\n') g_f++;
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

void t_or_eq(tok_type_t ne, tok_type_t eq) {
  *g_t++ = (tok_t) {
    .type = g_f[1] == '=' ? eq : ne,
    .pos  = g_f,
    .len  = g_f[1] == '=' ? 2 : 1,
  };
  g_f += g_f[1] == '=' ? 2 : 1;
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

void t_int() {
  const char * start = g_f;
  while (cc_digit(*g_f)) g_f++;
  *g_t++ = (tok_t) {
    .type = tt_int,
    .pos = start,
    .len = g_f - start,
  };
}

void t_next() {
  if (cc_ident_start(*g_f)) return t_identifier();
  if (cc_space(*g_f)) return t_space();
  if (cc_digit(*g_f)) return t_int();
  if (*g_f == '\'') return t_char();
  if (*g_f == '<') return t_or_eq(tt_lt, tt_lteq);
  if (*g_f == '>') return t_or_eq(tt_gt, tt_gteq);
  if (*g_f == '/' && g_f[1] == '/') return t_comment();
  if (*g_f == '"') return t_string();
  if (*g_f == '!') return t_or_eq(tt_not, tt_noteq);
  if (*g_f == '=') return t_or_eq(tt_eq, tt_eqeq);
  if (*g_f == '+') return t_punct(tt_plus);
  if (*g_f == '-') return t_punct(tt_minus);
  if (*g_f == '*') return t_punct(tt_mult);
  if (*g_f == '/') return t_punct(tt_div);
  if (*g_f == '%') return t_punct(tt_mod);
  if (*g_f == '(') return t_punct(tt_lparen);
  if (*g_f == ')') return t_punct(tt_rparen);
  if (*g_f == '{') return t_punct(tt_lbracket);
  if (*g_f == '}') return t_punct(tt_rbracket);
  if (*g_f == '[') return t_punct(tt_lsqbr);
  if (*g_f == ']') return t_punct(tt_rsqbr);
  if (*g_f == ';') return t_punct(tt_semicolon);
  if (*g_f == '.') return t_punct(tt_dot);
  if (*g_f == ',') return t_punct(tt_comma);

  t_err(g_f++, "invalid character");
}

void tokenise() { while (*g_f) t_next(); }

/////////////////////////////////////////////////////////////

// TODO: operator precedence
typedef enum oper_type_t {
  ot_nil,
  ot_div,
  ot_eqeq,
  ot_gt,
  ot_gteq,
  ot_lt,
  ot_lteq,
  ot_minus,
  ot_mod,
  ot_mult,
  ot_noteq,
  ot_plus,
} oper_type_t;
const char * oper_type_names[] = {
  [ot_nil]   = "nil",
  [ot_div]   = "div",
  [ot_eqeq]  = "eqeq",
  [ot_gt]    = "gt",
  [ot_gteq]  = "gteq",
  [ot_lt]    = "lt",
  [ot_lteq]  = "lteq",
  [ot_minus] = "minus",
  [ot_mod]   = "mod",
  [ot_mult]  = "mult",
  [ot_noteq] = "noteq",
  [ot_plus]  = "plus",
};
typedef enum var_type_t {
  vt_nil,
  vt_any,
  vt_char,
  vt_int,
  vt_size_t,
  vt_void,
} var_type_t;
const char * var_type_names[] = {
  [vt_nil]    = "nil",
  [vt_any]    = "any",
  [vt_char]   = "char",
  [vt_int]    = "int",
  [vt_size_t] = "size_t",
  [vt_void]   = "void",
};
typedef enum ast_type_t {
  at_nil,
  at_assign,
  at_binop,
  at_call,
  at_char,
  at_decl,
  at_err,
  at_extern,
  at_if,
  at_index,
  at_int,
  at_fn,
  at_neg,
  at_str,
  at_var,
  at_while,
} ast_type_t;
const char * ast_type_names[] = {
  [at_nil]    = "nil",
  [at_assign] = "assign",
  [at_binop]  = "binop",
  [at_call]   = "call",
  [at_char]   = "char",
  [at_decl]   = "decl",
  [at_err]    = "err",
  [at_extern] = "extern",
  [at_if]     = "if",
  [at_index]  = "index",
  [at_int]    = "int",
  [at_fn]     = "fn",
  [at_neg]    = "neg",
  [at_str]    = "str",
  [at_var]    = "var",
  [at_while]  = "while",
};
typedef struct ast_t {
  ast_type_t     type;
  const char   * pos;
  oper_type_t    oper_type;
  var_type_t     var_type;
  int            var_arity; // -1 == unknown[], 0 == none, n = some[n]
  tok_t          var_name;
  struct ast_t * dot;
  struct ast_t * args;
  struct ast_t * body;
  struct ast_t * next;
} ast_t;
ast_t g_asts[102400];
ast_t * g_a = g_asts;

ast_t * restore(const char * msg) {
  *g_a = (ast_t) { at_err, g_t->pos };
  warn(g_t->pos, msg);
  while (g_t->type && g_t->type != tt_semicolon) g_t++;
  g_t++;
  return g_a++;
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
  if (take_ident("any"))    return vt_any;
  if (take_ident("int"))    return vt_int;
  if (take_ident("char"))   return vt_char;
  if (take_ident("size_t")) return vt_size_t;
  return vt_nil;
}
var_type_t take_ret_type() {
  if (take_ident("void")) return vt_void;
  return take_var_type();
}

int take_positive_int() {
  if (g_t->type != tt_int) return -1;

  int res = 0;
  for (int i = 0; i < g_t->len; i++) {
    res = res * 10 + (g_t->pos[i] - '0');
  }
  g_t++;
  return res;
}

ast_t * a_var_type() {
  ast_t * r = g_a++;

  r->var_type = take_var_type();
  if (!r->var_type) return r;

  if (g_t->type != tt_lsqbr) return r;
  g_t++;

  if (g_t->type == tt_rsqbr) {
    g_t++;
    r->var_arity = -1;
    return r;
  }

  r->var_arity = take_positive_int();
  if (r->var_arity <= 0) return restore("array sizes must be a positive integer or undefined");

  if (g_t->type != tt_rsqbr) return restore("expecting closing square bracket");
  g_t++;
  return r;
}

ast_t * a_stmt();

ast_t * as_fn() {
  ast_t r = { 0 };

  r.pos = g_t->pos;
  r.var_type = take_ret_type();
  if (!r.var_type) return restore("invalid return type");

  // TODO: use a "take_ident" to avoid fns with kw name
  if (g_t->type != tt_ident) return restore("invalid function name");
  r.var_name = *g_t++;

  if (g_t->type != tt_lparen) return restore("expecting left-parenthesis");
  g_t++;

  ast_t * an = 0;
  while (g_t) {
    if (g_t->type == tt_rparen) break;

    ast_t * aa = a_var_type();
    if (aa->type == at_err) return aa;
    if (!aa->var_type) return restore("invalid parameter type");

    if (g_t->type == tt_ident) aa->var_name = *g_t++;

    if (g_t->type == tt_comma) g_t++;
    else if (g_t->type != tt_rparen) {
      if (!aa->var_name.type) return restore("expecting identifier, comma or right-parenthesis");
      else return restore("expecting comma or right-parenthesis");
    }

    if (!an) r.args = an = aa;
    else { an->next = aa; an = an->next; }
  }
  if (g_t->type != tt_rparen) return restore("eof expecting right-parenthesis");
  g_t++;

  r.type = at_fn;
  r.body = a_stmt();
  *g_a = r;
  return g_a++;
}

ast_t * as_block() {
  // TODO: better restore 
  // TODO: support for scopes
  if (g_t->type != tt_lbracket) return restore("expecting left bracket");
  g_t++;

  ast_t * r = 0;
  ast_t * n = 0;
  while (g_t && g_t->type != tt_rbracket) {
    ast_t * s = a_stmt();
    if (!n) n = r = s;
    else { n->next = s; n = s; }
  }
  if (g_t->type != tt_rbracket) return restore("eof expecting right bracket");
  g_t++;

  if (!r) r = g_a++;
  return r;
}
ast_t * as_expr();
ast_t * as_call(tok_t id) {
  ast_t * args = 0;
  ast_t * n = 0;
  while (g_t) {
    if (g_t->type == tt_rparen) break;

    ast_t * a = as_expr();
    if (a->type == at_err) return a;
    if (!n) n = args = a;
    else { n->next = a; n = a; }
    if (g_t->type == tt_comma) {
      g_t++;
      continue;
    }
  }
  if (g_t->type != tt_rparen) return restore("eof expecting right parenthesis");
  g_t++;

  *g_a = (ast_t) {
    .type = at_call,
    .var_name = id,
    .args = args,
  };
  return g_a++;
}
ast_t * as_index(tok_t id) {
  ast_t * a = as_expr();
  if (a->type == at_err) return a;

  if (g_t->type != tt_rsqbr) return restore("expecting right square bracket");
  g_t++;

  *g_a = (ast_t) {
    .type = at_index,
    .var_name = id,
    .args = a,
  };
  return g_a++;
}
ast_t * as_assign(tok_t l) {
  ast_t * r = as_expr();
  if (r->type == at_err) return r;

  *g_a = (ast_t) {
    .type     = at_assign,
    .pos      = l.pos,
    .var_name = l,
    .body     = r,
  };
  return g_a++;
}
ast_t * as_from_ident() {
  tok_t id = *g_t++;

  if (g_t->type == tt_lsqbr) {
    g_t++;
    return as_index(id);
  }
  if (g_t->type == tt_lparen) {
    g_t++;
    return as_call(id);
  }
  if (g_t->type == tt_eq) {
    g_t++;
    return as_assign(id);
  }

  ast_t * dot = 0;
  if (g_t->type == tt_dot) {
    g_t++;

    tok_t * t = g_t;
    if (g_t->type != tt_ident) return restore("expecting identifier after dot");
    // TODO: use a "take_ident" instead of testing/warning like this
    if (take_ret_type()) {
      g_t = t;
      return restore("unexpected type name");
    }

    dot = as_from_ident();
  }

  *g_a = (ast_t) {
    .type     = at_var,
    .pos      = id.pos,
    .var_name = id,
    .dot      = dot,
  };
  return g_a++;
}
ast_t * as_char() {
  *g_a = (ast_t) {
    .type = at_char,
    .pos = g_t->pos,
    .var_name = *g_t,
  };
  g_t++;
  return g_a++;
}
ast_t * as_int() {
  *g_a = (ast_t) {
    .type = at_int,
    .pos = g_t->pos,
    .var_name = *g_t,
  };
  g_t++;
  return g_a++;
}
ast_t * as_str() {
  *g_a = (ast_t) {
    .type = at_str,
    .pos = g_t->pos,
    .var_name = *g_t,
  };
  g_t++;
  return g_a++;
}
ast_t * as_l_expr() {
  if (g_t->type == tt_char)   return as_char();
  if (g_t->type == tt_ident)  return as_from_ident();
  if (g_t->type == tt_int)    return as_int();
  if (g_t->type == tt_string) return as_str();
  return restore("invalid expression");
}
ast_t * as_expr() {
  if (g_t->type == tt_lparen) {
    g_t++;

    ast_t * res = as_expr();
    if (res->type == at_err) return res;

    if (g_t->type != tt_rparen) return restore("expecting right parenthesis");

    g_t++;
    return res;
  }
  if (g_t->type == tt_minus) {
    tok_t * t = g_t++;
    ast_t * l = as_expr();
    if (l->type == at_err) return l;
    *g_a = (ast_t) {
      .type      = at_neg,
      .pos       = t->pos,
      .args      = l,
    };
    return g_a++;
  }

  ast_t * l = as_l_expr();
  if (l->type == at_err) return l;

  oper_type_t ot = ot_nil;
  switch (g_t->type) {
    case tt_plus:  ot = ot_plus;  break;
    case tt_minus: ot = ot_minus; break;
    case tt_mult:  ot = ot_mult;  break;
    case tt_div:   ot = ot_div;   break;
    case tt_mod:   ot = ot_mod;   break;
    case tt_lt:    ot = ot_lt;    break;
    case tt_gt:    ot = ot_gt;    break;
    case tt_lteq:  ot = ot_lteq;  break;
    case tt_gteq:  ot = ot_gteq;  break;
    case tt_eqeq:  ot = ot_eqeq;  break;
    case tt_noteq: ot = ot_noteq; break;
    default: return l;
  }
  g_t++;

  ast_t * r = as_expr();
  if (r->type == at_err) return l;

  l->next = r;
  *g_a = (ast_t) {
    .type      = at_binop,
    .pos       = l->pos,
    .oper_type = ot,
    .args      = l,
  };
  return g_a++;
}
ast_t * as_empty() {
  g_a->pos = g_t++->pos;
  return g_a++;
}
ast_t * as_var(ast_t * a) {
  if (g_t->type != tt_ident) return restore("expecting identifier");

  tok_t * t = g_t;
  if (take_ret_type()) {
    g_t = t;
    return restore("invalid identifier");
  }

  a->var_name = *g_t++;

  if (g_t->type == tt_eq) {
    g_t++;
    a->body = as_expr();
  } else if (g_t->type == tt_lparen) return restore("this looks like a function - did you forget to use the `fn <type> <name>` syntax?");

  if (g_t->type != tt_semicolon) return restore("expecting semi-colon");
  g_t++;

  a->type = at_decl;
  return a;
}
ast_t * as_if() {
  ast_t * e = as_expr();
  if (e->type == at_err) return e;

  ast_t * b = a_stmt();
  if (b->type == at_err) return b;

  *g_a = (ast_t) {
    .type = at_if,
    .args = e,
    .body = b,
  };
  return g_a++;
}
ast_t * as_while() {
  ast_t * e = as_expr();
  if (e->type == at_err) return e;

  ast_t * b = a_stmt();
  if (b->type == at_err) return b;

  *g_a = (ast_t) {
    .type = at_while,
    .args = e,
    .body = b,
  };
  return g_a++;
}
ast_t * a_stmt() {
  switch (g_t->type) {
    case tt_semicolon: return as_empty();
    case tt_lbracket:  return as_block();
    case tt_ident:     break;
    default:           return restore("expecting semi-colon");
  }

  if (take_ident("fn")) return as_fn();
  if (take_ident("if")) return as_if();
  if (take_ident("while")) return as_while();

  ast_t * var = a_var_type();
  if (var->type == at_err) return var;
  if (var->var_type) return as_var(var);

  if (take_ret_type()) return restore("this looks like a function - did you forget to use the `fn <type> <name>` syntax?");

  ast_t * r = as_expr();
  if (r->type == at_err) return r;

  if (g_t->type != tt_semicolon) return restore("expecting semi-colon");
  g_t++;

  return r;
}

ast_t * a_extern() {
  if (!take_ident("fn")) restore("only 'extern fn' supported");

  ast_t * r = as_fn();
  if (r->type == at_err) return r;
  r->type = at_extern;
  return r;
}
ast_t * a_top_level() {
  if (take_ident("extern")) return a_extern();
  return a_stmt();
}
ast_t * astify() {
  ast_t * r = 0;
  ast_t * a = 0;

  g_t = g_toks;
  while (g_t->type) {
    ast_t * n = a_top_level();
    if (a) { a->next = n; a = n; }
    else a = r = n;

    while (a->next) a = a->next;
  }

  if (!r) {
    r = g_a++;
    *r = (ast_t) { at_nil };
  }
  return r;
}

/////////////////////////////////////////////////////////////

void dump_ast(int ind, ast_t * r) {
  while (r) {
    write(1, "                            ", ind);
    write_str(ast_type_names[r->type]);
    if (r->var_type) {
      write_str(" ");
      write_str(var_type_names[r->var_type]);
    }
    if (r->oper_type) {
      write_str(" ");
      write_str(oper_type_names[r->oper_type]);
    }
    switch (r->var_arity) {
      case -1: write_str("[]"); break;
      case  0: break;
      default:
        write_str("[");
        write_num(r->var_arity);
        write_str("]");
        break;
    }
    write_str(" ");
    if (r->var_name.pos) write(1, r->var_name.pos, r->var_name.len); write_str("\n");
    dump_ast(ind + 1, r->dot);
    dump_ast(ind + 1, r->args);
    dump_ast(ind + 1, r->body);

    r = r->next;
  }
}
int main(int argc, char ** argv) {
  if (argc != 2) usage(argv[0]);
  slurp(argv[1]);
  tokenise();

  ast_t * r = astify();
  dump_ast(0, r);
}
