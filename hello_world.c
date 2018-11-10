#include <stdio.h>

void print_n(int n) {
  for (int i = 0; i < n; i++) {
    puts("Hello_world!");
  }
}

int main(int argc, char** argv) {
  print_n(5);
  return 0;
}
