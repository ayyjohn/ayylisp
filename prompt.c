#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

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

