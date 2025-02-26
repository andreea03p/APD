#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define INPUT_FILE "commands.txt"
#define LOG_FILE_S "log_s.txt"
#define LOG_FILE_P "log_p.txt"
#define MAX_WORKERS 11
#define BUFFER_SIZE 1024
#define ANAGRAM_BUFFER_SIZE (40320 * 9 + 100)

typedef struct 
{
    int worker_id;
    char result[BUFFER_SIZE];

} WorkerResult;

pthread_mutex_t lock;
int available_workers[MAX_WORKERS];
int num_workers;
FILE *log_file;


void initialize_workers() 
{
    for (int i = 0; i < MAX_WORKERS; i++) 
    {
        available_workers[i] = 1;
    }
}

int is_prime(int num) 
{
    if (num <= 1) 
    {
        return 0;
    }
    for (int i = 2; i <= sqrt(num); i++) 
    {
        if (num % i == 0)
        {
            return 0;
        }
    }
    return 1;
}

int count_primes(int n) 
{
    int count = 0;
    for (int i = 1; i <= n; i++) 
    {
        if (is_prime(i)) 
        {
            count++;
        }
    }
    return count;
}

int count_prime_divisors(int n) 
{
    int count = 0;
    for (int i = 2; i <= n; i++) 
    {
        if (n % i == 0 && is_prime(i)) 
        {
            count++;
        }
    }
    return count;
}


void generate_anagrams_rec(char *str, int start, int end, char **result, int *offset, int *capacity) 
{
    if (start == end) 
    {
        int len = strlen(str) + 1;

        while (*offset + len >= *capacity) 
        {
            *capacity *= 2;
            *result = realloc(*result, *capacity);
            if (*result == NULL) 
            {
                perror("fail reallocating result anagrams");
                exit(EXIT_FAILURE);
            }
        }

        snprintf(*result + *offset, *capacity - *offset, "%s\n", str);
        *offset += len;
    } 
    else 
    {
        for (int i = start; i <= end; i++) 
        {
            char temp = str[start];
            str[start] = str[i];
            str[i] = temp;

            generate_anagrams_rec(str, start + 1, end, result, offset, capacity);

            temp = str[start];
            str[start] = str[i];
            str[i] = temp;
        }
    }
}

char *generate_anagrams_return(char *str) 
{
    int offset = 0;
    int capacity = BUFFER_SIZE;
    char *result = (char*) malloc(capacity);
    if (!result) 
    {
        perror("fail allocating anagrams result");
        exit(EXIT_FAILURE);
    }
    result[0] = '\0';

    generate_anagrams_rec(str, 0, strlen(str) - 1, &result, &offset, &capacity);

    if (offset < capacity)
    {
        result[offset] = '\0';
    }

    return result;
}


void compute_serial() 
{
    FILE *commandsFile = fopen(INPUT_FILE, "r");
    if (commandsFile == NULL) 
    {
        perror("failed to open commands file serial");
        exit(EXIT_FAILURE);
    }

    FILE *logFile = fopen(LOG_FILE_S, "w");
    if (logFile == NULL) 
    {
        perror("failed to open log file serial");
        fclose(commandsFile);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    while (fgets(buffer, sizeof(buffer), commandsFile)) 
    {
        if (strncmp(buffer, "WAIT", 4) == 0) 
        {
            continue;
        } 

        char client[32], task[32], param[32];
        sscanf(buffer, "%s %s %s", client, task, param);

        if (strcmp(task, "PRIMES") == 0) 
        {
            int n = atoi(param);
            int count = count_primes(n);
            fprintf(logFile, "Found %d primes in the first %d numbers.\n", count, n);
            fflush(logFile);
        } 
        else if (strcmp(task, "PRIMEDIVISORS") == 0) 
        {
            int n = atoi(param);
            int count = count_prime_divisors(n);
            fprintf(logFile, "Found %d prime divisors of %d.\n", count, n);
            fflush(logFile);
        } 
        else if (strcmp(task, "ANAGRAMS") == 0) 
        {
            char word[32];
            strcpy(word, param);
            char *anagrams = generate_anagrams_return(param);
            fprintf(logFile, "Anagrams of %s are: %s\n", param, anagrams);
            fflush(logFile);
            free(anagrams);
        } 
        else 
        {
            fprintf(logFile, "Unknown command: %s", task);
            fflush(logFile);
        }
    }

    fclose(commandsFile);
    fclose(logFile);
}


void *receive_thread(void *arg) 
{
    char *result = (char*) malloc(ANAGRAM_BUFFER_SIZE);
    if (!result) 
    {
        perror("error allocating memory for result");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        MPI_Status status;
        MPI_Recv(result, ANAGRAM_BUFFER_SIZE, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

        int worker_id = status.MPI_SOURCE;

        if (strcmp(result, "STOP") == 0) 
        {
            printf("Received STOP from worker %d. Terminating receiver thread.\n", worker_id);
            break;
        }

        time_t now = time(NULL);
        char *time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = '\0';
        fprintf(log_file, "[%s] Task completed by worker %d\n", time_str, worker_id);
        fflush(log_file);

        char client[32] = {0};
        sscanf(result, "%31s", client);

        char filename[64] = {0};
        snprintf(filename, sizeof(filename), "%s.txt", client);

        FILE *client_file = fopen(filename, "a");
        if (!client_file) 
        {
            perror("failed to open client file");
            continue;
        }

        fprintf(client_file, "%s\n", result);
        fflush(client_file);

        fclose(client_file);

        pthread_mutex_lock(&lock);
        available_workers[worker_id] = 1;
        pthread_mutex_unlock(&lock);
    }

    free(result);
    return NULL;
}

void worker_process(int rank) 
{
    char *command = (char*) malloc(BUFFER_SIZE);
    char *result = (char*) malloc(ANAGRAM_BUFFER_SIZE);
    if (!command || !result) 
    {
        perror("failed to allocate memory in worker");
        exit(EXIT_FAILURE);
    }

    while (1) 
    {
        MPI_Recv(command, BUFFER_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (strcmp(command, "STOP") == 0) 
        {
            snprintf(result, BUFFER_SIZE, "STOP");
            MPI_Send(result, BUFFER_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
            break;
        }

        char client[32] = {0}, task[32] = {0}, param[32] = {0};
        sscanf(command, "%31s %31s %31s", client, task, param);

        if (strcmp(task, "PRIMES") == 0) 
        {
            int n = atoi(param);
            int count = count_primes(n);
            snprintf(result, BUFFER_SIZE, "%s\nFound %d primes in the first %d numbers.", client, count, n);
        } 
        else if (strcmp(task, "PRIMEDIVISORS") == 0) 
        {
            int n = atoi(param);
            int count = count_prime_divisors(n);
            snprintf(result, BUFFER_SIZE, "%s\nFound %d prime divisors of %d.", client, count, n);
        } 
        else if (strcmp(task, "ANAGRAMS") == 0) 
        {
            char *anagrams = generate_anagrams_return(param);
            snprintf(result, ANAGRAM_BUFFER_SIZE, "%s\nAnagrams of %s are: %s\n", client, param, anagrams);
            free(anagrams);
        }
        else 
        {
            snprintf(result, BUFFER_SIZE, "Unknown task: %s", command);
        }

        MPI_Send(result, ANAGRAM_BUFFER_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    }

    free(command);
    free(result);
}

void *send_thread(void *arg) 
{
    FILE *input_file = fopen(INPUT_FILE, "r");
    if (!input_file)
    {
        perror("failed to open commands file parallel");
        exit(EXIT_FAILURE);
    }

    char *buffer = (char*) malloc(BUFFER_SIZE);
    if (!buffer) 
    {
        perror("failed to allocate memory for buffer");
        exit(EXIT_FAILURE);
    }

    while (fgets(buffer, BUFFER_SIZE, input_file)) 
    {
        if (strncmp(buffer, "WAIT", 4) == 0) 
        {
            continue;
        }

        time_t now = time(NULL);
        char *time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = '\0';
        fprintf(log_file, "[%s] Command received: %s\n", time_str, buffer);
        fflush(log_file);

        int worker_found = 0;
        while (!worker_found) 
        {
            pthread_mutex_lock(&lock);
            for (int i = 1; i <= num_workers; i++) 
            {
                if (available_workers[i]) 
                {
                    available_workers[i] = 0;
                    MPI_Send(buffer, BUFFER_SIZE, MPI_CHAR, i, 0, MPI_COMM_WORLD);
                    now = time(NULL);
                    time_str = ctime(&now);
                    time_str[strlen(time_str) - 1] = '\0';
                    fprintf(log_file, "[%s] Task dispatched to worker %d: %s\n", time_str, i, buffer);
                    fflush(log_file);
                    worker_found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&lock);

            if (!worker_found) 
            {
                usleep(10000); // wait 1 ms before retrying
            }
        }
    }

    fclose(input_file);

    int all_available = 0;
    while (!all_available) 
    {
        all_available = 1;
        pthread_mutex_lock(&lock);
        for (int i = 1; i <= num_workers; i++) 
        {
            if (!available_workers[i]) 
            {
                all_available = 0;
                break;
            }
        }
        pthread_mutex_unlock(&lock);

        if (!all_available)
        {
            usleep(10000);
        }
    }

    for (int i = 1; i <= num_workers; i++) 
    {
        MPI_Send("STOP", BUFFER_SIZE, MPI_CHAR, i, 0, MPI_COMM_WORLD);
    }

    free(buffer);
    return NULL;
}


void main_server_process(int num_workers)
 {
    pthread_t send_tid, recv_tid;

    initialize_workers();

    log_file = fopen(LOG_FILE_P, "w");
    if (!log_file) 
    {
        perror("faield to open log file parallel");
        exit(EXIT_FAILURE);
    }

    pthread_create(&send_tid, NULL, send_thread, NULL);
    pthread_create(&recv_tid, NULL, receive_thread, NULL);

    pthread_join(send_tid, NULL);
    pthread_join(recv_tid, NULL);

    fclose(log_file);
}



int main(int argc, char *argv[]) 
{
    struct timespec start, finish;

    clock_gettime(CLOCK_MONOTONIC, &start);
    compute_serial();
    clock_gettime(CLOCK_MONOTONIC, &finish);
    double time_taken_serial = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1e9;

    clock_gettime(CLOCK_MONOTONIC, &start);
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size >= MAX_WORKERS) 
    {
        if (rank == 0) 
        {
            printf("\nWall-clock time SERIAL = %lf seconds\n", time_taken_serial);
            printf("Error: too many processes; MAX = %d\n", MAX_WORKERS);
        }
        exit(EXIT_FAILURE);
    }

    num_workers = size - 1;

    if (rank == 0) 
    {
        main_server_process(num_workers);
    } 
    else 
    {
        worker_process(rank);
    }
    MPI_Finalize();
    clock_gettime(CLOCK_MONOTONIC, &finish);
    double time_taken_parallel = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1e9;

    if (rank == 0) 
    {
        printf("\nWall-clock time SERIAL = %lf seconds\n", time_taken_serial);
        fflush(stdout);
        printf("Wall-clock time PARALLEL = %lf seconds\n", time_taken_parallel);
        fflush(stdout);
        printf("Speedup = %lf\n", time_taken_serial / time_taken_parallel);
        fflush(stdout);
    }

    return 0;
}