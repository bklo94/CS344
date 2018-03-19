#Brandon Lo
#CS344 HWK3
#program.py


#random module page
#https://docs.python.org/2/library/random.html
import random

#ascii table
#https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcQ7xTTog0ByygyfBp4csGBi0gKVnHyDYim2TsY1QGIE1FHgn_K3dOYKz8k
def createRandom(a):
    #write to a temp file based on the inputed character
    tempFile = "random" + str(a) + ".txt"
    file = open(str(tempFile), "a")
    #generate 10 random ascii dec numbers and convert to string. 97-122 is a-z on the ascii table
    for i in range(0,10):
        temp = random.randrange(97,122)
        file.write(chr(temp))
    file.write("\n")

#strip newline characterfrom output
#https://stackoverflow.com/questions/15233340/getting-rid-of-n-when-using-readlines
def readFile(a):
    tempFile = "random" + str(a) + ".txt"
    file = open(str(tempFile),"r")
    #strip the newline character when you print it to have propper formatting
    print file.readline().rstrip('\n')

#grab 2 random numbers in random range and then print and multiply them
def genRandInt():
    rand1 = random.randrange(1,42)
    rand2 = random.randrange(1,42)
    print rand1
    print rand2
    print rand1 * rand2

#main program that calls the functions. Creates 3 text files and reads them
#then creates 2 random ints and multiplies them
if __name__ == "__main__":
    for i in range(0,3): 
        createRandom(i)
        readFile(i)
    genRandInt()
