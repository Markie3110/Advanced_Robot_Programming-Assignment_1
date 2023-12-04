#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/wait.h>
#include <signal.h>
#include <strings.h>

void interrupt_signal_handler(int signum)
{
    sleep(3);
    printf("Interrupt Called\n");
    kill(-2, SIGINT);
    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = SIG_DFL;
    sigaction(SIGINT, &act, NULL);
    raise(SIGINT);
}

void watchdog_signal_handler(int signum)
{
    sleep(3);
    printf("PROCESSES KILLED BY WATCHDOG\n");
    kill(-2, SIGINT);
}

int main(int argc, char *argv[])
{
    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = &interrupt_signal_handler;
    sigaction(SIGINT, &act, NULL);

    struct sigaction act2;
    bzero(&act2, sizeof(act2));
    act2.sa_handler = &interrupt_signal_handler;
    sigaction(SIGUSR1, &act2, NULL);

    char *server_arg_list[] = {"konsole", "-e", "./server", NULL};
    char *userinterface_arg_list[] = {"konsole", "-e", "./userinterface", NULL};
    char *drone_arg_list[] = {"konsole", "-e", "./drone", NULL};
    char *watchdog_arg_list[] = {"konsole", "-e", "./watchdog", NULL};

    for (int i = 0; i < 4; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("Failed to fork");
            exit(1);
        }
        else if (pid == 0)
        {
            if (i == 0)
            {
                execvp(server_arg_list[0], server_arg_list);
                perror("Failed to create server");
                return -1;
            }
            else if (i == 1)
            {
                execvp(userinterface_arg_list[0], userinterface_arg_list);
                perror("Failed to create UI");
                return -1;
            }
            else if (i == 2)
            {
                execvp(drone_arg_list[0], drone_arg_list);
                perror("Failed to create drone process");
                return -1;
            }
            else if (i ==3)
            {
                execvp(watchdog_arg_list[0], watchdog_arg_list);
                perror("Failed to create watchdog process");
                return -1;
            }
        }
        else
        {
            switch (i)
            {
                case 0:
                    printf("Created the server with pid %d\n", pid);
                break;

                case 1:
                    printf("Created the UI with pid %d\n", pid);
                break;

                case 2:
                    printf("Created the drone with pid %d\n", pid);
                break;

                case 3:
                    printf("Created the watchdog with pid %d\n", pid);
                break;
            }
        }
    }

    for (int i = 0; i < 4; i++)
    {
        int status;
        pid_t pid = wait(&status);
        if (WIFEXITED(status))
        {
            printf("Child %d exited normally\n", pid);
        }
        else
        {
            printf("Child %d exited abnormally\n", pid);
        }
    }

    return 0;
}