// Maxim Tikhonov mtikhon@siue.edu
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>

// the followings are for shared memory / message queue----
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

#define SHM_KEY 7295
#define MSG_KEY 7299

#define NUM_REPEATS 200 // number of loops for high-priority processes
#define NUM_CHILD 4     // number of the child processes

#define BUFFER_SIZE 1024 // max. message queue size

#define ONE_SECOND 1000    //    1 second
#define THREE_SECONDS 3000 //    3 seconds

// function prototypes
unsigned int uniform_rand(void); // a random number generator
void millisleep(unsigned ms);    // for random sleep time

int create_msg_queue();
int msg_send(int msgid, int msg_number);
int msg_receive(int msgid);
int delete_msg_queue(int msgid);

int create_shared_mem();
struct my_mem *attach_shared_mem(int shm_id);
int detach_shared_mem(struct my_mem *p_shm);
int delete_shared_mem(int shm_id);

int create_child_processes(struct my_mem *p_shm, int msgid); // starts fork()s and calls child functions
void process_C1(struct my_mem *p_shm, int msgid);            // consumer 1
void process_C2(struct my_mem *p_shm, int msgid);            // consumer 2
void process_C3(struct my_mem *p_shm, int msgid);            // producer 1
void process_C4(struct my_mem *p_shm, int msgid);            // producer 2

// definition of message -------------------------------------------
struct message
{
    long mtype;
    int mnum;
};

// shared memory definition ---------------------------------------
struct my_mem
{
    unsigned int Go_Flag;
    unsigned int Done_Flag[4];
    int Individual_Sum[4];
    unsigned int Child_Count;
};

int main(void)
{
    int shm_id;           // the shared memory ID
    int shm_size;         // the size of the shared memoy
    struct my_mem *p_shm; // pointer to the attached shared memory

    int msgid = create_msg_queue();    // create the message queue
    shm_id = create_shared_mem();      // create the shared memory
    p_shm = attach_shared_mem(shm_id); // attach the shared memory

    int child_status = create_child_processes(p_shm, msgid); // create the child processes

    int msg_delete_status = delete_msg_queue(msgid);   // delete the message queue
    int mem_delete_status = delete_shared_mem(shm_id); // delete the shared memory

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
    int msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("msgget");
        return EXIT_FAILURE;
    }
    return msgid;
}

int msg_send(int msgid, int msg_number)
{
    struct message msg;
    msg.mtype = 1;
    msg.mnum = msg_number;

    int send_result = msgsnd(msgid, &msg, sizeof(msg.mnum), 0);
    if (send_result == -1)
    {
        perror("msgsnd");
        return EXIT_FAILURE;
    }
    return send_result;
}

int msg_receive(int msgid)
{
    struct message msg;
    msg.mtype = 1;
    msg.mnum = 0;
    int receive_result = msgrcv(msgid, &msg, sizeof(msg.mnum), 0, 0);
    if (receive_result == -1)
    {
        perror("msgrcv");
        return EXIT_FAILURE;
    }
    return msg.mnum;
}

int delete_msg_queue(int msgid)
{
    if (msgctl(msgid, IPC_RMID, nullptr) == -1)
    {
        perror("msgctl (delete)");
        return EXIT_FAILURE;
    }
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

struct my_mem *attach_shared_mem(int shm_id)
{
    // attach the new shared memory ----
    struct my_mem *p_shm = (struct my_mem *)shmat(shm_id, NULL, 0);
    if (p_shm == (struct my_mem *)-1)
    {
        fprintf(stderr, "Failed to attach the shared memory.  Terminating ..\n");
        exit(0);
    }

    // initialize the shared memory ----
    p_shm->Go_Flag = 0;
    p_shm->Child_Count = 0;
    for (int i = 0; i < 4; i++)
    {
        p_shm->Done_Flag[i] = 0;
        p_shm->Individual_Sum[i] = 0;
    }

    return p_shm;
}

int detach_shared_mem(struct my_mem *p_shm)
{
    // detach the shared memory ----
    int ret_val = shmdt(p_shm);
    if (ret_val != 0)
    {
        return -1; // Indicate error
    }
    return 0; // Indicate success
}

int delete_shared_mem(int shm_id)
{
    if (shmctl(shm_id, IPC_RMID, nullptr) == -1)
    {
        perror("shmctl (delete)");
        return -1; // Indicate error
    }
    return 0; // Indicate success
}

int create_child_processes(struct my_mem *p_shm, int msgid)
{

    printf("================ THE PARENT PROCESS STARTS ==================\n");
    for (int i = 0; i < NUM_CHILD; i++)
    {
        pid_t process_id = fork();

        if (process_id == 0)
        {
            // Child process
            switch (i)
            {
            case 0:
                process_C1(p_shm, msgid);
                break;
            case 1:
                process_C2(p_shm, msgid);
                break;
            case 2:
                process_C3(p_shm, msgid);
                break;
            case 3:
                process_C4(p_shm, msgid);
                break;
            default:
                break;
            }
        }
    }

    // The parent process ----------------------------- //
    int send_checksum;
    int recv_checksum;

    int k = 0; // spin wait until all child processes created
    while (p_shm->Child_Count < NUM_CHILD)
    {
        k = k + 1;
    }

    p_shm->Go_Flag = 1; // all children created, parent says GO
    printf("--------------- The parent process waits for children to terminate\n\n");

    while (true)
    { // spin wait till all children complete
        bool all_done = true;
        for (int i = 0; i < NUM_CHILD; i++)
        {
            if (p_shm->Done_Flag[i] != 1)
            {
                all_done = false;
                break; // at least one child not done
            }
        }
        if (all_done)
        {
            break; // exit loop when all children are done
        }
    }

    send_checksum = p_shm->Individual_Sum[2] + p_shm->Individual_Sum[3];
    recv_checksum = p_shm->Individual_Sum[0] + p_shm->Individual_Sum[1];
    printf("PARENT PROCESS REPORT ************************************\n");
    printf("    C1 checksum: %d\n", p_shm->Individual_Sum[0]);
    printf("    C2 checksum: %d\n", p_shm->Individual_Sum[1]);
    printf("    C3 checksum: %d\n", p_shm->Individual_Sum[2]);
    printf("    C4 checksum: %d\n", p_shm->Individual_Sum[3]);
    printf("    SEND-CHECKSUM: %d\n", send_checksum);
    printf("    RECV-CHECKSUM: %d\n", recv_checksum);
    printf("------------------------ the parent process is terminating ...\n");

    // detach the shared memory ---
    int ret_val = detach_shared_mem(p_shm);
    if (ret_val != 0)
    {
        printf("shared memory detach failed (P) ....\n");
    }
    return 0;
}

// Process C1 (consumer 1) =============================================================
void process_C1(struct my_mem *p_shm, int msgid)
{
    int i;                        // the loop counter
    int status;                   // result status code
    unsigned int my_rand;         // a randon number
    unsigned int checksum;        // the local checksum
    unsigned int child_index = 0; // child's index in the Done_Flag array

    // REQUIRED output #1 -------------------------------------------
    // NOTE: C1 can not make any output before this output
    printf("    Child Process #1 is created ....\n");
    printf("    I am the first consumer ....\n\n");

    p_shm->Child_Count = p_shm->Child_Count + 1;

    int k = 0; // spin wait until parent says READY
    while (p_shm->Go_Flag == 0)
    {
        k = k + 1;
    }

    // REQUIRED: shuffle the seed for random generator --------------
    srand(time(0));
    for (i = 0; i < NUM_REPEATS; i++)
    {
        my_rand = msg_receive(msgid);                                                      // receive the random number from the message queue
        p_shm->Individual_Sum[child_index] = p_shm->Individual_Sum[child_index] + my_rand; // update the local checksum
    }

    // REQUIRED 3 second wait ---------------------------------------
    millisleep(THREE_SECONDS);

    // REQUIRED output #2 -------------------------------------------
    // NOTE: after the following output, C1 can not make any output
    printf("    Child Process #1 is terminating (checksum: %d) ....\n\n", p_shm->Individual_Sum[child_index]);

    // raise my "Done_Flag" -----------------------------------------
    p_shm->Done_Flag[child_index] = 1; // I m done!

    int ret_val = detach_shared_mem(p_shm); // detach the shared memory
    if (ret_val != 0)
    {
        printf("shared memory detach failed (C1) ....\n");
    }
    exit(0); // terminate the process
}

// Process C2 (consumer 2) =============================================================
void process_C2(struct my_mem *p_shm, int msgid)
{
    int i;                        // the loop counter
    int status;                   // result status code
    unsigned int my_rand;         // a randon number
    unsigned int checksum;        // the local checksum
    unsigned int child_index = 1; // child's index in the Done_Flag array

    // REQUIRED output #1 -------------------------------------------
    // NOTE: C1 can not make any output before this output
    printf("    Child Process #2 is created ....\n");
    printf("    I am the second consumer ....\n\n");

    p_shm->Child_Count = p_shm->Child_Count + 1;

    int k = 0; // spin wait until parent says READY
    while (p_shm->Go_Flag == 0)
    {
        k = k + 1;
    }

    // REQUIRED: shuffle the seed for random generator --------------
    srand(time(0));
    for (i = 0; i < NUM_REPEATS; i++)
    {
        my_rand = msg_receive(msgid);                                                      // receive the random number from the message queue
        p_shm->Individual_Sum[child_index] = p_shm->Individual_Sum[child_index] + my_rand; // update the local checksum
    }

    // REQUIRED 3 second wait ---------------------------------------
    millisleep(THREE_SECONDS);

    // REQUIRED output #2 -------------------------------------------
    // NOTE: after the following output, C1 can not make any output
    printf("    Child Process #2 is terminating (checksum: %d) ....\n\n", p_shm->Individual_Sum[child_index]);

    // raise my "Done_Flag" -----------------------------------------
    p_shm->Done_Flag[child_index] = 1; // I m done!

    int ret_val = detach_shared_mem(p_shm); // detach the shared memory
    if (ret_val != 0)
    {
        printf("shared memory detach failed (C2) ....\n");
    }
    exit(0); // terminate the process
}

// Process C3 (producer 1) =============================================================
void process_C3(struct my_mem *p_shm, int msgid)
{
    int i;                        // the loop counter
    int status;                   // result status code
    unsigned int my_rand;         // a randon number
    unsigned int checksum;        // the local checksum
    unsigned int child_index = 2; // child's index in the Done_Flag array

    // REQUIRED output #1 -------------------------------------------
    // NOTE: C1 can not make any output before this output
    printf("    Child Process #3 is created ....\n");
    printf("    I am the first producer ....\n\n");

    p_shm->Child_Count = p_shm->Child_Count + 1;

    int k = 0; // spin wait until parent says READY
    while (p_shm->Go_Flag == 0)
    {
        k = k + 1;
    }

    // REQUIRED: shuffle the seed for random generator --------------
    srand(time(0));
    for (i = 0; i < NUM_REPEATS; i++)
    {
        my_rand = uniform_rand();
        p_shm->Individual_Sum[child_index] = p_shm->Individual_Sum[child_index] + my_rand; // update the local checksum
        status = msg_send(msgid, my_rand);                                                 // send the random number to the message queue
    }

    // REQUIRED 3 second wait ---------------------------------------
    millisleep(THREE_SECONDS);

    // REQUIRED output #2 -------------------------------------------
    // NOTE: after the following output, C1 can not make any output
    printf("    Child Process #3 is terminating (checksum: %d) ....\n\n", p_shm->Individual_Sum[child_index]);

    // raise my "Done_Flag" -----------------------------------------
    p_shm->Done_Flag[child_index] = 1; // I m done!

    int ret_val = detach_shared_mem(p_shm); // detach the shared memory
    if (ret_val != 0)
    {
        printf("shared memory detach failed (C3) ....\n");
    }
    exit(0); // terminate the process
}

// Process C4 (producer 2) =============================================================
void process_C4(struct my_mem *p_shm, int msgid)
{
    int i;                        // the loop counter
    int status;                   // result status code
    unsigned int my_rand;         // a randon number
    unsigned int checksum;        // the local checksum
    unsigned int child_index = 3; // child's index in the Done_Flag array

    // REQUIRED output #1 -------------------------------------------
    // NOTE: C1 can not make any output before this output
    printf("    Child Process #4 is created ....\n");
    printf("    I am the second producer ....\n\n");

    p_shm->Child_Count = p_shm->Child_Count + 1;

    int k = 0; // spin wait until parent says READY
    while (p_shm->Go_Flag == 0)
    {
        k = k + 1;
    }

    // REQUIRED: shuffle the seed for random generator --------------
    srand(time(0));
    for (i = 0; i < NUM_REPEATS; i++)
    {
        my_rand = uniform_rand();
        p_shm->Individual_Sum[child_index] = p_shm->Individual_Sum[child_index] + my_rand; // update the local checksum
        status = msg_send(msgid, my_rand);                                                 // send the random number to the message queue
    }

    // REQUIRED 3 second wait ---------------------------------------
    millisleep(THREE_SECONDS);

    // REQUIRED output #2 -------------------------------------------
    // NOTE: after the following output, C1 can not make any output
    printf("    Child Process #4 is terminating (checksum: %d) ....\n\n", p_shm->Individual_Sum[child_index]);

    // raise my "Done_Flag" -----------------------------------------
    p_shm->Done_Flag[child_index] = 1; // I m done!

    int ret_val = detach_shared_mem(p_shm); // detach the shared memory
    if (ret_val != 0)
    {
        printf("shared memory detach failed (C4) ....\n");
    }
    exit(0); // terminate the process
}