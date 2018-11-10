#include <stdio.h>
#include <stdlib.h>

#include<editline/readline.h>

int main(int argc, char** argv) {

  // print version and instructions
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  // infinite repl loop
  while (1) {
    // prompt user
    char* input = readline("lispy> ");
    // add input to history
    add_history(input);
    // echo input
    printf("No, you're a %s\n", input);
    // free retrieved input
    free(input);
  }

  return 0;
}
