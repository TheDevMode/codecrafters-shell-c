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
  
  //strcompare
   if (strcmp(input, "exit") == 0) {
      break;
  }
  if (strcmp(input, "echo ", 5) == 0) {
      printf("%s\n", input + 5);
  }

      printf("%s: command not found\n", input);


  }
  return 0;
}
