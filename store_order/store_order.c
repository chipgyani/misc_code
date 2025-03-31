// Author: chipgyani at gmail
// Copyright (c) 2025 chipgyani
// MIT License


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#include <stdint.h>
#include <errno.h>

#ifndef NUM_ITERS
 #define NUM_ITERS 128
#endif
#define CACHELINE_SIZE 64

void *write_data(void *args);
void *read_data(void *args);

pthread_barrier_t   barrier; // the barrier synchronization object
volatile uint32_t *flag;     // volatile is needed otherwise -O2 optimization causes hangs in read thread
                             // load of flag gets put outside the loop for "while(flag[0] != i);"
                             // loop only does cmp against a single load of flag into a reg
uint32_t *dat_buf;


int main() {
     pthread_t thread1, thread2;
     int  status;

     // Allocate flag and dat_buf in different cache lines
     status = posix_memalign((void **)&flag, (size_t) CACHELINE_SIZE, (size_t) CACHELINE_SIZE);
     if (status != 0) {
       errno = status;
       perror("posix_memalign returned error for flag");
       exit(EXIT_FAILURE);
     }

     status = posix_memalign((void **)&dat_buf, (size_t) 4*CACHELINE_SIZE, (size_t) NUM_ITERS * sizeof(uint32_t));
     if (status != 0) {
       errno = status;
       perror("posix_memalign returned error for dat_buf");
       free((void *)flag);
       exit(EXIT_FAILURE);
     }

     printf("flag : 0x%llx, dat_buf: 0x%llx\n", (long long int) flag, (long long int) dat_buf);

     pthread_barrier_init (&barrier, NULL, 2);
     flag[0] = rand();
     for (int i = 0; i < NUM_ITERS; i++) {
       dat_buf[i] = rand();
     }

    /* Create two independent threads: one to write the data & flag
       and the other to poll the flag then read the data             */

     status = pthread_create(&thread1, NULL, write_data, NULL);
     if (status != 0) {
       errno = status;
       perror("pthread_create failed for thread1");
       free((void *) flag);
       free(dat_buf);
       exit(EXIT_FAILURE);
     }
     
     status = pthread_create(&thread2, NULL, read_data, NULL);
     if (status != 0) {
       errno = status;
       perror("pthread_create failed for thread2");
       free((void *) flag);
       free(dat_buf);
       exit(EXIT_FAILURE);
     }

     pthread_join( thread1, NULL);
     pthread_join( thread2, NULL);

     free((void *)flag);
     free(dat_buf);
     pthread_barrier_destroy(&barrier);

     exit(EXIT_SUCCESS);
}

void *write_data(void *args) {
     uint32_t i;

     for (i = 0; i < NUM_ITERS; i++) {
       pthread_barrier_wait(&barrier);
       dat_buf[i] = 2*i;
       *flag = i;
     }

     return EXIT_SUCCESS;
}

void *read_data(void *args) {
     uint32_t i;
     int num_errors = 0;

     for (i = 0; i < NUM_ITERS; i++) {
       pthread_barrier_wait(&barrier);
       while (*flag != i);
       if (dat_buf[i] != 2*i) {
	 printf("Error: Ordering mismatch at iteration %d\n", i);
	 num_errors++;
       }
       //printf("Iteration %d: flag %d, dat_buf[%d] = %d\n", i, *flag, i, dat_buf[i]);
     }

     printf("Ordering errors: %d/%d\n", num_errors, i);

     return EXIT_SUCCESS;
}
