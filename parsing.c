// using quotes means it searches the current directory first
#include "mpc.h"

// include methods for if we run this on windows
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

// otherwise include editline headers
#else
#include<editline/readline.h>
#endif

/* create enum of possible lval types */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

/* declare lval (list value) structure */
typedef struct lval {
  // stores the type of lval, one of the above enum types
  int type;
  // stores the number of the lval (if it has one)
  long num;
  // stores the error of the lval (if it has one)
  char* err;
  // stores the symbol of the lval (if it has one)
  char* sym;
  // stores how many lvals are nested in cell
  int count;
  /* stores a recursive list of lvals */
  struct lval** cell;
} lval;

/* constructor for a pointer to a number type lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* constructor for a pointer to an error type lval */
lval* lval_err(char* m) {
  lval* v =  malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

/* constructor for a pointer to a new symbol type lval */
lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

/* constructor for a new, empty S-expression lval */
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* constructor for a new, empty Q-expression lval */
lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* method to delete an lval, depending on type */
void lval_del(lval* v) {
  switch(v->type) {
  /* do nothing for number type, no malloc calls */
  case LVAL_NUM: break;
  /* free string data for errors and symbols */
  case LVAL_ERR: free(v->err); break;
  case LVAL_SYM: free(v->sym); break;
    /* delete all lval elements recursively for s-expressions */
  case LVAL_QEXPR:
  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++) {
      lval_del(v->cell[i]);
    }
    /* also free memory allocated for containing the pointers */
    free(v->cell);
    break;
  }
  /* free memory allocated for the struct itself */
  free(v);
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
  /* if symbol or number return lval of that type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  /* if root, or s-expr then create an empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0)  { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))   { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))   { x = lval_qexpr(); }

  /* fill in the list with any valid expressions that follow */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0)  { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

/* macro to verify basic repetitive conditions */
#define LASSERT(args, cond, err) \
  if (!cond)) { lval_del(args); return lval_err(err); }

/* forward declare lval print so it can be called from lval_expr_print */
/* resolves circular dependency */
void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    /* print the contained value */
    lval_print(v->cell[i]);

    /* don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

lval* lval_pop(lval* v, int i) {
  /* get the item at index i */
  lval* x = v->cell[i];

  /* shift the memory after the item at i over it */
  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

  /* decrease the count of items in the list */
  v->count--;

  /* reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* builtin_op(lval* a, char* op) {

  /* ensure arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("cannot operate on non-number");
    }
  }

  lval* x = lval_pop(a, 0);

  /* if no arguments and it's subtraction, perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  /* while there are still elements */
  while (a->count > 0) {
    /* get the next element */
    lval* y = lval_pop(a, 0);
    /* perform operations */
    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x); lval_del(y);
        x = lval_err("Division By Zero!"); break;
      }
      x->num /= y->num;
    }
    lval_del(y);
  }
  lval_del(a);
  return x;
}

lval* lval_eval(lval* v);

lval* lval_eval_sexpr(lval* v) {
  /* evaluate children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  /* check for errors */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  /* empty expressions */
  if (v->count == 0) { return v; }

  /* single expressions */
  if (v->count == 1) { return lval_take(v, 0); }

  /* ensure the first element is a symbol */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression does not start with a symbol");
  }

  /* call builtin with operator */
  lval* result = builtin_op(v, f->sym);
  lval_del(f);
  return result;
}

lval* lval_eval(lval* v) {
  /* evaluates Sexpressions */
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
  /* all other lval types remain the same */
  return v;
}

/* how to print an lval */
void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

/* println for lvals */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

int main(int argc, char** argv) {
  // create parsers
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr  = mpc_new("sexpr");
  mpc_parser_t* Qexpr  = mpc_new("qexpr");
  mpc_parser_t* Expr   = mpc_new("expr");
  mpc_parser_t* aLisp  = mpc_new("aLisp");

  // define the parsers with the following language
  mpca_lang(MPCA_LANG_DEFAULT,
  "                                                        \
    number : /-?[0-9]+/ ;                                  \
    symbol : \"list\" | \"head\" | \"tail\"                \
           | \"join\" | \"eval\" | '+' | '-' | '*' | '/' ; \
    sexpr  : '(' <expr>* ')' ;                             \
    qexpr  : '{' <expr>* '}' ;                             \
    expr   : <number> | <symbol> | <sexpr> | <qexpr> ;     \
    aLisp  : /^/ <expr>* /$/ ;                             \
  ",
  Number, Symbol, Sexpr, Qexpr, Expr, aLisp); 
  // print version and instructions
  puts("lisp: by ayyjohn");
  puts("aLisp Version 0.0.0.0.5");
  puts("Press Ctrl+c to Exit\n");

  // infinite repl loop
  while (1) {

    // prompt user
    char* input = readline("aLisp> ");

    // add input to history
    add_history(input);

    // add parsing for user input
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, aLisp, &r)) {
      // on success print the abstract syntax tree
      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {
      // otherwise print the error
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    // free retrieved input
    free(input);
  }

  // clean up parsers
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, aLisp);
  return 0;
}
