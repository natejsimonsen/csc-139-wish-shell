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
void error(char *msg);

char *white_space_delim = " \t\n";
char *paths[MAX_PATH_SIZE];
int path_len = 1;

int main(int argc, char **argv) {
  paths[0] = "/bin";

  if (argc > 2) {
    printf("ERROR: only 1 argument (batch file) accepted\n");
    exit(1);
  }

  if (argc == 2)
    run_batch_file(argv[1]);
  else
    rum_prompt();
}

void run_batch_file(char *file) {
  printf("%s, is the file in batch mode\n", file);
}

void rum_prompt() {
  while (1) {
    printf("wish> ");
    char *input = NULL;
    size_t size = 0;
    getline(&input, &size, stdin);
    input[strlen(input) - 1] = '\0';
    run_cmd(input);
    free(input);
  }
}

char **split(char *str, int *len, char *delim) {
  char *str_cpy = strdup(str); // Duplicate the input command
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
  int redirects_len = 0;
  char **redirects = split(cmd_str, &redirects_len, ">");

  if (redirects_len > 2)
    error("more than one redirect\n");

  int files_len = 0;
  char **files;
  if (redirects_len == 2)
    files = split(redirects[1], &files_len, white_space_delim);

  if (files_len > 1)
    error("more than one file\n");

  char *output_file;
  if (files_len > 0)
    output_file = files[0];

  int arg_len = 0;
  char **args = split(redirects[0], &arg_len, white_space_delim);
  char *cmd = args[0];
  arg_len--;

  if (strcmp(cmd, "exit") == 0) {
    exit(0);
  } else if (strcmp(cmd, "cd") == 0) {
    if (arg_len != 1)
      error("Incorrect number of arguments\n");

    if (chdir(args[1]) != 0) {
      error("Something went wrong\n");
    }
  } else if (strcmp(cmd, "path") == 0) {
    if (arg_len > MAX_PATH_SIZE)
      error("overflow error in path");

    for (int i = 0; i < arg_len; i++)
      paths[i] = args[i + 1];

    path_len = arg_len;
  } else {
    for (int i = 0; i < path_len; i++) {
      char *path = strdup(paths[i]);

      if (access(path, X_OK) == 0) {
        pid_t child_pid = fork();

        if (child_pid < 0) {
          perror("Fork failed");
          exit(1);
        } else if (child_pid == 0) {
          char *arguments[arg_len + 2];

          strcat(path, "/");
          strcat(path, cmd);
          arguments[0] = path;

          for (int i = 1; i < arg_len + 1; i++)
            arguments[i] = args[i];
          arguments[arg_len + 1] = NULL;

          if (files_len == 1) {
            fclose(stdout);
            freopen(output_file, "w", stdout);
          }

          execv(path, arguments);

          perror("Execv failed");
          exit(1);
        } else {
          int status;
          waitpid(child_pid, &status, 0);

          if (status != 0)
            error("Execv failed\n");
          else {
            break;
          }
        }
      } else {
        error("no executables found in path\n");
      }
    }
  }
}

void error(char *msg) {
  fprintf(stderr, "%s", msg);
  exit(1);
}
