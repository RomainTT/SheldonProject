#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "./../API/track_position.h"

    //declaration and initialization of the different mutex
    pthread_mutex_t compute_pos_mux = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t track_pos_mux   = PTHREAD_MUTEX_INITIALIZER;

int main ()
{
    int signal [8] = {128, 255, 98, 3, 5, 0, -5, -2};

    //declaration of the different threads
    pthread_t thread_position;
    pthread_t thread_print_position;

    //immediate lock of the mutex printing the position so first we calculate it at start
    pthread_mutex_lock(&track_pos_mux);

    //creation of the thread calculating the position of the beacon
    if(pthread_create(&thread_position, NULL, compute_position, signal) == -1) {
	printf("pthread_create position fail");
    }

    //creation of the thread tracking the position of the beacon
    if(pthread_create(&thread_print_position, NULL, track_position, NULL) == -1) {
	printf("pthread_create position fail");
    }
    
    //********************************************************************
    //********************************************************************

    //simulation of the drone getting in line with a beacon
    //********************************************************************
    //********************************************************************

    //waiting for the threads to finish before closing the main
    pthread_join(thread_position, NULL);
    pthread_join(thread_print_position, NULL);
    
    //destroy the mutex before closing the main
    pthread_mutex_destroy(&compute_pos_mux);
    pthread_mutex_destroy(&track_pos_mux);

    return 0;
}
	
