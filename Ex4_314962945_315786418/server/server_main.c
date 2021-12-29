
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "SocketShared.h"
#include "SocketSendRecvTools.h"
#include "serverhardcoded.h"
#pragma comment(lib, "Ws2_32.lib")

HANDLE ThreadHandles[MAX_NUM_CLIENTS];
SOCKET ThreadInputs[MAX_NUM_CLIENTS];
char message_type[][] = {"CLIENT_REQUEST","CLIENT_VERSUS","CLIENT_PLAYER_MOVE","CLIENT_DISCONNECT","SERVER_APPROVED","SERVER_DENIED","SERVER_MAIN_MENU","GAME_STARTED","TURN_SWITCH","SERVER_MOVE_REQUEST","GAME_ENDED","SERVER_NO_OPPONENTS","GAME_VIEW","SERVER_OPPONENT_OUT"};

int main(int argc, char* argv[])
{
	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERROR_CODE;
	}
	int port_num = argv[PORT_NUM_IND];
	mainServer(port_num);
}


int mainServer(int port_num)
{
	int Ind;
	int Loop;
	SOCKET MainSocket = INVALID_SOCKET;
	unsigned long Address;
	SOCKADDR_IN service;
	int bindRes;
	int ListenRes;

	// Initialize Winsock.
	WSADATA wsaData;
	int StartupRes = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (StartupRes != NO_ERROR)
	{
		printf("error %ld at WSAStartup( ), ending program.\n", WSAGetLastError());
		// Tell the user that we could not find a usable WinSock DLL.                                  
		return ERROR_CODE;
	}

	/* The WinSock DLL is acceptable. Proceed. */

// Create a socket.    
	MainSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (MainSocket == INVALID_SOCKET)
	{
		printf("Error at socket( ): %ld\n", WSAGetLastError());
		clean_server();
	}
	// Bind the socket.
	// Create a sockaddr_in object and set its values.
	// Declare variables
	Address = inet_addr(SERVER_ADDRESS_STR);
	if (Address == INADDR_NONE)
	{
		printf("The string \"%s\" cannot be converted into an ip address. ending program.\n",
			SERVER_ADDRESS_STR);
		close_socket(MainSocket);
		clean_server();
	}
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = Address;
	service.sin_port = htons(port_num); //The htons function converts a u_short from host to TCP/IP network byte order 
									   //( which is big-endian ).

	// Call the bind function, passing the created socket and the sockaddr_in structure as parameters. 
	// Check for general errors.
	bindRes = bind(MainSocket, (SOCKADDR*)&service, sizeof(service));
	if (bindRes == SOCKET_ERROR)
	{
		printf("bind( ) failed with error %ld. Ending program\n", WSAGetLastError());
		close_socket(MainSocket);
		clean_server();
	}

	// Listen on the Socket.
	ListenRes = listen(MainSocket, SOMAXCONN);
	if (ListenRes == SOCKET_ERROR)
	{
		printf("Failed listening on socket, error %ld.\n", WSAGetLastError());
		close_socket(MainSocket);
		clean_server();
	}

	// Initialize all thread handles to NULL, to mark that they have not been initialized
	for (Ind = 0; Ind < MAX_NUM_CLIENTS; Ind++)
		ThreadHandles[Ind] = NULL;

	printf("Waiting for a client to connect...\n");

	for (Loop = 0; Loop < MAX_NUM_CLIENTS; Loop++)
	{
		SOCKET AcceptSocket = accept(MainSocket, NULL, NULL);
		if (AcceptSocket == INVALID_SOCKET)
		{
			printf("Accepting connection with client failed, error %ld\n", WSAGetLastError());
			goto server_cleanup_3;
		}

		printf("Client Connected.\n");

		Ind = FindFirstUnusedThreadSlot();

		if (Ind == MAX_NUM_CLIENTS) //no slot is available
		{
			printf("No slots available for client, dropping the connection.\n");
			closesocket(AcceptSocket); //Closing the socket, dropping the connection.
		}
		else
		{
			ThreadInputs[Ind] = AcceptSocket; // shallow copy: don't close 
											  // AcceptSocket, instead close 
											  // ThreadInputs[Ind] when the
											  // time comes.
			ThreadHandles[Ind] = CreateThread(
				NULL,
				0,
				(LPTHREAD_START_ROUTINE)ServiceThread,
				&(ThreadInputs[Ind]),
				0,
				NULL
			);
		}
	} // for ( Loop = 0; Loop < MAX_LOOPS; Loop++ )

}

void close_socket(SOCKET socket_to_close)
{
	if (closesocket(socket_to_close) == SOCKET_ERROR)
	{
		printf("Failed to close MainSocket, error %ld. Ending program\n", WSAGetLastError());
	}
}

int clean_server()
{
	if (WSACleanup() == SOCKET_ERROR)
	{
		printf("WSA Cleanup failed:, error %ld. Ending program.\n", WSAGetLastError());
		return ERROR_CODE;
	}

}

void graceful_shutdown(SOCKET socket_to_close)
{
	if (NO_ERROR != shutdown(socket_to_close, SD_SEND))
	{
		printf(":, error %ld. Ending program.\n", WSAGetLastError());//TODO add print
		return ERROR_CODE;
	}
	//wait for recv 0 
	//TransferResult_t ReceiveBuffer(char* OutputBuffer, int BytesToReceive, SOCKET sd)
	//recv()//TODO fixe 

}




