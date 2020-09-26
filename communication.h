#pragma once

#define  _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

#define MSG_MAX_LEN 1024

// Initialize the environment for the s-talk
// Upon success returns 0; otherwise returns a non-zero value
int Talk_init(int myPort, char* remoteMachineName, int remotePort);

// Create all threads to start talking
void Talk_start();

// Shutdown all threads and stop the connection
void Talk_shutdown();


// Thread to receive message from remote client
void* Receive_peer_message();

// Thread to print the received client message on screen
void* Print_received_message();

// Thread to listen keyboard message
void* Listen_keyboard_message();

// Thread to send message to peer
void* Send_message_to_peer();


void Receiver_shutdown();
void Sender_shutdown();



