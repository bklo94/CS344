//Name:Brandon Lo
//Assign:HW2 CS344
//lob.adventure.c

#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

//preprocessor constants
#define NUMROOMS 7

//mutex and threads declared
pthread_mutex_t objs[2];
pthread_t threads[2];

//struct that holds all the outbound connections, name, the number of current connections, and type
struct room {
  char name[256];
  char outbound[6][256];
  int currentConnections;
  int type;
};

//write the time to a text file
//https://stackoverflow.com/questions/2242963/get-the-current-time-in-seconds
void* writeThread(void *closeThread) {
  int *check = (int*)closeThread;
  char buffer[256];
  time_t t1;
  struct tm *timeOutput;
  FILE *timeFile;
  //the file loops until until the flag is set to close
  while (!*check) {
    //locks at mutex until main thread is unlocked
    pthread_mutex_lock(&objs[0]);
    //check the flag
    if (*check)
      break;
    //opens a text file and makes sure that it is accessible
    timeFile = fopen("./currentTime.txt", "w");
    if (timeFile == NULL) {
      perror("Error\n");
      exit(1);
    }
    //how to use strftime
    //https://stackoverflow.com/questions/1551597/using-strftime-in-c-how-can-i-format-time-exactly-like-a-unix-timestamp
    //use of localtime
    //https://stackoverflow.com/questions/8034469/c-how-to-get-the-actual-time-with-time-and-localtime
    //tm struct infor
    //https://www.tutorialspoint.com/c_standard_library/c_function_gmtime.htm
    else{
      //grab the current time and write the file
      //grab the time with t1, set it to the timeOutput struct, and then fill the buffer with strftime
      time (&t1);
      timeOutput = localtime (&t1);
      strftime(buffer, 50, "%I:%M%p, %A, %B %d, %Y", timeOutput);
      //sets AM/PM to lowercase
      buffer[5] = tolower(buffer[5]);
      buffer[6] = tolower(buffer[6]);
      //convert output to the time that we want, also sets buffer to +1 to take out the 0 from the time
      if ((timeOutput->tm_hour > 12 && timeOutput->tm_hour < 22 ) || timeOutput->tm_hour < 10)
        memmove(buffer, buffer+1, strlen(buffer));
      //write the file with the format
      fputs(buffer, timeFile);
      fclose(timeFile);
    }
    //unlock thread and sleeps it to let main thread to come back
    pthread_mutex_unlock(&objs[0]);
    usleep(50);
  }
  return;
}

//read the file to read the text file for the time
void* readThread(void *closeThread){
  int *check = (int*) closeThread;
  char buffer[256];
  FILE *timeFile;
  //the file loops until until the flag is set to close
  while (!*check) {
    //locks at mutex until main thread is unlocked
    pthread_mutex_lock(&objs[1]);
    //check the flag
    if (*check)
      break;
    //check to make sure the file is readable and gets the info and prints it out
    timeFile = fopen("./currentTime.txt", "r");
    fgets(buffer, 50, timeFile);
    printf("\n%s\n", buffer);
    fclose(timeFile);
    //unlock thread and sleeps it to let main thread to come back
    pthread_mutex_unlock(&objs[1]);
    usleep(50);
  }
  return;
}


//create a room file with process ID
//https://stackoverflow.com/questions/33332533/create-directory-and-store-file-c-programming
//reading to find the directory
//https://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c
//stats man page
//https://linux.die.net/man/2/stat
void getDirectory(char *buffer) {
  //works by storying the times and checking if the recent time is newer
  int oldTime = 0,latestTime = 0;
  struct stat stats;
  DIR *currentDir;
  struct dirent *fileStream;
  //open the directory and check if it's open
  currentDir = opendir("./");
  if (currentDir == NULL) {
    perror("Directory Error:\n");
    exit(1);
  }
  //read the directory
  while (fileStream = readdir(currentDir)) {
    //check file names if it starts with lob.rooms (9 characters)
    if (strncmp(fileStream->d_name, "lob.rooms", 9) == 0) {
      //grabs the stats of the directory
      stat(fileStream->d_name, &stats);
      //stores the time and compares it to the max, if its larger/newer it sets it as the new meax
      latestTime = (int)stats.st_mtime;
      if (latestTime > oldTime) {
        //directory of the newest file is saved with strcpy
        strcpy(buffer, fileStream->d_name);
        oldTime = latestTime;
      }
    }
  }
  //close the directory
  closedir(currentDir);
  return;
}


//read newline
//https://stackoverflow.com/questions/21180248/fgets-to-read-line-by-line-in-files
void populateRooms(char *buffer,struct room *rooms, int *startIndex){
  DIR *currentDir;
  FILE* outStream;
  char tempBuffer[256];
  int index, count = 0;
  struct dirent *fileStream;

  //open directory and make sure that it's readable
  currentDir = opendir(buffer);
  if (currentDir == NULL){
    perror("Unable to find directory.\n");
    exit(1);
  }

  //read the file infor from each directory
  while(fileStream=readdir(currentDir)){
    //ignore the ./ part of the directory name
    if (strlen(fileStream->d_name) > 2){
      //store the path to buffer
      sprintf(tempBuffer, "./%s/%s", buffer, fileStream->d_name);
      //read from buffer and make sure that it's readable
      outStream = fopen(tempBuffer,"r");
      if (outStream == NULL){
        perror("Unable to find file.\n");
        exit(1);
      }

      //get rid of newline
      //https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
      //while fgets use from
      //https://stackoverflow.com/questions/16291686/c-strtok-and-strcpy
      char *c;
      while(fgets(tempBuffer,255,outStream) != NULL){
        c = strchr(tempBuffer, '\n');
        *c = '\0';

        //look for : character and set it as index
        //everything after : is the information we want
        c = strchr(tempBuffer,':');
        index = c - tempBuffer;

        //use of strncmp, c+2 is the space between : and the information
        //http://www.cplusplus.com/reference/cstring/strncmp/

        if(strncmp(tempBuffer, "ROOM NAME", index) == 0)
          strcpy(rooms[count].name, c+2);
        else if (strncmp(tempBuffer, "CONNECTION", index-2) == 0)
          strcpy(rooms[count].outbound[rooms[count].currentConnections++], c+2);
        //type is set as 0 for mid, 1 for start, 2 as end
        else if(strncmp(tempBuffer, "ROOM TYPE", index) == 0){
          strcpy(tempBuffer,c+2);
          if(strcmp(tempBuffer,"END_ROOM") == 0)
              rooms[count].type = 2;
          //starting room is set here
          else if (strcmp(tempBuffer,"START_ROOM") == 0){
              rooms[count].type = 1;
              *startIndex = count;
          }
          //default is everything is set as 0 or mid room
          else
              rooms[count].type = 0;
        }
      }
      fclose(outStream);
      count++;
    }
  }
  closedir(currentDir);
  return;
}

//checks and look for the room names
//uses strcmp to check the newname to the val
int containsName( struct room *rooms, int count, char *val) {
  int i;
  for (i = 0; i < count; i++)
    if (strcmp(rooms[i].name, val) == 0)
      return i;
  return -1;
}

//checks and looks at the names inside each room struct
//uses strcmp to check the names inside the array to val
int containsRoom(char names[6][256], int count, char *val) {
  int i;
  for (i = 0; i < count; i++)
    if (strcmp(names[i], val) == 0)
      return i;
  return -1;
}

//game program to play the game
void playGame(struct room *rooms){
    int i,check = 0,steps = 0,current = 0,roomList[100];
    char *readLine = NULL;
    size_t readSize = 0;
    //sets whatever is in roomList as 0 for the edge case of having your next step as the end room
    roomList[steps++] = current;

    //checks if it is inside a room and then prints out the currentConnections inside the room along with the name
    //check is used to grab the proper user prompt such as after a user calls time then it does not need to call location
    do {
      if (check != INT_MAX) {
        printf("CURRENT LOCATION: %s\n", rooms[current].name);
        printf("POSSIBLE CONNECTIONS:");
        for (i = 0; i < rooms[current].currentConnections; i++) {
          //check if the output is the end or not to determine whether it needs a , or a .
          if (i+1 != rooms[current].currentConnections)
            printf(" %s,", rooms[current].outbound[i]);
          else
            printf(" %s.", rooms[current].outbound[i]);
        }
      }
      printf("\nWHERE TO? >");

      //remove newline character
      //https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
      char *c;
      check = getline(&readLine, &readSize, stdin);
      c = strchr(readLine, '\n');
      *c = '\0';

      //checks if file exists, if it does then it runs thread 1, if not it runs thread 0
      //how to use usleep
      //https://stackoverflow.com/questions/12777254/time-delay-in-c-usleep
      //unlocks thread 0 or the write thread and then locks it
      //unlocks thread 1 or the write thread and the reads the info and locks it
      if (strcmp(readLine, "time") == 0) {
        pthread_mutex_unlock(&objs[0]);
        usleep(15);
        pthread_mutex_lock(&objs[0]);
        pthread_mutex_unlock(&objs[1]);
        usleep(15);
        pthread_mutex_lock(&objs[1]);
        check = INT_MAX;
      }

      //checks the current room to see if the room gotten from user input is inside the struct
      else if(containsRoom(rooms[current].outbound, rooms[current].currentConnections, readLine) < 0)
        printf("\nHUH? I DON'T UNDERSTAND THAT ROOM. TRY AGAIN.\n\n");

      //from user input it grabs the the room's index and searches for it
      //sets the room's index as the new current room
      //sets the room into the roomList which holds the history of where the user went to
      else {
        check = containsName(rooms, 7, readLine);
        if (check < 0)
          continue;
        current = check;
        roomList[steps++] = check;
        printf("\n");
      }
      //when it enters a new room it resets the readLine and size
      free(readLine);
      readLine = NULL;
      readSize = 0;
      //while loop looks for 2 as the type which is the END_ROOM
    } while (rooms[current].type != 2);

    //prints out the end information along with all the names inside the roomList
    printf("YOU HAVE FOUND THE END ROOM. CONGRATULATIONS!\n");
    printf("YOU TOOK %i STEPS. YOUR PATH TO VICTORY WAS:\n", steps-1);
    for (i = 1; i < steps; i++)
      printf("%s\n", rooms[roomList[i]].name);
}

//main program mainly deals with thread initialization and destruction
int main() {
  //allocate 7 room structs in order to create the structures to store the room information
  struct room *rooms = malloc(sizeof(struct room)*NUMROOMS);
  //initialize flags and variables
  int i,check = 0,steps = 0,closeThread = 0,current =0;

  //initialize the threads
  //https://stackoverflow.com/questions/5138778/static-pthreads-mutex-initialization
  for (i = 0; i < 2; i++) {
    if(pthread_mutex_init(&objs[i], NULL) != 0){
      printf("Failed to initialize mutex %d\n", i+1);
      free(rooms);
      exit(1);
    }
    pthread_mutex_lock(&objs[i]);
  }
  //initialize the threads and their actions
  //thread 0 is set to write the text file/timeinfo
  //thread 1 is set to read the text file and print out the info
  pthread_create(&threads[0], NULL, &writeThread, &closeThread);
  pthread_create(&threads[1], NULL, &readThread, &closeThread);

  //buffer used to store file stream
  char buffer[256];
  //run to get the room information from directory and then populate the rooms
  getDirectory(buffer);
  populateRooms(buffer, rooms, &current);
  playGame(rooms);

  //how to safely destroy the threads
  //https://stackoverflow.com/questions/17169697/how-to-correctly-destroy-pthread-mutex
  //destroys the mutexes and frees the memory used to allocate the rooms
  pthread_mutex_unlock(&objs[0]);
  pthread_mutex_unlock(&objs[1]);
  pthread_mutex_destroy(&objs[0]);
  pthread_mutex_destroy(&objs[1]);
  free(rooms);
  return 0;
}
