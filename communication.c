#include "communication.h"
#include "list.h"

#include <pthread.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

// mutex and condition variable for listening keyboard and send message
static int waitingKeyboardMsg = 0;
static pthread_mutex_t transitMsgMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t receivedMsgFromKeyboard = PTHREAD_COND_INITIALIZER;

// mutex and condition variable for receiving and displaying message
static int waitingPeerMsg = 0;
static pthread_mutex_t recvMsgMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t receivedMsgFromPeer = PTHREAD_COND_INITIALIZER;

// thread Id
static pthread_t tid_listenKeyboard;
static pthread_t tid_sendMsg;
static pthread_t tid_recvMsg;
static pthread_t tid_printMsg;

// sockect variables
static int socketDescriptor;
static struct sockaddr_in sin;
static struct sockaddr_in sinRemote;

// shared lists
static List* pSendList = NULL;
static List* pRecvList = NULL;
static void List_node_free_fn(void* pItem)
{
	free(pItem);
	pItem = NULL;
}

int Talk_init(int myPort, char* remoteMachineName, int remotePort)
{
	// Create socket	
	socketDescriptor = socket(PF_INET, SOCK_DGRAM, 0);
	if(socketDescriptor == -1) {
		puts("Create socket failed");
		return -1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET; 
	sin.sin_addr.s_addr = htonl(INADDR_ANY); 
	sin.sin_port = htons(myPort);            
	
	// Bind socket
	if(bind (socketDescriptor, (struct sockaddr*) &sin, sizeof(sin))) {
		puts("bind socket failed");
		return -1;
	}

	struct hostent *remoteHost = gethostbyname(remoteMachineName);
	if(remoteHost == NULL) {
		printf("Talk_init failed: cannot find reomte machine %s\n", remoteMachineName);
		return -1;
	}

	// set remote machine info
	memset(&sinRemote, 0, sizeof(sinRemote));
	memcpy(&sinRemote.sin_addr, remoteHost->h_addr_list[0], remoteHost->h_length);
	sinRemote.sin_family = AF_INET;
	sinRemote.sin_port = htons(remotePort); 

	// create shared lists
	pRecvList = List_create();
	if(pRecvList == NULL) {
		puts("create recv list failed");
		return -1;
	}
	pSendList = List_create();
	if(pSendList == NULL) {
		puts("create send list failed");
		return -1;
	}

	return 0;
}

void Talk_start() {
	pthread_create(&tid_recvMsg, NULL, Receive_peer_message, NULL);
	pthread_create(&tid_printMsg, NULL, Print_received_message, NULL);
	pthread_create(&tid_listenKeyboard, NULL, Listen_keyboard_message, NULL);
	pthread_create(&tid_sendMsg, NULL, Send_message_to_peer, NULL);
}

void Talk_shutdown() {
	pthread_join(tid_recvMsg, NULL);
	pthread_join(tid_printMsg, NULL);
	pthread_join(tid_listenKeyboard, NULL);
	pthread_join(tid_sendMsg, NULL);

	List_free(pRecvList, List_node_free_fn);
	List_free(pSendList, List_node_free_fn);

	close(socketDescriptor);
}


void Receiver_shutdown() {
	pthread_cancel(tid_printMsg);
	pthread_cancel(tid_recvMsg);
}

void Sender_shutdown() {
	pthread_cancel(tid_listenKeyboard);
	pthread_cancel(tid_sendMsg);
}

void* Receive_peer_message() {
	int continueRecv = 1;
	while(continueRecv)
	{
		char msg[MSG_MAX_LEN];
		unsigned int sin_len = sizeof(struct sockaddr_in);
		int bytes = recvfrom(socketDescriptor, msg, MSG_MAX_LEN,
			0, (struct sockaddr *)&sinRemote, &sin_len);

		if(bytes > 0)
		{
			int len = bytes;
		        if(bytes >= MSG_MAX_LEN)
			{
				len = MSG_MAX_LEN - 1;
			}
			msg[len] = '\0';

			char* str = malloc(sizeof(char) * (len + 1));
			strcpy(str, msg);
			if(strcmp(msg, "!\n") == 0) {
				continueRecv = 0;
			}

			while(!waitingPeerMsg) {}	

			pthread_mutex_lock(&recvMsgMutex);
			{
				List_append(pRecvList, str);
				pthread_cond_signal(&receivedMsgFromPeer);
			}
			pthread_mutex_unlock(&recvMsgMutex);

		}
	}
	return NULL;
}


void* Print_received_message() {
	int continuePrint = 1;
	while(continuePrint)
	{
		pthread_mutex_lock(&recvMsgMutex);
		{
			waitingPeerMsg = 1;
			pthread_cond_wait(&receivedMsgFromPeer, &recvMsgMutex);
			waitingPeerMsg = 0;

			char* msg = List_first(pRecvList);
			if(strcmp(msg, "!\n") == 0)
			{
				continuePrint = 0;
			}

			printf("%s", msg);
			msg = List_remove(pRecvList);
			free(msg);
			msg = NULL;
		}
		pthread_mutex_unlock(&recvMsgMutex);
	}
	Sender_shutdown();
	return NULL;
}


void* Listen_keyboard_message() {
	int continueListenKeyboard = 1;
	while(continueListenKeyboard)
	{

		char* str = NULL;
		size_t size = 0;
		getline(&str, &size, stdin);
		if(strcmp(str, "!\n") == 0)
		{
			continueListenKeyboard = 0;
		}

		while(!waitingKeyboardMsg) {}
		pthread_mutex_lock(&transitMsgMutex);
		{
			waitingKeyboardMsg = 0;
			List_append(pSendList, str);
			pthread_cond_signal(&receivedMsgFromKeyboard);
		}
		pthread_mutex_unlock(&transitMsgMutex);
	}
	Receiver_shutdown();
	return NULL;
}

void* Send_message_to_peer() {
	int continueSend = 1;
	while(continueSend)
	{
		pthread_mutex_lock(&transitMsgMutex);
		{
			waitingKeyboardMsg = 1;
			pthread_cond_wait(&receivedMsgFromKeyboard, &transitMsgMutex);

			char* msg = List_first(pSendList);
			if(strcmp(msg, "!\n") == 0)
			{
				continueSend = 0;
			}

			unsigned int sin_len = sizeof(struct sockaddr_in);
			sendto( socketDescriptor, msg, strlen(msg),
				0, (struct sockaddr *)&sinRemote, sin_len);

			List_remove(pSendList);
			free(msg);
			msg = NULL;
		}
		pthread_mutex_unlock(&transitMsgMutex);
	}
	return NULL;
}


