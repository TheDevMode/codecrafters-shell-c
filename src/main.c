#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv_main[]) {
  setbuf(stdout, NULL);

  while (1) {
    printf("$ ");
    fflush(stdout);

    char input[512];
    if (fgets(input, sizeof(input), stdin) == NULL) {
      break;
    }

    input[strcspn(input, "\n")] = '\0';

    char *argv[128];
    int argc_count = 0;

    char buffer[512];
    int buffer_index = 0;
    
    char in_quotes = 0; 
    int is_escaped = 0; 

    for (int i = 0; input[i] != '\0'; i++) {
      char c = input[i];

      if (is_escaped) {
        if (buffer_index < (int)sizeof(buffer) - 1) {
          buffer[buffer_index++] = c;
        }
        is_escaped = 0;
      } 
      else if (in_quotes) {
        if (c == in_quotes) {
          in_quotes = 0;
        } 
        else if (in_quotes == '"' && c == '\\') {
          char next = input[i + 1];
          if (next == '"' || next == '\\') {
            if (buffer_index < (int)sizeof(buffer) - 1) {
              buffer[buffer_index++] = next;
            }
            i++;
          } else {
            if (buffer_index < (int)sizeof(buffer) - 1) {
              buffer[buffer_index++] = c;
            }
          }
        } 
        else {
          if (buffer_index < (int)sizeof(buffer) - 1) {
            buffer[buffer_index++] = c;
          }
        }
      } 
      else {
        if (c == '\\') {
          is_escaped = 1;
        } else if (c == '\'' || c == '"') {
          in_quotes = c;
        } else if (c == ' ') {
          if (buffer_index > 0) {
            buffer[buffer_index] = '\0';
            if (argc_count < 127) {
              argv[argc_count++] = strdup(buffer);
            }
            buffer_index = 0;
          }
        } else {
          if (buffer_index < (int)sizeof(buffer) - 1) {
            buffer[buffer_index++] = c;
          }
        }
      }
    }

    if (buffer_index > 0 && argc_count < 127) {
      buffer[buffer_index] = '\0';
      argv[argc_count++] = strdup(buffer);
    }
    argv[argc_count] = NULL;

    if (argc_count == 0) {
      continue;
    }

    int total_allocated = argc_count;

    char *outfile = NULL;
    int redirect_idx = -1;

    for (int i = 0; i < argc_count; i++) {
      if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], "1>") == 0) {
        if (i + 1 < argc_count) {
          outfile = argv[i + 1];
          redirect_idx = i;
          break;
        }
      }
    }

    if (redirect_idx != -1) {
      argv[redirect_idx] = NULL;
      argc_count = redirect_idx;
    }

    int saved_stdout = dup(STDOUT_FILENO);

    if (outfile != NULL) {
      int fd_out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd_out >= 0) {
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
      }
    }

    if (argc_count > 0) {
      // exit
      if (strcmp(argv[0], "exit") == 0) {
        for (int i = 0; i < total_allocated; i++) free(argv[i]);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
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
        if (argv[1] != NULL) {
          if (strcmp(argv[1], "exit") == 0 || strcmp(argv[1], "echo") == 0 ||
              strcmp(argv[1], "pwd") == 0  || strcmp(argv[1], "cd") == 0 ||
              strcmp(argv[1], "type") == 0) {
            printf("%s is a shell builtin\n", argv[1]);
          } else {
            int found = 0;
            char *env_path = getenv("PATH");
            if (env_path != NULL) {
              char path_copy[1024];
              strncpy(path_copy, env_path, sizeof(path_copy) - 1);
              path_copy[sizeof(path_copy) - 1] = '\0';

              for (char *path = strtok(path_copy, ":"); path != NULL; path = strtok(NULL, ":")) {
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s/%s", path, argv[1]);
                if (access(full_path, X_OK) == 0) {
                  printf("%s is %s\n", argv[1], full_path);
                  found = 1;
                  break;
                }
              }
            }
            if (!found) {
              printf("%s not found\n", argv[1]);
            }
          }
        }
      }
      // External commands
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
    }

    // Flush and restore standard stdout
    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    // Free memory
    for (int i = 0; i < total_allocated; i++) {
      free(argv[i]);
    }
  }

  return 0;
}