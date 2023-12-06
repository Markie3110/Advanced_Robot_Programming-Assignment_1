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
#include <errno.h>
#include "include/log.h"

#define WATCHDOG_SHM "/watchdog_server"
#define WATCHDOG_SEMAPHORE "/sem_watchdog"

#define WATCHDOG_SHM_SIZE 10
#define DELTA_TIME_USECONDS 100000

// Define global variables
int received_response = 0;
int server_buf[1] = {0};
int ui_buf[1] = {0};
int drone_buf[1] = {0};
sem_t *sem_watchdog;
FILE *logfd;
char *logpath = "Assignment_1/log/watchdog.log";

void terminate_signal_handler(int signum)
{
    kill(getppid(), SIGUSR1);
    kill(ui_buf[0], SIGTERM);
    kill(server_buf[0], SIGTERM);
    kill(drone_buf[0], SIGTERM);
}

void watchdog_signal_handler(int signum)
{
    received_response = 1;
}


int main(int argc, char *argv)
{
    // Open the log file
    logopen(logpath);
    
    // Declare the required variables
    int received_pids = 0;

    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = &watchdog_signal_handler;
    sigaction(SIGUSR2, &act, NULL);

    struct sigaction act2;
    bzero(&act2, sizeof(act2));
    act2.sa_handler = &terminate_signal_handler;
    sigaction(SIGUSR1, &act2, NULL);


    // Create a FIFO to receive the pid from the server
    int serverpid_fd;
    char *serverpidfifo = "Assignment_1/tmp/serverpidfifo";
    while(1)
    {
        serverpid_fd = open(serverpidfifo, O_RDONLY);
        if (serverpid_fd == -1)
        {
            logerror(logpath, "error: serverpidfifo open", errno);
        }
        else
        {
            logmessage(logpath, "Opened serverpidfifo");
            break;
        }
        usleep(10);
    }


    // Create a FIFO to receive the pid from the UI
    int UIpid_fd;
    char *UIpidfifo = "Assignment_1/tmp/UIpidfifo";
    while(1)
    {
        UIpid_fd = open(UIpidfifo, O_RDONLY);
        if (UIpid_fd == -1)
        {
            logerror(logpath, "error: UIpidfifo open", errno);
        }
        else
        {
            logmessage(logpath, "Opened UIpidfifo");
            break;
        }
        usleep(10);
    }


    // Create a FIFO to receive the pid from the drone
    int dronepid_fd;
    char *dronepidfifo = "Assignment_1/tmp/dronepidfifo";
    while(1)
    {
        dronepid_fd = open(dronepidfifo, O_RDONLY);
        if (dronepid_fd == -1)
        {
            logerror(logpath, "error: dronepidfifo", errno);
        }
        else
        {
            logmessage(logpath, "Opened dronepidfifo");
            break;
        }
        usleep(10);
    }


    int maxfd = -1;
    int fd[3] = {serverpid_fd, UIpid_fd, dronepid_fd};
    for (int i = 0; i < 3; i++)
    {
        if (fd[i] > maxfd)
        {
            maxfd = fd[i];
        }
    }


    // Open the shared memory that transfers watchdog PID
    while(1)
    {
        sem_watchdog = sem_open(WATCHDOG_SEMAPHORE, 0);
        if (sem_watchdog == SEM_FAILED)
        {
            logerror(logpath, "error: sem_wathcdog open", errno);
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
        sem_wait(sem_watchdog);
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
    int *watchdog_ptr = mmap(NULL, WATCHDOG_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_watchdog, 0);
    sem_post(sem_watchdog);


    // Write the watchdog PID to shared memory
    int pid = getpid();
    int list[1] = {pid};
    int size = sizeof(list);
    sem_wait(sem_watchdog);
    memcpy(watchdog_ptr, list, size);
    sem_post(sem_watchdog);
    logint(logpath, "watchdog pid", pid);
    logmessage(logpath, "sent watchdog pid to processes");


    // Receive the pids from every process
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(serverpid_fd, &rfds);
    FD_SET(UIpid_fd, &rfds);
    FD_SET(dronepid_fd, &rfds);
    tv.tv_usec = 10000;
    tv.tv_sec = 0;
    int retval = select((maxfd+1), &rfds, NULL, NULL, &tv);
    if (retval == -1)
    {
        logerror(logpath, "error: select", errno);
    }


    // Retrieve the pids of the processes
    for (int i = 0; i < 3; i++)
    {
        if(FD_ISSET(fd[i], &rfds))
        {
            if (fd[i] == serverpid_fd)
            {
                read(serverpid_fd, server_buf, sizeof(server_buf));
                received_pids = received_pids + 1;
            }
            else if (fd[i] == UIpid_fd)
            {
                read(UIpid_fd, ui_buf, sizeof(ui_buf));
                received_pids = received_pids + 1;
            }
            else if (fd[i] == dronepid_fd)
            {
                read(dronepid_fd, drone_buf, sizeof(drone_buf));
                received_pids = received_pids + 1;
            }
        }
    }
    int pidlist[3] = {server_buf[0], ui_buf[0], drone_buf[0]};
    logint(logpath, "server pid", pidlist[0]);
    logint(logpath, "UI pid", pidlist[1]);
    logint(logpath, "drone pid", pidlist[2]);


    // Declare the variables to be used in the watchdog
    int cycles = 0;
    int current_pid;


    sleep(3);


    while(1)
    {
        for (int i = 0; i < 3; i++)
        {
            // Check if the maximum cycles has been reached
            if (cycles == 3)
            {
                 // Send a kill signal to every pid in pidlist and kill self
                    char str[100];
                    sprintf(str, "No response from %d: KILLING ALL PROCESSES", current_pid);
                    logmessage(logpath, str);
                    kill(getppid(), SIGUSR1);
                    kill(ui_buf[0], SIGTERM);
                    kill(server_buf[0], SIGTERM);
                    kill(drone_buf[0], SIGTERM);

                    sem_close(sem_watchdog);
                    sem_unlink(WATCHDOG_SEMAPHORE);
                    munmap(watchdog_ptr, WATCHDOG_SHM_SIZE);
                    if (shm_unlink(WATCHDOG_SHM) == -1)
                    {
                        logerror(logpath, "error: window_shm close", errno);
                        return -1;
                    }

                    raise(SIGTERM);
            }


            // Iterate through every pid in pidlist
            current_pid = pidlist[i];
            logint(logpath, "current pid", current_pid);


            // Send a signal to the pid
            char str[100];
            kill(current_pid, SIGUSR1);
            sprintf(str, "sent SIGUSR1 to %d", current_pid);
            logmessage(logpath, str);
            usleep(50);


            // Wait for a certain number of cycles
            while (cycles < 3)
            {
                // Wait a certain duration between cycles
                usleep(DELTA_TIME_USECONDS);

                // Check if the flag variable has changed
                if (received_response == 1)
                {
                    // Reset all variables and move to next pid
                    sprintf(str, "received response from %d", current_pid);
                    logmessage(logpath, str);
                    cycles = 0;
                    received_response = 0;
                    break;
                }
                else
                {
                    // Increment cycle count
                    cycles++;
                }
                logint(logpath, "cycles", cycles);
            }
        }
    }

    return 0;
}
