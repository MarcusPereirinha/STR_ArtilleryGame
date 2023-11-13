/* Standard includes. */
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <locale.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"


/* Priorities at which the tasks are created. */
#define	mainMAP_TASK_PRIORITY			( tskIDLE_PRIORITY + 1 )
#define	mainATTACK_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
#define mainDEFENSE_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
/*-----------------------------------------------------------*/

/* Simulation parameters. */ 
#define simuMAP_WORLD_SIZE 20
#define simuMAP_CITY_SIZE (simuMAP_WORLD_SIZE / 2)
#define simuMAX_MISSILES_PER_ROUND 3
#define simuMAX_HABITABLE_ZONES 80
#define simuATTACK_MISSILE_SPEED  180
#define simuDEFENSE_MISSILE_SPEED 80
#define simuMAX_ATTACK_DELAY 5
/*-----------------------------------------------------------*/

/* Private functions prototypes. */
static void prvAttackTask(void* pvParameters);
static void prvDefenseTask(void* pvParameters);
static void prvMapTask(void* pvParameters);
//static void prvValidator(void* pvParameters);
/*-----------------------------------------------------------*/

/* A software timer that is started from the tick hook. */
static TimerHandle_t xTimer = NULL;

/* Structs */
typedef struct {
	int x;
	int y;
} Point;

typedef struct {
	Point start;
	Point target;
	TickType_t sent_time;
} MissileInfo;

/* Variables */
static int numMissiles;
static MissileInfo missiles[simuMAX_MISSILES_PER_ROUND];
boolean hasMissileMsg;
char map[simuMAP_WORLD_SIZE][simuMAP_WORLD_SIZE];
Point habitableZones[simuMAX_HABITABLE_ZONES];


int metric_attack_missiles_sent;
int metric_sent_missiles_habitable;
int metric_defense_missiles_sent;
int metric_intercepted_missiles;
int metric_not_intercepted_missiles;
int metric_missiles_desert;
float metric_defense_success_rate;
float metric_attack_success_rate;

/*-----------------------------------------------------------*/

void main_artillery_game(void){

	/* Start tasks . */
	xTaskCreate(prvMapTask,     "Map",     configMINIMAL_STACK_SIZE, NULL, mainMAP_TASK_PRIORITY,     NULL);
	xTaskCreate(prvAttackTask,  "Attack",  configMINIMAL_STACK_SIZE, NULL, mainATTACK_TASK_PRIORITY,  NULL);
	xTaskCreate(prvDefenseTask, "Defense", configMINIMAL_STACK_SIZE, NULL, mainDEFENSE_TASK_PRIORITY, NULL);

	/* Create the software timer, but don't start it yet. */
	xTimer = xTimerCreate("Timer",   /* The text name assigned to the software timer - for debug only as it is not used by the kernel. */
		200000,				 /* The period of the software timer in ticks. */
		pdFALSE,					 /* xAutoReload is set to pdFALSE, so this is a one shot timer. */
		NULL,						 /* The timer's ID is not used. */
		NULL);  /* The function executed when the timer expires. */

	/* Start the tasks. */
	vTaskStartScheduler();
	
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	for (;;);
}
/*-----------------------------------------------------------*/




static void prvMapTask(void* pvParameters)
{
	const unsigned long ulMSToSleep = 10000;

	/* Just to remove compiler warnings. */
	(void)pvParameters;

	for (int i = 0; i < simuMAP_WORLD_SIZE; i++) {
		for (int j = 0; j < simuMAP_WORLD_SIZE; j++) {
			map[i][j] = '.';
		}
	}

	for (int i = 0; i < simuMAX_HABITABLE_ZONES; i++) {
		habitableZones[i].x = (simuMAP_WORLD_SIZE / 4) + (rand() % simuMAP_CITY_SIZE);
		habitableZones[i].y = (simuMAP_WORLD_SIZE / 4) + (rand() % simuMAP_CITY_SIZE);

		map[habitableZones[i].x][habitableZones[i].y] = 'H';
	}

	metric_attack_missiles_sent = 0;
	metric_sent_missiles_habitable = 0;
	metric_defense_missiles_sent = 0;
	metric_intercepted_missiles = 0;
	metric_not_intercepted_missiles = 0;
	metric_missiles_desert = 0;
	metric_defense_success_rate = 0.0;
	metric_attack_success_rate = 0.0;

	/* This task is just used to test the eTaskStateGet() function.  It
	does not have anything to do. */
	for (;; )
	{
		printf("\n[prvMapTask] *************************************\r\n");


		for (int i = 0; i < simuMAP_WORLD_SIZE; i++) {
			printf("\t");
			for (int j = 0; j < simuMAP_WORLD_SIZE; j++) {
				printf("%c ", map[i][j]);
			}
			printf("\n");
		}

		if (metric_sent_missiles_habitable > 0) {
			metric_defense_success_rate = (float) metric_intercepted_missiles / metric_sent_missiles_habitable;
			metric_attack_success_rate = (float) metric_not_intercepted_missiles / metric_sent_missiles_habitable;
		}


		printf("\n\tMETRICS\r\n");

		printf("\n\t\tmetric_attack_missiles_sent = %d", metric_attack_missiles_sent);
		printf("\n\t\tmetric_sent_missiles_habitable = %d", metric_sent_missiles_habitable);
		printf("\n\t\tmetric_defense_missiles_sent = %d", metric_defense_missiles_sent);
		printf("\n\t\tmetric_intercepted_missiles = %d", metric_intercepted_missiles);
		printf("\n\t\tmetric_not_intercepted_missiles = %d", metric_not_intercepted_missiles);
		printf("\n\t\tmetric_missiles_desert = %d", metric_missiles_desert);
		printf("\n\t\tmetric_defense_success_rate = %2f", metric_defense_success_rate);
		printf("\n\t\tmetric_attack_success_rate = %2f", metric_attack_success_rate);

		printf("\r\n");


		/* Sleep to reduce CPU load, but don't sleep indefinitely in case there are
		tasks waiting to be terminated by the idle task. */
		Sleep(ulMSToSleep);
	}

	/* Tasks must not attempt to return from their implementing
		function or otherwise exit.  In newer FreeRTOS port
		attempting to do so will result in an configASSERT() being
		called if it is defined.  If it is necessary for a task to
		exit then have the task call vTaskDelete( NULL ) to ensure
		its exit is clean. */
	vTaskDelete(NULL);
}



static void prvAttackSendMissiles(void)
{
	double attack_distance;
	double attack_time_to_target;

	for (int i = 0; i < numMissiles; i++)
	{
		for (int j = 0; j < simuMAX_HABITABLE_ZONES; j++)
		{
			if ((missiles[i].target.x == habitableZones[j].x) && (missiles[i].target.y == habitableZones[j].y)) {

				//printf("\t\tmissiles[%d] - target(%2d, %2d)\n", i, missiles[i].target.x, missiles[i].target.y);
				
				missiles[i].sent_time = xTimerGetPeriod(xTimer);

				break;
			}
		}

	}

	metric_attack_missiles_sent++;	

	printf("\tMissiles sent!\r\n");
	hasMissileMsg = TRUE;
}

static void prvAttackTask(void* pvParameters)
{
	unsigned long ulMSToSleep;

	/* Just to remove compiler warnings. */
	(void)pvParameters;

	/* This task is just used to test the eTaskStateGet() function.  It
	does not have anything to do. */
	for (;; )
	{
		printf("\n[prvAttackTask] *************************************\r\n");		

		//int numMissiles;
		numMissiles = rand() % (simuMAX_MISSILES_PER_ROUND + 1);  /* Random number of missiles */ 
		printf("\tnumMissiles = %d\n", numMissiles);

		//MissileInfo missiles[simuMAX_MISSILES_PER_ROUND];

		for (int i = 0; i < numMissiles; i++) {
			/* Sets random Start Point outside of MapCity and inside of Map World */ 
			if (rand() % 2)	{
				missiles[i].start.x = rand() % (simuMAP_WORLD_SIZE/4);
			}
			else {
				missiles[i].start.x = (rand() % (simuMAP_WORLD_SIZE/4)) + simuMAP_WORLD_SIZE*3/4;
			}

			if (rand() % 2){
				missiles[i].start.y = rand() % (simuMAP_WORLD_SIZE / 4);
			}
			else {
				missiles[i].start.y = (rand() % (simuMAP_WORLD_SIZE / 4)) + simuMAP_WORLD_SIZE * 3 / 4;
			}

			/* Sets random Target within Map City */ 
			missiles[i].target.x = (simuMAP_WORLD_SIZE / 4) + (rand() % simuMAP_CITY_SIZE);
			missiles[i].target.y = (simuMAP_WORLD_SIZE / 4) + (rand() % simuMAP_CITY_SIZE);

			printf("\t\tmissiles[%d] - start(%2d, %2d) - target(%2d, %2d)\n", i, missiles[i].start.x, missiles[i].start.y, missiles[i].target.x, missiles[i].target.y);
		}

		prvAttackSendMissiles();

		ulMSToSleep = 2000 + 2000 * (rand() % (simuMAX_ATTACK_DELAY + 1));  /* Random number of missiles */
		printf("\tNext Random Attack Milliseconds Delay = %d\r\n", ulMSToSleep);

		/* Sleep to reduce CPU load, but don't sleep indefinitely in case there are
		tasks waiting to be terminated by the idle task. */
		Sleep(ulMSToSleep);
	}
	
	/* Tasks must not attempt to return from their implementing
		function or otherwise exit.  In newer FreeRTOS port
		attempting to do so will result in an configASSERT() being
		called if it is defined.  If it is necessary for a task to
		exit then have the task call vTaskDelete( NULL ) to ensure
		its exit is clean. */
	vTaskDelete(NULL);
}

static void prvDefenseRecvMissiles(void)
{
	printf("\tMissiles received!");
	hasMissileMsg = FALSE;
}

static void prvDefenseTask(void* pvParameters)
{
	const unsigned long ulMSToSleep = 1000;
	/* Just to remove compiler warnings. */
	(void)pvParameters;

	Point defenseCenter = { simuMAP_CITY_SIZE / 2, simuMAP_CITY_SIZE / 2 };
	double attack_distance;
	double attack_time_to_target;

	double defense_distance;
	double defense_time_to_target;

	TickType_t intercept_time;



	/* This task is just used to test the eTaskStateGet() function.  It
	does not have anything to do. */
	for (;; )
	{
		printf("\n[prvDefenseTask] *************************************\r\n");

		if (hasMissileMsg) {
			prvDefenseRecvMissiles();

			for (int i = 0; i < numMissiles; i++)
			{
				for (int j = 0; j < simuMAX_HABITABLE_ZONES; j++)
				{
					if ((missiles[i].target.x == habitableZones[j].x) && (missiles[i].target.y == habitableZones[j].y)) {

						metric_sent_missiles_habitable++;
						metric_defense_missiles_sent++;
						
						printf("\n\t\tmissiles[%d] - target(%2d, %2d)", i, missiles[i].target.x, missiles[i].target.y);
						
						attack_distance = sqrt(pow(missiles[i].target.x - missiles[i].start.x, 2) + pow(missiles[i].target.y - missiles[i].start.y, 2));
						attack_time_to_target = attack_distance / simuATTACK_MISSILE_SPEED;

						defense_distance = sqrt(pow(missiles[i].target.x - defenseCenter.x, 2) + pow(missiles[i].target.y - defenseCenter.y, 2));
						defense_time_to_target = defense_distance / simuDEFENSE_MISSILE_SPEED;

						printf("\n\t\tTrajectories calculated!");

						intercept_time = xTimerGetPeriod(xTimer);

						if ((intercept_time + defense_time_to_target) < (missiles[i].sent_time + attack_time_to_target))
						{
							printf("\n\t\tMissile intercepted!!\n");							
							metric_intercepted_missiles++;							
						}
						else 
						{
							printf("\n\t\tMissile not intercepted!!\n");

							map[missiles[i].target.x][missiles[i].target.y] = 'X';
							metric_not_intercepted_missiles++;
						}							

						break;
					}
					else
					{
						metric_missiles_desert++;						
					}

				}		
				
			}
		}
		else
		{
			printf("\tNo missiles received! \r\n");
		}

		/* Sleep to reduce CPU load, but don't sleep indefinitely in case there are
		tasks waiting to be terminated by the idle task. */
		Sleep(ulMSToSleep);		
	}

	/* Tasks must not attempt to return from their implementing
		function or otherwise exit.  In newer FreeRTOS port
		attempting to do so will result in an configASSERT() being
		called if it is defined.  If it is necessary for a task to
		exit then have the task call vTaskDelete( NULL ) to ensure
		its exit is clean. */
	vTaskDelete(NULL);
}