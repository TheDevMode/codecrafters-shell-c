#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
    setbuf(stdout, NULL);

  // TODO: Uncomment the code below to pass the first stage
  while (1) {
    printf("$ ");
    char input[100];
    fgets(input, 100, stdin);

    // Remove the trailing newline
    input[strlen(input) - 1] = '\0';

    // Compare input commands
    if (strcmp(input, "exit") == 0) {
      break;
    } else if (strncmp(input, "echo ", 5) == 0) {
      printf("%s\n", input + 5);
    }
    else if (strncmp(input ,"type " , 5) == 0) {
      if (strcmp(input ,"exit") == 0) {
        printf("exit is a shell builtin\n");
      }
      else if (strcmp(input ,"echo") == 0) {
        printf("echo is a shell builtin\n");
      }
      else if(strcmp(input ,"type") == 0) {
        printf("type is a shell builtin\n");
      }
      else {
        printf("%s: command not found\n", input + 5);
      }
    }
     else {
      printf("%s: command not found\n", input);
    }
  }

  return 0;
}
