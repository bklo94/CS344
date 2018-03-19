//Brandon Lo
//CS344
//Project 4
//otp_enc_d.c

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
void encodeDecode(char **str, char *key, int enc);
int mod(int num, int denom);
int authClient(char *auth_str, char *read);
int getSockMessage(int fd, char **buffer);
int sendSockMessage(int fd, char *outSend, int size);
void resizeBuffer(char **buffer, int new_cap);

//global variables
int connectionCount = 0;
int checkConn = 0;
char charList[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
int maxConn = 5;
int initBuffSize = 500;

//signal handler for the child, deincrements the connectionCount and
void handleChildSig(int inputSignal){
  connectionCount--;
  int exitVal;
  wait(&exitVal);
}

//sets the close flag for the SA_handler
void checkSig(int inputSignal){
  checkConn = 1;
}

//main function
int main(int argc, char **argv){
  if(argc < 2 || !isdigit((int)argv[1][0])){
    fprintf(stderr,"Usage: %s <port>\n", argv[0]);
    return 1;
  }
  //declare the signal handler structs
  struct sigaction SIGCHLD_action = {{0}}, SIGUSR1_action = {{0}};
  //functions to toggle for foreground mode
  //set up structure to specify the new action
  //meant for the parent process
  //setting the sa_flag to prevent zombies
  //https://stackoverflow.com/questions/17015830/how-can-i-prevent-zombie-child-processes
  //Signal ignore use
  //https://www.gnu.org/software/libc/manual/html_node/Basic-Signal-Handling.html
  SIGCHLD_action.sa_handler = handleChildSig;
  sigfillset(&SIGCHLD_action.sa_mask);
  SIGCHLD_action.sa_flags = SA_RESTART;
  SIGUSR1_action.sa_handler = checkSig;
  sigfillset(&SIGUSR1_action.sa_mask);
  SIGUSR1_action.sa_flags = 0;

  //enable signal handler to register handlers with kernal
  //https://stackoverflow.com/questions/5113545/enable-a-signal-handler-using-sigaction-in-c
  // Restore the old action for SIGINT amd SIGTSTP and reset the mask to the old mask with sigaction
  //http://www.cs.kent.edu/~farrell/sys95/notes/examples/prog/signal/
  sigaction(SIGCHLD, &SIGCHLD_action, NULL);
  sigaction(SIGUSR1, &SIGUSR1_action, NULL);

  //variable defintions for flags, port, and buffer size
  int check, checkFg, checkFgConn, portNum, bufferSize;
  //socket variable used to accept calls
  //http://pubs.opengroup.org/onlinepubs/7908799/xns/syssocket.h.html
  socklen_t addressSize;
  //used to fork the child PID
  pid_t childPID;
  //create buffers for the client and authentication
  char *buffer = NULL, *c, *accessErr = "Error: otp_dec cannot use otp_enc_d#", *connExceed = "Error: Connections Exceeded#", *accepted = "accepted#",
       *authSeq = "encode";

  //struct for the client info and server
  //https://stackoverflow.com/questions/11684008/how-do-you-cast-sockaddr-structure-to-a-sockaddr-in-c-networking-sockets-ubu
  struct sockaddr_in servAddr, cliAddr;

  //how to use the sockets
  //http://www.cs.rpi.edu/~moorthy/Courses/os98/Pgms/socket.html
  //gets the port from argv, contains the code for the address family, then convert to network byte order, and then grab the IP address with sin_addr
  memset((char *)&servAddr, '\0', sizeof(servAddr));
  portNum = atoi(argv[1]);
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(portNum);
  servAddr.sin_addr.s_addr = INADDR_ANY;

  //open a new socket
  checkFg = socket(AF_INET, SOCK_STREAM, 0);
  if(checkFg < 0){
    perror("otp_enc_d Error Opening Socket");
    return 1;
  }

  //bind the port and check if it worked or not
  if(bind(checkFg,(struct sockaddr *)&servAddr, sizeof(servAddr)) < 0){
    perror("otp_enc_d Error binding port");
    return 1;
  }

  //listen for incoming connections form the socket and get size of address for the client that will accept
  listen(checkFg, maxConn);
  addressSize = sizeof(cliAddr);

  //loop to check all the requests
  do{
    // Accept a connection, blocking if one is not available until one connects, client info written to cliAddr
    checkFgConn = accept(checkFg,(struct sockaddr *)&cliAddr, &addressSize);
    if(checkFgConn < 0){
      if(checkConn)
        break;
      fprintf(stderr, "Connect Error: %s\n", strerror(errno));
      continue;
    }
    //check if max number of connections reached, then the socket is closed
    //https://stackoverflow.com/questions/27798419/closing-client-socket-and-keeping-server-socket-active
    if(connectionCount  >= maxConn){
      bufferSize = getSockMessage(checkFgConn, &buffer);
      sendSockMessage(checkFgConn, connExceed, strlen(connExceed));
      close(checkFgConn);
      free(buffer);
      buffer = NULL;
      continue;
    }

    //fork childPID if connectionCount is under the maxConn
    childPID = fork();
    //if creating the child failed then print out error and close socket
    if(childPID == -1){
        fprintf(stderr, "otp_dec_d Fork Error! Child Process Creation Failed\n");
        close(checkFgConn);
      }
    //if the child is interacting with the child process
    else if(childPID == 0){
        //read the Socket message and check if it could read the socket
        bufferSize = getSockMessage(checkFgConn, &buffer);
        //if socket couldn't be read, then free the buffer and close the socket
        if(bufferSize < 0){
          free(buffer);
          close(checkFgConn);
          return 1;
        }
        //check if the client has authorization or else send an error
        if((check = authClient(authSeq, buffer)) < 0){
          sendSockMessage(checkFgConn, accessErr, strlen(accessErr) + 1);
          close(checkFgConn);
          return 1;
        }
        //else if client is otp_dec
        else{
          //send the message the clientand free the buffer for the cipher/key
          sendSockMessage(checkFgConn, accepted, strlen(accepted) + 1);
          free(buffer); buffer = NULL;
          bufferSize = getSockMessage(checkFgConn, &buffer);
          //null terminator is inserted here in order to seperate cipher from key
          //how to search for &
          ////https://www.tutorialspoint.com/c_standard_library/c_function_strchr.htm
          c = strchr(buffer, '&');
          *c = 0;
          c++;
          //encode the plaintext in the buffer, flag is set to 1, message is sent to client
          encodeDecode(&buffer, c, 1);
          sendSockMessage(checkFgConn, buffer, strlen(buffer) + 1);
        }
        //close the socket and free the buffer
        close(checkFgConn);
        free(buffer);
        return 0;
      }
      else{
        connectionCount++;
        close(checkFgConn);
      }
  } while(!checkConn);

  return 0;
}

//encodes and decodes the string depending on the flag, 1 to encode, 0 to decode
void encodeDecode(char **str, char *key, int enc){
  //get the size of the plaintext file
  int size = strlen(*str), i = 0;
  //for loop that looks through each character
  for(i = 0; i < size; i++){
    int a, b, index;
    //if the character is space, set it to 25 or else subtract 'A' for the index
    //how to get the index by subtracting
    //https://stackoverflow.com/questions/1747154/how-to-return-the-index-of-ascii-char-in-c
    a =((*str)[i] < 'A') ? 26 :(*str)[i] - 'A';
    b =(key[i] < 'A') ? 26 : key[i] - 'A';
    //if encoding is the sum of a and b, else subtract a - b
    //use the modulous to get the result from the plaintext
    index = mod((enc) ? a + b : a - b, 27);
    //assign the encoded character to str
   (*str)[i] = charList[index];
  }
 (*str)[i] = '#';
}

//modulous function
int mod(int num, int denom){
  //ge the remainder
  int remain = num % denom;
  //wrap the remainder around if < 0 or else return the remainder
  return(remain < 0) ? remain + denom : remain;
}

//search for the authentication string to allow it to authorize the client
//use strstr to exact match
//https://stackoverflow.com/questions/2665106/strstr-whole-string-match?rq=1
int authClient(char *authString, char *input){
  char *c;
  if((c = strstr(input, authString)) == NULL)
    return -1;
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
////https://stackoverflow.com/questions/12917727/resizing-an-array-in-c
void resizeBuffer(char **buffer, int new_cap){
  //allocate the buffer size
  char *temp = calloc(new_cap,sizeof(char));
  //buffer is copied to temp and then freed and temp is set to the new buffer
  strcpy(temp, *buffer);
  free(*buffer);
  *buffer = temp;
}
