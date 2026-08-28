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
    int argc_count = 0;

    char buffer[256];
    int buffer_index = 0;
    
    char in_quotes = 0; // Tracks '\'', '"', or 0
    int is_escaped = 0; // Tracks if previous char was an unquoted '\'

    for (int i = 0; input[i] != '\0'; i++) {
      char c = input[i];

      if (is_escaped) {
        // Unquoted backslash: always add next char literally
        if (buffer_index < sizeof(buffer) - 1) {
          buffer[buffer_index++] = c;
        }
        is_escaped = 0;
      } 
      else if (in_quotes) {
        if (c == in_quotes) {
          // Closing matching quote
          in_quotes = 0;
        } 
        else if (in_quotes == '"' && c == '\\') {
          // Inside double quotes: check the NEXT character
          char next = input[i + 1];
          if (next == '"' || next == '\\') {
            // Escaped special char: skip the '\' and output the next char
            if (buffer_index < sizeof(buffer) - 1) {
              buffer[buffer_index++] = next;
            }
            i++; // Skip the escaped character in loop
          } else {
            // Unescaped char: preserve the '\' literally
            if (buffer_index < sizeof(buffer) - 1) {
              buffer[buffer_index++] = c;
            }
          }
        } 
        else {
          // Inside single quotes or regular char inside double quotes
          if (buffer_index < sizeof(buffer) - 1) {
            buffer[buffer_index++] = c;
          }
        }
      } 
      else {
        // Outside quotes
        if (c == '\\') {
          is_escaped = 1;
        } else if (c == '\'' || c == '"') {
          in_quotes = c;
        } else if (c == ' ') {
          if (buffer_index > 0) {
            buffer[buffer_index] = '\0';
            if (argc_count < 63) {
              argv[argc_count++] = strdup(buffer);
            }
            buffer_index = 0;
          }
        } else {
          if (buffer_index < sizeof(buffer) - 1) {
            buffer[buffer_index++] = c;
          }
        }
      }
    }

    // Push final argument from buffer
    if (buffer_index > 0 && argc_count < 63) {
      buffer[buffer_index] = '\0';
      argv[argc_count++] = strdup(buffer);
    }
    argv[argc_count] = NULL;

    // Ignore empty lines
    if (argc_count == 0) {
      continue;
    }
    //exit
    if (strcmp(argv[0], "exit") == 0) {
      for (int i = 0; i < argc_count; i++) free(argv[i]);
      break;
    }
    // echo
    else if (strcmp(argv[0], "echo") == 0) {
      for (int i = 1; i < argc_count; i++) {
        printf("%s%s", argv[i], (i == argc_count - 1) ? "" : " ");
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
        for (int i = 0; i < argc_count ; i++) free(argv[i]);
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
    for (int i = 0; i < argc_count ; i++) {
      free(argv[i]);
    }
  }

  return 0;
}