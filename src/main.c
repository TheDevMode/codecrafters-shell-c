#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>

const char *builtins[] = {"echo", "exit", NULL};

void enable_raw_mode(struct termios *orig_termios) {
  tcgetattr(STDIN_FILENO, orig_termios);
  struct termios raw = *orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode(struct termios *orig_termios) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios);
}

// Executes a single command array with optional redirection handling
void execute_single_command(char **argv, int argc_count, int total_allocated) {
  // Check for output/error redirections
  char *outfile = NULL;
  int redirect_idx = -1;
  int target_fd = STDOUT_FILENO;
  int open_flags = O_WRONLY | O_CREAT;

  for (int i = 0; i < argc_count; i++) {
    if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], "1>") == 0) {
      if (i + 1 < argc_count) {
        outfile = argv[i + 1];
        redirect_idx = i;
        target_fd = STDOUT_FILENO;
        open_flags |= O_TRUNC;
        break;
      }
    } else if (strcmp(argv[i], ">>") == 0 || strcmp(argv[i], "1>>") == 0) {
      if (i + 1 < argc_count) {
        outfile = argv[i + 1];
        redirect_idx = i;
        target_fd = STDOUT_FILENO;
        open_flags |= O_APPEND;
        break;
      }
    } else if (strcmp(argv[i], "2>") == 0) {
      if (i + 1 < argc_count) {
        outfile = argv[i + 1];
        redirect_idx = i;
        target_fd = STDERR_FILENO;
        open_flags |= O_TRUNC;
        break;
      }
    } else if (strcmp(argv[i], "2>>") == 0) {
      if (i + 1 < argc_count) {
        outfile = argv[i + 1];
        redirect_idx = i;
        target_fd = STDERR_FILENO;
        open_flags |= O_APPEND;
        break;
      }
    }
  }

  if (redirect_idx != -1) {
    argv[redirect_idx] = NULL;
    argc_count = redirect_idx;
  }

  int saved_fd = -1;
  if (outfile != NULL) {
    saved_fd = dup(target_fd);
    int fd_out = open(outfile, open_flags, 0644);
    if (fd_out >= 0) {
      dup2(fd_out, target_fd);
      close(fd_out);
    }
  }

  // Handle Builtins & External Commands
  if (strcmp(argv[0], "exit") == 0) {
    for (int i = 0; i < total_allocated; i++) free(argv[i]);
    if (outfile != NULL && saved_fd != -1) {
      dup2(saved_fd, target_fd);
      close(saved_fd);
    }
    exit(0);
  } else if (strcmp(argv[0], "echo") == 0) {
    for (int i = 1; i < argc_count; i++) {
      printf("%s%s", argv[i], (i == argc_count - 1) ? "" : " ");
    }
    printf("\n");
  } else if (strcmp(argv[0], "pwd") == 0) {
    char cwd[512];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf("%s\n", cwd);
    }
  } else if (strcmp(argv[0], "cd") == 0) {
    const char *directory = argv[1];
    if (directory != NULL && strcmp(directory, "~") == 0) {
      directory = getenv("HOME");
    }
    if (directory == NULL || chdir(directory) != 0) {
      printf("cd: %s: No such file or directory\n", argv[1] ? argv[1] : "");
    }
  } else if (strcmp(argv[0], "type") == 0) {
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
  } else {
    pid_t pid = fork();
    if (pid == 0) {
      execvp(argv[0], argv);
      printf("%s: command not found\n", argv[0]);
      exit(EXIT_FAILURE);
    } else if (pid > 0) {
      wait(NULL);
    }
  }

  // Restore streams after single command execution
  if (outfile != NULL && saved_fd != -1) {
    if (target_fd == STDOUT_FILENO) fflush(stdout);
    else fflush(stderr);
    dup2(saved_fd, target_fd);
    close(saved_fd);
  }
}

int main(int argc, char *argv_main[]) {
  setbuf(stdout, NULL);
  struct termios orig_termios;

  while (1) {
    printf("$ ");
    fflush(stdout);

    char input[512];
    int input_len = 0;

    enable_raw_mode(&orig_termios);

    while (1) {
      char c = getchar();

      if (c == EOF) {
        disable_raw_mode(&orig_termios);
        exit(0);
      }
      if (c == '\n' || c == '\r') {
        input[input_len] = '\0';
        printf("\n");
        break;
      }
      if (c == 127 || c == '\b') {
        if (input_len > 0) {
          input_len--;
          printf("\b \b");
        }
        continue;
      }
      if (c == '\t') {
        input[input_len] = '\0';
        int matches = 0;
        const char *matched_builtin = NULL;

        for (int i = 0; builtins[i] != NULL; i++) {
          if (strncmp(builtins[i], input, input_len) == 0) {
            matches++;
            matched_builtin = builtins[i];
          }
        }

        if (matches == 1) {
          const char *completion = matched_builtin + input_len;
          printf("%s ", completion);
          strcat(input, completion);
          strcat(input, " ");
          input_len = strlen(input);
        } else {
          printf("\a");
        }
        continue;
      }

      if (input_len < (int)sizeof(input) - 1) {
        input[input_len++] = c;
        putchar(c);
      }
    }

    disable_raw_mode(&orig_termios);

    if (input_len == 0) continue;

    // Tokenize Input
    char *argv[128];
    int argc_count = 0;
    char buffer[512];
    int buffer_index = 0;
    char in_quotes = 0; 
    int is_escaped = 0; 

    for (int i = 0; input[i] != '\0'; i++) {
      char ch = input[i];

      if (is_escaped) {
        if (buffer_index < (int)sizeof(buffer) - 1) buffer[buffer_index++] = ch;
        is_escaped = 0;
      } else if (in_quotes) {
        if (ch == in_quotes) {
          in_quotes = 0;
        } else if (in_quotes == '"' && ch == '\\') {
          char next = input[i + 1];
          if (next == '"' || next == '\\') {
            if (buffer_index < (int)sizeof(buffer) - 1) buffer[buffer_index++] = next;
            i++;
          } else {
            if (buffer_index < (int)sizeof(buffer) - 1) buffer[buffer_index++] = ch;
          }
        } else {
          if (buffer_index < (int)sizeof(buffer) - 1) buffer[buffer_index++] = ch;
        }
      } else {
        if (ch == '\\') {
          is_escaped = 1;
        } else if (ch == '\'' || ch == '"') {
          in_quotes = ch;
        } else if (ch == ' ') {
          if (buffer_index > 0) {
            buffer[buffer_index] = '\0';
            if (argc_count < 127) argv[argc_count++] = strdup(buffer);
            buffer_index = 0;
          }
        } else {
          if (buffer_index < (int)sizeof(buffer) - 1) buffer[buffer_index++] = ch;
        }
      }
    }

    if (buffer_index > 0 && argc_count < 127) {
      buffer[buffer_index] = '\0';
      argv[argc_count++] = strdup(buffer);
    }
    argv[argc_count] = NULL;

    if (argc_count == 0) continue;

    int total_allocated = argc_count;

    // Check for Pipeline '|'
    int pipe_idx = -1;
    for (int i = 0; i < argc_count; i++) {
      if (strcmp(argv[i], "|") == 0) {
        pipe_idx = i;
        break;
      }
    }

    if (pipe_idx != -1) {
      // Split commands around pipe
      argv[pipe_idx] = NULL;
      char **cmd1_argv = &argv[0];
      char **cmd2_argv = &argv[pipe_idx + 1];

      int pipefd[2];
      if (pipe(pipefd) == -1) {
        perror("pipe");
        for (int i = 0; i < total_allocated; i++) free(argv[i]);
        continue;
      }

      pid_t pid1 = fork();
      if (pid1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(cmd1_argv[0], cmd1_argv);
        printf("%s: command not found\n", cmd1_argv[0]);
        exit(EXIT_FAILURE);
      }

      pid_t pid2 = fork();
      if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(cmd2_argv[0], cmd2_argv);
        printf("%s: command not found\n", cmd2_argv[0]);
        exit(EXIT_FAILURE);
      }

      // Close parent pipe ends and wait for both children
      close(pipefd[0]);
      close(pipefd[1]);
      waitpid(pid1, NULL, 0);
      waitpid(pid2, NULL, 0);

    } else {
      // Normal execution without pipeline
      execute_single_command(argv, argc_count, total_allocated);
    }

    // Free memory
    for (int i = 0; i < total_allocated; i++) {
      free(argv[i]);
    }
  }

  return 0;
}