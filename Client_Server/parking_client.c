#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// --------------------------------------
// GLOBAL VARIABLES
// --------------------------------------

#define NMAX 100
volatile char order;   // Order to execute
volatile int N;        // Number of available places
volatile int end = 0;  // Indicator for the end of the progam
volatile int my_time;  // The clock
volatile int wait = 0; // Use to wait for an available place
volatile int goToServer = 0; // Use to indicate a transfert to the Server
char address_server[15]; // server's address
char answer_from_server[1];
char msg[1];			// Message for the server
volatile int mysocket;  // Use for the socket


sem_t sem_enter;  	// Semaphore for entry_task
sem_t sem_exit;   	// Semaphore for exit_task
sem_t sem_disp;   	// Semaphore for display_task
sem_t sem_server;   // Semaphore for thread server
pthread_mutex_t mutex_N = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_disp = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_mutex_N;

/* ==================================================================
 * FUNCTION ENTER_TASK
 * We wait for a token, if there is a token and the program is still
 * running and the button enter is pressed.
 * Then, if the parking is open, we lock the number of available places N.
 * If there is a place, we reduce N by 1, we post a token to the display task.
 * If there is no place, we wait for a place, and we send a token to the client_task
 * in order to ask the server to take the waiting car. 
 * If the server is agree, the say to the car it can go to the server. Else, we wait
 * for an available place and if it is OK, we reduce N by 1.
 * ==================================================================*/
void *task_enter(void *args)
{
	while(!end){
		sem_wait(&sem_enter); // Waiting for the order
		if(!end){
			if(my_time < 1 || my_time >5){
				pthread_mutex_lock(&mutex_N); // Lock N
				
				// There is a place we can enter
				if(N > 0){
					N--;									
					sem_post(&sem_disp);
				}
				
				// There is no place, we wait for a place
				if(N <= 0){
					wait = 1;
					// We ask the help of the server
					sem_post(&sem_server);
					if(answer_from_server[0] !='Y'){
						printf("[ANSW] : %c\n", answer_from_server[0]);					
						sem_post(&sem_disp);					
						pthread_cond_wait(&cond_mutex_N, &mutex_N);
						wait = 0;
						N--;					
						sem_post(&sem_disp);
					}else{ // If the Server accepted the demand
						goToServer = 1;
						sem_post(&sem_disp);
						goToServer = 0;
					}
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
 * If N is under the maximum available places, we incremente N by 1, 
 * we post a token to the display task.
 * Finally, we send a signal to indicate an available place.
 * ==================================================================*/
void *task_exit(void *args)
{
	while(!end){
		sem_wait(&sem_exit);
		if(!end){			
			pthread_mutex_lock(&mutex_N); // Lock N			
			// We are in the parking and we want to get out
			if(N < NMAX){
				N++; 
				pthread_mutex_lock(&mutex_disp); // Lock the display
				sem_post(&sem_disp);
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
			 }
			 else{
				 // A car want to get in but there is no place
				 if(wait && N==0 && !goToServer){
					 printf("There is no place, you need to wait...\n");
				 }
				 else if(N<=0 && !wait){
					 // A car has just gotten in, there is no more place
					 printf("The parking is FULL\n");
				 }
				 // Server accepted to recieve the car
				 else if(wait && N<=0 && goToServer && answer_from_server[0]=='Y'){
					 printf("No place here, Go to Server...\n");
					 wait = 0; // We send the car to the Server
				 }
				 else if(wait && N<=0 && goToServer && answer_from_server[0]!='Y'){
					 printf("No place nowhere, wait...\n");
				 }
				 else{ // There are available places
					 printf("Number of places available = %d\n", N);
				 }		 
			 }
		 }
	 }
	 pthread_exit(0);
 }
 

/* ==================================================================
 * FUNCTION KEYBORAD_TASK
 * ==================================================================*/
 void *keyboard_task()
 {	 
	 while(order != 'F' && !end){
		 printf("\tE ---------------------------> ENTER\n\n");
		 printf("\tS ---------------------------> EXIT\n\n");
		 printf("\tF ---------------------------> QUIT\n\n");
		 
		 printf("What do you want? ");
		 order = getchar();	 
		 if(order == 'E'){
			 sem_post(&sem_enter);
		 }
		 else if(order == 'S'){
			 sem_post(&sem_exit);
		 }
		 else if(order == 'F'){
			 end = 1;
			 sem_post(&sem_enter);
			 sem_post(&sem_exit);
			 sem_post(&sem_disp);
			 break;
		 } 
		 sleep(2);
	 }
	 pthread_exit(0);
 }
 
 
 
 /* ==================================================================
 * FUNCTION CLOCK_TASK
 * Every 5 seconds we incremente the time by 1, and when the time is equal
 * to 24 we intialize it to 0.
 * ==================================================================*/
 void *clock_task()
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
 * FUNCTION CLIENT_TASK
 * While the program is running, we wait for a token. If there is a token
 * we send a help message to the server. And if the message is 
 * we wait for the server's answer.
 * ==================================================================*/
 void *task_client()
 {
	while(!end){
		if(!end){
			sem_wait(&sem_server);
			msg[0]='H';
			if( send(mysocket, msg, 1, 0) < 0) {
				printf("Error : send failed\n");			
			}else{
				recv(mysocket, answer_from_server, 1, 0);	 		
				printf("Server answers : %c\n", answer_from_server[0]);
			}
			msg[0]='N';
		}
	 }	 
     pthread_exit(0);
 }

/* ==================================================================
 * MAIN PROGRAM
 * ==================================================================*/
int main(int argc, char* argv[])
{
	pthread_t thKeyboard, thEnter, thExit, thDisplay, thClock, thClient;
	int tokens = 0;
	N = 3; // Initialize N
	
	if(argc!=2) {  // Check there is a parameter given from the execution
      printf("Usage : %s ip_du_serveur\n",argv[0]);
      return -1;
    }
	
	strcpy(address_server, argv[1]); // Get the server's addres from the console
	printf("[IP] : %s\n", address_server);
		
	struct sockaddr_in server;
	mysocket = socket(AF_INET , SOCK_STREAM , 0);  // Create the socket
	if (mysocket == -1) {
		printf("Error : socket creation\n");
		return -1;
	} 
		 
	server.sin_addr.s_addr = inet_addr(address_server); // Define the internet address
	server.sin_family = AF_INET;  			// Define the family
	server.sin_port = htons(6666);			// Define the communication's port
	
	// Connect the socket to the address defined in sockaddr
	if (connect(mysocket, (struct sockaddr *)&server, sizeof(server)) < 0) {
		printf("Error : connection failed\n");
		return -1;
	} 
		
	sem_init(&sem_enter, 0, tokens); // Initialize sem_enter
	sem_init(&sem_exit, 0, tokens); // Initialize sem_exit
	sem_init(&sem_disp, 0, tokens); // Initialize sem_display
	sem_init(&sem_server, 0, tokens); // Initialize sem_server
	
	pthread_mutex_init(&mutex_N, NULL);
	pthread_mutex_init(&mutex_disp, NULL);
	pthread_cond_init(&cond_mutex_N, NULL);
	
	pthread_create(&thEnter, NULL, task_enter, NULL);	
	pthread_create(&thExit, NULL, task_exit, NULL);
	pthread_create(&thDisplay, NULL, task_display, NULL);
	pthread_create(&thKeyboard, NULL, keyboard_task, NULL);
	pthread_create(&thClock, NULL, clock_task, NULL);
	pthread_create(&thClient, NULL, task_client, NULL);
			
	pthread_join(thEnter, NULL);
	pthread_join(thExit, NULL);
	pthread_join(thDisplay, NULL);
	pthread_join(thKeyboard, NULL);
	pthread_join(thClock, NULL);
	pthread_join(thClient, NULL);
	
	sem_destroy(&sem_enter); // Destroy token
	sem_destroy(&sem_exit); // Destroy token
	sem_destroy(&sem_disp); // Destroy token
	sem_destroy(&sem_server); // Destroy token
	
	pthread_cond_destroy(&cond_mutex_N);
	pthread_mutex_destroy(&mutex_N);
	pthread_mutex_destroy(&mutex_disp);
	
	close(mysocket);
	
	return 0;
}
