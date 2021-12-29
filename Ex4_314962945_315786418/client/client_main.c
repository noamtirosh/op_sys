
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#define STRINGS_ARE_EQUAL( Str1, Str2 ) ( strcmp( (Str1), (Str2) ) == 0 )


#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "SocketShared.h"
#include "SocketSendRecvTools.h"
#include "client_hard_coded.h"
#pragma comment(lib, "Ws2_32.lib")

HANDLE g_wait_for_server_event;
SOCKET g_m_socket;
char* gp_recived_massge = NULL;
char* gp_massage_to_send = NULL;
BOOLEAN g_is_time_out = FALSE;
BOOLEAN g_is_game_end = FALSE;
static HANDLE g_send_semapore = NULL;
static HANDLE g_rcev_semapore = NULL;
char  g_user_name[MAX_USERNAME_LEN] = { 0 };
char g_other_player_username[MAX_USERNAME_LEN] = { 0 };
int g_other_player_num = 0;

char message_type[][MASSGAE_TYPE_MAX_LEN] = { "CLIENT_REQUEST","CLIENT_VERSUS","CLIENT_PLAYER_MOVE","CLIENT_DISCONNECT","SERVER_APPROVED","SERVER_DENIED","SERVER_MAIN_MENU","GAME_STARTED","TURN_SWITCH","SERVER_MOVE_REQUEST","GAME_ENDED","SERVER_NO_OPPONENTS","GAME_VIEW","SERVER_OPPONENT_OUT" };
char client_meaasge[][MASSGAE_TYPE_MAX_LEN] = { "CLIENT_REQUEST","CLIENT_VERSUS","CLIENT_PLAYER_MOVE","CLIENT_DISCONNECT" };
char server_massage[NUM_SERVER_TYPES][MASSGAE_TYPE_MAX_LEN] = { "SERVER_APPROVED","SERVER_DENIED","SERVER_MAIN_MENU","GAME_STARTED","TURN_SWITCH","SERVER_MOVE_REQUEST","GAME_ENDED","SERVER_NO_OPPONENTS","GAME_VIEW","SERVER_OPPONENT_OUT" };


int main(int argc, char* argv[])
{

	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERROR_CODE;
	}
	char* server_ip = argv[SERVER_IP_IND];
	int server_port = atoi(argv[SERVER_PORT_IND]);
	char user_name[MAX_USERNAME_LEN] = { 0 };
	strcpy(user_name, argv[USERNAME_IND]);
	while (g_is_time_out)
	{

	}
	
}


//Reading data coming from the server
static DWORD RecvDataThread(void)
{
	TransferResult_t RecvRes;

	while (1)
	{
		DWORD wait_code;
		BOOL ret_val;
		char* AcceptedStr = NULL;
		RecvRes = ReceiveString(&AcceptedStr, g_m_socket, g_wait_for_server_event);

		if (RecvRes == TRNS_FAILED)
		{
			printf("Socket error while trying to write data to socket\n");
			return 0x555;
		}
		else if (RecvRes == TRNS_DISCONNECTED)
		{
			printf("Server closed connection. Bye!\n");
			return 0x555;
		}
		else
		{
			if (NULL != gp_recived_massge)
			{
				free(gp_recived_massge);
			}
			gp_recived_massge = AcceptedStr;
			//release semaphore for next thred
			ret_val = ReleaseSemaphore(
				g_rcev_semapore,
				1, 		/* Signal that exactly one cell was emptied */
				&wait_code);
			if (FALSE == ret_val)
			{
				printf("Error when releasing\n");
				return ERROR_CODE;
			}
		}

	}

	return 0;
}

//Sending data to the server
static DWORD SendDataThread(void)
{
	TransferResult_t SendRes;
	DWORD wait_code;

	while (1)
	{
		wait_code = WaitForSingleObject(g_send_semapore, INFINITE);
		if (wait_code != WAIT_OBJECT_0)
		{
			printf("Error when waiting for semapore\n");
			return ERROR_CODE;
		}
		SendRes = SendString(gp_massage_to_send, g_m_socket);
		if (SendRes == TRNS_FAILED)
		{
			printf("Socket error while trying to write data to socket\n");
			return 0x555;
		}
		//free the client massage to send buffer
		free(gp_massage_to_send);
		gp_massage_to_send = NULL;
		wait_code = WaitForSingleObject(g_wait_for_server_event, MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
		//wait for the server respond
		if (WAIT_TIMEOUT == wait_code)
		{
			g_is_time_out = TRUE;
			printf("server is not rsponding error while trying to write data to socket\n");
			//TODO close graceful
		}
	}
}

void MainClient()
{
	SOCKADDR_IN clientService;
	HANDLE hThread[2];

	// Initialize Winsock.
	WSADATA wsaData; //Create a WSADATA object called wsaData.
	//The WSADATA structure contains information about the Windows Sockets implementation.

	//Call WSAStartup and check for errors.
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR)
		printf("Error at WSAStartup()\n");

	//Call the socket function and return its value to the m_socket variable. 
	// For this application, use the Internet address family, streaming sockets, and the TCP/IP protocol.

	// Create a socket.
	g_m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	if (g_m_socket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		WSACleanup();
		return;
	}
	/*
	 The parameters passed to the socket function can be changed for different implementations.
	 Error detection is a key part of successful networking code.
	 If the socket call fails, it returns INVALID_SOCKET.
	 The if statement in the previous code is used to catch any errors that may have occurred while creating
	 the socket. WSAGetLastError returns an error number associated with the last error that occurred.
	 */


	 //For a client to communicate on a network, it must connect to a server.
	 // Connect to a server.

	 //Create a sockaddr_in object clientService and set  values.
	clientService.sin_family = AF_INET;
	clientService.sin_addr.s_addr = inet_addr(SERVER_ADDRESS_STR); //Setting the IP address to connect to
	clientService.sin_port = htons(SERVER_PORT); //Setting the port to connect to.

	/*
		AF_INET is the Internet address family.
	*/


	// Call the connect function, passing the created socket and the sockaddr_in structure as parameters. 
	// Check for general errors.
	if (connect(g_m_socket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
		printf("Failed to connect.\n");
		WSACleanup();
		return;
	}

	// Send and receive data.
	/*
		In this code, two integers are used to keep track of the number of bytes that are sent and received.
		The send and recv functions both return an integer value of the number of bytes sent or received,
		respectively, or an error. Each function also takes the same parameters:
		the active socket, a char buffer, the number of bytes to send or receive, and any flags to use.

	*/

	hThread[0] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)SendDataThread,
		NULL,
		0,
		NULL
	);
	hThread[1] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)RecvDataThread,
		NULL,
		0,
		NULL
	);

	WaitForMultipleObjects(2, hThread, FALSE, INFINITE);

	TerminateThread(hThread[0], 0x555);
	TerminateThread(hThread[1], 0x555);

	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	closesocket(g_m_socket);

	WSACleanup();

	return;
}

void send_to_server(const char* Str, SOCKET m_socket)
{
	DWORD wait_res;
	TransferResult_t SendRes;
	SendRes = SendString(Str, m_socket);
	if (SendRes == TRNS_FAILED)
	{
		printf("Socket error while trying to write data to socket\n");
		return 0x555;
	}
	wait_res = WaitForSingleObject(g_wait_for_server_event, MAX_TIME_FOR_TIMEOUT*1000);//*1000 for secends
	//wait for the server respond
	if (WAIT_TIMEOUT == wait_res)
	{
		printf("server is not rsponding error while trying to write data to socket\n");
		//TODO close graceful
	}
}
void send_massge()
{
	message_type[0];


 }



void wait_for_server()
{
	/* Parameters for CreateEvent */
	static const char P_EVENT_NAME[] = "WAIT_FOR_7_BOOM_SERVER_Event";

	HANDLE wait_for_server_event;
	DWORD last_error;

	/* Get handle to event by name. If the event doesn't exist, create it */
	wait_for_server_event = CreateEvent(
		NULL, /* default security attributes */
		FALSE,        /* Auto-reset event */
		FALSE,      /* initial state is non-signaled */
		P_EVENT_NAME);         /* name */
	/* Check if succeeded and handle errors */

	last_error = GetLastError();
	if (ERROR_SUCCESS != last_error)
	{
		//TODO add error handel
	}
	/* If last_error is ERROR_SUCCESS, then it means that the event was created.
	   If last_error is ERROR_ALREADY_EXISTS, then it means that the event already exists */

	return wait_for_server_event;
}

void sync_with_srver()
{
	//create semapore
	g_send_semapore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - */
		1,		/* Maximum Count */
		NULL); /* un-named */
	if (NULL == g_send_semapore)
	{
		printf("error when Create Semaphore\n");
	}

	g_rcev_semapore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - */
		1,		/* Maximum Count */
		NULL); /* un-named */
	if (NULL == g_send_semapore)
	{
		printf("error when Create Semaphore\n");
	}




	//create event to track time out
		/* Parameters for CreateEvent */
	static const char P_EVENT_NAME[] = "WAIT_FOR_7_BOOM_SERVER_Event";
	DWORD last_error;

	/* Get handle to event by name. If the event doesn't exist, create it */
	g_wait_for_server_event = CreateEvent(
		NULL, /* default security attributes */
		FALSE,        /* Auto-reset event */
		FALSE,      /* initial state is non-signaled */
		P_EVENT_NAME);         /* name */
	/* Check if succeeded and handle errors */
	last_error = GetLastError();
	if (ERROR_SUCCESS != last_error)
	{
		//TODO add error handel
	}
}

void client_ux(int stage)
{
	char SendStr[256];
	printf("Choose what to do next:\n1. Play against another client\n2. Quit");
	gets_s(SendStr, sizeof(SendStr)); //Reading a string from the keyboard

	if (STRINGS_ARE_EQUAL(SendStr, "quit"))
		return 0x555; //"quit" signals an exit from the client side
	switch (stage)
	{
	case 2:
		printf("Server on <ip>:<port> denied the connection request\nChoose what to do next:\n1. Try to reconnect\n2. Exit\n");
		break;
	case 3:
		printf("Choose what to do next:\n1. Play against another client\n2. Quit\n");
		break;
	case 4:
		printf("Game is on!\n");
		break;
	case 5:
		if (STRINGS_ARE_EQUAL(g_other_player_username, g_user_name))
		{
			printf("Your turn!\n");
		}
		else
		{
			printf("%s's turn!\n", g_other_player_username);
		}		
		break;
	case 6:
		printf("Enter the next number or boom:\n");
		break;
	case 7:
		printf("%s won!\n", g_other_player_username);
		break;
	case 8:
		//TODO wait 30 sec and then main manue
		break;
	case 9:
		;
		char is_end_msg[END_MSG_MAX_LEN] = CONTINUE_GAME;
		if (g_is_game_end)
			strcpy(is_end_msg, END_GAME);
		if (BOOM_VLUE == g_other_player_num)
		{
			printf("%s move was %s %s\n", g_other_player_username,BOOM_TEXT, is_end_msg);
		}
		else
		{
			printf("%s move was %d %s\n", g_other_player_username, g_other_player_num, is_end_msg);
		}
		break;
	case 10:
		printf("Opponent quit .\n");
		break;
	}
	
	
	printf("Error:Illegal command\n");





}

char* readLine()
{
	/// <summary>
	/// the funion will read char with unknown length
	/// </summary>
	/// <returns>return the string we wrote</returns>
	int index = 0, c, capacity = 1;
	char* buffer;

	if (NULL == (buffer = (char*)malloc(capacity)))
	{
		return NULL;
	}

	for (c = getchar(); c != '\n'; c = getchar())
	{
		if (index == capacity - 1)
		{
			if (NULL == (buffer = (char*)realloc(buffer, capacity + 1)))
				return NULL;
			capacity += 1;
		}
		buffer[index++] = c;
	}
	buffer[index] = '\0';
	return buffer;
}

void get_the_command(char* p_command)
{
	/// <summary>
	/// the code will get the input of the player and will send it to the game
	/// </summary>
	BOOL break_status = FALSE;
	while (break_status == 0)
	{
		printf("Enter the next number or boom:");
		p_command = readLine();
		int alphabet = 0, i;
		for (i = 0; p_command[i] != '\0'; i++)
		{

			// check for alphabets
			if (isalpha(p_command) != 0)
				alphabet++;
		}
		if (p_command == NULL)
		{
			printf("memory alloction fail.\n");
			return ERROR_CODE;
		}
		break_status = TRUE;
		if (((atoi(p_command) == 0) || alphabet != 0) && (strcmp(p_command, "boom") != 0))
		{
			printf("Error:Illegal command\n");
			break_status = FALSE;
		}
	}

	return 0;
}

int read_massage_from_server(char* Str)
{
	int index = 0;
	int type = 0;
	char massage_type[MASSGAE_TYPE_MAX_LEN] = { 0 };
	//if(strlen(Str)) TODO is can be not str?
	while (Str[index] != MASSGAE_TYPE_SEPARATE)
	{	
		massage_type[index] = Str[index];
		index++;
	}
	massage_type[index] = '\0';
	//TODO to catlog massge
	for (int i = 0; i < NUM_SERVER_TYPES; i++)
	{
		if (STRINGS_ARE_EQUAL(server_massage[i], massage_type))
		{
			type = i;
		}
	}
	if (0 == type)
	{
		//TODO not good massge
	}
	while (Str[index]!= MASSGAE_END)
	{
		// has name parmeter
		if (4 == type || 6 == type|| 8 == type)//TURN_SWITCH/GAME_ENDED/GAME_VIEW
		{
			int name_ind = 0;
			while (Str[index] != PRAMS_SEPARATE && Str[index] != MASSGAE_END)
			{
				g_other_player_username[name_ind] = Str[index];
				index++;
				name_ind++;
			}
			g_other_player_username[name_ind] = 0;
		}
		if (8 == type)//GAME_VIEW
		{
			index++;
			int start_ind = index;
			BOOL is_boom = TRUE;
			//check if boom
			for (int i = 0; i < strlen(BOOM_TEXT); i++)
			{
				if (BOOM_TEXT[i] != Str[index])
				{
					is_boom = FALSE;
					break;
				}
				index++;
			}
			if (is_boom)
			{
				//TODO handle boom 
				g_other_player_num = BOOM_VLUE;
			}
			else
			{

				while (Str[index] != PRAMS_SEPARATE && Str[index] != MASSGAE_END)
				{
					index++;
				}
				Str[index] = '\0';
				g_other_player_num = atoi(Str + start_ind);//human readable

			}
			//is end
			char is_end_msg[END_MSG_MAX_LEN] = { 0 };
			int msg_ind = 0;
			while (Str[index] != MASSGAE_END)
			{
				is_end_msg[msg_ind]= Str[index];
				index++;
				msg_ind++;
			}
			Str[index] = '\0';
			g_is_game_end = STRINGS_ARE_EQUAL(is_end_msg, END_GAME);
			
			}
		index++;
	}
	return type;
}

//		gets_s(SendStr, sizeof(SendStr)); //Reading a string from the keyboard
//		if (STRINGS_ARE_EQUAL(SendStr, "quit"))
//return 0x555; //"quit" signals an exit from the client side

