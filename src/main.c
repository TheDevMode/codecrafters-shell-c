#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv_main[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  char saved_path[512];
  char *env_path = getenv("PATH");

  while (1) {
    printf("$ ");
    char input[100];
    if (fgets(input, sizeof(input), stdin) == NULL) {
      break;
    }

    // Remove the trailing newline
    input[strcspn(input, "\n")] = '\0';

    char *argv[64];
    int argc = 0;

    char buffer[100];
    int buffer_index = 0;
    int in_quotes = 0;

    for (int i = 0; input[i] != '\0'; i++) {
      if (input[i] == '\'') {
        // Toggle single-quote mode without adding the quote character itself
        in_quotes = !in_quotes;
      } 
      else if (input[i] == ' ' && !in_quotes) {
        // Space outside quotes marks argument boundaries
        if (buffer_index > 0) {
          buffer[buffer_index] = '\0';
          if (argc < 63) {
            argv[argc++] = strdup(buffer);
          }
          buffer_index = 0;
        }
      } 
      else {
        // Normal character (or space inside single quotes)
        if (buffer_index < sizeof(buffer) - 1) {
          buffer[buffer_index++] = input[i];
        }
      }
    }

    // Push the final argument left in the buffer
    if (buffer_index > 0 && argc < 63) {
      buffer[buffer_index] = '\0';
      argv[argc++] = strdup(buffer);
    }
    argv[argc] = NULL;

    // Ignore empty lines
    if (argc == 0) {
      continue;
    }

    if (strcmp(argv[0], "exit") == 0) {
      for (int i = 0; i < argc; i++) free(argv[i]);
      break;
    }
    // echo
    else if (strcmp(argv[0], "echo") == 0) {
      for (int i = 1; i < argc; i++) {
        printf("%s%s", argv[i], (i == argc - 1) ? "" : " ");
      }
      printf("\n");
    }
    // pwd
    else if (strcmp(argv[0], "pwd") == 0) {
      char cwd[512];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
      } else {
        printf("getcwd error\n");
      }
    }
    // cd
    else if (strcmp(argv[0], "cd") == 0) {
      const char *directory = argv[1];
      if (directory != NULL && strcmp(directory, "~") == 0) {
        directory = getenv("HOME");
      }
      if (directory == NULL || chdir(directory) != 0) {
        printf("cd: %s: No such file or directory\n", argv[1] ? argv[1] : "");
      }
    }
    // type
    else if (strcmp(argv[0], "type") == 0) {
      if (argv[1] == NULL) {
        for (int i = 0; i < argc; i++) free(argv[i]);
        continue;
      }
      if (strcmp(argv[1], "exit") == 0 || strcmp(argv[1], "echo") == 0 ||
          strcmp(argv[1], "pwd") == 0  || strcmp(argv[1], "cd") == 0 ||
          strcmp(argv[1], "type") == 0) {
        printf("%s is a shell builtin\n", argv[1]);
      } else {
        int found = 0;
        if (env_path != NULL) {
          strncpy(saved_path, env_path, sizeof(saved_path) - 1);
          saved_path[sizeof(saved_path) - 1] = '\0';
        }
        for (char *path = strtok(saved_path, ":"); path != NULL; path = strtok(NULL, ":")) {
          char full_path[512];
          snprintf(full_path, sizeof(full_path), "%s/%s", path, argv[1]);
          if (access(full_path, X_OK) == 0) {
            printf("%s is %s\n", argv[1], full_path);
            found = 1;
            break;
          }
        }
        if (!found) {
          printf("%s not found\n", argv[1]);
        }
      }
    }
   
    else {
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
    }

    // Clean up allocated memory 
    for (int i = 0; i < argc; i++) {
      free(argv[i]);
    }
  }

  return 0;
}