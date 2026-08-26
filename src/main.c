#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
    setbuf(stdout, NULL);

  char saved_path[512];
  char *env_path = getenv("PATH");

  // TODO: Uncomment the code below to pass the first stage
  while (1) {
    printf("$ ");
    char input[100];
    fgets(input, 100, stdin);

    // Remove the trailing newline
    input[strlen(input) - 1] = '\0';

  char *argv[64];
  int argc = 0;

  char buffer[100];
  int buffer_index = 0;
  int in_quotes = 0;

// 1. Loop until reaching the null terminator '\0' instead of calling strlen()
  for (int i = 0; input[i] != '\0'; i++) {
      if (input[i] == '\'') {
    // Toggle quote flag without adding '\'' to the argument string
    in_quotes = !in_quotes;
      }
      else if (input[i] == ' ' && !in_quotes) {
          if (buffer_index > 0) {
              buffer[buffer_index] = '\0';
              // 2. Prevent exceeding argv array bounds
              if (argc < 63) {
                  argv[argc++] = strdup(buffer);
              }
             buffer_index = 0;
          }
     } 
      else {
          // 3. Prevent buffer overflow (leave 1 space for '\0')
          if (buffer_index < sizeof(buffer) - 1) {
              buffer[buffer_index++] = input[i];
          }
      }
  }

  // Store the remaining argument if any exists
  if (buffer_index > 0 && argc < 63) {
      buffer[buffer_index] = '\0';
      argv[argc++] = strdup(buffer);
  }

  argv[argc] = NULL;

  

    // Compare input commands
    //exit
    if (strcmp(input, "exit") == 0) {
      break;
    //echo
    // echo
    } else if (strcmp(argv[0], "echo") == 0) {
    for (int i = 1; i < argc; i++) {
        printf("%s%s", argv[i], (i == argc - 1) ? "" : " ");
    }
    printf("\n");
    }

    //pwd
    else if (strcmp(input, "pwd") == 0) {
      char cwd[512];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
      } else {
        printf("getcwd error");
      }
    }
    //cd
    else if (strncmp(input, "cd ", 3) == 0) {
      const char *directory = input + 3;
      if (strcmp(directory, "~") == 0) {
        directory = getenv("HOME");
      }
      if (directory == NULL || chdir(directory) != 0) {
        printf("cd: %s: No such file or directory\n", input + 3);
      }
    }
    //type
    else if (strncmp(input ,"type " , 5) == 0) {
      if (strcmp(input + 5 ,"exit") == 0) {
        printf("exit is a shell builtin\n");
      }
      else if (strcmp(input + 5 ,"echo") == 0) {
        printf("echo is a shell builtin\n");
      }
      else if (strcmp(input + 5, "pwd") == 0) {
        printf("pwd is a shell builtin\n");
      }
      else if (strcmp(input + 5, "cd") == 0) {
        printf("cd is a shell builtin\n");
      }
      else if(strcmp(input + 5 ,"type") == 0) {
        printf("type is a shell builtin\n");
      }
      else {
        // Search for the command in the PATH
      int found = 0;
      if (env_path != NULL) {
        strncpy(saved_path, env_path, sizeof(saved_path) - 1);
      }
        for (char *path = strtok(saved_path, ":"); path != NULL; path = strtok(NULL, ":")) {
          char full_path[512];
          snprintf(full_path, sizeof(full_path), "%s/%s", path, input + 5);
          if (access(full_path, X_OK) == 0) {
            printf("%s is %s\n", input + 5, full_path);
            found += 1;
            break;
          }
        }
        if (found != 1) {
        printf("%s not found\n", input + 5);
      }
    }
    }
     else {
      // Split the input into arguments
      char *argv[64];
      int argc = 0;
      char input_copy[100];
      strncpy(input_copy, input, sizeof(input_copy) - 1);

      char *slice = strtok(input_copy, " ");
      while (slice != NULL  && argc < 63) {
        argv[argc++] = slice;
        slice = strtok(NULL, " ");
      }
      argv[argc] = NULL;

      if (argc > 0) {
        // Execute the command using execvp
        pid_t pid = fork();
        if (pid == 0) {
          execvp(argv[0], argv);
          printf("%s: command not found\n", argv[0]);
          exit(EXIT_FAILURE);
        } else if (pid > 0) {
          wait(NULL);
        } else {
          perror("fork");
          exit(EXIT_FAILURE);
        }
      } else {
      printf("%s: command not found\n", input);
      }
    }
  }

  return 0;
}

