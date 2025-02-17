// Maxim Tikhonov mtikhon@siue.edu
#include <iostream>
#include <stdio.h>
#include <stdlib.h> 
#include <sys/types.h>
#include <unistd.h> 

// the followings are for shared memory ----
#include <sys/ipc.h>
#include <sys/shm.h> 

using namespace std;

#define SHM_KEY           7295    // the shared memory key (i think this is mine idk)
#define MSG_KEY           7299    // (unique) message queue key (will need to make sure)

#define NUM_REPEATS        200    // number of loops for high-priority processes
#define NUM_CHILD            4    // number of the child processes

#define BUFFER_SIZE       1024    // max. message queue size

#define ONE_SECOND     1000000    //    1 second
#define THREE_SECONDS  3000000    //    3 seconds

// function prototypes
unsigned int uniform_rand(void);  // a random number generator
void millisleep(unsigned ms);     // for random sleep time

int main(void) {
    pid_t  process_id;
    int    i;                     // external loop counter  
    int    j;                     // internal loop counter  
    int    k = 0;                 // dummy integer  

    int    ret_val;               // system-call return value    
    int    sem_id;                // 
    int    shm_id;                // the shared memory ID 
    int    shm_size;              // the size of the shared memoy  
    struct my_mem * p_shm;        // pointer to the attached shared memory 

    // shared memory definition ----   
    struct my_mem {
        long int counter;
        int      parent;
        int      child;  
    };

    return 0;
}

unsigned int uniform_rand(void)
/* generate a random number 0 ~ 999 */
{
	unsigned int my_rand;
	my_rand = rand() % 1000;
	return (my_rand);
}

void millisleep(unsigned milli_seconds)
{ 
    usleep(milli_seconds * 1000); 
}

// definition of message -------------------------------------------
struct message{
    long mtype;
    int mnum;
};

