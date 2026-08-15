#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
    setbuf(stdout, NULL);

  // TODO: Uncomment the code below to pass the first stage
  while (1) {
    printf("$ ");
    char input[100];
    fgets(input, 100, stdin);

   //Remove the trailing
      input[strlen(input) - 1] = '\0' ;
      printf("%s: command not found\n", input);

  //strcompare
  if strcmp(input, "exit" == 0) {
    break;
  }
  
  }
  return 1;
}
