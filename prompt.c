#include <stdio.h>
#include <stdlib.h>

/* handle compilation on Windows */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* fake readline function */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  cpy[strlen(cpy)-1] = '\0';
  return cpy
}

/* fake history function */
void add_history(char* unused) {}
/* handle compilation on macOS */
#else
#include <editline/readline.h>
#endif

int main(int argc, char** argv) {
  /* print version and exit info */
  puts("Lispy Version 0.0.0.0.1");
  puts("press ^c to exit\n");

  while (1) {
    /* output prompt and get input */
    char* input = readline("lispy> ");

    /* add input to a list of previous inputs available using up arrow key */
    add_history(input);

    /* echo back input */
    printf("%s\n", input);

    /* delete what's in input */
    free(input);
  }

  return 0;
}

