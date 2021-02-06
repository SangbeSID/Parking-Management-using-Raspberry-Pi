#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <assert.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include "grovepi.h"
#include "grove_rgb_lcd.h"

using namespace GrovePi;

// --------------------------------------
// GLOBAL VARIABLES
// --------------------------------------

#define NMAX 100
volatile char order;   // Order to execute
volatile int N;        // Number of available places
volatile int end = 0;  // Indicator for the end of the progam
volatile int my_time;  // The clock
volatile int wait = 0; // Use to wait for an available place
char msg[50]; // Use to display the number of place on the LCD

char address_client[15]; // client's address
char answer_for_client[1];
char help_msg[1];		// Message from the client
volatile int mysocket;  // Use for the socket
volatile int client_socket;  // Use for the cleint's socket


sem_t sem_enter;  // Semaphore for entry_task
sem_t sem_exit;   // Semaphore for exit_task
sem_t sem_disp;   // Semaphore for display_task
pthread_mutex_t mutex_N = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_disp = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_mutex_N;


LCD lcd;
int pin_btn_enter = 2;
int pin_btn_exit = 8;
int pin_LED_exit = 7;
int pin_LED_enter = 3;
int state_btn_enter;
int state_btn_exit;

/* ==================================================================
 * FUNCTION ENTER_TASK
 * We wait for a token, if there is a token and the program is still
 * running and the button enter is pressed.
 * Then, if the parking is open, we lock the number of available places N.
 * If there is a place, we decremente N, we post a token to the display task.
 * And, we on the LED.
 * If there is no place, we wait for a place, and if it is OK, we reduce N.
 * ==================================================================*/
void *task_enter(void *args)
{
	while(!end){
		sem_wait(&sem_enter); // Wait for the order
		if(!end && state_btn_enter){
			if(my_time < 1 || my_time >5){
				pthread_mutex_lock(&mutex_N); // Lock N
				
				// There is a place we can enter
				if(N > 0){
					N--;									
					sem_post(&sem_disp);
					digitalWrite(pin_LED_enter, HIGH);
					delay(3000);
					digitalWrite(pin_LED_enter, LOW);
				}
				
				// There is no place, we wait for a place
				if(N <= 0){
					wait = 1;  // Indicator for a waiting car		
					sem_post(&sem_disp);					
					pthread_cond_wait(&cond_mutex_N, &mutex_N);
					wait = 0; // The waiting car can get in
					N--;	  // We reduce the number of available place				
					sem_post(&sem_disp);
					digitalWrite(pin_LED_enter, HIGH);
					delay(3000);
					digitalWrite(pin_LED_enter, LOW);
				}				
				pthread_mutex_unlock(&mutex_N);
			}
			sem_post(&sem_disp);
		}
	}
	pthread_exit(0);
}


/* ==================================================================
 * FUNCTION EXIT_TASK
 * We wait for a token, if there is a token and the program is still
 * running and the button exit is pushed.
 * Then, we lock the number of available places N.
 * If N is under the maximum available places, we incremente N, 
 * we post a token to the display task. And, we on the LED.
 * Finally, we send a signal to indicate an available place.
 * ==================================================================*/
void *task_exit(void *args)
{
	while(!end){
		sem_wait(&sem_exit);
		if(!end && state_btn_exit){			
			pthread_mutex_lock(&mutex_N); // Lock N			
			// We are in the parking and we want to get out
			if(N < NMAX){
				N++; 
				pthread_mutex_lock(&mutex_disp); // Lock the display
				sem_post(&sem_disp);
				digitalWrite(pin_LED_exit, HIGH);
				delay(3000);
				digitalWrite(pin_LED_exit, LOW);
				pthread_mutex_unlock(&mutex_disp);
				// if a car is waiting we send a signal for 
				// an available place
				pthread_cond_signal(&cond_mutex_N);
			}
			pthread_mutex_unlock(&mutex_N); // Unlock N
		}
	}
	pthread_exit(0);
}

/* ==================================================================
 * FUNCTION DISPLAY_TASK
 * ==================================================================*/
void *task_display(void *args)
 {
	 while(!end){ 
		 if(!end){
			 sem_wait(&sem_disp); // Wait for the token to display N
			 // Display the time
			 printf("\nIt's %d o'clock!\n", my_time);
			 if(my_time >= 1 && my_time <=5){		
				 printf("CLOSED\n");		 
				 printf("Number of places available = %d\n", N);
				 sprintf(msg, "%dh00, CLOSED\nNb. Places = %d", my_time, N);
				 lcd.setText(msg);
				 delay(2000);
			 }
			 else{
				 // A car want to get in but there is no place
				 if(wait && N==0){
					 printf("There is no place, you need to wait...\n");
					 sprintf(msg, "%dh00, FULL, wait...\nNb. Places = %d", my_time, N);
					 lcd.setText(msg);
					 delay(1000);
				 }
				 else if(N<=0 && !wait){
					 // A car has just gotten in, there is no more place
					 printf("The parking is FULL\n");
					 sprintf(msg, "%dh00, FULL\nNb. Places = %d", my_time, N);
					 lcd.setText(msg);
					 delay(2000);
				 }
				 else{ // There are available places
					 printf("Number of places available = %d\n", N);
					 sprintf(msg, "%dh00, not FULL\nNb. Places = %d", my_time, N);
					 lcd.setText(msg);
					 delay(2000);
				 }
			 }
		 }
	 }
	 pthread_exit(0);
 }
 

/* ==================================================================
 * FUNCTION KEYBORAD_TASK
 * While the program runs, we send a token to the display task, then we
 * get the states of button enter and button exit.
 * if a button is pressed we post a token to the corresponding task.
 * ==================================================================*/
 void *task_keyboard(void *args)
 {
	 while(1){
		 sem_post(&sem_disp);
		 state_btn_enter = digitalRead(pin_btn_enter);
		 state_btn_exit = digitalRead(pin_btn_exit);
		 if(state_btn_enter){
			 sem_post(&sem_enter);
		 }
		 else if(state_btn_exit){
			 sem_post(&sem_exit);
		 }
	 }
	 pthread_exit(0);
 }
 
 /* ==================================================================
 * FUNCTION CLOCK_TASK
 * Every 5 seconds we incremente the time by 1, and when the time 
 * is equal to 24 we intialize it to 0.
 * ==================================================================*/
 void *clock_task(void *args)
 {
	 while(!end){
		 sleep(5);
		 my_time++;
		 if(my_time == 24){
			 my_time = 0;
		 }
	 }
	 pthread_exit(0);
 }
 
 
 
 /* ==================================================================
 * FUNCTION SERVER_TASK
 * While the program runs, we recieve the message from the client.
 * If it is a help message and there is an available place, then,
 * we lock the number of available places and we send an agreement message
 * to the client, and, we reduce N by 1.
 * Else if there is no place, we send a negative message.
 * ==================================================================*/
void *task_server(void *args)
{
	while(!end){
		if(!end){
			recv(client_socket, help_msg, 1, 0);
			printf("Serveur received %c\n",help_msg[0]);
			if(help_msg[0]=='H' && N>0){ // If the client ask for help
				pthread_mutex_lock(&mutex_N); // Lock N
				//Send back an answer
				answer_for_client[0]='Y';
				write(client_socket, answer_for_client, 1);				
				N--;
				pthread_mutex_unlock(&mutex_N); // unlock N
				sem_post(&sem_disp);
				answer_for_client[0]='X';				
			}
			else if(help_msg[0]=='H' && N<=0){
				answer_for_client[0]='N';
				write(client_socket, answer_for_client, 1);
			}
			help_msg[0]=='X';
		}
	}
	pthread_exit(0);
}


/* ==================================================================
 * MAIN PROGRAM
 * ==================================================================*/
int main()
{
	pthread_t thKeyboard, thEnter, thExit, thDisplay, thClock, thServer;
	int tokens = 0;
	N = 10; // Initialize N
	struct sockaddr_in server, client;
	int size, read_size;
	
	mysocket = socket(AF_INET , SOCK_STREAM , 0); // Create the socket
    if (mysocket == -1) {
        printf("Error : socket creation\n");
		return -1;
    }

    server.sin_family = AF_INET; 			// Define the family
    server.sin_addr.s_addr = INADDR_ANY;	// Define the internet address
    server.sin_port = htons(6666);			// Define the communication's port
	// Link the socket to the data structure sockaddr
    if(bind(mysocket,(struct sockaddr *)&server , sizeof(server)) < 0) {
      printf("Error : bind failed\n");
      return -1;
    }
    
    listen(mysocket, 3);  	// Define the size of the connection link
    size = sizeof(struct sockaddr_in);
	
	// Accept the connection from an other socket.
    client_socket = accept(mysocket, (struct sockaddr *)&client, (socklen_t*)&size);
    if (client_socket < 0) {
      printf("accept failed\n");
      return -1;
    }
    
    
	sprintf(msg, "%dh00, PARKING\nNb. Places= %d", my_time, N);
	initGrovePi();
	pinMode(pin_btn_exit, INPUT);
	pinMode(pin_btn_enter, INPUT);
	pinMode(pin_LED_exit, OUTPUT);
	pinMode(pin_LED_enter, OUTPUT);
			
	lcd.connect();			
	lcd.setRGB(20, 88, 125);
	lcd.setText(msg);

	
	sem_init(&sem_enter, 0, tokens); // Initialize sem_enter
	sem_init(&sem_exit, 0, tokens); // Initialize token_exit
	sem_init(&sem_disp, 0, tokens); // Initialize token_display
	
		
	pthread_mutex_init(&mutex_N, NULL);
	pthread_mutex_init(&mutex_disp, NULL);
	pthread_cond_init(&cond_mutex_N, NULL);
	
	pthread_create(&thEnter, NULL, task_enter, NULL);	
	pthread_create(&thExit, NULL, task_exit, NULL);
	pthread_create(&thDisplay, NULL, task_display, NULL);
	pthread_create(&thKeyboard, NULL, task_keyboard, NULL);
	pthread_create(&thClock, NULL, clock_task, NULL);
	pthread_create(&thServer, NULL, task_server, NULL);
			
	pthread_join(thEnter, NULL);
	pthread_join(thExit, NULL);
	pthread_join(thDisplay, NULL);
	pthread_join(thKeyboard, NULL);
	pthread_join(thServer, NULL);
	
	sem_destroy(&sem_enter); // Destroy token
	sem_destroy(&sem_exit); // Destroy token
	sem_destroy(&sem_disp); // Destroy token
	
	pthread_cond_destroy(&cond_mutex_N);
	pthread_mutex_destroy(&mutex_N);
	pthread_mutex_destroy(&mutex_disp);
	
	close(mysocket);
	return 0;
}

