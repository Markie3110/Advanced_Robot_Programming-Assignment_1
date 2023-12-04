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

#define WATCHDOG_SHM "/watchdog_server"
#define WATCHDOG_SEMAPHORE "/sem_watchdog"

#define WATCHDOG_SHM_SIZE 10
#define DELTA_TIME_USECONDS 100000

// Define global variables
int received_response = 0;

void watchdog_signal_handler(int signum)
{
    received_response = 1;
    printf("Received SIGUSR2\n");
}

int main(int argc, char *argv)
{
    // Declare the required variables
    int received_pids = 0;

    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = &watchdog_signal_handler;
    sigaction(SIGUSR2, &act, NULL);

    // Create a FIFO to receive the pid from the server
    int serverpid_fd;
    char *serverpidfifo = "/home/mark/Robotics_Masters/Advanced_Robot_Programming/Assignments/Assignment_1/tmp/serverpidfifo";
    while(1)
    {
        serverpid_fd = open(serverpidfifo, O_RDONLY);
        if (serverpid_fd == -1)
        {
            perror("Failed to open serverpidfifo\n");

        }
        else
        {
            printf("Opened serverpidfifo\n");
            break;
        }
    }

    // Create a FIFO to receive the pid from the UI
    int UIpid_fd;
    char *UIpidfifo = "/home/mark/Robotics_Masters/Advanced_Robot_Programming/Assignments/Assignment_1/tmp/UIpidfifo";
    while(1)
    {
        UIpid_fd = open(UIpidfifo, O_RDONLY);
        if (UIpid_fd == -1)
        {
            printf("Failed to open UIpidfifo\n");
        }
        else
        {
            printf("Opened UIpidfifo\n");
            break;
        }
    }

    // Create a FIFO to receive the pid from the drone
    int dronepid_fd;
    char *dronepidfifo = "/home/mark/Robotics_Masters/Advanced_Robot_Programming/Assignments/Assignment_1/tmp/dronepidfifo";
    while(1)
    {
        dronepid_fd = open(dronepidfifo, O_RDONLY);
        if (dronepid_fd == -1)
        {
            printf("Failed to open dronepidfifo\n");
        }
        else
        {
            printf("Opened dronepidfifo\n");
            break;
        }
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
    sem_t *sem_watchdog = sem_open(WATCHDOG_SEMAPHORE, 0);
    if (sem_watchdog == SEM_FAILED)
    {
        perror("Failed to create sem_watchdog");
    }
    else
    {
        printf("Created sem_watchdog\n");
    }
    sem_wait(sem_watchdog);
    int shm_watchdog = shm_open(WATCHDOG_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shm_watchdog == -1)
    {
        perror("Failed to create shm_watchdog");
    }
    else
    {
        printf("Created shm_watchdog\n");
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
    printf("%d\n", pid);

    fd_set rfds;
    struct timeval tv;

    // Receive the pids from every process
    int server_buf[1] = {0};
    int ui_buf[1] = {0};
    int drone_buf[1] = {0};
    FD_ZERO(&rfds);
    FD_SET(serverpid_fd, &rfds);
    FD_SET(UIpid_fd, &rfds);
    FD_SET(dronepid_fd, &rfds);
    tv.tv_usec = 10000;
    tv.tv_sec = 0;
    int retval = select((maxfd+1), &rfds, NULL, NULL, &tv);
    if (retval == -1)
    {
        perror("Select() error");
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

    printf("%d %d %d %d\n", server_buf[0], ui_buf[0], drone_buf[0], getsid(0));
    //printf("%d %d %d\n", getppid(server_buf[0]), getpgid(ui_buf[0]), getpgid(drone_buf[0]));
    int pidlist[3] = {server_buf[0], ui_buf[0], drone_buf[0]};

    // Declare the variables to be used in the watchdog
    int cycles = 0;
    //int i = 0;
    int current_pid;

    sleep(5);

    while(1)
    {
        for (int i = 0; i < 3; i++)
        {
            // Check if the maximum cycles has been reached
            if (cycles == 2)
            {
                 // Send a kill signal to every pid in pidlist and kill self
                    printf("No response from %d: KILLING ALL PROCESSES\n", current_pid);
                    kill(getppid(), SIGUSR1);
                    kill(ui_buf[0], SIGTERM);
                    kill(server_buf[0], SIGTERM);
                    kill(drone_buf[0], SIGTERM);

                    sem_close(sem_watchdog);
                    sem_unlink(WATCHDOG_SEMAPHORE);
                    munmap(watchdog_ptr, WATCHDOG_SHM_SIZE);
                    if (shm_unlink(WATCHDOG_SHM) == -1)
                    {
                        perror("Failed to close window_shm");
                        return -1;
                    }

                    raise(SIGTERM);
            }

            // Iterate through every pid in pidlist
            current_pid = pidlist[i];
            printf("%d\n", current_pid);

            // Send a signal to the pid
            kill(current_pid, SIGUSR1);
            printf("2\n");
            usleep(50);
            printf("Received_response count: %d\n", received_response);

            // Wait for a certain number of cycles
            while (cycles < 2)
            {
                // Wait a certain duration between cycles
                printf("Cycles: %d\n", cycles);
                usleep(DELTA_TIME_USECONDS);
                printf("3\n");

                // Check if the flag variable has changed
                if (received_response == 1)
                {
                    // Reset all variables and move to next pid
                    printf("Received\n");
                    cycles = 0;
                    received_response = 0;
                    break;
                }
                else
                {
                    // Increment cycle count
                    cycles++;
                }
            }
        }
    }

    return 0;
}
