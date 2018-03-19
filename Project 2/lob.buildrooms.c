//Name:Brandon Lo
//Assign:HW2 CS344
//lob.buildrooms.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <time.h>

//preprocessor constants
#define MIN_CONNECTIONS 3
#define MAX_CONNECTIONS 6
#define NUMROOMS 7

//initialize the roomNames, types, an array of indexes(matches the position of room names), temp array for use later
char* roomNames[10] = {"California","Oregon","Washington","Nevada","Arizona","Utah","Idaho","Colorado","Texas","Florida"};
char* type[3] = {"START_ROOM","MID_ROOM","END_ROOM"};
int indexArr[10] = {0,1,2,3,4,5,6,7,8,9};
int tempRooms[7];

//create a struct of ints since it would be the simplest to just grab the relative position of the roomNames
//total connections = randomly generated
//current connections = depends on what is in the outbound array
struct room{
  char *type;
  int name;
  int totalConnections;
  int currentConnections;
  int outbound[MAX_CONNECTIONS];
};

//create a room file with process ID
//https://stackoverflow.com/questions/33332533/create-directory-and-store-file-c-programming
void createDir(int pid, struct room roomList[]){
  FILE *outFile;
  char folderDIR[256];
  int i;
  //create the room file with pid at the end
  sprintf(folderDIR,"lob.rooms.%d",pid);
  //check to see if permissions are readable
  if (mkdir(folderDIR, 0777) != 0) {
    perror("Error Making Directory.\n");
    exit(1);
  }
  //write all the data to the file
  for (i=0;i < 7;i++) {
    sprintf(folderDIR, "./lob.rooms.%d/%s", pid, roomNames[tempRooms[roomList[i].name]]);
    outFile = fopen(folderDIR, "w");
    //error check if the file is writeable
    if (outFile == NULL) {
      perror("Error opening file.\n");
      exit(1);
    }
    //set the room name and then puts it in the file
    sprintf(folderDIR, "ROOM NAME: %s\n", roomNames[tempRooms[roomList[i].name]]);
    fputs(folderDIR, outFile);
    //sets all the connections inside the file
    int x;
    for(x=0; x < roomList[i].currentConnections;x++){
      int temp = x+1;
      sprintf(folderDIR, "CONNECTION %i: %s\n", temp, roomNames[tempRooms[roomList[i].outbound[x]]]);
      fputs(folderDIR, outFile);
    }
    //sets the room type inside the file
    sprintf(folderDIR, "ROOM TYPE: %s\n", roomList[i].type);
    fputs(folderDIR, outFile);

    fclose(outFile);
  }
}

//get random room using rand in a range
//https://stackoverflow.com/questions/1202687/how-do-i-get-a-specific-range-of-numbers-from-rand
int getRandomRoom(int min, int max){
  int r = rand() % (max + 1 - min) + min;
  return r;
}

//deletes the element from the array by shift everything left
//concept from here
//http://www.codeforwin.in/2015/07/c-program-to-delete-element-from-array.html
void deleteElement(int index){
  int counter;
  int size = sizeof(indexArr)/sizeof(indexArr[0]);
  for(counter = index; counter < size - 1; counter++){
        indexArr[counter] = indexArr[counter+1];
    }
}

//for loop to find if the a value is currently in the array inside the struct
bool contains(int *arr, int count, int val) {
  int i;
  for (i = 0; i < count; i++)
    if (arr[i] == val)
      return 1;
  //default is that it is not found
  return 0;
}

//assigns the rooms and sets them based on position
void buildRooms(){
  int randRooms[7];
  struct room roomList[7];
  int i = 0;
  int size = sizeof(indexArr)/sizeof(indexArr[0])-1;

  // Seed random generator
  time_t t;
  srand((unsigned)time(&t));

  //grabs 7 random numbers
  //moves the array over and "deletes" the element to make sure no number is grabbed twice
  while (i != 7){
    int temp = getRandomRoom(0,size);
    *(randRooms+i) = indexArr[temp];
    deleteElement(temp);
    //printf("%i ",temp);
    i++;
    size--;
  }
  //temp array is used to grab the room position and the struct grabs the position inserted into temp
  //this allows that rand is never able to grab rooms outside the 7 that are grabbed
  int a;
  for (a=0; a< 7;a++){
    tempRooms[a] = randRooms[a];
    roomList[a].name = a;
    //by default everything is set as mid room
    roomList[a].type = type[1];
    //set random number of connections 3-6 to the room
    int tempConn = getRandomRoom(3,6);
    roomList[a].totalConnections = tempConn;
    roomList[a].currentConnections = 0;
  }
  //set the start and end rooms randomly
  int start = getRandomRoom(0,6);
  //set a new seed to make make sure that it randomly grabs a new number
  srand (time(NULL));
  int end = getRandomRoom(0,6);
  //while loop to make sure rand are not the same
  while (end == start){
    end = getRandomRoom(0,6);
  }
  //set the room types
  roomList[start].type = type[0];
  roomList[end].type = type[2];

  //loop through all 7 room files to set the outbound connections
  int x;
  for(x=0; x < 7; x++){
    //use the current and total connections. current is number of currently connected rooms. total connections is the randomly assigned number of rooms from 3-6
    while(roomList[x].currentConnections < roomList[x].totalConnections){
      int temp = getRandomRoom(0,6);
      //make sure that it is different from the current struct and that it doesn't already contain the position
      while(contains(roomList[x].outbound,roomList[x].currentConnections,temp) || roomList[x].name == temp){
        temp = getRandomRoom(0,5);
      }
      //set the random position inside the struct
      roomList[x].outbound[roomList[x].currentConnections] = temp;
      //checks the random position's room to see if the current room needs to be added
      if(contains(roomList[temp].outbound,roomList[temp].currentConnections,roomList[x].name) == 0){
        //printf("%i\n", temp);
        roomList[temp].outbound[roomList[temp].currentConnections] = roomList[x].name;
      }
      roomList[x].currentConnections = roomList[x].currentConnections + 1;
    }
  }
  //creates the directory with the pid
  int pid = getpid();
  createDir(pid,roomList);
}

//main program that builds the room
int main(int argc, char const *argv[]) {
  buildRooms();
  return 0;
}
