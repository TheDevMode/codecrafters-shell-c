#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>

const char *builtins[] = {"echo", "exit", "pwd", "cd", "type", NULL};

void enable_raw_mode(struct termios *orig_termios) {
  tcgetattr(STDIN_FILENO, orig_termios);
  struct termios raw = *orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode(struct termios *orig_termios) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios);
}

int is_builtin(const char *cmd) {
  if (!cmd) return 0;
  for (int i = 0; builtins[i] != NULL; i++) {
    if (strcmp(cmd, builtins[i]) == 0) return 1;
  }
  return 0;
}

// Executes a single command array. 
// in_child = 1 when running inside a fork()'d pipeline stage child process.
// in_child = 0 when running directly from the main shell loop.
void run_command(char **argv, int argc_count, int in_child) {
  if (argc_count == 0 || argv[0] == NULL) {
    if (in_child) exit(0);
    return;
  }

  // Parse Redirections (>, >>, 1>, 1>>, 2>, 2>>)
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

  // Builtins vs External Commands Dispatching
  if (strcmp(argv[0], "exit") == 0) {
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
      if (is_builtin(argv[1])) {
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
    // External command execution logic
    if (in_child) {
      execvp(argv[0], argv);
      printf("%s: command not found\n", argv[0]);
      exit(EXIT_FAILURE);
    } else {
      pid_t pid = fork();
      if (pid == 0) {
        execvp(argv[0], argv);
        printf("%s: command not found\n", argv[0]);
        exit(EXIT_FAILURE);
      } else if (pid > 0) {
        waitpid(pid, NULL, 0);
      }
    }
  }

  // Clean up standard descriptors if redirected
  if (outfile != NULL && saved_fd != -1) {
    if (target_fd == STDOUT_FILENO) fflush(stdout);
    else fflush(stderr);
    dup2(saved_fd, target_fd);
    close(saved_fd);
  }

  if (in_child) {
    exit(0);
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

    // Command Tokenization
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

    // Split tokens into individual sub-command arrays separated by '|'
    char **cmds[64];
    int cmds_argc[64];
    int num_cmds = 0;

    cmds[num_cmds] = &argv[0];
    int current_cmd_argc = 0;

    for (int i = 0; i < argc_count; i++) {
      if (strcmp(argv[i], "|") == 0) {
        argv[i] = NULL; // Replace '|' with NULL to terminate previous command slice
        cmds_argc[num_cmds] = current_cmd_argc;
        num_cmds++;
        cmds[num_cmds] = &argv[i + 1];
        current_cmd_argc = 0;
      } else {
        current_cmd_argc++;
      }
    }
    cmds_argc[num_cmds] = current_cmd_argc;
    num_cmds++;

    if (num_cmds > 1) {
      // Execute multi-stage pipeline across N commands
      int prev_pipe_read = -1;
      pid_t pids[64];

      for (int i = 0; i < num_cmds; i++) {
        int pipefd[2];
        if (i < num_cmds - 1) {
          if (pipe(pipefd) == -1) {
            perror("pipe");
            break;
          }
        }

        pid_t pid = fork();
        if (pid == 0) {
          // Connect STDIN from previous stage pipe
          if (prev_pipe_read != -1) {
            dup2(prev_pipe_read, STDIN_FILENO);
            close(prev_pipe_read);
          }

          // Connect STDOUT to next stage pipe
          if (i < num_cmds - 1) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
          }

          run_command(cmds[i], cmds_argc[i], 1);
        }

        pids[i] = pid;

        // Close parent pipe descriptors
        if (prev_pipe_read != -1) {
          close(prev_pipe_read);
        }
        if (i < num_cmds - 1) {
          close(pipefd[1]);
          prev_pipe_read = pipefd[0];
        }
      }

      // Wait for all child stages to complete
      for (int i = 0; i < num_cmds; i++) {
        if (pids[i] > 0) {
          waitpid(pids[i], NULL, 0);
        }
      }

    } else {
      // Single command execution
      if (strcmp(argv[0], "cd") == 0 || strcmp(argv[0], "exit") == 0) {
        if (strcmp(argv[0], "exit") == 0) {
          for (int i = 0; i < total_allocated; i++) free(argv[i]);
          exit(0);
        } else {
          run_command(argv, argc_count, 0);
        }
      } else {
        run_command(argv, argc_count, 0);
      }
    }

    // Free heap memory for tokenized string args
    for (int i = 0; i < total_allocated; i++) {
      free(argv[i]);
    }
  }

  return 0;
}