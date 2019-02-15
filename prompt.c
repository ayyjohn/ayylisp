#include <stdio.h>

/* declare a buffer for user input, size 2048 bytes */
static char input[2048];

int main(int argc, char** argv) {
  /* print version and exit info */
  puts("Lispy Version 0.0.0.0.1");
  puts("press ^c to exit\n");

  while (1) {
    /* output prompt */
    fputs("lispy> ", stdout);

    /* read user input of max size 2048 */
    fgets(input, 2048, stdin);
    /* echo back input */
    printf("%s", input);
  }

  return 0;
}

