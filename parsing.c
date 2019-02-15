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
  /* enums values are represented as strings under the hood */
  /* so type and err are ints, even though they're enum vals */
  int err;
} lval;

/* constructor for a number type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* constructor for an error type lval */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  /* see above for why v.err is an int */
  v.err = x;
  return v;
}

/* how to print an lval */
void lval_print(lval v) {
  switch (v.type) {
    /* if v is a number, print it */
    case LVAL_NUM:
      printf("%li", v.num);
      break;
    /* if v is an error print the corresponding error message */
    case LVAL_ERR:
      switch (v.err) {
        case LERR_DIV_ZERO:
          printf("error: division by zero"); break;
        case LERR_BAD_OP:
          printf("error: invalid operator"); break;
        case LERR_BAD_NUM:
          printf("Error: invalid number"); break;
    }

  }
}

/* println for lvals */
void lval_println(lval v) { lval_print(v); putchar('\n'); }

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
  mpc_parser_t* ALisp      = mpc_new("aLisp");

  // define the parsers with the following language
  mpca_lang(MPCA_LANG_DEFAULT,
  "                                         \
   number : /-?[0-9]+/ ;                    \
   symbol : '+' | '-' | '*' | '/' ;         \
   sexpr  : '(' <expr>* ')' ;               \
   expr   : <number> | <symbol> | <sexpr> ; \
   aLisp  : /^/ <expr>* /$/ ;               \
  ",
  Number, Symbol, Sexpr, Expr, Alisp); 
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
    if (mpc_parse("<stdin>", input, ALisp, &r)) {
      // on success print the abstract syntax tree
      lval result = eval(r.output);
      lval_println(result);
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
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, ALisp);
  return 0;
}
