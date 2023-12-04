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

void interrupt_signal_handler(int signum)
{
    flag = 1;
    printf("Interrupt called\n");
}

void watchdog_signal_handler(int signum)
{
    kill(watchdog_pid, SIGUSR2);
    printf("Received SIGUSR1\n");
}

double CalculatePositionX(int forceX, double x1, double x2)
{
    double deltatime = 0.1;
    double num = (forceX*(deltatime*deltatime)) - (MASS * x2) + (2 * MASS * x1) + (VISCOSITY * deltatime * x1);
    double den = MASS + (VISCOSITY * deltatime);
    printf("%f\n", (num/den));
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
    // Declare a signal handler to handle an INTERRUPT SIGNAL
    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = &interrupt_signal_handler;
    sigaction(SIGTERM, &act, NULL); 

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
    char *dronepidfifo = "/home/mark/Robotics_Masters/Advanced_Robot_Programming/Assignments/Assignment_1/tmp/dronepidfifo";
    while(1)
    {
        if(mkfifo(dronepidfifo, 0666) == -1)
        {
            sleep(10);
            perror("Failed to create dronepidfifo");
            remove(dronepidfifo);
        }
        else
        {
            printf("Created dronepidfifo\n");
            break;
        }
        usleep(10);
    }
    dronepid_fd = open(dronepidfifo, O_WRONLY);
    if (dronepid_fd == -1)
    {
        printf("Failed to open dronepidfifo\n");
    }
    else
    {
        printf("Opened dronepidfifo\n");
    }

    // Send the pid to the watchdog
    int pid = getpid();
    int pid_buf[1] = {pid};
    write(dronepid_fd, pid_buf, sizeof(pid_buf));
    
    // Create the shared memory for the watchdog PID
    sem_t *sem_watchdog = sem_open(WATCHDOG_SEMAPHORE, O_CREAT, S_IRWXU | S_IRWXG, 1);
    if (sem_watchdog == SEM_FAILED)
    {
        perror("Failed to create sem_watchdog");
    }
    else
    {
        printf("Created sem_watchdog\n");
    }
    sem_init(sem_watchdog, 1, 0);
    int shm_watchdog = shm_open(WATCHDOG_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shm_watchdog == -1)
    {
        perror("Failed to create shm_watchdog");
    }
    else
    {
        printf("Created shm_watchdog\n");
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
    }
    watchdog_pid = watchdog_list[0];
    printf("Watchdog PID: %d\n", watchdog_list[0]);

    // Open a FIFO to receive the keypressed values
    int keypressed_fd;
    char *keypressedfifo = "/home/mark/Robotics_Masters/Advanced_Robot_Programming/Assignments/Assignment_1/tmp/keypressedfifo";
    fd_set rfds;
    struct timeval tv;
    while(1)
    {
        keypressed_fd = open(keypressedfifo, O_RDONLY);
        if (keypressed_fd == -1)
        {
            perror("Failed to open keypressedfifo");
        }
        else
        {
            printf("Opened keypressedfifo\n");
            break;
        }
    }

    // Open the shared memory for the window size
    sem_t *sem_window = sem_open(WINDOW_SHM_SEMAPHORE, 0);
    if (sem_window == SEM_FAILED)
    {
        perror("Drone - Failed to open sem_window");
    }
    sem_wait(sem_window);
    int shm_window = shm_open(WINDOW_SIZE_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shm_window == -1)
    {
        perror("Drone - Failed to create shm_window");
    }
    int *window_ptr = mmap(NULL, WINDOW_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_window, 0);
    sem_post(sem_window);

    // Open the shared memory for the drone position
    sem_t *sem_drone = sem_open(DRONE_POSITION_SEMAPHORE, 0);
    if (sem_drone == SEM_FAILED)
    {
        perror("Drone - Failed to open sem_drone");
    }
    sem_init(sem_drone, 1, 0);
    int shm_drone = shm_open(DRONE_POSITION_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shm_drone == -1)
    {
        perror("Drone - Failed to create shm_drone");
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
            perror("Select() error");
        }
        else if (retval)
        {
            read(keypressed_fd, buf, sizeof(buf));
            //printf("%d\n", buf[0]);
        }
        keyvalue = buf[0];

        // Receive size of window from UI
        int window_list[2];
        int size = sizeof(window_list);
        sem_wait(sem_window);
        memcpy(window_list, window_ptr, size);
        sem_post(sem_window);
        droneMaxY = window_list[0];
        droneMaxX = window_list[1];
        //printf("%d %d\n", window_list[0], window_list[1]);

        // Calculate the drone position
        if ((dronePosition[0] > 2) && (dronePosition[1] > 2) && (dronePosition[0] <= (droneMaxY-2)) && (dronePosition[1] <= (droneMaxX-2)))
        {
            dronePositionChange[0] = CalculatePositionY(forceY, Y[0], Y[1]);
            dronePositionChange[1] = CalculatePositionX(forceX, X[0], X[1]);
            dronePosition[0] = droneStartPosition[0] + dronePositionChange[0];
            dronePosition[1] = droneStartPosition[1] + dronePositionChange[1];
            X[1] = X[0];
            X[0] = dronePositionChange[1];
            Y[1] = Y[0];
            Y[0] = dronePositionChange[0];
            printf("%d %d\n", dronePosition[0], dronePosition[1]);
            printf("%d %d\n", droneMaxY, droneMaxX);
        }
        else
        {
            // Remove
            dronePositionChange[0] = CalculatePositionY(forceY, Y[0], Y[1]);
            dronePositionChange[1] = CalculatePositionX(forceX, X[0], X[1]);
            dronePosition[0] = droneStartPosition[0] + dronePositionChange[0];
            dronePosition[1] = droneStartPosition[1] + dronePositionChange[1];
            X[1] = X[0];
            X[0] = dronePositionChange[1];
            Y[1] = Y[0];
            Y[0] = dronePositionChange[0];
            printf("%d %d\n", dronePosition[0], dronePosition[1]);
            printf("%d %d\n", droneMaxY, droneMaxX);
        }

        // Send drone position to UI
        size = sizeof(dronePosition);
        sem_wait(sem_drone);
        memcpy(drone_ptr, dronePosition, size);
        sem_post(sem_drone);

        // Set the force according to the key pressed
        if (keyvalue == 113)
        {
            forceX = forceX - INPUT_FORCE;
            forceY = forceY - INPUT_FORCE;
            printf("TOP LEFT\n");
        }
        else if (keyvalue == 119)
        {
            forceY = forceY - INPUT_FORCE;
            printf("TOP\n");
        }
        else if (keyvalue == 101)
        {
            forceX = forceX + INPUT_FORCE;
            forceY = forceY - INPUT_FORCE;
            printf("TOP RIGHT\n");
        }
        else if (keyvalue == 97)
        {
            forceX = forceX - INPUT_FORCE;
            printf("LEFT\n");
        }
        else if (keyvalue == 115)
        {
            forceX = 0;
            forceY = 0;
            printf("STOP\n");
        }
        else if (keyvalue == 100)
        {
            forceX = forceX + INPUT_FORCE;
            printf("RIGHT\n");
        }
        else if (keyvalue == 122)
        {
            forceX = forceX - INPUT_FORCE;
            forceY = forceY + INPUT_FORCE;
            printf("BOTTOM LEFT\n");
        }
        else if (keyvalue == 120)
        {
            forceY = forceY + INPUT_FORCE;
            printf("BOTTOM\n");
        }
        else if (keyvalue == 99)
        {
            forceX = forceX + INPUT_FORCE;
            forceY = forceY + INPUT_FORCE;
            printf("BOTTOM RIGHT\n");
        }

        // Flag that executes if an interrupt is encountered
        if (flag == 1)
        {
            sem_close(sem_window);
            sem_unlink(WINDOW_SHM_SEMAPHORE);
            munmap(window_ptr, WINDOW_SHM_SIZE);
            if (shm_unlink(WINDOW_SIZE_SHM) == -1)
            {
                perror("Drone - Failed to close window_shm");
                printf("Drone - Failed to close window_shm\n");
            }

            close(keypressed_fd);
            close(dronepid_fd);

            sem_close(sem_drone);
            sem_unlink(USER_INPUT_SEMAPHORE);
            munmap(drone_ptr, USER_SHM_SIZE);
            if (shm_unlink(DRONE_POSITION_SHM) == -1)
            {
                perror("Failed to close window_shm");
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
        //sleep(2);
    }

    return 0;
}