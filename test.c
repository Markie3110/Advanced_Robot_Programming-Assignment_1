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
#include <time.h>
#include <sys/wait.h>

char currenttime[150];

void* current_time()
{

    int day, month, year, hours, minutes, seconds, milliseconds, microseconds;

    time_t t_now;

    time(&t_now);

    struct tm *local = localtime(&t_now);

    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    local = localtime(&time_now.tv_sec);

    day = local -> tm_mday;
    month = local -> tm_mon + 1;
    year = local -> tm_year + 1900;
    hours = local -> tm_hour;
    minutes = local -> tm_min;
    seconds = local -> tm_sec;
    milliseconds = time_now.tv_usec/1000;
    microseconds = time_now.tv_usec%1000;

    sprintf(currenttime, "%02d-%02d-%02d %02d:%02d:%02d:%d:%d ", day, month, year, hours, minutes, seconds, milliseconds, microseconds);
    return &currenttime;
}

int main(int argc, char *argv)
{
    char *now = current_time();
    printf("%s\n", now);
    return 0;
}