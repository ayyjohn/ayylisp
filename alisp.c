/* using quotes means it searches the current directory first */
#include "mpc.h"

/* include methods for if we compile this on windows */
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

/* otherwise include editline headers for if running on MacOS*/
#else
#include<editline/readline.h>
#endif

/* forward declare parsers  */

mpc_parser_t* Number;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* aLisp;

/* forward declare lval and lenv structs to avoid cyclic dependency */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* create enum of possible lval types */
enum { LVAL_ERR, LVAL_NUM,   LVAL_SYM, LVAL_STR,
       LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

/* define pointer-to-function lbuiltin */
typedef lval*(*lbuiltin)(lenv*, lval*);

/* declare lval (list value) structure */
struct lval {
  /* type of lval, one of the above enum values */
  int type;

  /* for the basic type lvals (nums, errors, symbols) */
  long num;
  char* err;
  char* sym;
  char* str;

  /* for function type lvals */
  lbuiltin builtin;
  lenv* env;
  lval* formals;
  lval* body;

  /* for expression type lvals (s and q expressions)*/
  int count;
  lval** cell;
};

/* constructor for a pointer to a number type lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* constructor for a pointer to an error type lval */
lval* lval_err(char* fmt, ...) {
  lval* v =  malloc(sizeof(lval));
  v->type = LVAL_ERR;
  /* create varargs list and initialize it */
  va_list va;
  va_start(va, fmt);

  /* allocate default 512 bytes for error message */
  /* will reallocate to smaller later */
  v->err = malloc(512);

  /* printf the error string with a maximum of 511 chars */
  vsnprintf(v->err, 511, fmt, va);

  /* reallocate error string to actual size */
  v->err = realloc(v->err, strlen(v->err)+1);

  /* cleanup */
  va_end(va);

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

/* constructor for a pointer to a new string type lval */
lval* lval_str(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_STR;
  v->str = malloc(strlen(s) + 1);
  strcpy(v->str, s);
  return v;
}

/* constructor for a pointer to a new function type lval */
lval* lval_builtin(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;
  return v;
}

/* forward declare constructor for new lenv for use in user defined methods */
lenv* lenv_new(void);

/* constructor for user defined functions */
/* formals represent the arguments in the function definition */
/* body is a q-expression containing the function body */
lval* lval_lambda(lval* formals, lval* body) {
  /* set aside memory and set type */
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  /* differentiates between user defined and builtin functions */
  v->builtin = NULL;
  /* initialize new environment for variables to be set */
  v->env = lenv_new();

  v->formals = formals;
  v->body = body;
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

/* forward declare lenv_del method so it can be
   used to clean up user defined functions */
void lenv_del(lenv* e);

/* method to delete an lval, depending on type */
void lval_del(lval* v) {
  switch(v->type) {
  /* do nothing for number type, no nested malloc calls */
  case LVAL_NUM: break;
  /* only nested malloc calls for user defined functions, not builtins */
  case LVAL_FUN:
    if (!v->builtin) {
      lenv_del(v->env);
      lval_del(v->formals);
      lval_del(v->body);
    }
    break;
  /* free string data for errors and symbols */
  case LVAL_ERR: free(v->err); break;
  case LVAL_SYM: free(v->sym); break;
  case LVAL_STR: free(v->str); break;
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

/* forward declare lenv copy method for use in lval copy */
lenv* lenv_copy(lenv* e);

/* method for copying an lval, necessary for getting and putting
   lvals to the environment as variables */
lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
    case LVAL_FUN:
      if (v->builtin) {
        x->builtin = v->builtin;
      } else {
        x->builtin = NULL;
        x->env = lenv_copy(v->env);
        x->formals = lval_copy(v->formals);
        x->body = lval_copy(v->body);
      }
    break;
    /* for non-nested lval, just copy contents directly */
    case LVAL_NUM: x->num = v->num; break;

    /* copy string-containing lvals with strcpy */
    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err); break;
    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym); break;
    case LVAL_STR:
      x->str = malloc(strlen(v->str) + 1);
      strcpy(x->sym, v->sym); break;

    /* for nested lvals, copy sub expressions recursively */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
    break;
  }

  return x;
}

/* method to add one lval to the lvals of another lval */
lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

/* method to return the lval at index i of a given lval */
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

/* takes two lvals and adds all lvals from one to the other */
lval* lval_join(lval* x, lval* y) {
  /* add all cells in y to x */
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }
  /* y is now empty, x has all its values */
  lval_del(y);
  return x;
}

/* takes an lval, gets and returns the lval at index i from it */
lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

/* forward declare lval print so it can be called from lval_expr_print */
/* resolves circular dependency */
void lval_print(lval* v);

/* recursively prints out a string representation of a nested lval */
void lval_print_expr(lval* v, char open, char close) {
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

/* how to print a string lval, ensures characters are escaped correctly */
void lval_print_str(lval* v) {
  char* escaped = malloc(strlen(v->str)+1);
  strcpy(escaped, v->str);
  escaped = mpcf_escape(escaped);
  /* print the lval string field between quotes */
  printf("\"%s\"", escaped);

  /* cleanup */
  free(escaped);
}

/* how to print an lval. for s-expr and q-expr recursively call
   to print out all lvals nested in the cell */
void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_FUN:
      if (v->builtin) {
        printf("<builtin>");
      } else {
        printf("(\\ "); lval_print(v->formals);
        putchar(' '); lval_print(v->body); putchar(')');
      }
      break;
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_STR:   lval_print_str(v); break;
    case LVAL_SEXPR: lval_print_expr(v, '(', ')'); break;
    case LVAL_QEXPR: lval_print_expr(v, '{', '}'); break;
  }
}

/* println for lvals */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

/* comparison method for lvals. compare all relevant fields for each type */
int lval_eq(lval* x, lval* y) {
  /* different types are never equal, immediate short circuit */
  if (x->type != y->type) { return 0; }

  switch (x->type) {
    /* numbers compare value */
    case LVAL_NUM: return (x->num == y->num);

    /* string-containing lvals compare string values */
    case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
    case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);
    case LVAL_STR: return (strcmp(x->str, y->str) == 0);

    /* for functions, compare builtin if builtin, otherwise compare formals and args individually */
    case LVAL_FUN:
      if (x->builtin || y->builtin) {
        return x->builtin == y->builtin;
      } else {
        return lval_eq(x->formals, y->formals) && lval_eq(x->body, y->body);
      }
      /* if list, compare each element */
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      if (x->count != y->count) { return 0; }
      for (int i = 0; i < x->count; i++) {
        /* return false as soon as any values don't equal each other */
        if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
      }
      return 1;
    break;
  }
  return 0;
}
/* method to translate lval enum into human-readable names */
char* ltype_name(int t) {
  switch (t) {
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_STR: return "String";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}

/* represents the environment, stores twin lists of
 variable names and their associated values */
struct lenv {
  /* parent environment, null for global environment */
  lenv* par;
  /* track number of entries */
  /* there should be exactly 1 variable name for each value */
  /* at the same index */
  int count;
  /* list of variable names */
  char** syms;
  /* list of values for the above variable names */
  lval** vals;
};

/* constructor for empty environment */
lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  /* environment starts with no variables defined and no parent */
  e->par = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

/* method to delete an environment */
void lenv_del(lenv* e) {
  /* delete each variable defined and free the memory
     of the strings used to name the variables */
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lenv* lenv_copy(lenv* e) {
  /* allocate space for new lenv, copy simple params */
  lenv* n = malloc(sizeof(lenv));
  n->par = e->par;
  n->count = e->count;
  n->syms = malloc(sizeof(char*) * n->count);
  n->vals = malloc(sizeof(lval*) * n->count);

  /* copy syms and vals iteratively */
  for (int i = 0; i < e->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  return n;
}
/* method to get values from the environment */
lval* lenv_get(lenv* e, lval* k) {
  /* iterate over all existing symbols,
     see if any of the strings match the current symbol
     if so, return a copy of that lval, otherwise
     return an error because that variable was not defined */
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  /* if the symbol is not found, check parent environments
     to ensure that the symobl is not defined higher up */
  if (e->par) {
    return lenv_get(e->par, k);
  } else {
    return lval_err("unbound symbol '%s'", k->sym);
  }
}

/* method to put a new variable definition into the local environment */
void lenv_put(lenv* e, lval* k, lval* v) {
  /* if the current variable name is already defined, overwrite
     the lval. otherwise, allocate space for a new entry, and
     copy the new variable into the environment */
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  e->count++;
  /* add space for the new entry */
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  /* perform copy over into new location */
  e->vals[e->count-1] = lval_copy(v);
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1], k->sym);
}

/* method to put a new variable definiton to the global environment */
void lenv_def(lenv* e, lval* k, lval* v) {
  /* iterate up to the global environment */
  while (e->par) { e = e->par; }
  /* place the variable name and contents into the global env */
  lenv_put(e, k, v);
}
/* macro to verify basic repetitive conditions */
#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args); \
    return err; \
  }

/* macro to confirm that a function is passed the correct type */
#define LASSERT_TYPE(func, args, index, expect) \
  LASSERT(args, args->cell[index]->type == expect, \
    "function '%s' passed incorrect type for argument %i. got %s, expected %s.", \
          func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

/* macro to confirm that a function is passed the correct number of arguments */
#define LASSERT_NUM(func, args, num) \
  LASSERT(args, args->count == num, \
    "function %s passed incorrect number of arguments. got %i, expected %i", \
    func, args->count, num)

/* method to confirm that a function is passed in an expression at all */
#define LASSERT_NOT_EMPTY(func, args, index) \
  LASSERT(args, args->cell[index]->count != 0, \
          "function %s passed empty expression for argument %i.", func, index)

/* forward declare to avoid cyclic dependency */
lval* lval_eval(lenv* e, lval* v);

/* builtin method to create a user defined function out of two q-expressions as input */
lval* builtin_lambda(lenv* e, lval* a) {
  /* assert that there are two arguments, both q-expressions */
  LASSERT_NUM("\\", a, 2);
  LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

  /* assert that the first q-expression contains only symbols */
  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "cannot define non-symbol. got %s, expected %s",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }

  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(formals, body);
}

/* method to convert an lval into a q-expression */
lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

/* method to retrieve the first element of an lval */
lval* builtin_head(lenv* e, lval* a) {
  /*
    takes in an lval, verifies that it only has one cell, that the cell is
    a q-expression, and that that q-expression is not empty.
    if all those conditions are met, returns the first element in the
    lval and discards the rest
   */
  LASSERT_NUM("head", a, 1);
  LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("head", a, 0);

  lval* v = lval_take(a, 0);
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

/* method te retrieve the last element of an lval */
lval* builtin_tail(lenv* e, lval* a) {
  /*
     takes in an lval, verifies that it only has one cell, that the cell is
     a q-expression, and that that q-expression is not empty.
     if all those conditions are met, removes the head of the lval
     and returns the lval 
  */
  LASSERT_NUM("tail", a, 1);
  LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("tail", a, 0);

  lval* v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

/* method to evaluate an s-expression written as a q-expression */
lval* builtin_eval(lenv* e, lval* a) {
  LASSERT_NUM("eval", a, 1);
  LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

/* method to concatenate q-expressions */
lval* builtin_join(lenv* e, lval* a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE("join", a, i, LVAL_QEXPR);
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

/* method to perform basic mathematical operators */
lval* builtin_op(lenv* e, lval* a, char* op) {

  /* ensure arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE(op, a, i, LVAL_NUM);
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

/* all builtin basic math operations */
lval* builtin_add(lenv* e, lval* a) {
  return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
  return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
  return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "/");
}

/* method to add a new variable to the environment */
lval* builtin_var(lenv* e, lval* a, char* func) {
  /* if input isn't a q-expression the compiler will attempt
     to evaluate it immediately, so confirm input is q-expression */
  LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

  /* after definition, must be followed by a symbol list */
  lval* syms = a->cell[0];

  /* ensure all elements in first symbol list are symbols */
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "function '%s' cannot define non-symbol"
            "got %s, expected %s",
            func, ltype_name(LVAL_SYM), ltype_name(syms->cell[i]->type));
  }

  /* ensure that the method receives count-1
     inputs, where the -1 comes from removing the
     method name */
  LASSERT(a, (syms->count == a->count-1),
          "function '%s' passed too many arguments for symbols. "
          "got %i, expected %i",
          func, a->count-1, syms->count);

  /* assign variable names for each value in a */
  for (int i = 0; i < syms->count; i++) {
    /* if 'def' define globally, if 'put' define locally */
    if (strcmp(func, "def") == 0) {
      lenv_def(e, syms->cell[i], a->cell[i+1]);
    }
    if (strcmp(func, "=") == 0) {
      lenv_put(e, syms->cell[i], a->cell[i+1]);
    }
  }

  /* clean up */
  lval_del(a);
  /* return an empty s-expression on success */
  return lval_sexpr();
}

/* builtin method to put a variable to the global scope */
lval* builtin_def(lenv* e, lval* a) {
  return builtin_var(e, a, "def");
}

/* builtin method to put a variable to the local scope */
lval* builtin_put(lenv* e, lval* a) {
  return builtin_var(e, a, "=");
}

/* method to compare numbers */
lval* builtin_ord(lenv* e, lval* a, char* op) {
  LASSERT_NUM(op, a, 2);
  LASSERT_TYPE(op, a, 0, LVAL_NUM);
  LASSERT_TYPE(op, a, 1, LVAL_NUM);

  int r;
  if (strcmp(op, ">") == 0) {
    r = (a->cell[0]->num > a->cell[1]->num);
  }
  if (strcmp(op, "<") == 0) {
    r = (a->cell[0]->num < a->cell[1]->num);
  }
  if (strcmp(op, ">=") == 0) {
    r = (a->cell[0]->num >= a->cell[1]->num);
  }
  if (strcmp(op, "<=") == 0) {
    r = (a->cell[0]->num <= a->cell[1]->num);
  }
  lval_del(a);
  return lval_num(r);
}

/* builtins for comparison operators */
lval* builtin_gt(lenv* e, lval* a) {
  return builtin_ord(e, a, ">");
}


lval* builtin_lt(lenv* e, lval* a) {
  return builtin_ord(e, a, "<");
}


lval* builtin_ge(lenv* e, lval* a) {
  return builtin_ord(e, a, ">=");
}

lval* builtin_le(lenv* e, lval* a) {
  return builtin_ord(e, a, "<=");
}

/* builtin method to compare equality between two lvals */
lval* builtin_cmp(lenv* e, lval* a, char* op) {
  /* ensure two inputs to compare equality for */
  LASSERT_NUM(op, a, 2);
  int r;
  if (strcmp(op, "==") == 0) {
    r = lval_eq(a->cell[0], a->cell[1]);
  }
  if (strcmp(op, "!=") == 0) {
    r = !lval_eq(a->cell[0], a->cell[1]);
  }
  lval_del(a);
  return lval_num(r);
}

/* builtins for equals and not equals */
lval* builtin_eq(lenv* e, lval* a) {
  return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
  return builtin_cmp(e, a, "!=");
}

/* method to add builtin method to the environment */
void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_builtin(func);
  lenv_put(e, k, v);
  lval_del(k); lval_del(v);
}

lval* builtin_if(lenv* e, lval* a) {
  /* verify that there are three inputs, a number 0 or 1 */
  /* and two possible q-expressions to evaluat */
  LASSERT_NUM("if", a, 3);
  LASSERT_TYPE("if", a, 0, LVAL_NUM);
  LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
  LASSERT_TYPE("if", a, 2, LVAL_QEXPR);
  /* mark both expressions as s-expressions to make them evaluable */
  lval* x;
  a->cell[1]->type = LVAL_SEXPR;
  a->cell[2]->type = LVAL_SEXPR;

  /* if the boolean is true, evaluate the first expression,
     otherwise evaluate the second */
  if (a->cell[0]->num) {
    x = lval_eval(e, lval_pop(a, 1));
  } else {
    x = lval_eval(e, lval_pop(a, 2));
  }
  lval_del(a);
  return x;
}

lval* lval_read(mpc_ast_t* t);

/* load and evaluate a file */
lval* builtin_load(lenv* e, lval* a) {
  LASSERT_NUM("load", a, 1);
  LASSERT_TYPE("load", a, 0, LVAL_STR);

  /* parse file given by string name */
  mpc_result_t r;
  if (mpc_parse_contents(a->cell[0]->str, aLisp, &r)) {

    /* read file contents */
    lval* expr = lval_read(r.output);
    mpc_ast_delete(r.output);

    /* evaluate each expression */
    while (expr->count) {
      lval* x = lval_eval(e, lval_pop(expr, 0));
      /* print any errors encountered */
      if (x->type == LVAL_ERR) { lval_println(x); }
      lval_del(x);
    }

    /* cleanup */
    lval_del(expr);
    lval_del(a);

    return lval_sexpr();

  } else {
    /* get parse error as a string */
    char* err_msg = mpc_err_string(r.error);
    mpc_err_delete(r.error);

    /* create a new error message using the parse error */
    lval* err = lval_err("could not load Library %s", err_msg);
    free(err_msg);
    lval_del(a);

    return err;
  }
}

lval* builtin_print(lenv* e, lval* a) {
  for (int i = 0; i < a->count; i++) {
    lval_print(a->cell[i]); putchar(' ');
  }

  putchar('\n');
  lval_del(a);

  return lval_sexpr();
}

lval* builtin_error(lenv* e, lval* a) {
  LASSERT_NUM("error", a, 1);
  LASSERT_TYPE("error", a, 0, LVAL_STR);

  lval* err = lval_err(a->cell[0]->str);

  lval_del(a);
  return err;
}

/* method to add the basic functions to a newly initialized environment */
void lenv_add_builtins(lenv* e) {
  /* list functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);

  /* mathematical functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);

  /* comparison functions */
  lenv_add_builtin(e, "if", builtin_if);
  lenv_add_builtin(e, "==", builtin_eq);
  lenv_add_builtin(e, "!=", builtin_ne);
  lenv_add_builtin(e, ">",  builtin_gt);
  lenv_add_builtin(e, "<",  builtin_lt);
  lenv_add_builtin(e, ">=", builtin_ge);
  lenv_add_builtin(e, "<=", builtin_le);

  /* variable functions */
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);
  lenv_add_builtin(e, "\\", builtin_lambda);

  /* string functions */
  lenv_add_builtin(e, "load",  builtin_load);
  lenv_add_builtin(e, "error", builtin_error);
  lenv_add_builtin(e, "print", builtin_print);
}

/* method to call functions */
lval* lval_call(lenv* e, lval* f, lval* a) {
  /* if builtin, just call */
  if (f->builtin) { return f->builtin(e, a); }

  /* record argument counts */
  int given = a->count;
  int total = f->formals->count;

  /* while there are still arguments to be processed */
  while (a->count) {
    /* if we've been given too many arguments */
    if (f->formals->count == 0) {
      lval_del(a); return lval_err(
         "function passed too many arguments. "
         "got %i, expected %i", given, total);
    }

    /* fetch the first symbol from the formals */
    /* and arguments and bind it to the environment */
    lval* sym = lval_pop(f->formals, 0);
    /* check for ampersand symbol to evaluate variable length arguments */
    if (strcmp(sym->sym, "&") == 0) {
      /* verify that & is followed by another symbol */
      if (f->formals->count != 1) {
        lval_del(a);
        return lval_err("function format invalid. "
                        "symbol '&' not followed by a single symbol.");
      }

      /* bind next formal to the remaining arguments */
      lval* nsym = lval_pop(f->formals, 0);
      lenv_put(f->env, nsym, builtin_list(e, a));
      lval_del(sym); lval_del(nsym);
      break;
    }
    lval* val = lval_pop(a, 0);
    lenv_put(f->env, sym, val);

    /* clean up */
    lval_del(sym); lval_del(val);
  }
  /* argument list is now bound, so can be cleaned up */
  lval_del(a);

  /* account for empty varargs list in evaluation */
  if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {
    if (f->formals->count != 2) {
      return lval_err("function format invalid. "
                      "symbol '&' not followed by a single symbol");
    }

    /* pop and delete the '&' symbol */
    lval_del(lval_pop(f->formals, 0));

    /* create an empty list and bind the following symbol to it */
    lval* sym = lval_pop(f->formals, 0);
    lval* val = lval_qexpr();
    lenv_put(f->env, sym, val);
    lval_del(sym); lval_del(val);
  }
  /* allow for partial evaluation; less than the desired number of */
  /* arguments can be passed in and we will return a partially */
  /* evaluated function, otherwise evaluate */
  if (f->formals->count == 0) {
    f->env->par = e;
    return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    return lval_copy(f);
  }
}

/* method to evaluate an s-expression */
lval* lval_eval_sexpr(lenv* e, lval* v) {
  /* evaluate children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  /* check for errors */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  /* empty expressions */
  if (v->count == 0) { return v; }

  /* single expressions */
  if (v->count == 1) { return lval_take(v, 0); }

  /* ensure the first element is a function after evaluation */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval* err = lval_err(
      "s-expression starts with incorrect type. "
      "got %s, expected %s",
      ltype_name(f->type), ltype_name(LVAL_FUN));
    lval_del(f);
    lval_del(v);
    return err;
  }

  lval* result = lval_call(e, f, v);
  lval_del(f);
  return result;
}

/* method to evaluate an lval */
lval* lval_eval(lenv* e, lval* v) {
  /* check to see if symbol is defined, if not, return an error */
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  /* evaluates Sexpressions */
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
  /* all other lval types remain the same */
  return v;
}

/* defines how to read a number and convert to an lval */
lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

/* defines how to read a string to convert to an lval */
lval* lval_read_str(mpc_ast_t* t) {
  /* cut off the final quote character */
  t->contents[strlen(t->contents)-1] = '\0';
  /* copy all but the first quote character */
  char* unescaped = malloc(strlen(t->contents+1)+1);
  strcpy(unescaped, t->contents+1);
  unescaped = mpcf_unescape(unescaped);
  lval* str = lval_str(unescaped);

  free(unescaped);
  return str;
}

/* method to define how to read different inputs as
   determined by the grammar parsing */
lval* lval_read(mpc_ast_t* t) {
  /* if symbol or number return lval of that type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
  if (strstr(t->tag, "string")) { return lval_read_str(t); }

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
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    if (strstr(t->children[i]->tag, "comment"))     { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

int main(int argc, char** argv) {
  /* create parsers */
  Number  = mpc_new("number");
  Symbol  = mpc_new("symbol");
  String  = mpc_new("string");
  Comment = mpc_new("comment");
  Sexpr   = mpc_new("sexpr");
  Qexpr   = mpc_new("qexpr");
  Expr    = mpc_new("expr");
  aLisp   = mpc_new("aLisp");

  /* define the parsers with the following language */
  mpca_lang(MPCA_LANG_DEFAULT,
  "                                                         \
    number  : /-?[0-9]+/ ;                                  \
    symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;            \
    string  : /\"(\\\\.|[^\"])*\"/ ;                        \
    comment : /;[^\\r\\n]*/ ;                               \
    sexpr   : '(' <expr>* ')' ;                             \
    qexpr   : '{' <expr>* '}' ;                             \
    expr    : <number>  | <symbol> | <string>               \
            | <comment> | <sexpr>  | <qexpr> ;              \
    aLisp  : /^/ <expr>* /$/ ;                              \
  ",
  Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, aLisp);
  /* print version and instructions */
  puts("lisp: by ayyjohn");
  puts("aLisp Version 0.0.0.0.14");
  puts("Press Ctrl+c to Exit\n");

  /* set up environment */
  lenv* e = lenv_new();
  /* add base methods */
  lenv_add_builtins(e);

  /* if just aLisp is invoked */
  if (argc == 1) {
    /* infinite repl loop */
    while (1) {

      /* prompt user */
      char* input = readline("aLisp> ");

      /* add input to history */
      add_history(input);

      /* add parsing for user input */
      mpc_result_t r;
      if (mpc_parse("<stdin>", input, aLisp, &r)) {
        /* on success evaluate the AST */
        lval* x = lval_eval(e, lval_read(r.output));
        lval_println(x);
        lval_del(x);
        mpc_ast_delete(r.output);
      } else {
        /* otherwise print the error */
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
      }

      /* free retrieved input */
      free(input);
    }
  }

  /* if filenames are supplied to aLisp */
  if (argc >= 2) {

    /* loop over each supplied filename (starting from 1) */
    for (int i = 1; i < argc; i++) {

      /* Argument list with a single argument, the filename */
      lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));

      /* Pass to builtin load and get the result */
      lval* x = builtin_load(e, args);

      /* If the result is an error be sure to print it */
      if (x->type == LVAL_ERR) { lval_println(x); }
      lval_del(x);
    }
}
  /* clean up parsers */
  mpc_cleanup(8,
              Number, Symbol, String, Comment,
              Sexpr, Qexpr, Expr, aLisp);
  return 0;
}
