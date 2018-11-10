#include <stdio.h>

// declare a buffer for user input of 2048 bytes
static char input[2048];

int main(int argc, char** argv) {

  // print version and instructions
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  // infinite repl loop
  while (1) {
    // prompt user
    fputs("lispy> ", stdout);
    // get input, max size 2048
    fgets(input, 2048, stdin);
    // echo input (for now)
    printf("No, you're a %s", input);
  }

  return 0;
}
