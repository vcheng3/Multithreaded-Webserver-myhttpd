

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netdb.h>
#include	<netinet/in.h>
#include	<inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>

//variables


int head = -1;
int tail  = -1;
int sleepTime = 60;
int port = 8080 ;
int daemonMode = 0 ;
int server = 0 ;
int client = 0 ;
int policy =  0;
int fsize ;

struct queueNode{
	char* incomingRequest ;
	char* arrivalTime ;
	char* fileName ;
	char* responseTime;
	int clientAddress;
	int fileSize ;
	int flag ;
	int socketFD ;

}queueSharedNode;

struct queueNode requestQueue[100];



char * dir ;
char * logFile ;
char * hostName ;
char * targetName ;
int numberOfThreads = 4 ;

int logging = 0 ;
int debug = 0 ;
int serverSocketFD ;
int chgDir = 0 ;
int conditionV = 0 ;

pthread_mutex_t workerThreadsMutex = PTHREAD_MUTEX_INITIALIZER, queueMutex = PTHREAD_MUTEX_INITIALIZER , sharedNodeMutex = PTHREAD_MUTEX_INITIALIZER ;
pthread_cond_t schedulerCondition = PTHREAD_COND_INITIALIZER;


void
usage()
{
	printf("\n1. -d Debugging Mode."
				   "\n2. -d : Debugging Mode.Do not run as Daemon and output to console."
				   "\n3. -h : Usage Summary and exit."
				   "\n4. -l file : Log to entered file."
				   "\n5. -p port : Listen on entered port. Default is 8080."
				   "\n6. -r dir : Debugging Mode. "
				   "\n7. -t time : Queueing time. Default is 60s."
				   "\n8. -n threadnum : Set number of threads.Default is 4. "
				   "\n9. -s sched : Scheduling Policy. sjf or fcfs. Default is fcfs. ");

	exit(1);
}

void * serverRunner(void * adrg){

	printf("\nServer started .. Waiting for connection!");
	struct sockaddr_in serverSocket,  cli_addr ;
	int sockfd = socket(AF_INET,SOCK_STREAM,0);

	serverSocket.sin_family = AF_INET;
	serverSocket.sin_addr.s_addr = INADDR_ANY;
	serverSocket.sin_port = htons(port);

	char * fileName ;
	struct stat structstat ;
	char buffer[256];


	int bindRet = bind(sockfd, (struct sockaddr *) &serverSocket, sizeof(serverSocket)) ;

	printf("\nBind =  %d " , bindRet);
	if(bindRet<0) {printf("\nError - Unable to bind at the given socket."); exit(1);}

	socklen_t clienlen = sizeof(cli_addr);
	off_t fileSize ;
	int retcode , client_fd ;

	listen(sockfd,10);
	struct sockaddr_in client ;
	char * req ;

	while(1){


		memset(buffer,0,256);



		client_fd = accept(sockfd,(struct sockaddr *) &cli_addr ,&clienlen);


		if(client_fd>=0){
			printf("\nConnection established.");
			//memset(buffer,0,255);

			if(!fork()){
				//child process
				close(sockfd);
				struct queueNode tempNode ;
				retcode = recv(client_fd, buffer, 256, 0);

				int addr = cli_addr.sin_addr.s_addr;
				int one = cli_addr.sin_addr.s_addr&0xFF;

				printf("\nBuffer -%s " , buffer);
				printf("\nClient address -%d " , addr);


				time_t now = time(0);
				char * arrivalTime = ctime(&now);
				char * req = buffer ;
				char * temp = strtok(buffer, " ");
				if(temp!=NULL) {
					temp = strtok(NULL, " ");
					temp = strtok(temp, "/");

					size_t d = strlen(temp);
					temp[d-2] = '\0';


					if (temp != NULL)
						fileName = temp;


					//ssize_t  ssize = send(client_fd,arrivalTime,strlen(arrivalTime),0);

					if(stat(fileName, &structstat)==0){
						fileSize = structstat.st_size;

						 tempNode.fileName = fileName ;
						 tempNode.arrivalTime = arrivalTime;
						 tempNode.flag = 0 ;
						 tempNode.fileSize = fileSize;
						 tempNode.incomingRequest = req ;
						 tempNode.clientAddress = addr ;

						pthread_mutex_lock(&queueMutex);

						if(head==-1&&tail==-1){
							requestQueue[0] = tempNode;
							head = 0;
							tail  = 0;
						}

						else if(head<=tail){
							requestQueue[tail+1]=tempNode;
							tail++;
						}

						pthread_mutex_unlock(&queueMutex);

					}

					else{
						printf("File not found.");
					}

				}
			} //end fork



		}


		//close(client_fd);

	}


}

void * schedulerRunner(void * arg){
	//sleep for entered time

	//printf("\nWaiting for Queing time .. %d " , sleepTime);



	struct queueNode tempNode;
	sleep(sleepTime);


	while(1){
		if(head!=-1 && tail !=-1){


			pthread_mutex_lock(&queueMutex);

			if(policy==1){
				int i = head ;
				struct queueNode copy , temp;
				int shortest = head;



				for(i=head;i<tail ; i++){
					if(requestQueue[i].fileSize<requestQueue[i+1].fileSize)
						shortest = i ;

				}


				copy = requestQueue[head];
				requestQueue[head]=requestQueue[shortest];
				requestQueue[shortest] = copy ;

			}

			tempNode = requestQueue[head];

			if(head==tail && head!=-1)
				tail++;
			head++;
			pthread_mutex_unlock(&queueMutex);

			//mutex on shared node
			pthread_mutex_lock(&sharedNodeMutex);
			if(queueSharedNode.flag!=1){
				queueSharedNode = tempNode;
				queueSharedNode.flag = 1;
				//signal waiting worker threads
				// the condition variables requires to be associated with a mutex
				//pthread_cond_signal(&schedulerCondition);

			}
			pthread_mutex_unlock(&sharedNodeMutex);

			conditionV = 1 ;

		}



	}


}

void * workerRunner(void * arg){
	//printf("\nWorker threads waiting..");
	struct queueNode temp ;
	char * respTime ;
	char buffer[2048] ;


	while(1){

		while(conditionV==0) ;

		//the conditional wait requires to be associated with a mutex -
		// this is not a lock - so would not cause deadlock.

		//pthread_cond_wait(&schedulerCondition,&sharedNodeMutex);


		pthread_mutex_lock(&sharedNodeMutex);


		temp = queueSharedNode;

		if(queueSharedNode.flag==1){
			conditionV = 0;
			queueSharedNode.flag = 0;
			time_t now = time(0);
			temp.responseTime = ctime(&now);
			queueSharedNode.responseTime = ctime(&now);
		}


		pthread_mutex_unlock(&sharedNodeMutex);


		if(temp.flag==1){

			int ip = temp.clientAddress;
			char * arrival = temp.arrivalTime;
			char * request = temp.incomingRequest;
			char * rTime = temp.responseTime;
			int respSize = temp.fileSize;

			int one = ip&0xFF ;
			int two = ip&0xFF00>>8;
			int three = ip&0xFF0000>>16;
			int four = ip&0xFF000000>>24;

			char * fName = temp.fileName;
			FILE * f , *flog;
			int img = 0 ;
			int status = 0 ;
			if(strstr(fName,".gif")) img = 1 ;
			if(strstr(fName,".jpeg")) img = 1 ;
			if(strstr(fName,".jpg")) img = 1;
			if(strstr(fName,".png")) img = 1;
			if(strstr(fName,".gif")) img = 1 ;

			struct tm *clock;
			struct stat attr;

			stat(fName, &attr);
			clock = gmtime(&(attr.st_mtime));
			char * lastMod = asctime(clock);

			if(fopen(fName,"rb")) {

					if(img==1){

						status =  200;
					}
					else{

						status = 201 ;
					}


			}
			else{
				printf("404 File Not Found.");
				status = 404;

			}


			int s = status ;
			if(s==201) s = 200;

			char * logmsg ;
			sprintf(logmsg, "\n%d.%d.%d.%d \t [%s]\t[%s]\t%s\t%d\t%d\n",one , two , three, four, arrival,rTime,request, s, respSize );


			if(logging==1){


				//char * ret = fopen(fileName,"ab");
				// write to file
				//127.0.0.1 - [19/Sep/2011:13:55:36 -0600] [19/Sep/2011:13:58:21 -0600]
				//"GET /index.html HTTP/1.0" 200 326
				flog = fopen(logFile,"a");
				fwrite(logmsg,strlen(logmsg),1,flog);
				fclose(flog);

			}

			if(debug==1){

				//write to stdout
				printf("\n%s" , logmsg);

			}

			FILE * fwriteBack ;
			fwriteBack = fopen(fName,"rb");
			memset(buffer,0,2048);
			ssize_t sz = fread(buffer,2048,1,fwriteBack);


			size_t  ssize ;
			int get = 0 ;
			int head = 0 ;
			if(strstr("GET", request))  get = 1 ;
			if(strstr("HEAD" , request)) head = 1;


			char hostname[1024];
			hostname[1023] = '\0';
			gethostname(hostname, 1023);
			struct hostent* h;
			h = gethostbyname(hostname);
			char * hostName = h->h_name;
			char * headMsg ;

			time_t now = time(0);
			char * currentTime = ctime(&now);


			if(head==1){
				if(status==200){
					sprintf( headMsg,
							        "Date : %s.\n"
									"Server : %s.\n"
									"Last-Modified : %s.\n"
									"Content-Type : image/gif\n"
									"Content-Length : %d\n"
					,currentTime,hostName,lastMod,respSize);
				}

				if(status==201){

					sprintf(headMsg,
							"Date : %s.\n"
									"Server : %s.\n"
									"Last-Modified : %s.\n"
									"Content-Type : text/html\n"
									"Content-Length : %d\n"
							,currentTime,hostName,lastMod,respSize);

				}

				if(status==404){
					sprintf(headMsg,
							"Date : %s.\n"
									"Server : %s.\n"
									"Last-Modified : Not Found.\n"
									"Content-Type : Not Found\n"
									"Content-Length : Not Found\n"
							,currentTime,hostName);

				}

				ssize_t  ssize1 = send(temp.socketFD,headMsg,strlen(headMsg),0);
			}


			if(get==1){

				if(status==201){
					char * m1 = "\nHTTP/1.0 200 OK"
							"\nContent-Type:image/gif";
					ssize = send(temp.socketFD,m1,strlen(m1),0);
					ssize = send(temp.socketFD,buffer,strlen(buffer),0);
				}

				if(status==200){
					char * m2 = "\nHTTP/1.0 200 OK"
							"\nContent-Type:text/html";
					ssize = send(temp.socketFD,m2,strlen(m2),0);
					ssize = send(temp.socketFD,buffer,strlen(buffer),0);
				}

				if(status==404){
					char * m3 = "<html><body><h1>404 NOT Found</h1></body></html>";
					ssize = send(temp.socketFD,m3,strlen(m3),0);

					ssize = send(temp.socketFD,buffer,strlen(buffer),0);

				}
			}





		}



	}
}
int main(int argc,char *argv[]) {


	int i = 0 , j ;


	for(i=0;i<argc;i++){

		char * temp = argv[i];
		if(strcmp(temp,"-p")==0){ port = atoi(argv[i+1]);
		printf("\nPort set to - %d" , port );}

		if(strcmp(temp,"-d")==0){
			printf("\nEntering debugging mode.");
			numberOfThreads = 1;
			debug =1;
		}

		if(strcmp(temp,"-l")==0){
			logFile = argv[i+1];
			logging= 1;

		}
		if(strcmp(temp,"-h")==0){
			usage();


		}
		if(strcmp(temp,"-r")==0){
			int d = chdir(argv[i+1]);
			if(d<0){
				printf("\nDirectory not found");
				exit(1);
			}
		}

		if(strcmp(temp,"-t")==0){
		 	sleepTime = atoi(argv[i+1]);
			printf("\nQueuing time set to %d" , sleepTime);

		}

		if(strcmp(temp,"-n")==0){
			numberOfThreads = atoi(argv[i+1]);

		}
		if(strcmp(temp,"-s")==0){
			if(strcmp(argv[i+1],"sjf")==0||strcmp(argv[i+1],"SJF")==0)
				policy = 1;


		}

	}


	queueSharedNode.flag = 0 ;

	pthread_t serverThread , schedulerThread ;
	pthread_t workerThreads[numberOfThreads];
	pthread_attr_t attr;
	pthread_attr_t attr1;
	pthread_attr_init(&attr);
	pthread_attr_init(&attr1);

	int n = 0;

	printf("\nCreating threads .. ");
	pthread_create(&serverThread,&attr,serverRunner,NULL);

	printf("\nWaiting for Queing time .. %d " , sleepTime);
	pthread_create(&schedulerThread,NULL,schedulerRunner,NULL);

	for(n=0 ; n<numberOfThreads;n++)
		pthread_create(&workerThreads[n],NULL, workerRunner, NULL );


	pthread_join(serverThread, NULL);
	pthread_join(schedulerThread,NULL);


	for(n=0 ; n<numberOfThreads;n++)
		pthread_join(workerThreads[n], NULL );


	return 0;
}
