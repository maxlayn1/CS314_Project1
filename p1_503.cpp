// Maxim Tikhonov mtikhon@siue.edu
#include <iostream>
#include <stdio.h>
#include <stdlib.h> 
#include <sys/types.h>
#include <unistd.h> 

// the followings are for shared memory / message queue----
#include <sys/ipc.h>
#include <sys/shm.h> 
#include <sys/msg.h>

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

int create_msg_queue();
int msg_send(int msgid, int msg_number);
int delete_msg_queue(int msgid);

int create_shared_mem();
struct my_mem* attach_shared_mem(int mem_id);
int delete_shared_mem(int mem_id);

int create_child_processes(struct my_mem * p_shm, int mem_id);


// definition of message -------------------------------------------
struct message{
    long mtype;
    int mnum;
};

// shared memory definition ---------------------------------------
struct my_mem {
    long int counter;
    int      parent;
    int      child;  
};

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

    int msg_number = uniform_rand();
    int msgid = create_msg_queue();
    int msg_sent = msg_send(msgid, msg_number);

    shm_id = create_shared_mem();
    p_shm = attach_shared_mem(shm_id);

    int child_status = create_child_processes(p_shm, shm_id);

    int msg_delete_status = delete_msg_queue(msgid);
    int mem_delete_status = delete_shared_mem(shm_id);

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

int create_msg_queue() 
{
    int msgid = msgget(SHM_KEY, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget");
        return EXIT_FAILURE;
    }
    //cout << "Message queue ID: " << msgid << endl;

    return msgid;
}

int msg_send(int msgid, int msg_number) 
{
    struct message msg;
    msg.mtype = 1;
    msg.mnum = msg_number;

    int send_result = msgsnd(msgid, &msg, sizeof(msg.mnum), 0);
    if (send_result == -1) {
        perror("msgsnd");
        return EXIT_FAILURE;
    }

    return send_result;
}

int delete_msg_queue(int msgid) 
{
    if (msgctl(msgid, IPC_RMID, nullptr) == -1) {
        perror("msgctl (delete)");
        return EXIT_FAILURE;
    }

    cout << "Message queue successfully deleted." << endl;
    return EXIT_SUCCESS;
}

int create_shared_mem() 
{
    // find the shared memory size in bytes ----
    int shm_size = sizeof(my_mem);   
    if (shm_size <= 0)
    {  
        fprintf(stderr, "sizeof error in acquiring the shared memory size. Terminating ..\n");
        exit(0); 
    }

    // create a shared memory ----
   int shm_id = shmget(SHM_KEY, shm_size, IPC_CREAT | 0666);         
   if (shm_id < 0) 
   {
      fprintf(stderr, "Failed to create the shared memory. Terminating ..\n");  
      exit(0);  
   } 
   return shm_id;
}

struct my_mem* attach_shared_mem(int mem_id) 
{
   // attach the new shared memory ----
   struct my_mem *p_shm = (struct my_mem *)shmat(mem_id, NULL, 0);     
   if (p_shm == (struct my_mem*) -1)
   {
      fprintf(stderr, "Failed to attach the shared memory.  Terminating ..\n"); 
      exit(0);   
   }   

   // initialize the shared memory ----
   p_shm->counter  = 0;  
   p_shm->parent   = 0;   
   p_shm->child    = 0;

   return p_shm;
}

int delete_shared_mem(int mem_id) 
{
    if (shmctl(mem_id, IPC_RMID, nullptr) == -1) {
        perror("shmctl (delete)");
        return -1; // Indicate error
    }
    cout << "Shared memory deleted successfully." << endl;
    return 0; // Indicate success
}

int create_child_processes(struct my_mem * p_shm, int mem_id) 
{
    for (int i = 0; i < NUM_CHILD; i++) {
        // spawn a child process ----
        int process_id = fork();

        // The child process ----------------------------- //
        if (process_id == 0)
        {
            printf("I am the child process ...\n");

            // initialize child process ----    
            p_shm->child = 1; // child started .. 
        
            // wait for the parent to get ready ----
            int k = 0;
            while (p_shm->parent != 1)     
            { k = k + 1; }
 
            for (int i = 0; i < NUM_REPEATS; i++)  
            { 
                // subtract 1 from the shared memory ----
                p_shm->counter = p_shm->counter - 1;  
                printf("subtracter: %d\n", p_shm->counter);   
            }

            // declare the completion ----
            p_shm->child = 0;  
            cout << "child process completed." << endl;

            // detach the shared memory ----
            int ret_val = shmdt(p_shm);  
            if (ret_val != 0)
            {  printf("detaching the shared memory failed (C) ...\n"); }
 
            exit(0); 
        }

        // The parent process ----------------------------- //   
        else 
        {     
            printf("I am the parent process ...\n");  

            // initialize the parent process ----
            p_shm->parent = 1;  // the parent process started ..

            // wait for the child pocess to get ready ----
            int k = 0;
            while (p_shm->child != 1)
            {  k = k + 1; }
            //int k = 0;   
        
            for (int i = 0; i < NUM_REPEATS; i++)   
            { 
                // THE CRITICAL SECTION ============= // 
                p_shm->counter = p_shm->counter + 1;  
                printf("adder: %d\n", p_shm->counter);
            }

            // declare completion ----
            p_shm->parent = 0;  
            cout << "Parent process completed." << endl;

            // wait for the child process to complete ---- 
            k = 0;
            while (p_shm->child != 0)  
            {  k = k + 1;  }   

            printf("Shared Memory Counter: %d\n", p_shm->counter);  

            // detach the shared memory ---
            int ret_val = shmdt(p_shm);  
            if (ret_val != 0) 
            {  printf ("shared memory detach failed (P) ....\n"); }

            ret_val = shmctl(mem_id, IPC_RMID, 0);  
            if (ret_val == -1)
            {  printf("shm remove ID failed ... \n"); }
 
            exit(0);  
        }
        return 0;
    }
}