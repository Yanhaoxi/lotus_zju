#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void execute_command(const char *cmd) {
  system(cmd);  // Dangerous sink
}

char *sanitize(const char *input) {
  char *output = strdup(input);
  for (int i = 0; output[i]; i++) {
    if (output[i] == ';' || output[i] == '|') {
      output[i] = '_';
    }
  }
  return output;
}

int main(void) {
  char user_input[256];
  printf("Enter command: ");
  scanf("%255s", user_input);  // Tainted source

  // Bug: direct flow from source to sink
  execute_command(user_input);

  // Safe: sanitized before use
  char *safe_cmd = sanitize(user_input);
  execute_command(safe_cmd);
  free(safe_cmd);

  return 0;
}
