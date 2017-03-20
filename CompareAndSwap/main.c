#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include <papi.h>

#define MAX_THREADS 64
#define CACHELINE_SIZE 64

#ifndef ATOMIC_BARRIER
#define ATOMIC_BARRIER
//#define PTHREAD_BARRIER

#ifdef ATOMIC_BARRIER
struct BVAR {
    volatile unsigned int b;
}__attribute__((aligned(CACHELINE_SIZE)));

static struct BVAR* thread_ctrs;
#endif
#ifdef PTHREAD_BARRIER
pthread_barrier_t barrier;
#endif

volatile long long sum = 0;

pthread_t threads[MAX_THREADS];

// Double Buffers
struct BBUFFER {
    volatile unsigned int value;
    char padding[60];
}__attribute__((aligned(CACHELINE_SIZE)));

int NUM_OF_BUFFER = 2;
struct BBUFFER buffer_1[MAX_THREADS][2];
struct BBUFFER buffer_2[MAX_THREADS][2];

void *thread_main(void *);
void *thread_combining(void *);

int SIZE_OF_INT_SOURCES = 16;
int int_data_source[16] = {0};

// Number of threads & iteration
int num_of_threads = -1;
int num_of_iteration = -1;

void RTS_sync(int tid)
{
#ifdef ATOMIC_BARRIER
    int i;
    __atomic_add_fetch(&thread_ctrs[tid].b,1,5);
    for(i = 0; i < num_of_threads; i++){
        while(__atomic_load_n(&thread_ctrs[i].b,5)<thread_ctrs[tid].b);
    }
#endif
#ifdef PTHREAD_BARRIER
    pthread_barrier_wait(&barrier);
#endif
}

// Randomly generate input value less than the ceiling value, in this case which is 1024
void init() {
    {
        int i;
        int ceiling = 1024;
        
        for(i=0; i<SIZE_OF_INT_SOURCES; i++){
            int_data_source[i] = rand() & (ceiling-1);
        }
    }
}

int change_buffer(int turn) {
    return (turn == 0) ? 1 : 0;
}

// Compare And Swap Actor
static inline void CompareAndSwap(int thread_index) {
    
    
    // change Global "firstTurn" var to Local "turn" var
    int turn = 1;
    
    int i;
    
    int laminar_x1;
    int laminar_x2;
    int laminar_x3;
    int laminar_x4;
    
    int interval;
    int source_index;
    
    for(i = 0; i < num_of_iteration; i++) {
        // Read from data source
        {
            // Calulate source index
            interval = num_of_threads * NUM_OF_BUFFER * i;
            source_index = (interval + (thread_index * NUM_OF_BUFFER)) % SIZE_OF_INT_SOURCES;
            
            laminar_x1 = int_data_source[source_index];
            laminar_x2 = int_data_source[source_index + 1];
        }
        
        // Compare and Swap
        {
            int x1 = laminar_x1;
            int x2 = laminar_x2;
            
            if (x1 > x2) {
                laminar_x3 = x2;
                laminar_x4 = x1;
            } else {
                laminar_x3 = x1;
                laminar_x4 = x2;
            }
        }
        
        // Write on Doulbe Buffers
        {
            if (turn) {
                buffer_1[thread_index][0].value = laminar_x3;
                buffer_1[thread_index][1].value = laminar_x4;
            } else {
                buffer_2[thread_index][0].value = laminar_x3;
                buffer_2[thread_index][1].value = laminar_x4;
            }
            turn = change_buffer(turn);
        }
        
        RTS_sync(thread_index);
    }
    RTS_sync(thread_index);
}

static inline void combining_thread(int thread_index) {
    int turn = 1;
    int i, j;
    
    // We have to read nothing on the initial state
    RTS_sync(thread_index);
    
    // Read Double Buffers
    for (i = 0; i < num_of_iteration; i++) {
        if (turn) {
            for (j = 0; j<num_of_threads; j++) {
                sum += buffer_1[j][0].value;
                sum += buffer_1[j][1].value;
            }
        } else {
            for (j = 0; j<num_of_threads; j++) {
                sum += buffer_2[j][0].value;
                sum += buffer_2[j][1].value;
            }
        }
        turn = change_buffer(turn);
        RTS_sync(thread_index);
    }
}

void usage() {
    printf("\nParalellized CompareAndSwap Usage : \n\n");
    printf("Options -t is The Number of threads. (1 ~ 64)\n");
    printf("Options -i is The Number of itertaion. (1 ~ 10000)\n\n");
    exit(1);
}

int arg_to_int(char* arg) {
    return (int) strtol(arg, NULL, 10);
}

void validate_global_values() {
    if (num_of_iteration == -1 || num_of_threads == -1 || num_of_threads > MAX_THREADS) {
        usage();
        exit(1);
    }
}

int main(int argc, char **argv) {
    int i;
    int rc;
    int status;
    int c;
    long long elapsed_us, elapsed_cyc;
    long_long values[1] = {(long_long) 0};
    
    while(1)
    {
        static struct option long_options[] = {
            {"help" , no_argument, 0, 'h'},
            {"iteration" , required_argument, 0, 'i'},
            {"thread" , required_argument, 0, 't'},
            {0, 0, 0, 0}
        };
        int option_index = 0;
        
        c = getopt_long(argc, argv, "hit:", long_options, &option_index);
        
        if (c  == -1)
            break;
        
        switch(c)
        {
            case 0:
                break;
            case 'h':
                usage();
                break;
            case 'i':
                num_of_iteration = atoi(optarg);
                break;
            case 't':
                num_of_threads = atoi(optarg);
                break;
            default:
                usage();
        }
    }
    
    validate_global_values();
    
    /*Initialize the PAPI library */
    PAPI_library_init(PAPI_VER_CURRENT);
    
    elapsed_us = PAPI_get_real_usec();
    elapsed_cyc = PAPI_get_real_cyc();
    
    // init input array
    init();
#ifdef ATOMIC_BARRIER
    posix_memalign((void**)&thread_ctrs, CACHELINE_SIZE, sizeof(struct BVAR)*num_of_threads);
    for (i = 0; i < num_of_threads; i++){
        thread_ctrs[i].b=0;
    }
#endif
#ifdef PTHREAD_BARRIER
    // Init pthread barrier
    pthread_barrier_init(&barrier, NULL, num_of_threads+1);
#endif
    
    // Create Actor Threads
    for (i = 0; i < num_of_threads; i++) {
        pthread_create(&threads[i], NULL, &thread_main, (void *)i);
    }
    
    // Create Combining Thread
    pthread_create(&threads[num_of_threads], NULL, &thread_combining, (void *)num_of_threads);
    
    // join All Threads
    for (i = num_of_threads; i >= 0; i--)
    {
        pthread_join(threads[i], (void **)&status);
    }
    
#ifdef PTHREAD_BARRIER
    // Destroy pthread barrier
    pthread_barrier_destroy(&barrier);
#endif
    
    printf("sum: %lld\n", sum);
    
    elapsed_cyc = PAPI_get_real_cyc() - elapsed_cyc;
    elapsed_us = PAPI_get_real_usec() - elapsed_us;
    
    printf("Master real usec   : \t%lld\n", elapsed_us);
    printf("Master real cycles : \t%lld\n", elapsed_cyc);
    
    return 0;
}

void *thread_main(void *arg)
{
    CompareAndSwap((int)arg);
    pthread_exit((void *) 0);
}

void *thread_combining(void *arg)
{
    combining_thread((int)arg);
    pthread_exit((void *) 0);
}
