#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <syscall.h>

#define NUM_PEOPLE 99 // multiple of 3
#define UNIT_COUNT 8
#define UNIT_CAPACITY 3

// FUNCTION DECLARATION
void *healthcareStaffMember(void *number);
void *person(void *number);
void ventilate(int num);
void announce(int num);
int fillingForm();
int generatePerson();
void resetUnitState();

// Semaphores
sem_t *unitCap;        //There are 8 units each containing up to 3 people at most.
sem_t unitMutex;       //Initially 0. People will enter units mutually exclusively.
sem_t *staffVentilate; // Initially all 0. There are 8 healthcare staff and they ventilate the units when the testing process is done.
sem_t *awaitSem;       // Initially all 0. Waiting semaphore to test 3 people at the same time and empty the unit at the same time.
sem_t waitAnnounce;
sem_t waitPeopleLeave;
int unitStates[UNIT_COUNT]; /* If the state is 0 the unit is not used. If the state is
1 then it means the unit is used. */
int waitingPeople;          // People waiting for the units to be emptied.
int testingPeople;          // People waiting for the third person while filling forms.

int main()
{
    // threads for the people and the staff
    pthread_t people[NUM_PEOPLE];
    pthread_t healthcareStaff[UNIT_COUNT];

    // initialization
    waitingPeople = NUM_PEOPLE;
    testingPeople = 0;
    // set unit states to 0 which is initially as not used
    for (int i = 0; i < UNIT_COUNT; i++)
    {
        unitStates[i] = 0;
    }

    // semaphore initialization
    sem_init(&unitMutex, 0, 1);
    sem_init(&waitAnnounce, 0, 0);
    sem_init(&waitPeopleLeave, 0, 0);
    unitCap = malloc(UNIT_COUNT * sizeof(sem_t));
    staffVentilate = malloc(UNIT_COUNT * sizeof(sem_t));
    awaitSem = malloc(UNIT_COUNT * sizeof(sem_t));

    for (int i = 0; i < UNIT_COUNT; i++)
    {
        sem_init(unitCap + i, 0, UNIT_CAPACITY);
        sem_init(staffVentilate + i, 0, 0);
        sem_init(awaitSem + i, 0, 0);
    }

    for (int i = 0; i < UNIT_COUNT; i++)
    {
        pthread_create(&healthcareStaff[i], NULL, healthcareStaffMember, (void *)i);
    }
    usleep(300000); // sleep for 0.3 seconds

    // Randomly choose incoming people
    int array[NUM_PEOPLE];
    for (int i = 0; i < NUM_PEOPLE; i++){
        array[i] = i; 
    }
    srand(time(0));
    for (int i = 0; i < NUM_PEOPLE; i++){
        int temp = array[i];
        int randomIndex = rand() % NUM_PEOPLE;

        array[i] = array[randomIndex];
        array[randomIndex] = temp;
    }


    for (int i = 0; i < NUM_PEOPLE; i++)
    {
        pthread_create(&people[i], NULL, person, (void *)array[i]);
        usleep(generatePerson()); // Generate a person between periods of seconds.
    }

    
    for (int i = 0; i < NUM_PEOPLE; i++)
    {
        pthread_join(&people[i], NULL);
    }
    for (int i = 0; i < NUM_PEOPLE; i++)
    {
        pthread_join(&staffVentilate[i], NULL);
    }
    return 0;
}
void resetUnitState()
{
    int state = 0;
    for (int i = 0; i < UNIT_COUNT; i++)
    {
        if (unitStates[i] == 0)
        {
            state = 1;
            break;
        }
    }

    if (state == 0)
    {
        for (int i = 0; i < UNIT_COUNT; i++)
        {
            unitStates[i] = 0;
        }
    }
}
void *person(void *number)
{ // thread for people to be tested
    int num = (int)number;
    printf("\nPerson with the id-%d has arrived for testing.\n", num);

    sem_wait(&unitMutex);
    resetUnitState();
    // find the nearly full unit
    int min = 10000;
    int index = 0;

    for (int i = 0; i < UNIT_COUNT; i++)
    {
        int capacity;
        sem_getvalue(&unitCap[i], &capacity);

        if (unitStates[i] == 0 && capacity > 0 && capacity < min)
        {
            min = capacity;
            index = i;
        }
    }
    if (min == 10000)
    {
        printf("All units are occupied.\nPerson with the id-%d is waiting at the hospital.\n", num);
    }
    sem_wait(&unitCap[index]); // Person enters the unit if there is space for them. Otherwise wait
    waitingPeople--;
    testingPeople++;
    printf("\nPerson with the id-%d has entered the unit-%d\n", num, index);

    sem_post(&staffVentilate[index]); // Staff member ventilates the room
    sem_wait(&waitAnnounce);          // People wait for the announce to be made
    sem_post(&unitMutex);             // Release the mutex for the unit

    sem_wait(&awaitSem[index]); // Wait until all people leave the unit at the same time
    printf("\n---------------------------------------------------------");
    printf("\nPerson with the id-%d is tested and left the unit.-%d.\n", num, index);
    printf("---------------------------------------------------------");
    testingPeople--;
    sem_post(&waitPeopleLeave);
}

void *healthcareStaffMember(void *number)
{ // thread for staff members
    int num = (int)number;
    ventilate(num); // All the units are ventilated before testing begins
    while (1)
    {
        sem_wait(&staffVentilate[num]);
        int capacity;
        sem_getvalue(&unitCap[num], &capacity);
        if (capacity > 0) // If the capacity is higher than 0 it means that there exists space in the unit
        {
            printf("\nThe last %d people,let'start! Please, pay attention to your social distance and hygiene; use a mask.\n", capacity, num);
            usleep(20);
            sem_post(&waitAnnounce); // Announcement is made
        }
        else
        {
            unitStates[num] = 1; // The state is set to 1 which means the unit is full
            printf("\n^^^^^^^^^^^^^^^^UNIT-%d IS FULL^^^^^^^^^^^^^^^^\n", num);
            sem_post(&waitAnnounce);
            sleep(fillingForm()); // People are filling the forms and getting ready to be tested

            for (int i = 0; i < UNIT_CAPACITY; i++)
            {
                sem_post(&awaitSem[num]); // Discharge people
            }
            for (int i = 0; i < UNIT_CAPACITY; i++)
            {
                sem_wait(&waitPeopleLeave); // Wait for people to leave the unit
            }
            printf("\n\n^^^^^^^^^^^^^^^^UNIT-%d IS EMPTY^^^^^^^^^^^^^^^^\n", num);
            ventilate(num); // All units are ventilated again after testing finishes

            // END CONDITION
            if (waitingPeople == 0 && testingPeople == 0)
            {
                printf("\nAll candidates are tested.\n");
                exit(0);
            }

            for (int i = 0; i < UNIT_CAPACITY; i++)
            {
                sem_post(&unitCap[num]); // Call for other people waiting at the hospital since this room is full
            }
        }
    }
}
void ventilate(int num)
{
    printf("*****Healthcare staff member -%d is ventilating the unit-%d*****\n", num, num);
    usleep(10);
}
int fillingForm()
{ // The 3 people who got into the same unit fill the forms and get tested between 3 to 5 seconds period
    int randomTime = (rand() % 3) + 3;
    return randomTime;
}

int generatePerson()
{   // Generate people every 0 to 1.5 seconds randomly              
    int upper = 1500; 
    int lower = 0;   
    int randomTime = (rand() %
                      (upper - lower + 1)) +
                     lower;
    return randomTime * 1000;
}
