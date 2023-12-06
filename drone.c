#include <stdio.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <poll.h>
#include <errno.h>
#include "include/log.h"


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


#define DELTA_TIME_USEC 100000
#define MASS 1
#define VISCOSITY 1
#define INPUT_FORCE 1


// Declare global variables
int flag = 0; // Flag variable to clean memory, close links and quit process
int watchdog_pid;
char *logpath = "Assignment_1/log/drone.log";
FILE *logfd;


void interrupt_signal_handler(int signum)
{
    logmessage(logpath ,"Received SIGTERM\n");
    flag = 1;
}


void watchdog_signal_handler(int signum)
{
    /*
    A signal handler that sends a response back to the watchdog.
    */

    kill(watchdog_pid, SIGUSR2);

}


double CalculatePositionX(int forceX, double x1, double x2)
{
    /*
    Calculates the X position of the drone using the positions the drone occupied in the previous two time intervals, x1(one time interval) and 
    (two time intervals) x2 respectively.
    */

    double deltatime = 0.1;
    double num = (forceX*(deltatime*deltatime)) - (MASS * x2) + (2 * MASS * x1) + (VISCOSITY * deltatime * x1);
    double den = MASS + (VISCOSITY * deltatime);
    return (num/den);
    
}


double CalculatePositionY(int forceY, double y1, double y2)
{
    double deltatime = 0.1;
    double num = (forceY*(deltatime*deltatime)) - (MASS * y2) + (2 * MASS * y1) + (VISCOSITY * deltatime * y1);
    double den = MASS + (VISCOSITY * deltatime);
    return (num/den);
}


int main(int argc, char *argv[])
{
    // Create a log file and open it
    logopen(logpath);


    // Declare a signal handler to handle an INTERRUPT SIGNAL
    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = &interrupt_signal_handler;
    sigaction(SIGTERM, &act, NULL); 

    // Declare a signal handler to handle a SIGUSR1 signal sent from the watchdog
    struct sigaction act2;
    bzero(&act2, sizeof(act2));
    act2.sa_handler = &watchdog_signal_handler;
    sigaction(SIGUSR1, &act2, NULL);


    // Declare variables
    int droneMaxY, droneMaxX;
    int forceX = 0;
    int forceY = 0;
    int dronePosition[2] = {0, 0};
    double droneStartPosition[2] = {15, 15};
    double dronePositionChange[2] = {0, 0};
    dronePosition[0] = droneStartPosition[0] + dronePositionChange[0];
    dronePosition[1] = droneStartPosition[1] + dronePositionChange[1];
    double X[2] = {0, 0};
    double Y[2] = {0, 0};
    int keyvalue;


    // Create a FIFO to send the pid to the watchdog
    int dronepid_fd;
    char *dronepidfifo = "Assignment_1/tmp/dronepidfifo";
    while(1)
    {
        if(mkfifo(dronepidfifo, 0666) == -1)
        {
            logerror(logpath, "error: dronepidfifo creation", errno);
            remove(dronepidfifo);
        }
        else
        {
            logmessage(logpath, "Created dronepidfifo");
            break;
        }
        usleep(10);
    }
    dronepid_fd = open(dronepidfifo, O_WRONLY);
    if (dronepid_fd == -1)
    {
        logerror(logpath, "error: dronepidfifo open", errno);
    }
    else
    {
        logmessage(logpath, "Opened dronepidfifo");
    }


    // Send the pid to the watchdog
    int pid = getpid();
    int pid_buf[1] = {pid};
    write(dronepid_fd, pid_buf, sizeof(pid_buf));
    logint(logpath, "Drone PID", pid);

    
    // Open the shared memory for the watchdog PID
    sem_t *sem_watchdog;
    while(1)
    {
        sem_watchdog = sem_open(WATCHDOG_SEMAPHORE, O_CREAT, S_IRWXU | S_IRWXG, 1);
        if (sem_watchdog == SEM_FAILED)
        {
            logerror(logpath, "error: sem_watchdog open", errno);
        }
        else
        {
            logmessage(logpath, "Opened sem_watchdog");
            break;
        }
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
    logint(logpath, "Watchdog PID", watchdog_pid);


    // Open a FIFO to receive the keypressed values
    int keypressed_fd;
    char *keypressedfifo = "Assignment_1/tmp/keypressedfifo";
    fd_set rfds;
    struct timeval tv;
    while(1)
    {
        keypressed_fd = open(keypressedfifo, O_RDONLY);
        if (keypressed_fd == -1)
        {
            logerror(logpath, "error: keypressedfifo open", errno);
        }
        else
        {
            logmessage(logpath, "Opened keypressedfifo");
            break;
        }
    }


    // Open the shared memory for the window size
    sem_t *sem_window;
    while(1)
    {
        sem_window = sem_open(WINDOW_SHM_SEMAPHORE, 0);
        if (sem_window == SEM_FAILED)
        {
            logerror(logpath, "error: sem_window open", errno);
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
            logerror(logpath, "error: shm_window open", errno);
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


    // Open the shared memory for the drone position
    sem_t *sem_drone;
    while(1)
    {
        sem_drone = sem_open(DRONE_POSITION_SEMAPHORE, 0);
        if (sem_drone == SEM_FAILED)
        {
            logerror(logpath, "error: sem_drone open", errno);
        }
        else
        {
            logmessage(logpath, "Opened sem_drone");
            break;
        }
    }
    int shm_drone;
    while(1)
    {
        sem_init(sem_drone, 1, 0);
        shm_drone = shm_open(DRONE_POSITION_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
        if (shm_drone == -1)
        {
            logerror(logpath, "error: shm_drone open", errno);
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


    while(1)
    {
        // Receive the keypressed value from UI
        int buf[1] = {0};
        FD_ZERO(&rfds);
        FD_SET(keypressed_fd, &rfds);
        tv.tv_usec = 10000;
        tv.tv_sec = 0;
        int retval = select((keypressed_fd+1), &rfds, NULL, NULL, &tv);
        if (retval == -1)
        {
            logerror(logpath, "error: select", errno);
        }
        else if (retval)
        {
            read(keypressed_fd, buf, sizeof(buf));
        }
        keyvalue = buf[0];
        logint(logpath, "keyvalue", keyvalue);


        // Receive size of window from UI
        int window_list[2];
        int size = sizeof(window_list);
        sem_wait(sem_window);
        memcpy(window_list, window_ptr, size);
        sem_post(sem_window);
        droneMaxY = window_list[0];
        droneMaxX = window_list[1];
        logint(logpath, "droneMaxY", droneMaxY);
        logint(logpath, "droneMaxX", droneMaxX);


        // Calculate the drone position
        if ((dronePosition[0] > 0) && (dronePosition[1] > 0) && (dronePosition[0] <= (droneMaxY-2)) && (dronePosition[1] <= (droneMaxX-2)))
        {
            dronePositionChange[0] = CalculatePositionY(forceY, Y[0], Y[1]);
            dronePositionChange[1] = CalculatePositionX(forceX, X[0], X[1]);
        }
        else if (dronePosition[0] == (droneMaxY-1))
        {
            if (forceY < 0)
            {
                dronePositionChange[0] = CalculatePositionY(forceY, Y[0], Y[1]);
            }
            dronePositionChange[1] = CalculatePositionX(forceX, X[0], X[1]);
        }
        else if (dronePosition[0] == 0)
        {
            if (forceY > 0)
            {
                dronePositionChange[0] = CalculatePositionY(forceY, Y[0], Y[1]);
            }
            dronePositionChange[1] = CalculatePositionX(forceX, X[0], X[1]);
        }
        else if (dronePosition[1] == (droneMaxX - 1))
        {
            if (forceX < 0)
            {
                dronePositionChange[1] = CalculatePositionX(forceX, X[0], X[1]);
            }
            dronePositionChange[0] = CalculatePositionY(forceY, Y[0], Y[1]);
        }
        else if (dronePosition[1] == 0)
        {
            if (forceX > 0)
            {
                dronePositionChange[1] = CalculatePositionX(forceX, X[0], X[1]);
            }
            dronePositionChange[0] = CalculatePositionY(forceY, Y[0], Y[1]);
        }
        dronePosition[0] = droneStartPosition[0] + dronePositionChange[0];
        dronePosition[1] = droneStartPosition[1] + dronePositionChange[1];
        logint(logpath, "DronePositionY", dronePosition[0]);
        logint(logpath, "DronePositionY", dronePosition[1]);
        logint(logpath, "ForceY", forceY);
        logint(logpath, "forceX", forceX);
        X[1] = X[0];
        X[0] = dronePositionChange[1];
        Y[1] = Y[0];
        Y[0] = dronePositionChange[0];


        // Send drone position to UI
        size = sizeof(dronePosition);
        sem_wait(sem_drone);
        memcpy(drone_ptr, dronePosition, size);
        sem_post(sem_drone);
        logmessage(logpath, "Sent dronepos to UI");


        // Set the force according to the key pressed
        if (keyvalue == 113)
        {
            forceX = forceX - INPUT_FORCE;
            forceY = forceY - INPUT_FORCE;
        }
        else if (keyvalue == 119)
        {
            forceY = forceY - INPUT_FORCE;
        }
        else if (keyvalue == 101)
        {
            forceX = forceX + INPUT_FORCE;
            forceY = forceY - INPUT_FORCE;
        }
        else if (keyvalue == 97)
        {
            forceX = forceX - INPUT_FORCE;
        }
        else if (keyvalue == 115)
        {
            forceX = 0;
            forceY = 0;
        }
        else if (keyvalue == 100)
        {
            forceX = forceX + INPUT_FORCE;
        }
        else if (keyvalue == 122)
        {
            forceX = forceX - INPUT_FORCE;
            forceY = forceY + INPUT_FORCE;
        }
        else if (keyvalue == 120)
        {
            forceY = forceY + INPUT_FORCE;
        }
        else if (keyvalue == 99)
        {
            forceX = forceX + INPUT_FORCE;
            forceY = forceY + INPUT_FORCE;
        }


        // Flag that executes if an interrupt is encountered
        if (flag == 1)
        {
            logmessage(logpath, "closing links and pipes");

            sem_close(sem_window);
            sem_unlink(WINDOW_SHM_SEMAPHORE);
            munmap(window_ptr, WINDOW_SHM_SIZE);
            if (shm_unlink(WINDOW_SIZE_SHM) == -1)
            {
                perror("Drone - Failed to close window_shm");
                logerror(logpath, "error: window_shm close", errno);
            }

            close(keypressed_fd);
            close(dronepid_fd);

            sem_close(sem_drone);
            sem_unlink(USER_INPUT_SEMAPHORE);
            munmap(drone_ptr, USER_SHM_SIZE);
            if (shm_unlink(DRONE_POSITION_SHM) == -1)
            {
                perror("Failed to close window_shm");
                logerror(logpath, "error: drone_shm close", errno);
                return -1;
            }

            struct sigaction act;
            bzero(&act, sizeof(act));
            act.sa_handler = SIG_DFL;
            sigaction(SIGINT, &act, NULL);
            raise(SIGINT);

        return -1;
        }


        usleep(DELTA_TIME_USEC);
    }

    return 0;
}