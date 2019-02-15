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

long eval_op(long x, char* op, long y) {
  if (strcmp(op, "+") == 0) { return x + y; }
  if (strcmp(op, "-") == 0) { return x - y; }
  if (strcmp(op, "*") == 0) { return x * y; }
  if (strcmp(op, "/") == 0) { return x / y; }
  return 0;
}

long eval(mpc_ast_t* t) {

  // return numbers immediately
  if(strstr(t->tag, "number")) {
    return atoi(t->contents);
  }

  // the operator is always 2nd
  char* op = t->children[1]->contents;

  // third child may be either a number or expression
  long x = eval(t->children[2]);

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
  mpc_parser_t* Operator   = mpc_new("operator");
  mpc_parser_t* Expression = mpc_new("expression");
  mpc_parser_t* ALisp      = mpc_new("aLisp");

  // define the parsers with the following language
  mpca_lang(MPCA_LANG_DEFAULT,
  "                                                           \
   number     : /-?[0-9]+/ ;                                  \
   operator   : '+' | '-' | '*' | '/' ;                       \
   expression : <number> | '(' <operator> <expression>+ ')' ; \
   aLisp      : /^/ <operator> <expression>+ /$/ ;            \
  ",
  Number, Operator, Expression, ALisp);

  // print version and instructions
  puts("lisp: by ayyjohn");
  puts("aLisp Version 0.0.0.0.2");
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
      long result = eval(r.output);
      printf("%li\n", result);
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
  mpc_cleanup(4, Number, Operator, Expression, ALisp);
  return 0;
}
