Advanced_Robot_Programming-Assignment_1
================================

Introduction
----------------------
The following repository contains the solution to the first assignment for the Advanced Robot Programming course, found in the Robotics Masters Programme at the University of Genoa, Italy. As part of this assignment,
students were required to program a drone simulator in which a drone, represented by a blue '+', was to be depicted in a window and was capable of being actuated around this space via user inputs.
The following work has been performed by Mark Henry Dsouza.

Table of Contents
----------------------

Overview
----------------------


Installation
----------------------
To download the repository's contents to your local system you can do one of the following:

1. Using git from your local system<br>
To download the repo using git simply go to your terminal and go to the directory you want to save the project in. Type the following command to clone the repository to your local folder:
```bash
$ git clone "https://github.com/Markie3110/Advanced_Robot_Programming-Assignment_1"
```

2. Download the .zip from Github<br>
In a browser go to the repository on Github and download the .zip file availabe in the code dropdown box found at the top right. Unzip the file to access the contents.

How to Run
----------------------
To both build the executables and run the system, navigate to the src folder within a terminal and type in the following command:
```bash
make
```
The simulator should compile and then execute.<br>
NOTE: Due to the use of named FIFOs with paths, it is important that the Assignment_1 folder is stored in the root directory of your system. There should be no intermediary folders between the root and Assignment_1
folder or else the system will not run.

Operational instructions
----------------------
To operate the drone use the following keys:
```
'q' 'w' 'e'
'a' 's' 'd'
'z' 'x' 'c'
```
The keys represent the following movements for the drone
* `q`: TOP-LEFT
* 'w': TOP
* 'e': TOP-RIGHT
* 'a': LEFT
* 's': STOP
* 'd': RIGHT
* 'z': BOTTOM-LEFT
* 'x': BOTTOM
* 'c': BOTTOM-RIGHT
