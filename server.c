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
#include <sys/time.h>
#include <sys/types.h>
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

#define WINDOW_SHM_SIZE 5
#define USER_SHM_SIZE 3
#define DRONE_SHM_SIZE 5
#define WATCHDOG_SHM_SIZE 10

#define DELTA_TIME_USECONDS 100000


// Declare global variables
int flag = 0; // Flag variable to clean memory, close links and quit process
int watchdog_pid;
FILE *logfd;
char *logpath = "Assignment_1/log/server.log";


void interrupt_signal_handler(int signum)
{
    flag = 1;
}


void watchdog_signal_handler(int signum)
{
    /*
    A signal handler that sends a response back to the watchdog
    */
    kill(watchdog_pid, SIGUSR2);
}


int main(int argc, char *argv[])
{
    // Create a log file and open it
    logopen(logpath);


    // Declare a signal handler to handle the signals received from the watchdog
    struct sigaction act2;
    bzero(&act2, sizeof(act2));
    act2.sa_handler = &watchdog_signal_handler;
    sigaction(SIGUSR1, &act2, NULL);


    // Create a FIFO to send the pid to the watchdog
    int serverpid_fd;
    char *serverpidfifo = "Assignment_1/tmp/serverpidfifo";
    while(1)
    {
        if(mkfifo(serverpidfifo, 0666) == -1)
        {
            if (errno != EEXIST)
            {
                logerror(logpath, "error: serverpidfifo create", errno);
                remove(serverpidfifo);
            }
        }
        else
        {
            logmessage(logpath, "Created serverpidfifo");
            break;  
        }
        usleep(10);
    }
    serverpid_fd = open(serverpidfifo, O_WRONLY);
    if (serverpid_fd == -1)
    {
        logerror(logpath, "error: serverpidfifo open", errno);
    }
    else
    {
        logmessage(logpath, "Opened serverpidfifo");
    }


    // Send the pid to the watchdog
    int pid = getpid();
    int pid_buf[1] = {pid};
    write(serverpid_fd, pid_buf, sizeof(pid_buf));
    logmessage(logpath, "sent pid to watchdog");


    // Open the shared memory for the watchdog PID
    sem_t *sem_watchdog = sem_open(WATCHDOG_SEMAPHORE, O_CREAT, S_IRWXU | S_IRWXG, 1);
    if (sem_watchdog == SEM_FAILED)
    {
        logerror(logpath, "error: sem_watchdog open", errno);
    }
    else
    {
        logmessage(logpath, "Opened sem_watchdog");
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
    int buf[1] = {0};
    memcpy(watchdog_ptr, buf, sizeof(buf));
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


    // Create the shared memory for the window size
    sem_t *sem_window = sem_open(WINDOW_SHM_SEMAPHORE, O_CREAT, S_IRWXU | S_IRWXG, 1);
    if (sem_window == SEM_FAILED)
    {
        logerror(logpath, "error: sem_window create", errno);
    }
    else
    {
        logmessage(logpath, "Created sem_window");
    }
    sem_init(sem_window, 1, 0);
    int shm_window = shm_open(WINDOW_SIZE_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shm_window == -1)
    {
        logerror(logpath, "error: shm_window create", errno);
    }
    else
    {
        logmessage(logpath, "Created shm_window");
    }
    ftruncate(shm_window, WINDOW_SHM_SIZE);
    void *window_ptr = mmap(NULL, WINDOW_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_window, 0);
    sem_post(sem_window);


    // Create the shared memory for the drone position
    sem_t *sem_drone = sem_open(DRONE_POSITION_SEMAPHORE, O_CREAT, S_IRWXU | S_IRWXG, 1);
    if (sem_drone == SEM_FAILED)
    {
        logerror(logpath, "error: sem_drone create", errno);
    }
    else
    {
        logmessage(logpath, "Created sem_drone");
    }
    sem_init(sem_drone, 1, 0);
    int shm_drone = shm_open(DRONE_POSITION_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shm_drone == -1)
    {
        logerror(logpath, "error: shm_drone create", errno);
    }
    else
    {
        logmessage(logpath, "Created shm_drone");
    }
    ftruncate(shm_drone, DRONE_SHM_SIZE);
    int *drone_ptr = mmap(NULL, DRONE_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_drone, 0);
    sem_post(sem_drone);


    while (1)
    {
        usleep(DELTA_TIME_USECONDS);
        logmessage(logpath, "Server performed 1 cycle");

        if (flag == 1)
        {
            sem_close(sem_window);
            sem_unlink(WINDOW_SHM_SEMAPHORE);
            munmap(window_ptr, WINDOW_SHM_SIZE);
            if (shm_unlink(WINDOW_SIZE_SHM) == -1)
            {
                perror("Failed to close window_shm");
                return -1;
            }

            sem_close(sem_drone);
            sem_unlink(DRONE_POSITION_SEMAPHORE);
            munmap(drone_ptr, DRONE_SHM_SIZE);
            if (shm_unlink(DRONE_POSITION_SHM) == -1)
            {
                perror("Failed to close window_shm");
                return -1;
            }

            sem_close(sem_watchdog);
            sem_unlink(WATCHDOG_SEMAPHORE);
            munmap(watchdog_ptr, WATCHDOG_SHM_SIZE);
            if (shm_unlink(WATCHDOG_SHM) == -1)
            {
                perror("Failed to close window_shm");
                return -1;
            }

            close(serverpid_fd);

            struct sigaction act;
            bzero(&act, sizeof(act));
            act.sa_handler = SIG_DFL;
            sigaction(SIGINT, &act, NULL);
            raise(SIGINT);

        }
    }
    
    return 0;
}