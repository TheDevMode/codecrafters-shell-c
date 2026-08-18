#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
    setbuf(stdout, NULL);

  char *env_path = getenv("PATH");

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
      if (strcmp(input + 5 ,"exit") == 0) {
        printf("exit is a shell builtin\n");
      }
      else if (strcmp(input + 5 ,"echo") == 0) {
        printf("echo is a shell builtin\n");
      }
      else if(strcmp(input + 5 ,"type") == 0) {
        printf("type is a shell builtin\n");
      }
      else {
        for (char *path = strtok(env_path, ":"); path != NULL; path = strtok(NULL, ":")) {
          char full_path[512];
          snprintf(full_path, sizeof(full_path), "%s/%s", path, input + 5);
          if (access(full_path, X_OK) == 0) {
            printf("%s is %s\n", input + 5, full_path);
            return;
          }
        }
        printf("%s not found\n", input + 5);
      }
    }
     else {
      printf("%s: command not found\n", input);
    }
  }

  return 0;
}
