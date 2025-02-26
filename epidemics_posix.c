
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

long N = 0;
int MAX_X_COORD = 0;
int MAX_Y_COORD = 0;
int TOTAL_SIMULATION_TIME = 0;
int ThreadNumber = 0;
char *InputFileName;
int debugMode = 0;

#define INFECTED_DURATION 5
#define IMMUNE_DURATION 3
#define NORTH 0
#define SOUTH 1
#define EAST 2
#define WEST 3
#define INFECTED 0
#define SUSCEPTIBLE 1
#define IMMUNE 2

typedef struct Person
{
    int personId;
    int x;
    int y;
    int currentStatus;
    int futureStatus;
    int movementPatternDirection;
    int movementPatternAmplitude;
    int infectionCounter;
    int sicknessDuration;
    int immunityDuration;

} Person;

Person *people;
int **infectedGrid;
pthread_barrier_t barrier;

int checkCoordinates(Person *a, Person *b)
{
    return (a->x == b->x) && (a->y == b->y);
}

void readDataFromInputFile(char *fileName)
{
    FILE *file = fopen(fileName, "r");
    if (file == NULL)
    {
        perror("Error reading from input file\n");
        exit(-1);
    }

    fscanf(file, "%d %d", &MAX_X_COORD, &MAX_Y_COORD);
    fscanf(file, "%ld", &N);

    people = (Person *)calloc(N, sizeof(Person));
    if (people == NULL)
    {
        perror("error allocating memory for people array\n");
        exit(-1);
    }

    for (int i = 0; i < N; i++)
    {
        fscanf(file, "%d %d %d %d %d %d", &people[i].personId, &people[i].x, &people[i].y,
               &people[i].currentStatus, &people[i].movementPatternDirection, &people[i].movementPatternAmplitude);

        people[i].immunityDuration = 0;
        people[i].infectionCounter = 0;
        people[i].sicknessDuration = 0;

        if (people[i].currentStatus == INFECTED)
        {
            people[i].sicknessDuration = INFECTED_DURATION;
            people[i].infectionCounter = 1;
        }

        people[i].futureStatus = people[i].currentStatus;
    }

    fclose(file);
}

const char *getDirectionName(int direction)
{
    switch (direction)
    {
        case NORTH:
            return "NORTH";
        case SOUTH:
            return "SOUTH";
        case EAST:
            return "EAST";
        case WEST:
            return "WEST";
        default:
            return "UNKNOWN";
    }
}

void displayPeople()
{
    for (int i = 0; i < N; i++)
    {
        printf("Person %d -> Position: (%d, %d), Status: %d, Infections: %d, Direction: %s, Steps: %d, Imunity: %d, Sickness: %d\n",
               people[i].personId, people[i].x, people[i].y, people[i].currentStatus,
               people[i].infectionCounter, getDirectionName(people[i].movementPatternDirection),
               people[i].movementPatternAmplitude, people[i].immunityDuration, people[i].sicknessDuration);
    }
    printf("\n");
}

void saveResultsToFile(char *filename)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        perror("error opening file for writing output\n");
        return;
    }

    for (int i = 0; i < N; i++)
    {
        fprintf(file, "Person %d: (%d, %d), Status: %d, Infections: %d\n",
                people[i].personId, people[i].x, people[i].y, people[i].currentStatus, people[i].infectionCounter);
    }

    fclose(file);
}

void move(Person *p)
{
    switch (p->movementPatternDirection)
    {
    case NORTH:
        if (p->y + p->movementPatternAmplitude > MAX_Y_COORD)
        {
            p->movementPatternDirection = SOUTH;
            p->y = MAX_Y_COORD - (p->y + p->movementPatternAmplitude - MAX_Y_COORD);
        }
        else
        {
            p->y += p->movementPatternAmplitude;
        }
        break;
    case SOUTH:
        if (p->y - p->movementPatternAmplitude < 0)
        {
            p->movementPatternDirection = NORTH;
            p->y = -(p->y - p->movementPatternAmplitude);
        }
        else
        {
            p->y -= p->movementPatternAmplitude;
        }
        break;
    case EAST:
        if (p->x + p->movementPatternAmplitude > MAX_X_COORD)
        {
            p->movementPatternDirection = WEST;
            p->x = MAX_X_COORD - (p->x + p->movementPatternAmplitude - MAX_X_COORD);
        }
        else
        {
            p->x += p->movementPatternAmplitude;
        }
        break;
    case WEST:
        if (p->x - p->movementPatternAmplitude < 0)
        {
            p->movementPatternDirection = EAST;
            p->x = -(p->x - p->movementPatternAmplitude);
        }
        else
        {
            p->x -= p->movementPatternAmplitude;
        }
        break;
    default:
        break;
    }
}

void updateStatusOnePerson(Person *p)
{
    if (p->currentStatus == INFECTED)
    {
        p->sicknessDuration--;
        if (p->sicknessDuration <= 0)
        {
            p->futureStatus = IMMUNE;
            p->immunityDuration = IMMUNE_DURATION;
        }
        else
        {
            p->futureStatus = INFECTED;
        }
    }
    else if (p->currentStatus == IMMUNE)
    {
        p->immunityDuration--;
        if (p->immunityDuration <= 0)
        {
            p->futureStatus = SUSCEPTIBLE;
        }
        else
        {
            p->futureStatus = IMMUNE;
        }
    }

    if (p->currentStatus != INFECTED && p->futureStatus == INFECTED)
    {
        p->infectionCounter++;
        p->sicknessDuration = INFECTED_DURATION;
    }
}

void updateGrid()
{
    for (int i = 0; i <= MAX_X_COORD; i++)
    {
        for (int j = 0; j <= MAX_Y_COORD; j++)
        {
            infectedGrid[i][j] = 0;
        }
    }

    for (int i = 0; i < N; i++)
    {
        if (people[i].currentStatus == INFECTED)
        {
            infectedGrid[people[i].x][people[i].y] = 1;
        }
    }
}

void setFutureStatus(int start, int end)
{
    for (int i = start; i < end; i++)
    {
        if (people[i].currentStatus == SUSCEPTIBLE && infectedGrid[people[i].x][people[i].y])
        {
            people[i].futureStatus = INFECTED;
        }
    }
}

void computeSerial()
{
    for (int time = 1; time <= TOTAL_SIMULATION_TIME; time++)
    {
        for (int i = 0; i < N; i++)
        {
            if(people[i].currentStatus == INFECTED)
            {
                infectedGrid[people[i].x][people[i].y] = 0;
            }
            move(&people[i]);
            updateStatusOnePerson(&people[i]);
            people[i].currentStatus = people[i].futureStatus;
        }

        for(int i = 0; i < N; i++)
        {
            if(people[i].currentStatus == INFECTED)
            {
                infectedGrid[people[i].x][people[i].y] = 1;
            }
        }

        setFutureStatus(0, N);

        if (debugMode)
        {
            printf("Serial Iteration: %d\n", time);
            displayPeople(people, N);
        }
    }
}

void *compute_parallel(void *arg)
{
    int thread_id = *(int *)arg;
    int start = (thread_id * N) / ThreadNumber;
    int end = (thread_id == ThreadNumber - 1) ? N : ((thread_id + 1) * N) / ThreadNumber;

    printf("thread id: %d; start: %d, end: %d\n", thread_id, start, end);

    for (int t = 1; t <= TOTAL_SIMULATION_TIME; t++)
    {
        for (int i = start; i < end; i++)
        {
            if (people[i].currentStatus == INFECTED)
            {
                infectedGrid[people[i].x][people[i].y] = 0;
            }
            move(&people[i]);
            updateStatusOnePerson(&people[i]);
            people[i].currentStatus = people[i].futureStatus;
        }
        pthread_barrier_wait(&barrier);

        for(int i = start; i < end; i++)
        {
            if(people[i].currentStatus == INFECTED)
            {
                infectedGrid[people[i].x][people[i].y] = 1;
            }
        }
        pthread_barrier_wait(&barrier);

        setFutureStatus(start, end);
        pthread_barrier_wait(&barrier);

        if (debugMode)
        {
            if (thread_id == 0)
            {
                printf("Parallel Iteration: %d\n", t);
                displayPeople();
            }
            pthread_barrier_wait(&barrier);
        }
    }
    pthread_exit(NULL);
}

int compareFiles(char *file1, char *file2)
{
    FILE *f1 = fopen(file1, "r");
    if (f1 == NULL)
    {
        perror("error opening file 1(serial) out\n");
        exit(-1);
    }
    FILE *f2 = fopen(file2, "r");
    if (f2 == NULL)
    {
        perror("error opening file 2(parallel) out\n");
        exit(-1);
    }

    int s, p;

    while ((s = fgetc(f1)) != EOF && (p = fgetc(f2)) != EOF)
    {
        if (s != p)
        {
            fclose(f1);
            fclose(f2);

            return 0;
        }
    }

    if (fgetc(f1) == EOF && fgetc(f2) == EOF)
    {
        fclose(f1);
        fclose(f2);

        return 1;
    }

    fclose(f1);
    fclose(f2);

    return 0;
}


int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Usage: %s TOTAL_SIMULATION_TIME InputFileName ThreadNumber MODE(debug-1 / normal-0)\n", argv[0]);
        exit(-1);
    }

    TOTAL_SIMULATION_TIME = atoi(argv[1]);
    InputFileName = argv[2];
    ThreadNumber = atoi(argv[3]);
    debugMode = atoi(argv[4]);

    struct timespec start, finish;

    char *serialOut = malloc(100 * sizeof(char));
    char *parallelOut = malloc(100 * sizeof(char));
    if (serialOut == NULL || parallelOut == NULL)
    {
        perror("error allocating memory serial/parallel output file");
        exit(-1);
    }
    int lengthWithoutExtension = strlen(InputFileName) - 4;
    char nameOutWithoutExtension[50];
    strncpy(nameOutWithoutExtension, InputFileName, lengthWithoutExtension);
    nameOutWithoutExtension[lengthWithoutExtension] = '\0';
    snprintf(serialOut, 80, "%s_serial_out.txt", nameOutWithoutExtension);
    snprintf(parallelOut, 80, "%s_parallel_out.txt", nameOutWithoutExtension);

    // SERIAL

    readDataFromInputFile(InputFileName);
    if (debugMode)
    {
        printf("\nserial read: \n");
        displayPeople();
    }

    infectedGrid = malloc((MAX_X_COORD + 1) * sizeof(int *));
    if (infectedGrid == NULL)
    {
        perror("error allocating memory for grid\n");
        exit(-1);
    }
    for (int i = 0; i <= MAX_X_COORD; i++)
    {
        infectedGrid[i] = calloc(MAX_Y_COORD + 1, sizeof(int));
        if (infectedGrid[i] == NULL)
        {
            perror("error allocating memory for grid row\n");
            exit(-1);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    updateGrid();
    setFutureStatus(0, N);
    computeSerial();
    clock_gettime(CLOCK_MONOTONIC, &finish);
    double time_taken_serial = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1e9;

    saveResultsToFile(serialOut);

    free(people);


    // PARALLEL

    readDataFromInputFile(InputFileName);
    if (debugMode)
    {
        printf("\nparallel read: \n");
        displayPeople();
    }

    pthread_barrier_init(&barrier, NULL, ThreadNumber);
    pthread_t threads[ThreadNumber];
    int thread_ids[ThreadNumber];

    clock_gettime(CLOCK_MONOTONIC, &start);
    updateGrid();
    setFutureStatus(0, N);
    for (int i = 0; i < ThreadNumber; i++)
    {
        thread_ids[i] = i;
        if(pthread_create(&threads[i], NULL, compute_parallel, (void *)&thread_ids[i]) != 0)
        {
            perror("error creating thread\n");
            exit(-1);
        }
    }
    for (int i = 0; i < ThreadNumber; i++)
    {
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &finish);
    double time_taken_parallel = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1e9;

    pthread_barrier_destroy(&barrier);

    saveResultsToFile(parallelOut);
    

    printf("\nWall-clock time SERIAL = %lf seconds\n", time_taken_serial);
    printf("Wall-clock time PARALLEL = %lf seconds\n", time_taken_parallel);
    double speedup = time_taken_serial / time_taken_parallel;
    printf("input: %s, iterations: %d, threads: %d\nSPEEDUP: %f\n", InputFileName, TOTAL_SIMULATION_TIME, ThreadNumber, speedup);

    int x = compareFiles(serialOut, parallelOut);
    if (x == 1)
    {
        printf("\nserial output EQUALS parallel output\n\n");
    }
    else
    {
        printf("\nserial output DIFFERENT from parallel output\n\n");
    }

    for (int i = 0; i <= MAX_X_COORD; i++)
    {
        free(infectedGrid[i]);
    }
    free(infectedGrid);

    free(people);
    free(serialOut);
    free(parallelOut);

    return 0;
}
