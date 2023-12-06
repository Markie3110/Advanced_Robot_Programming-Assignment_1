// gcc userinterface.c -lncurses -o userinterface

#include <stdio.h>
#include <ncurses.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <curses.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <strings.h>
#include <semaphore.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/stat.h> 
#include <string.h>
#include <errno.h>
#include "include/log.h"

#define DELTA_TIME_MILLISEC 100
#define WINDOW_SIZE_SHM "/window_server" 
#define USER_INPUT_SHM "/input_server"
#define DRONE_POSITION_SHM "/position_server"
#define WATCHDOG_SHM "/watchdog_server"
#define WINDOW_SHM_SEMAPHORE "/sem_window"
#define USER_INPUT_SEMAPHORE "/sem_input"
#define DRONE_POSITION_SEMAPHORE "/sem_drone"
#define WATCHDOG_SEMAPHORE "/sem_watchdog"

#define WINDOW_SHM_SIZE 3
#define USER_SHM_SIZE 3
#define DRONE_SHM_SIZE 5
#define WATCHDOG_SHM_SIZE 10


// Declare global variables
int flag = 0; // Flag variable to clean memory, close links and quit process
int watchdog_pid;
char *logpath = "Assignment_1/log/UI.log";
FILE *logfd;


void interrupt_signal_handler(int signum)
{
    flag = 1;
}


void watchdog_signal_handler(int signum)
{
    kill(watchdog_pid, SIGUSR2);
}


int main(int argc, char *argv)
{
    // Create a log file and open it
    logopen(logpath);

    // Declare a signal handler to handle an INTERRUPT SIGNAL
    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = &interrupt_signal_handler;
    sigaction(SIGTERM, &act, NULL); 


    struct sigaction act2;
    bzero(&act2, sizeof(act2));
    act2.sa_handler = &watchdog_signal_handler;
    sigaction(SIGUSR1, &act2, NULL);


    // Initialize pointers to the shared memory
    int droneMaxY, droneMaxX;
    int dronePositionX, dronePositionY;


    /* NCURSES START*/
    initscr();
    noecho();
    cbreak();
    curs_set(0);
    logmessage(logpath, "initialised ncurses");


    // Get the windows maximum height and width
    int yMax, xMax;
    getmaxyx(stdscr, yMax, xMax);
    logint(logpath, "initial yMax", yMax);
    logint(logpath, "initial xMax", xMax);


    char *s = "DRONE WINDOW";
    mvprintw(0, 2, "%s", s);


    // Initialize the window variables
    WINDOW *dronewin;
    WINDOW *inspectwin;


    // Get the required window dimensions
    int droneWin_height, droneWin_width, droneWin_startY, droneWin_startX;
    droneWin_height = yMax/2;
    droneWin_width = xMax - 2;
    droneWin_startY = 1;
    droneWin_startX = 1;


    // Determine the dimensions of the inspector window
    int inspWin_height, inspWin_width, inspWin_startY, inspWin_startX;
    inspWin_height = yMax/6;
    inspWin_width = droneWin_width;
    inspWin_startY = droneWin_height + (yMax/6);
    inspWin_startX = 1;


    // Maximum positions for drone
    droneMaxY = droneWin_height;
    droneMaxX = droneWin_width;
    logint(logpath, "droneMaxY", droneMaxY);
    logint(logpath, "droneMaxX", droneMaxX);


    int dimList[9] = {droneWin_height, droneWin_width, droneWin_startY, droneWin_startX, inspWin_height, inspWin_width, inspWin_startY, inspWin_startX};


    // Intialise the interface
    dronewin = newwin(dimList[0], dimList[1], dimList[2], dimList[3]);
    mvprintw(dimList[0]+1, 2, "List of commands:");
    char *s1 = "Reset: K    ";
    char *s2 = "Quit: L     ";
    char *s3 = "Stop: S     ";
    char *s4 = "Left: A     ";
    char *s5 = "Right: D    ";
    char *s6 = "Top: W      ";
    char *s7 = "Bottom: X   ";
    char *s8 =  "Top-Left: Q    ";
    char *s9=  "Top-Right: E   ";
    char *s10 = "Bottom-Left: Z ";
    char *s11 = "Bottom-Right: C";
    mvprintw(dimList[0]+2, 2, "%s %s", s1, s2);
    mvprintw(dimList[0]+3, 2, "%s %s %s %s %s %s %s %s %s", s3, s4, s5, s6, s7, s8, s9, s10, s11);
    mvprintw(dimList[6] - 1, 2, "INSPECTOR WINDOW");
    inspectwin = newwin(dimList[4], dimList[5], dimList[6], dimList[7]);
    box(dronewin, 0, 0);
    box(inspectwin, 0, 0);
    refresh();
    wrefresh(dronewin);
    wrefresh(inspectwin);


    // Create a FIFO to send the pid to the watchdog
    int UIpid_fd;
    char *UIpidfifo = "Assignment_1/tmp/UIpidfifo";
    while(1)
    {
        if(mkfifo(UIpidfifo, 0666) == -1)
        {
            logerror(logpath, "error: UIpidfifo create", errno);
            remove(UIpidfifo);
        }
        else
        {
            logmessage(logpath, "Created UIpidfifo");
            break;
        }
        usleep(10);
    }
    UIpid_fd = open(UIpidfifo, O_WRONLY);
    if (UIpid_fd == -1)
    {
        logerror(logpath, "error: UIpidfifo open", errno);
    }
    else
    {
        logmessage(logpath, "Opened UIpidfifo");
    }


    // Send the pid to the watchdog
    int pid = getpid();
    int pid_buf[1] = {pid};
    write(UIpid_fd, pid_buf, sizeof(pid_buf));
    logmessage(logpath, "Sent pid to watchdog");


    // Open the shared memory for the watchdog PID
    sem_t *sem_watchdog = sem_open(WATCHDOG_SEMAPHORE, O_CREAT, S_IRWXU | S_IRWXG, 1);
    while(1)
    {
        if (sem_watchdog == SEM_FAILED)
        {
            logerror(logpath, "error: sem_watchdog open", errno);
        }
        else
        {
            logmessage(logpath, "Opened sem_watchdog");
            break;
        }
        usleep(10);
    }
    int shm_watchdog;
    while(1)
    {
        sem_init(sem_watchdog, 1, 0);
        shm_watchdog = shm_open(WATCHDOG_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
        if (shm_watchdog == -1)
        {
            logerror(logpath, "error: shm_watchdog open", errno);
        }
        else
        {
            logmessage(logpath, "Opened shm_watchdog");
            break;
        }
        sem_post(sem_watchdog);
        usleep(10);
    }
    ftruncate(shm_watchdog, DRONE_SHM_SIZE);
    int *watchdog_ptr = mmap(NULL, WATCHDOG_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_watchdog, 0);
    sem_post(sem_watchdog);


    // Read the watchdog PID from shared memory
    int watchdog_list[1] = {0};;
    int size = sizeof(watchdog_list);
    while (watchdog_list[0] == 0)
    {
        sem_wait(sem_watchdog);
        memcpy(watchdog_list, watchdog_ptr, size);
        sem_post(sem_watchdog);
        usleep(10);
    }
    watchdog_pid = watchdog_list[0];
    logmessage(logpath, "received watchdog pid");
    logint(logpath, "watchdog_pid", watchdog_pid);


    // Create a FIFO to pass the keypressed values to the drone
    int keypressed_fd;
    char *keypressedfifo = "Assignment_1/tmp/keypressedfifo";
    while(1)
    {
        if(mkfifo(keypressedfifo, 0666) == -1)
        {
            logerror(logpath, "error: keypressedfifo create", errno);
            remove(keypressedfifo);
        }
        else
        {
            logmessage(logpath, "Created keypressedfifo");
            break;
        }
        usleep(10);
    }
    keypressed_fd = open(keypressedfifo, O_WRONLY);
    if (keypressed_fd == -1)
    {
        logerror(logpath, "errorL keypressedfio open", errno);
    }


    // Open the shared memory for the window size
    sem_t *sem_window;
    while(1)
    {
        sem_window = sem_open(WINDOW_SHM_SEMAPHORE, 0);
        if (sem_window == SEM_FAILED)
        {
            logerror(logpath, "error: open sem_window", errno);
        }
        else
        {
            logmessage(logpath, "Opened sem_window");
            break;
        }
    }
    int shm_window;
    while(1)
    {
        sem_wait(sem_window);
        shm_window = shm_open(WINDOW_SIZE_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
        if (shm_window == -1)
        {
            logerror(logpath, "error: open shm_window", errno);
        }
        else
        {
            logmessage(logpath, "Opened shm_window");
            break;
        }
        sem_post(sem_window);
        usleep(10);
    }
    int *window_ptr = mmap(NULL, WINDOW_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_window, 0);
    sem_post(sem_window);
    wrefresh(inspectwin);


    // Open the shared memory for drone position
    sem_t *sem_drone;
    while(1)
    {
        sem_drone = sem_open(DRONE_POSITION_SEMAPHORE, 0);
        if (sem_drone == SEM_FAILED)
        {
            logerror(logpath, "error: open sem_drone", errno);
        }
        else
        {
            logmessage(logpath, "Opened sem_drone");
            break;
        }
        usleep(10);
    }
    int shm_drone;
    while(1)
    {
        sem_init(sem_drone, 1, 0);
        shm_drone = shm_open(DRONE_POSITION_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
        if (shm_drone == -1)
        {
            logerror(logpath, "error: open shm_drone", errno);
        }
        else
        {
            logmessage(logpath, "Opened shm_drone");
            break;
        }
        sem_post(sem_drone);
        usleep(10);
    }
    int *drone_ptr = mmap(NULL, DRONE_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_drone, 0);
    sem_post(sem_drone);
    wrefresh(inspectwin);


    while(1)
    {
        // Grab the input for the current timecycle
        wtimeout(dronewin, DELTA_TIME_MILLISEC);
        int c = wgetch(dronewin);
        logint(logpath, "user input", c);


        // Pass on the user input to the drone file  
        if (c != 410)
        {
            int buf[1] = {c};
            write(keypressed_fd, buf, sizeof(buf));
            logmessage(logpath, "passed input to drone");
        }


        // Pass the current window sizes to the drone
        int list[2] = {droneMaxY, droneMaxX};
        int size = sizeof(list);
        sem_wait(sem_window);
        memcpy(window_ptr, list, size);
        sem_post(sem_window);
        wrefresh(inspectwin);
        logmessage(logpath, "passed window size to drone");


        // Check if the window has been resized
        int yMax_now, xMax_now;
        getmaxyx(stdscr, yMax_now, xMax_now);
        logint(logpath, "yMax_now", yMax_now);
        logint(logpath, "xMax_now", xMax_now);
        if ((yMax_now != yMax) || (xMax_now != xMax))
        {
            yMax = yMax_now;
            xMax = xMax_now;

            // Get the required window dimensions
            int droneWin_height, droneWin_width, droneWin_startY, droneWin_startX;
            droneWin_height = yMax/2;
            droneWin_width = xMax - 2;
            droneWin_startY = 1;
            droneWin_startX = 1;

            // Determine the dimensions of the inspector window
            int inspWin_height, inspWin_width, inspWin_startY, inspWin_startX;
            inspWin_height = yMax/6;
            inspWin_width = droneWin_width;
            inspWin_startY = droneWin_height + (yMax/6);
            inspWin_startX = 1;

            // Maximum positions for drone
            droneMaxY = droneWin_height;
            droneMaxX = droneWin_width;


            int dimList[9] = {droneWin_height, droneWin_width, droneWin_startY, droneWin_startX, inspWin_height, inspWin_width, inspWin_startY, inspWin_startX};


            // Resize the entire interface according to the new dimensions
            clear();
            wresize(dronewin, 1, 1);
            mvwin(dronewin, dimList[2], dimList[3]);
            wresize(dronewin, dimList[0], dimList[1]);
            mvprintw(dimList[0]+1, 2, "List of commands:");
            char *s1 = "Reset: K    ";
            char *s2 = "Quit: L     ";
            char *s3 = "Stop: S     ";
            char *s4 = "Left: A     ";
            char *s5 = "Right: D    ";
            char *s6 = "Top: W      ";
            char *s7 = "Bottom: X   ";
            char *s8 =  "Top-Left: Q    ";
            char *s9=  "Top-Right: E   ";
            char *s10 = "Bottom-Left: Z ";
            char *s11 = "Bottom-Right: C";
            mvprintw(dimList[0]+2, 2, "%s %s", s1, s2);
            mvprintw(dimList[0]+3, 2, "%s %s %s %s %s %s %s %s %s", s3, s4, s5, s6, s7, s8, s9, s10, s11);
            mvprintw(dimList[6] - 1, 2, "INSPECTOR WINDOW");
            wresize(inspectwin, 1, 1);
            mvwin(inspectwin, dimList[6], dimList[7]);
            wresize(inspectwin, dimList[4], dimList[5]);
            box(dronewin, 0, 0);
            box(inspectwin, 0, 0);
            refresh();
            wrefresh(dronewin);
            wrefresh(inspectwin);
            
        }
        

        // Access the drone location from shared memory 
        int dronePosition[2];
        size = sizeof(dronePosition);
        sem_wait(sem_drone);
        memcpy(dronePosition, drone_ptr, size);
        sem_post(sem_drone);
        dronePositionX = dronePosition[1];
        dronePositionY = dronePosition[0];
        wclear(inspectwin);
        box(inspectwin, 0, 0);
        mvwprintw(inspectwin, 1, 1, "%d %d", dronePosition[0], dronePosition[1]);
        wrefresh(inspectwin);
        logint(logpath, "dronePositionY", dronePositionY);
        logint(logpath, "dronePositionX", dronePositionX);


        // Display the drone 
        wclear(dronewin);
        box(dronewin, 0, 0);
        mvwprintw(dronewin, dronePosition[0], dronePosition[1], "+");
        wrefresh(dronewin);


        // Close all links and shutdown program
        if (flag == 1)
        {
            munmap(window_ptr, WINDOW_SHM_SIZE);
            if (shm_unlink(WINDOW_SIZE_SHM) == -1)
            {
                perror("Failed to close window_shm");
                printf("Failed to close window_shm\n");
            }
            sem_close(sem_window);
            sem_unlink(WINDOW_SHM_SEMAPHORE);

            sem_close(sem_drone);
            sem_unlink(DRONE_POSITION_SEMAPHORE);
            munmap(drone_ptr, DRONE_SHM_SIZE);
            if (shm_unlink(DRONE_POSITION_SHM) == -1)
            {
                perror("Failed to close drone_shm");
            }

            close(keypressed_fd);
            close(UIpid_fd);

            struct sigaction act;
            bzero(&act, sizeof(act));
            act.sa_handler = SIG_DFL;
            sigaction(SIGINT, &act, NULL);
            raise(SIGTERM);

            return -1;
        }        

    }

    endwin();
    /* NCURSES END*/

    return 0;
}