//Brandon Lo
//CS344
//Project 3
//smallsh.c

//preprocessor directives
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

//define macro for compare function
# ifndef LT
# define LT(A, B) ((A) < (B))
# endif
# ifndef EQ
# define EQ(A, B) ((A) == (B))
# endif

//enum to represent the commands that are built in
enum commands{CD = 0, EXIT, STATUS};

//status flag struct to help hold values to help return the value from terminates processes and hold an exit value flag
struct statusFlags{
  int status;
  int sigCheck;
};

//dynamic array pointer that holds the commands
struct dynArr{
  char** data;
  int currentSize;
  int capacity;
  int start;
};

//dynamic array that holds the PID's
struct pidArr{
  int *data;
  int currentSize;
  int capacity;
};

typedef struct pidArr pidArr;
typedef struct dynArr dynArr;

//initialize the foreground flag with the default as it is off
int foreFlag = 0;

//prototype functions
//command/signal handing
void checkSIGINT(int currentSig);
void checkSIGTSTP(int currentSig);
int readCommands(char *str, dynArr *elements, int *checkFlag);
int trimString(char *out, int size, char *str);
int splitString(char *str, dynArr *cur, char delim);
size_t expand$$(char* str, int index, size_t *strSize);
int numDigits(int pid);
int changeDir(char *path);
void exitProccess(pidArr *pids);
void getStatus(int statusFlag, int sigCheck);
pid_t fgFunction(dynArr *elements, void (*handle_int)(int));
int bgFunction(dynArr *elements);
struct statusFlags getExit(int exitChild);
int redirectInput(dynArr *elements, int defaultNULL);
int redirectOutput(dynArr *elements, int defaultNULL);

int compare(char* left, char* right);

//PID array functions
void initPIDArr(pidArr *a, int capacity);
pidArr* newPIDArr(int size);
void setPIDCapacity(pidArr *a, int newCap);
int getPIDArrSize(pidArr *a);
void addPIDArr(pidArr *a, int val);
void removePIDArr(pidArr *a, int val);
int getPIDArr(pidArr *a, int pos);
int getTopPIDArr(pidArr *a);
void popPIDArr(pidArr *a);
void removeIndexPIDArr(pidArr *a, int index);
int checkEmptyPIDArr(pidArr *a);

//command dynArr functions
void initDynArr(dynArr *a, int capacity);
dynArr* createDynArr(int size);
int indexOfDynArr(dynArr *a, char* val);
int getIndexDynArr(dynArr *a , int index);
void setDynArrCapacity(dynArr *a, int newCap);
void addBackDynArr(dynArr *a, char* val, int size);
char* getBackDynArr(dynArr *a);
void removeBackDynArr(dynArr *a);
char* getDynArr(dynArr *a, int pos);
char* getFrontDynArr(dynArr *a);
int checkEmptyDynArr(dynArr *a);
int getSizeDynArr(dynArr *a);
void removeIndexDynArr(dynArr *a, int index);

//cleanup functions
void deleteArr(pidArr *a);
void freeArr(pidArr *a);
void deleteDynArr(dynArr *a);
void freeDynArr(dynArr *a);
void clearDynArr(dynArr *a);

int main(int argc, char const *argv[]){
  //declare the signal handler structs
  struct sigaction actionSIGINT = {{0}}, actionSIGTSTP = {{0}};
  //set up structure to specify the new action
  //meant for the parent process
  //setting the sa_flag to prevent zombies
  //https://stackoverflow.com/questions/17015830/how-can-i-prevent-zombie-child-processes
  //Signal ignore use
  //https://www.gnu.org/software/libc/manual/html_node/Basic-Signal-Handling.html
  actionSIGINT.sa_handler = SIG_IGN;
  //SIGTSP used to switch between foreground only
  actionSIGTSTP.sa_handler = checkSIGTSTP;
  sigfillset(&actionSIGTSTP.sa_mask);

  //enable signal handler to register handlers with kernal
  //https://stackoverflow.com/questions/5113545/enable-a-signal-handler-using-sigaction-in-c
  // Restore the old action for SIGINT amd SIGTSTP and reset the mask to the old mask with sigaction
  //http://www.cs.kent.edu/~farrell/sys95/notes/examples/prog/signal/
  actionSIGTSTP.sa_flags = SA_RESTART;
  sigaction(SIGINT, &actionSIGINT, NULL);
  sigaction(SIGTSTP, &actionSIGTSTP, NULL);

  //create dynamic array to store the new inpuutedcommands
  dynArr *newCommands = createDynArr(6);
  //dynamic array created for built in commands
  dynArr *commands = createDynArr(3);
  addBackDynArr(commands, "cd", 3);
  addBackDynArr(commands, "exit", 5);
  addBackDynArr(commands, "status", 7);

  //create new array to store the PID's so you can kill it more easily later
  pidArr *bgPID = newPIDArr(20);

  //create character pointers for buffers to store user input, trim the user input and character pointer
  char *buffer = NULL,*trim = NULL,*c;
  //create checks for the exit,background, size for trimming, index, check child exit status, and i for loops
  int checkFlag = 0,checkBG = 0,newSize, index,exitChild = -1, i;
  //stores the child PID
  pid_t childPID = -1;

  //uses statusFlags struct to store the exit value and signal value
  struct statusFlags statusFlag;
  statusFlag.status = -1;
  statusFlag.sigCheck = -1;

  //used buffersizes for the getline function
  size_t bufferSize = 0,bufferCap = 0;

  //do while loop to get user commands
  do {
    //print  prompt  and get userinput with getline
    //https://www.gnu.org/software/gawk/manual/html_node/Getline.html
    printf(": ");
    fflush(stdout);
    bufferSize = getline(&buffer, &bufferCap, stdin);
    //searches for the new line
    //https://www.tutorialspoint.com/c_standard_library/c_function_strchr.htm
    if((c = strchr(buffer, '\n')) != NULL)
      *c = 0;

    //trim the string if it was empty or there's whitespace in the input
    trim = malloc(sizeof(char) * bufferSize);
    newSize = trimString(trim, bufferSize, buffer);
    if(newSize == -1 || trim[0] == '#'){
      if(trim != NULL){
        free(trim);
        trim = NULL;
      }
    }

    //if command was not empty or is a comment
    else {
      //parse through the commands
      readCommands(trim, newCommands, &checkBG);
      //if the command is built in then use it
      if((index = indexOfDynArr(commands, getFrontDynArr(newCommands))) != -1){
        //if cd command used
        if(index == CD)
          changeDir(getDynArr(newCommands, 1));
        //if exit command used, then kill all background PID's
        //set the checkFlag which is used to exit the while loop
        else if(index == EXIT){
          exitProccess(bgPID);
          checkFlag = 1;
        }
        //if status command used
        else if(index == STATUS)
          getStatus(statusFlag.status, statusFlag.sigCheck);
      }
      //else if the command is not built in
      else {
        //check if background check is null or if in foreground only mode
        if(!checkBG || foreFlag){
          //handle foreground commands and pass SIGINT handler to childPID
          childPID = fgFunction(newCommands, &checkSIGINT);
          //wait until child process is complete
          childPID = waitpid(childPID, &exitChild, 0);
          //send exit signal
          statusFlag = getExit(exitChild);
          //print signal
          if(statusFlag.sigCheck)
            printf("terminated by signal %d\n", statusFlag.status);
        }
        //else it is a background command
        else {
          //call background function
          childPID = bgFunction(newCommands);
          //add the background PID to the array
          addPIDArr(bgPID, (int)childPID);
          //print the PID
          printf("Background pid is %d\n", (int)childPID);
        }
      }
    }
    //if the background array is not  empty
    if(!checkEmptyPIDArr(bgPID)){
      //loop through all the the PID's looking for the exit
      for(i = 0; i < getPIDArrSize(bgPID); i++){
        //use the WNOHANG flag to make sure that it completes the process
        //https://www.gnu.org/software/libc/manual/html_node/Process-Completion.html
        pid_t temp = waitpid((pid_t)getPIDArr(bgPID, i), &exitChild, WNOHANG);
        //if temp is not 0, then it has exited
        if(temp != 0){
          //get exit status and then print it out
          struct statusFlags stat = getExit(exitChild);
          printf("Background pid %d is done: %s %d\n", (int)temp,
              (stat.sigCheck) ? "terminated by signal " : "exit status ", stat.status);
          //once it has exited, remove the PID from the background PID array
          removePIDArr(bgPID, getPIDArr(bgPID,i));
          i--;
        }
      }
    }
    //clear the last commands and trim it
    clearDynArr(newCommands);
    if(trim != NULL)
      free(trim);
    //clear the buffers for the next command
    free(buffer);
    trim = NULL;
    buffer = NULL;
  } while(!checkFlag);

  //free the memory allocated to the arrays
  deleteArr(bgPID);
  deleteDynArr(newCommands);
  deleteDynArr(commands);
  return 0;
}

//Commands for the smallsh shell

//signal interrupt handler for foreground meant for foregrand commands SA_REST will be set s oSIGINT will kill on second raise
//http://www.cplusplus.com/reference/csignal/signal/
void checkSIGINT(int currentSig){
  raise(SIGINT);
}

//SIGTSTP Handler that checks if foreFlag is 0 or 1 and switches between foreground and background
//if foreFlag is 1 then smallsh is running in the foreground
//how to get back to shell after signal stop for ctrl C and ctrl Z
//https://stackoverflow.com/questions/33723635/how-to-get-back-to-shell-after-sigtstp
void checkSIGTSTP(int currentSig){
  //checks if foreflag is 1 or 0. If foreflag isn't set, then it writes a message to smallsh with write
  //using write for stdin and stout
  //https://stackoverflow.com/questions/7383803/writing-to-stdin-and-reading-from-stdout-unix-linux-c-programming/7388389
  if(!foreFlag){
    char *message = "\nEntering foreground-only mode(& is now ignored)\n: ";
    write(STDOUT_FILENO, message, strlen(message));
    foreFlag = 1;
  }
  //output message that it is exiting foreground mode and the foreflag is reset
  else {
    char *message = "\nExiting foreground-only mode\n: ";
    write(STDOUT_FILENO, message, strlen(message));
    foreFlag = 0;
  }
}

//Handling character buffer input to see if command is in the dynamic array and if it is in the background
int readCommands(char *str, dynArr *elements, int *checkFlag){
  *checkFlag = 0;
  int check;
  //splits the string by delimiter into an array with str_split
  check = splitString(str, elements, ' ');
  //error check if string cannot be split
  if(check == -1)
    return -1;
  //if the last word is &, then you set the flag to 1 and remove the & from the the dynamic array
  if(strcmp(getBackDynArr(elements), "&") == 0){
    *checkFlag = 1;
    removeBackDynArr(elements);
  }
  return 0;
}

//takes out the trailing whitespace. If the string is empty, then it returns -1
int trimString(char *out, int size, char *str){
  if(size == 0)
    return 0;
  const char *endPos;
  size_t outputSize;
  //use of isspace to check whitespace
  //https://www.tutorialspoint.com/c_standard_library/c_function_isspace.htm
  while(isspace((unsigned char)*str))
    str++;
  //if the entire string is whitespace
  if(*str == 0)
    return -1;
  //the trailing whitespace is trim started from the back until a non whitespace is found
  endPos = str + strlen(str) - 1;
  while(endPos > str && isspace((unsigned char)*endPos))
    endPos--;
  //null term /0 is added
  endPos++;
  //output size is new string length and buffersize - 1
  //the string is them copied and has a null terminator added
  outputSize = (endPos - str) < size-1 ? (endPos - str) : size-1;
  memcpy(out, str, outputSize);
  out[outputSize] = 0;
  return outputSize;
}

//splits the String buffer by delimiter into dynArr
int splitString(char *str, dynArr *cur, char delim){
  FILE *stream = fmemopen(str, strlen(str), "r");
  //error check to see if the stream is opened
  if(stream == NULL)
    return -1;
  //create buffer and a character pointer
  char *buffer = NULL,*c;
  size_t bufferCapacity = 0, bufferSize;

  //How to get the delim  from the buffer
  //http://pubs.opengroup.org/onlinepubs/9699919799/functions/getdelim.html
  //how to use getdelim
  //http://en.cppreference.com/w/c/experimental/dynamic/getline
  while((bufferSize = getdelim(&buffer, &bufferCapacity, delim, stream)) != -1){
    //searches for the end space in the buffer with strchr and then make null terminator
    c = strchr(buffer, ' ');
    //error check if c is not null
    if(c != NULL)
      *c = 0;
    //if space isn't found size is increased since getdelim doesn't count null
    else
      bufferSize += 1;
    //searches for the "$$" command, expand smallsh pid
    //index is set as offset between c and buffer and then the buffersize is expanded
    while((c = strstr(buffer, "$$")) != NULL){
      int index = c - buffer;
      bufferSize = expand$$(buffer, index, &bufferCapacity);
    }
    //add the word to the dynamic array
    addBackDynArr(cur, buffer, bufferSize);

    //free the buffer and allocate space for next work
    free(buffer);
    buffer = NULL;
    bufferCapacity = 0;
  }
  //free buffer and memstream
  free(buffer);
  fclose(stream);
  return 0;
}

//when "$$" calls this function, then it expands to process pid
size_t expand$$(char* str, int index, size_t *strSize){
  //initialize the pid, the number of digits in the pid, the size of ths tring, and a resize flag
  int shellPID = (int)getpid(), lenPID = numDigits(shellPID), bufferSize = strlen(str),resize = 0;
  //create a dynamic pidArr to hold nonexpanded elements
  dynArr *buffers = createDynArr(2);
  //$$ is replaced with null terminating character \0\0
  *(str + index) = 0;
  *(str + index + 1) = 0;
  //check if the current string size need to be resized then set the resize flag and resize it
  if(*strSize <= bufferSize + lenPID){
    resize = 1;
    *strSize = (strlen(str) + lenPID) * sizeof(char);\
  }
  //if $$ is at the start of the string then you add the str to the dynArr and resize the string if needed
  //the pid is printed at the end
  if(index == 0){
    addBackDynArr(buffers, str + index + 2, strlen(str + index + 2));
    if(resize){
      free(str);
      str = malloc(*strSize);
    }
    sprintf(str, "%d%s", shellPID, getBackDynArr(buffers));
  }
  //if $$ is at the end of the string then you add the str to the dynArr and resize the string if needed
  //the pid is printed at the end
  else if(index + 2 == bufferSize){
    addBackDynArr(buffers, str, strlen(str));
    if(resize){
      free(str);
      str = malloc(*strSize);
    }
    sprintf(str, "%s%d", getBackDynArr(buffers), shellPID);
  }
  //else if $$ is somewhere else in the string then you add the str to the dynArr and resize the string if needed
  //the pid is printed at the end
  else {
    addBackDynArr(buffers, str, strlen(str));
    addBackDynArr(buffers, str + index + 2, strlen(str + index + 2));
    if(resize){
      free(str);
      str = malloc(*strSize);
    }
    sprintf(str, "%s%d%s", getFrontDynArr(buffers), shellPID, getBackDynArr(buffers));
  }
  //free dynArr and return the strlen
  deleteDynArr(buffers);
  return strlen(str);
}

//if pid is < 10 then you divide by 10 until the base is found
//1 is added for each digit recursively
int numDigits(int pid){
  return (pid < 10) ? 1 : 1 + numDigits(pid / 10);
}

//changes the current directory to a specified path, but if no path is specified, then it goes to home
int changeDir(char *path){
  int check;
  //if cd doesn't specify anything then it moves to home
  if(path == NULL)
    check = chdir(getenv("HOME"));
  else
    check = chdir(path);
  //error check and return 1 if changing directory is unsucessful
  if(check < 0){
    printf("Error: %s\n", strerror(errno));
    return 1;
  }
  return 0;
}

//exits the background process and sends a SIGKILL to each bg process of smallsh
void exitProccess(pidArr *pids){
  int exitChild = 0;
  pid_t curPID;
  //while loop parses PID array until every process in the PID array is killed
  //wait for the PID to be killed and then removes the PID from the array once it is killed
  while(!checkEmptyPIDArr(pids)){
    curPID = (pid_t)getTopPIDArr(pids);
    kill(curPID, SIGKILL);
    waitpid(curPID, &exitChild, 0);
    popPIDArr(pids);
  }
  return;
}

//if the status command is called return the int flag
void getStatus(int statusFlag, int sigCheck){
  //if there is no forground command
  if(statusFlag == -1 && sigCheck == -1){
    printf("No command has been executed\n");
    return;
  }
  //if command is terminated, then print the signal value
  if(sigCheck)
    printf("terminated by signal %d\n", statusFlag);
  //else print the exit value
  else
    printf("exit value %d\n", statusFlag);
}

//foreground function that forks a child process, registers signal handlers, redirectio of stdin/stdout
pid_t fgFunction(dynArr *elements, void (*handle_int)(int)){
  pid_t newPID = -1;
  //args set for execvp
  char **args = NULL;
  //signal handler structs initialized
  struct sigaction actionSIGINT = {{0}}, actionSIGTSTP = {{0}};
  int check, i;
  //child is forked
  newPID = fork();
  //error check
  if(newPID == -1){
    perror("Error");
    exit(1);
  }
  //signal handling
  //https://www.gnu.org/software/libc/manual/html_node/Basic-Signal-Handling.html#Basic-Signal-Handling
  //signal handling structure taken from
  //https://www.gnu.org/software/libc/manual/html_node/Sigaction-Function-Example.html
  //how to use sigaction strut
  //http://man7.org/linux/man-pages/man2/sigaction.2.html
  else if(newPID == 0){
      //set up structure to specify the new action
      //meant for the child process
      //setting the sa_flag to prevent zombies
      //https://stackoverflow.com/questions/17015830/how-can-i-prevent-zombie-child-processes
      actionSIGINT.sa_handler = handle_int;
      sigfillset(&actionSIGINT.sa_mask);
      actionSIGINT.sa_flags = SA_RESETHAND;

      //enable signal handler to register handlers with kernal
      //https://stackoverflow.com/questions/5113545/enable-a-signal-handler-using-sigaction-in-c
      // Restore the old action for SIGINT amd SIGTSTP and reset the mask to the old mask with sigaction
      //http://www.cs.kent.edu/~farrell/sys95/notes/examples/prog/signal/
      actionSIGTSTP.sa_handler = SIG_IGN;
      sigaction(SIGINT, &actionSIGINT, NULL);
      sigaction(SIGTSTP, &actionSIGTSTP, NULL);

      //attempt to redirect input
      check = redirectInput(elements, 0);
      if(check == -1)
        exit(1);
      //attempt to redirect output
      check = redirectOutput(elements, 0);
      if(check == -1)
        exit(1);

      //filll the array with command currently in dynArr
      args = malloc(sizeof(char*) * getSizeDynArr(elements));
      //for loop that goes through the buffer and adds itt to the args array
      for(i = 0; i < getSizeDynArr(elements); i++){
        char *buffer = getDynArr(elements, i);
        args[i] = malloc(sizeof(char) * strlen(buffer));
        strcpy(args[i], buffer);
      }

      //how to use execvp
      //https://stackoverflow.com/questions/23417442/how-to-match-string-to-execvp-in-c
      //execvp used to replace copy of smallsh with command
      args[i] = NULL;
      execvp(args[0], args);
      //error check execvp
      fprintf(stderr, "%s: %s\n", args[0], strerror(errno));
      exit(1);
  }
  return newPID;
}

//background function that forks a child process, registers signal handlers, redirectio of stdin/stdout
//similar structure to foreground function
int bgFunction(dynArr *elements){
  pid_t newPID = -1;
  char **args = NULL;
  //signal handler structs initialized
  struct sigaction actionSIGINT = {{0}}, actionSIGTSTP = {{0}};
  int check,i;
  //child is forked
  newPID = fork();
  //error check
  if(newPID == -1){
    perror("Error");
    exit(1);
  }

  else if(newPID == 0){
    //enable signal handler to register handlers with kernal
    //https://stackoverflow.com/questions/5113545/enable-a-signal-handler-using-sigaction-in-c
    // Restore the old action for SIGINT amd SIGTSTP and reset the mask to the old mask with sigaction
    //http://www.cs.kent.edu/~farrell/sys95/notes/examples/prog/signal/
    //Signal ignore use
    //https://www.gnu.org/software/libc/manual/html_node/Basic-Signal-Handling.html
    actionSIGINT.sa_handler = SIG_IGN;
    actionSIGTSTP.sa_handler = SIG_IGN;
    sigaction(SIGINT, &actionSIGINT, NULL);
    sigaction(SIGTSTP, &actionSIGTSTP, NULL);
    //attempt to redirect input
    check = redirectInput(elements, 1);
    if(check == -1)
      exit(1);
    //attempt to redirect output
    check = redirectOutput(elements, 1);
    if(check == -1)
      exit(1);

    //filll the array with command currently in dynArr
    args = malloc(sizeof(char*) * getSizeDynArr(elements));
    //for loop that goes through the buffer and adds itt to the args array
    for(i = 0; i < getSizeDynArr(elements); i++){
      char *buffer = getDynArr(elements, i);
      args[i] = malloc(sizeof(char) * strlen(buffer));
      strcpy(args[i], buffer);
    }
    //how to use execvp
    //https://stackoverflow.com/questions/23417442/how-to-match-string-to-execvp-in-c
    //execvp used to replace copy of smallsh with command
    args[i] = NULL;
    execvp(args[0], args);
    exit(1);
  }
  return newPID;
}

//checks the exitChild flag to see if it needs to be redirecte by returning the exit status.
//If there is a terminating signal it returns the status
struct statusFlags getExit(int exitChild){
  struct statusFlags statusFlag;
  //check if process exited with WIFEXITED
  //https://www.gnu.org/software/libc/manual/html_node/Process-Completion-Status.html
  //if it did not return normalled then it returns nonzero
  //set the signal Check flag and the current status of WEXITSTATUS status to status
  //WEXITSTATUS macro evaluates and returns exit status
  if(WIFEXITED(exitChild) != 0){
    statusFlag.sigCheck = 0;
    statusFlag.status = WEXITSTATUS(exitChild);
    return statusFlag;
  }
  //else set the flags and use WTERMSIG to return the flag that caused the child process to exit(terminating signal)
  else{
    statusFlag.sigCheck = 1;
    statusFlag.status = WTERMSIG(exitChild);
    return statusFlag;
  }
}

//redirects the dynArr to the file specified with "<"
int redirectInput(dynArr *elements, int defaultNULL){
  //grab the index of where the "<" is called
  int index = indexOfDynArr(elements, "<");
  int fileDescriptor;

  //check to see if file was specified and a "<" is in the command
  //if it was found, then open a file to be readonly
  //use of dup2 and O_RDONLY to clone a file descriptor
  //https://stackoverflow.com/questions/9084099/re-opening-stdout-and-stdin-file-descriptors-after-closing-them
  if(index != -1 && index < getSizeDynArr(elements)-1){
    fileDescriptor = open(getDynArr(elements,index + 1), O_RDONLY);
    if(fileDescriptor == -1){
      fprintf(stderr, "cannot open %s for input: %s\n", getDynArr(elements, index+1), strerror(errno));
      return -1;
    }
    //clones a file descriptor and then removes the redirect from the command
    //dup2 used to redirect stdin to stdin file
    //http://www.cs.loyola.edu/~jglenn/702/S2005/Examples/dup2.html
    dup2(fileDescriptor, 0);
    removeIndexDynArr(elements, index);
    removeIndexDynArr(elements, index);
  }

  //if redirect command wasn't specified or file forgotten to be typed
  else {
    //remove the redirect symbol if it's there
    if(index != -1)
      removeIndexDynArr(elements,index);
    //redirect to /dev/null if flag was set
    if(defaultNULL){
      fileDescriptor = open("/dev/null", O_RDONLY);
      if(fileDescriptor == -1){
        fprintf(stderr, "cannot open %s for input: %s\n", "/dev/null", strerror(errno));
        return -1;
      }
      //clones a file descriptor
      //dup2 used to redirect stdin to file
      //http://www.cs.loyola.edu/~jglenn/702/S2005/Examples/dup2.html
      dup2(fileDescriptor, 0);
    }
  }
  return 0;
}

//when redirectOutput function called with >, then it prints the output of the file
int redirectOutput(dynArr *elements, int defaultNULL){
  //grab the index of where the ">" is called
  int index = indexOfDynArr(elements, ">");
  int fileDescriptor;

  //check to see if file was specified and a ">" is in the command
  if(index != -1 && index < getSizeDynArr(elements)-1){
    //open the file for writing, or Create if it doesn't exit or truncate if it already exists
    //structure from
    //https://stackoverflow.com/questions/16180837/redirect-stdout-with-pipes-and-fork-in-c
    fileDescriptor = open(getDynArr(elements, index + 1), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fileDescriptor == -1){
      fprintf(stderr, "cannot open %s for output: %s\n", getDynArr(elements, index+1), strerror(errno));
      return -1;
    }
    //clones a file descriptor and then removes the redirect from the command
    //dup2 used to redirect output to output file
    //http://www.cs.loyola.edu/~jglenn/702/S2005/Examples/dup2.html
    dup2(fileDescriptor, 1);
    removeIndexDynArr(elements, index);
    removeIndexDynArr(elements, index);
  }

  //if redirect command wasn't specified or file forgotten to be typed
  else {
    if(index != -1)
      removeIndexDynArr(elements,index);
    //redirect to /dev/null if flag was set
    if(defaultNULL){
      fileDescriptor = open("/dev/null", O_WRONLY);
      if(fileDescriptor == -1){
        fprintf(stderr, "cannot open %s for output: %s\n", "/dev/null", strerror(errno));
        return -1;
      }
      //dup2 used to redirect output to output file
      //http://www.cs.loyola.edu/~jglenn/702/S2005/Examples/dup2.html
      dup2(fileDescriptor, 1);
    }
  }
  return 0;
}


//General function
int compare(char* left, char* right){
  if(strcmp(left, right) < 0)
    return -1;
  else if(strcmp(left, right) > 0)
    return 1;
  else
    return 0;
}

//Arr functions meant to store the PID's

//initialize the Arr to store the PID
void initPIDArr(pidArr *a, int capacity){
  //make sure you don't initialize and array that is > 0 and that a is not null
	assert(capacity > 0);
	assert(a!= 0);

  //allocate the memory
	a->data = (int *) malloc(sizeof(int) * capacity);
	assert(a->data != 0);
	a->currentSize = 0;
	a->capacity = capacity;
}

//stores the PID's
//allocate and initialize an pidArr
pidArr* newPIDArr(int size){
  //make sure size is not 0
	assert(size > 0);

  //allocate the memory
	pidArr *a = (pidArr *)malloc(sizeof( pidArr));
	assert(a != 0);
	initPIDArr(a,size);
	return a;
}

//resize the pidArr capacity to the new size
void setPIDCapacity(pidArr *a, int newCap){
  int i = 0;
  //allocate the memory to the size capacity
  int *temp = (int *)malloc(newCap * sizeof(int));

  //copy old array to the temp array
  for(i = 0; i < a->currentSize; i++)
     temp[i] = a->data[i];
  //free the old array
  free(a->data);
  //set the old array to the temp array and change the capacity to the new array capacity
  a->data = temp;
  a->capacity = newCap;
}

//return the currentSize of the array
int getPIDArrSize(pidArr *a){
	return a->currentSize;
}

//add int value(PID) to the array
void addPIDArr(pidArr *a, int val){
  //check if array needs to be resized, if needs to be resized, double it
  if(a->currentSize >= a->capacity)
      setPIDCapacity(a, 2 * a->capacity);
  //else add the value to the current array
  a->data[a->currentSize] = val;
  a->currentSize++;
  return;
}

//remove everything from pidArr
void removePIDArr(pidArr *a, int val){
  int i = 0;
  for(i = 0; i < a->currentSize; i++){
      if( val == a->data[i] ){
          removeIndexPIDArr(a, i);
          return;
      }
  }
}

//get the element from the position in the array
int getPIDArr(pidArr *a, int pos){
  //check to see if the position exists inside the current array
	assert(pos >= 0 && pos <= a->currentSize);
  return a->data[pos];
}

//return the "top" element from the array
int getTopPIDArr(pidArr *a){
	return (a->data[a->currentSize - 1]);
}

//remove the "top" element from the array
void popPIDArr(pidArr *a){
	removeIndexPIDArr(a, a->currentSize - 1);
}

//remove an item at the position from the array
void removeIndexPIDArr(pidArr *a, int index){
  int i = index;
  ////check to see if the position exists inside the current array
  assert(index >= 0 && index <= a->currentSize);

  //changes the currentSize
  if(index == a->currentSize - 1)
    a->currentSize--;

  //deletes the element from the array by shift everything left
  //concept from here
  //http://www.codeforwin.in/2015/07/c-program-to-delete-element-from-array.html
  else{
    for(i = 0; i < a->currentSize - 1; i++)
      a->data[i] = a->data[i + 1];
    a->currentSize--;
  }
}

//checks to see if the array is empty
int checkEmptyPIDArr(pidArr *a){
	if(a->currentSize == 0)
        return 1;
  return 0;
}

//dynArr functions to hold the commands

//initialize the dynamic array
void initDynArr(dynArr *a, int capacity){
  //make sure you don't initialize and array that is > 0 and that v is not null
	assert(capacity > 0);
	assert(a != 0);

  //allocate the memory
	a->data = (char* *) malloc(sizeof(char*) * capacity);
	assert(a->data != 0);
	a->currentSize = 0;
	a->capacity = capacity;
	a->start = 0;
}

//allocate and initialize dynamic array for integers
dynArr* createDynArr(int size){
	assert(size > 0);
	dynArr *a = (dynArr *)malloc(sizeof( dynArr));
	assert(a != 0);
	initDynArr(a,size);
	return a;
}

//get the index of the character,
int indexOfDynArr(dynArr *a, char* val){
  int i = 0;
  //for loop searches through the dynArr and compares the character
	for(i = 0; i < getSizeDynArr(a); i++)
      if(compare(a->data[getIndexDynArr(a,i)], val) == 0 )
         return i;
  //if character is not found it returns -1
  return -1;
}

//get the index so the array goes from front to back instead of wrapping
int getIndexDynArr(dynArr *a , int index){
	int offset = a->start + index;
	int absIndex = offset;

  //if offset is already inside the array
	if(offset < 0)
		absIndex = offset + a->capacity;

  //if offset starts to wrap
	if(offset >= a->capacity)
		absIndex = offset - a->capacity;

  //return the index
	return absIndex;
}

//resize the dynamic array to the new capaccity
void setDynArrCapacity(dynArr *a, int newCap){
	int i;
  //allocate the memory to the size capacity
	char** newData = (char**)malloc(sizeof(char*)*newCap);
	assert(newData != 0);
  //copy old array to the temp array
	for(i = 0; i <  a->currentSize; i++){
		newData[i] = a->data[getIndexDynArr(a,i)];
	}
  //set the remaining values to 0 to fill array
	for(i = a->currentSize; i < newCap; i++)
		newData[i] = 0;
  //free the old array
	free(a->data);
  //set the old array to the temp array and change the capacity to the new array capacity
	a->data = newData;
	a->capacity = newCap;
	a->start = 0;
}

//adds to the back of the dynamic array
void addBackDynArr(dynArr *a, char* val, int size){
	int index;
  //allocate memory
  char* temp = malloc(sizeof(char*) * size);
  //check if it needs to resize
	if(a->currentSize >= a->capacity)
		setDynArrCapacity(a, 2*a->capacity);

  //copy the value over and set the new currentSize
	strcpy(temp, val);
	index = getIndexDynArr(a, a->currentSize);
	a->data[index] = temp;
	a->currentSize++;
}

//return the last value in the dynamic array
char* getBackDynArr(dynArr *a){
  //check if array is empty
	if(checkEmptyDynArr(a))
		return NULL;
  //return the last value in the currentSize-1
	int index = getIndexDynArr(a, a->currentSize - 1);
	return a->data[index];
}

//remove the last item in the dynamic array
void removeBackDynArr(dynArr *a){
  //check if array is empty
  if(checkEmptyDynArr(a))
		return;
  //else free the last value's memory and reinitialize the new currentSize
  free(a->data[getIndexDynArr(a, a->currentSize-1)]);
	a->currentSize--;
}

//get element from the dynArr from the position
char* getDynArr(dynArr *a, int pos){
  //check to see if position is insize the array
  if(pos >= a->currentSize || pos < 0 || getSizeDynArr(a) <= pos)
     return NULL;
  return a->data[getIndexDynArr(a, pos)];
}

//get item from the front of the array
char* getFrontDynArr(dynArr *a){
  //check to see if it is empty
	if(checkEmptyDynArr(a))
		return NULL;
	return a->data[getIndexDynArr(a,0)];
}

//checks currentSize to see if it is empty
int checkEmptyDynArr(dynArr *a){
	return !(a->currentSize);
}

//returns the currentSize of the array based on the struct
int getSizeDynArr(dynArr *a){
	return a->currentSize;
}

//removes the item at the dynArr based on the index
void removeIndexDynArr(dynArr *a, int index){
  int i;
  //checks to see if the index is inside the array
  if(index >= a->currentSize || index < 0)
    return;
  //free the value
  free(a->data[getIndexDynArr(a,index)]);
  //shift everything right and reinitialize the new currentSize
  for(i = index; i <= a->currentSize-2; i++)
    a->data[getIndexDynArr(a,i)] = a->data[getIndexDynArr(a,i+1)];
  a->currentSize--;
  return;
}

//cleanup functions

//deletes the array and free s the memory
void deleteArr(pidArr *a){
	freeArr(a);
	free(a);
  return;
}

//frees the array and resets the struct
void freeArr(pidArr *a){
	if(a->data != 0){
		free(a->data);
		a->data = 0;
	}
	a->currentSize = 0;
	a->capacity = 0;
  return;
}

//deletes the dynArr
void deleteDynArr(dynArr *a){
	freeDynArr(a);
	free(a);
  return;
}

//frees the dynArr and resets the struct
void freeDynArr(dynArr *a){
	if(a->data != 0){
        clearDynArr(a);
		free(a->data);
		a->data = 0;
	}
	a->currentSize = 0;
	a->capacity = 0;
	a->start = 0;
  return;
}

//clears/frees all the elements in the dynamic array
void clearDynArr(dynArr *a){
  int i = 0;
  for(i = 0; i < a->currentSize; i++)
    free(a->data[getIndexDynArr(a,i)]);
  a->currentSize = 0;
  a->start = 0;
  return;
}
