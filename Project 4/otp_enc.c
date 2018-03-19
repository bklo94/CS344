//Brandon Lo
//CS344
//Project 4
//otp_enc.c

//preprocessor directives
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netdb.h>

//prototype functions
int getSockMessage(int fd, char **buffer);
int sendSockMessage(int fd, char *to_send, int size);
void resizeBuffer(char **buffer, int new_cap);
int isValidChars(char *input);

//global variables
char charList[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
int charSize = 27;
int initBuffSize = 500;
int connectionCount = 0;
int checkConn = 0;

//main function
int main(int argc, char **argv){
  //check if the arguments are coorrect
  if(argc < 4 || !isdigit((int)argv[3][0])){
    fprintf(stderr, "Usage: %s <plaintext file> <key file> <port>\n", argv[0]);
    return 1;
  }
  //declare file stream
  FILE *file;
  //declare variables to hold socket file descriptor, port, key file size
  int sockDesc, port, keySize, plaintextSize;
  //declare variables to hold the buffer size, capacity of the key, plaintext capacity
  size_t bufferSize, keyCap = 0, plaintextCap = 0;
  //hold the information for server connection
  struct sockaddr_in serverAddr;
  //holds the host information
  struct hostent *serverHost;
  //initize buffer for the key, plaintext, hostname, authentication, and character pointer
  char *keyBuffer = NULL, *plaintextBuffer = NULL, *host = "localhost", *auth = "encode#", *c;
  //open a plaintext to read and check if opened succesfully
  file = fopen(argv[1], "r");
  if(file == NULL){
    fprintf(stderr, "Error Opening %s: %s\n", argv[1], strerror(errno));
    return 1;
  }
  //read from the file and check if it read succesfully
  //https://stackoverflow.com/questions/9206091/going-through-a-text-file-line-by-line-in-c
  bufferSize =  getline(&plaintextBuffer, &plaintextCap, file);
  if(bufferSize < 0){
    fclose(file);
    fprintf(stderr, "Read Error %s: %s\n", argv[1], strerror(errno));
    return 1;
  }
  //check if the plaintext is not empty or only has a \n
  else if(strlen(plaintextBuffer) <= 1){
    fprintf(stderr, "Error: '%s'is Empty\n", argv[1]);
    free(plaintextBuffer);
    fclose(file);
    return 1;
  }
  //close the file and replace the newline character
  //how to replace the newline character
  ////https://www.tutorialspoint.com/c_standard_library/c_function_strchr.htm
  fclose(file);
  c = strchr(plaintextBuffer, '\n');
  *c = 0;
  //check if the file has all valid characters(the preset 27 possibles ones )
  if(!isValidChars(plaintextBuffer)){
    fprintf(stderr, "Error: input contains bad characters\n");
    free(plaintextBuffer);
    return 1;
  }
  //replace newline
  *c = '&';
  //open a file for writing and print message on the error
  file = fopen(argv[2], "r");
  if(file == NULL){
    fprintf(stderr, "Error Opening %s: %s\n", argv[2], strerror(errno));
    return 1;
  }
  //read the key from the file and check if it successfully got the key based on the buffer size using getline
  //https://stackoverflow.com/questions/9206091/going-through-a-text-file-line-by-line-in-c
  bufferSize =  getline(&keyBuffer, &keyCap, file);
  if(bufferSize < 0){
    fprintf(stderr, "Read Error %s: %s\n", argv[2], strerror(errno));
    fclose(file);
    return 1;
  }
  //if the key buffer is empty or contains a newline, free the data and print and error message
  else if(strlen(keyBuffer) <= 1){
    fprintf(stderr, "Error: '%s'is Empty\n", argv[2]);
    free(plaintextBuffer);
    free(keyBuffer);
    fclose(file);
    return 1;
  }
  //close the key file and replace the newline with a #
  fclose(file);
  c = strchr(keyBuffer, '\n');
  *c = '#';

  //get the size of the keyBuffer and get the size of the plaintext buffer
  keySize = strlen(keyBuffer);
  plaintextSize = strlen(plaintextBuffer);
  //check to see if the key is smaller than the plaintextSize
  if(keySize < plaintextSize){
    free(keyBuffer);
    free(plaintextBuffer);
    fprintf(stderr, "Error: key '%s' is too short\n", argv[2]);
    return 1;
  }
  //check to see if the plaintextSize and keySize has enough buffer
  if(plaintextSize + keySize + 1 >= plaintextCap)
    resizeBuffer(&plaintextBuffer, plaintextSize + keySize + 1);

  //add the key to the end of the plaintext buffer so it loks like
  // plaintext & key #
  strcat(plaintextBuffer, keyBuffer);
  free(keyBuffer);
  keyBuffer = NULL;

  //clear serverAddress and fill it with \0 characters(null terminators)
  memset((char*)&serverAddr, '\0', sizeof(serverAddr));

  //how to use the sockets
  //http://www.cs.rpi.edu/~moorthy/Courses/os98/Pgms/socket.html
  //gets the port from argv, contains the code for the address family, then convert to network byte order, and then grab the IP address with sin_addr
  port = atoi(argv[3]);
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  serverHost = gethostbyname(host);
  //check if the host info could be gotten
  if(serverHost == NULL){
    fprintf(stderr, "Error: Could not resolve %s\n", host);
    return 1;
  }
  //write the hostname information to h_addr and h_length
  memcpy((char*)&serverAddr.sin_addr.s_addr,(char*)serverHost->h_addr, serverHost->h_length);
  //make a call to socket to get the descriptor and check if it recieved it
  sockDesc = socket(AF_INET, SOCK_STREAM, 0);
  if(sockDesc < 0){
    free(plaintextBuffer);
    fprintf(stderr, "Error: %s\n", strerror(errno));
    return 1;
  }
  //connect to the server using file descriptor and check if it could connect
  if(connect(sockDesc,(struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
    close(sockDesc);
    free(plaintextBuffer);
    fprintf(stderr, "Error: could not contact otp_enc_d on port %d\n", port);
    return 2;
  }
  //send authentication to server
  sendSockMessage(sockDesc, auth, strlen(auth) + 1);

  //get the message and check if authorized
  keySize = getSockMessage(sockDesc, &keyBuffer);
  if((c = strstr(keyBuffer, "Error")) != NULL){
    fprintf(stderr, "Received %s\n", keyBuffer);
    close(sockDesc);
    free(keyBuffer);
    free(plaintextBuffer);
    return 1;
  }

  //send the full key and plaintext and then free the buffer
  sendSockMessage(sockDesc, plaintextBuffer, strlen(plaintextBuffer) + 1);
  free(keyBuffer);
  keyBuffer = NULL;

  //get response from server and put into keySize and then print the returned dataa
  keySize = getSockMessage(sockDesc, &keyBuffer);
  printf("%s\n", keyBuffer);

  //free resources before exit
  free(keyBuffer);
  free(plaintextBuffer);
  close(sockDesc);
  return 0;
}

//reads the message and looks for the # character
int getSockMessage(int fd, char **buffer){
  //check if the current buffer is null. then allocate the size
  if(*buffer == NULL)
    *buffer = calloc(initBuffSize, sizeof(char));
  //buffer capacity set along with the size set.
  int bufferCapacity = initBuffSize, bufferSize = 0;
  char *c;
  //do while loop that looks through the message until the # is found
  do {
    //buffer is allocated to recieve the string
    char *temp = calloc(initBuffSize + 1, sizeof(char));
    int charsRecv;
    //use recv to read the message from buffer
    //how to use recv to read the buffer
    //https://stackoverflow.com/questions/3074824/reading-buffer-from-socket
    charsRecv = recv(fd, temp, initBuffSize, 0);
    if(charsRecv < 0){
      fprintf(stderr, "Error: %s\n", strerror(errno));
      return -1;
    }
    //if the current bufferSize isn't enough, then resize the capacity
    if(bufferSize + charsRecv >= bufferCapacity){
      bufferCapacity *= 3;
      resizeBuffer(buffer, bufferCapacity);
    }
    //add the bytes read to the buffer and update the size
    strcat(*buffer, temp);
    bufferSize += charsRecv;
    //free the buffer
    free(temp);
  } while((c = strchr(*buffer, '#')) == NULL);
  //# is replaced with the null terminator
  *c = 0;
  return bufferSize;
}

//sends the size of the characters to the socket
//sending socket message
//https://linux.die.net/man/2/send
//using send and recv
//https://stackoverflow.com/questions/4172538/sending-multiple-messages-via-send-recv-socket-programming-c
int sendSockMessage(int fd, char *buff, int size){
  //initialize variables for the bytes sent, outbound bytes to send, and the returned value from send()
  int sent = 0, outSend = size, returnVal = 0;
  //loop until size bytes are sent
  while(sent < size){
    //send remaining bytes to socket
    //https://stackoverflow.com/questions/13479760/c-socket-recv-and-send-all-data
    returnVal = send(fd, buff+sent, outSend, 0);
    //check if the message was sent correctly
    if(returnVal < 0){
      fprintf(stderr, "Error: %s\n", strerror(errno));
      return -1;
    }
    //update the number of bytes sent
    sent += returnVal;
    outSend -= returnVal;
  }
  return sent;
}

//resize the buufer
//https://stackoverflow.com/questions/12917727/resizing-an-array-in-c
void resizeBuffer(char **buffer, int new_cap){
  //allocate the buffer size
  char *temp = calloc(new_cap,sizeof(char));
  //buffer is copied to temp and then freed and temp is set to the new buffer
  strcpy(temp, *buffer);
  free(*buffer);
  *buffer = temp;
}

//checks to see if the characters are within the 27 predefined ones
int isValidChars(char* input){
  int size = strlen(input), i = 0, j = 0;
  //foor loop to check the input
  //similar to selection sort
  //http://www.geeksforgeeks.org/selection-sort/
  for(i = 0; i < size; i++){
    int found = 0;
    //loops through charList to see if input is inside the character list
    //if it it was found, then set set foudn = 1 then break
    for(j = 0; j < charSize; j++){
      if(input[i] == charList[j]){
        found = 1;
        break;
      }
    }
    //if it is not found in the set then return 0
    if(!found)
      return 0;
  }
  //if everything is inside the set return 1
  return 1;
}
