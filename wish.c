#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PATH_SIZE 256

void run_batch_file(char *file);
void rum_prompt();
void run_cmd(char *cmd);
char **split(char *cmd, int *arg_len, char *delim);
void error();
void trim(char *str);
void run_multiple(char *input);

char *white_space_delim = " \t\n";
char *paths[MAX_PATH_SIZE];
int path_len = 1;

int main(int argc, char **argv) {
  paths[0] = "/bin"; // initialize path here to avoid error

  if (argc > 2) {
    error();
    exit(1);
  }

  if (argc == 2)
    run_batch_file(argv[1]);
  else
    rum_prompt();
}

void run_batch_file(char *file) {
  FILE *file_ptr;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  // open file
  file_ptr = fopen(file, "r");
  if (file_ptr == NULL) {
    error();
    exit(1);
  }

  // run file line by line
  while ((read = getline(&line, &len, file_ptr)) != -1) {
    line[strlen(line) - 1] = '\0';
    run_multiple(line);
  }

  fclose(file_ptr);

  if (line != NULL) {
    free(line);
  }
}

void run_multiple(char *input) {
  int cmds_len = 0;
  char **multiple_cmds = split(input, &cmds_len, "&");
  pid_t child_pid;

  for (int i = 0; i < cmds_len; i++) {
    int len = 0;
    char **cmds = split(multiple_cmds[i], &len, white_space_delim);
    char *cmd = "";

    if (cmds_len == 1 && len > 0) {
      cmd = cmds[0];
      trim(cmd);
    }

    // if command needs to be executed as parent
    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "cd") == 0 ||
        strcmp(cmd, "path") == 0) {
      run_cmd(multiple_cmds[i]);
      free(cmd);
    } else {
      // create child process and run multiple_cmds[i]
      child_pid = fork();
      if (child_pid < 0) {
        error();
        _exit(1);
      } else if (child_pid == 0) {
        run_cmd(multiple_cmds[i]);
        free(cmds);
        _exit(0);
      }
    }
  }

  // wait for all child processes to finish
  int status;
  while (wait(&status) > 0)
    ;

  free(multiple_cmds);
}

void rum_prompt() {
  while (1) {
    printf("wish> ");

    char *input = NULL;
    size_t size = 0;
    getline(&input, &size, stdin);
    input[strlen(input) - 1] = '\0';

    run_multiple(input);

    free(input);
  }
}

char **split(char *str, int *len, char *delim) {
  char *str_cpy = strdup(str);
  char *token = strtok(str_cpy, delim);

  while (token != NULL) {
    (*len)++;
    token = strtok(NULL, delim);
  }

  char **args = malloc(*len * sizeof(char *));

  str_cpy = strdup(str);

  int i = 0;
  token = strtok(str_cpy, delim);
  while (token != NULL) {
    args[i] = strdup(token);
    i++;
    token = strtok(NULL, delim);
  }

  return args;
}

void run_cmd(char *cmd_str) {
  trim(cmd_str);
  if (strcmp(cmd_str, "") == 0)
    return;

  int redirects_len = 0;
  char **redirects = split(cmd_str, &redirects_len, ">");
  char *result = strchr(cmd_str, '>');

  if (redirects_len > 2 || (result != NULL && redirects_len == 1)) {
    error();
    return;
  }

  int files_len = 0;
  char **files;
  if (redirects_len == 2)
    files = split(redirects[1], &files_len, white_space_delim);

  if (files_len > 1) {
    error();
    return;
  }

  char *output_file;
  if (files_len > 0)
    output_file = files[0];

  int arg_len = 0;
  char **args = split(redirects[0], &arg_len, white_space_delim);
  char *cmd = args[0];

  arg_len--;

  if (strcmp(cmd, "") == 0) {
    return;
  }
  if (strcmp(cmd, "exit") == 0) {
    if (arg_len > 0) {
      error();
    } else {
      exit(0);
    }
  } else if (strcmp(cmd, "cd") == 0) {
    if (arg_len != 1) {
      error();
      return;
    } else if (chdir(args[1]) != 0) {
      error();
      return;
    }
  } else if (strcmp(cmd, "path") == 0) {
    if (arg_len > MAX_PATH_SIZE) {
      error();
      return;
    }

    for (int i = 0; i < arg_len; i++)
      paths[i] = args[i + 1];

    path_len = arg_len;
  } else if (strcmp(cmd, "ppath") == 0) {
    for (int i = 0; i < path_len; i++)
      printf("%s\n", paths[i]);
  } else {
    if (path_len == 0) {
      error();
      return;
    }

    int found = 0;
    for (int i = 0; i < path_len; i++) {
      char *path = strdup(paths[i]);
      strcat(path, "/");
      strcat(path, cmd);

      if (access(path, X_OK) == 0) {
        found = 1;
        pid_t child_pid = fork();

        if (child_pid < 0) {
          error();
          return;
        } else if (child_pid == 0) {
          // forked process
          char *arguments[arg_len + 2];

          arguments[0] = cmd;

          for (int i = 1; i < arg_len + 1; i++)
            arguments[i] = args[i];
          arguments[arg_len + 1] = NULL;

          if (files_len == 1) {
            fclose(stdout);
            freopen(output_file, "w", stdout);
          }

          execv(path, arguments);

          // if there is an error, exit the child process
          _exit(1);
        } else {
          int status;
          waitpid(child_pid, &status, 0);

          if (status == 0)
            break;
        }
      }
    }
    if (found != 1)
      error();
  }
}

void error() { fprintf(stderr, "An error has occurred\n"); }

void trim(char *str) {
  int start = 0;
  int end = strlen(str) - 1;

  while (isspace(str[start]))
    start++;

  while (end >= 0 && isspace(str[end]))
    end--;

  if (start > end)
    str[0] = '\0';
  else {
    for (int i = 0; i <= end - start; i++) {
      str[i] = str[start + i];
    }
    str[end - start + 1] = '\0';
  }
}
