![UnB](https://github.com/user-attachments/assets/dfad9149-cb02-4821-8b15-1ed1b413db16)
## Universidade de Brasília 
- Gabriel de Castro Dias - 211055432

## What is MemoryMaze?
This is my game created for the discipline of Introduction of Embedded Systems at University of Brasília 2025.1

You can read the project requirements on "DE1-Games.pdf"


![Maze](imgs/maze.jpeg)
- The game is pretty simple, you are the character at top left of the screen and you need to reach the destination at bottom right, you can't pass through walls, so you need to find a passage on the maze BUT, you only have 10 seconds to find a way! after that time the screen will turn black and the objective is try to reach de destination only with your memory (or luck).

![BlackScreen](imgs/blackScreen.jpeg)
- This is where the game start and you can move your character, no need to worry about the black screen little one, I made two characters with RGB for you! so at least the game will run smoothly because RGB = Better FPS (you know that). Oh! almost forgot to tell you that you will have 1 minute to complete the maze otherwise YOU WILL DIE!  Good luck :)

![DE1-SoC](imgs/board.jpeg)
### Controls: 
- Led0 -> ON/OFF Change the character between square and circle
- Keys -> Up, Down, Left, Right / Press any key to continue after screens (Gray, Green, Red)
## How to run
1. Clone the repository or download the .ZIP file.
2. Follow the steps on "DE1-SoC_Guide.pdf" to setup your board properly
3. Transfer the repository files to the linux installed on DE1-SoC. You can use one of these methods
    1. USB
    2. Networks Protocols
    3. Copy your files to clipboard and paste on the board's linux with "nano" - (Be careful, clipboard and nano can paste things out of order if you copy too much information, try copying blocks of code instead of the whole code)

4. Now you just need to compile with the command
```c
 gcc game.c maps.c -o memory_maze -std=c99
 ``` 
5. And run the project with 
 ```c
 ./memory_maze
 ```
All files has comments explaining the basic of how it works
### ```All files has comments with basic explanations of how the code works (Comments only in Brazilian-Portuguese) ```

 ## Files
 ```c
 maps.h
 ```
 - Header file of project's settings, it defines the dimensions and the quantity of maps, it also turns the variable of maps into a global variable to be reutilized by other files.

```c
 maps.c
 ```

- Here it is implemented the maps, each one has 24 rows and 32 columns, as defined on header file. The file saves 10 maps as matrix of booleans; 1 = wall and 0 = available path to move.
```c

 game.c
 ```
 - This file is the heart of the project, it implements all of the game logic, function calls, definitions and connection of the physical memory of the board with linux.


 

