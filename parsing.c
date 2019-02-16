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

/* create enum of possible errors */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* create enum of possible lval types */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };

/* declare lval (list value) structure */
typedef struct {
  int type;
  long num;
  char* err;
  char* sym;
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

/* method to delete an lval, depending on type */
void lval_del(lval* v) {
  switch(v->type) {
  /* do nothing for number type, no malloc calls */
  case LVAL_NUM: break;
  /* free string data for errors and symbols */
  case LVAL_ERR: free(v->err); break;
  case LVAL_SYM: free(v->sym); break;
    /* delete all lval elements recursively for s-expressions */
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
  v->cell = realloct(v->cell, sizeof(lval*) * v->count);
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
  if (strstr(t->tag, "number")) { return lval_sym(t->contents); }

  /* if root, or s-expr then create an empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">" == 0)) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))   { x = lval_sexpr(); }

  /* fill in the list with any valid expressions that follow */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0)  { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}


/* forward declare lval print so it can be called from lval_expr_print */
/* resolves circular dependency */
void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    /* print the contained value */
    lval_print(v->cell[i]);

    /* don't print trailing space if last element */
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

/* how to print an lval */
void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  }
}

/* println for lvals */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval eval_op(lval x, char* op, lval y) {

  /* if either value is an error, just return it */
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  /* otherwise do math on the values */
  if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
  /* special treatment necessary for division because of infinity */
  if (strcmp(op, "/") == 0) {
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
  }
  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {

  // return numbers immediately
  if(strstr(t->tag, "number")) {
    /* check if there's an error in conversion */
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  // the operator is always 2nd
  char* op = t->children[1]->contents;

  // third child may be either a number or expression
  lval x = eval(t->children[2]);

  // go through remaining children and evaluate recursively
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

int main(int argc, char** argv) {
  // create parsers
  mpc_parser_t* Number     = mpc_new("number");
  mpc_parser_t* Symbol     = mpc_new("symbol");
  mpc_parser_t* Sexpr      = mpc_new("sexpr");
  mpc_parser_t* Expr       = mpc_new("expr");
  mpc_parser_t* aLisp      = mpc_new("aLisp");

  // define the parsers with the following language
  mpca_lang(MPCA_LANG_DEFAULT,
  "                                         \
   number : /-?[0-9]+/ ;                    \
   symbol : '+' | '-' | '*' | '/' ;         \
   sexpr  : '(' <expr>* ')' ;               \
   expr   : <number> | <symbol> | <sexpr> ; \
   aLisp  : /^/ <expr>* /$/ ;               \
  ",
  Number, Symbol, Sexpr, Expr, aLisp); 
  // print version and instructions
  puts("lisp: by ayyjohn");
  puts("aLisp Version 0.0.0.0.3");
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
      lval* x = lval_read(r.output);
      lval_println(x);
      lval_del(x);
    } else {
      // otherwise print the error
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    // free retrieved input
    free(input);
  }

  // clean up parsers
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, aLisp);
  return 0;
}
