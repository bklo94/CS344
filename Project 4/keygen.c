//Brandon Lo
//CS344
//Project 4
//keygen.c

//preprocessor directives
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//How to generate a random seed using srand()
//https://stackoverflow.com/questions/822323/how-to-generate-a-random-number-in-c
int main(int argc, char const *argv[]) {
  char charList[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
  //conditional to check if input is correct
  if(argc < 2 || !isdigit(argv[1][0])){
    fprintf(stderr, "Usage: %s <num chars>\n", argv[0]);
    return 1;
  }
  //seed for the random generator
  srand(time(NULL));
  int num_chars = atoi(argv[1]), i;
  char *key = malloc(sizeof(char)*(num_chars + 1));
  //fill the buffer with random characters from the charList of the 27 possible characters
  for(i = 0; i < num_chars;i++){
    int random = rand() % 27;
    key[i] = charList[random];
  }
  //null terminator added and the result is printed to stdout with a newline at the end
  key[i] = 0;
  printf("%s\n", key);
  return 0;
}
