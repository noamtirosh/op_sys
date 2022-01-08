
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
#include "client_hard_coded.h"
#pragma comment(lib, "Ws2_32.lib")

typedef struct message_cell_s {
	struct message_cell_s* p_next;
	char* p_message;
	int  wait_for_response;
}message_cell_t;

HANDLE g_wait_for_server_event;
SOCKET g_main_socket;
char* g_server_adress = NULL;
int g_server_port = 0;
BOOLEAN g_is_time_out_arr = FALSE;
BOOLEAN g_is_game_end = FALSE;
static HANDLE g_hads_mutex = NULL;
message_cell_t* gp_recived_massge_hade =  NULL;
message_cell_t* gp_massage_to_send_hade = NULL;
static HANDLE g_send_semapore = NULL;
static HANDLE g_rcev_semapore = NULL;
BOOL g_game_on = FALSE;
char  g_user_name[MAX_USERNAME_LEN] = { 0 };
char g_log_file_name[MAX_USERNAME_LEN + LOG_FILE_NAME_BASE_LEN] = { 0 };
char g_server_input_username[MAX_USERNAME_LEN] = { 0 };
int g_other_player_num = 0;
enum client_types { CLIENT_REQUEST, CLIENT_VERSUS, CLIENT_PLAYER_MOVE, CLIENT_DISCONNECT };
enum server_types { SERVER_APPROVED, SERVER_DENIED, SERVER_MAIN_MENU, GAME_STARTED, TURN_SWITCH, SERVER_MOVE_REQUEST, GAME_ENDED, SERVER_NO_OPPONENTS, GAME_VIEW, SERVER_OPPONENT_OUT };
char message_type[][MASSGAE_TYPE_MAX_LEN] = { "CLIENT_REQUEST","CLIENT_VERSUS","CLIENT_PLAYER_MOVE","CLIENT_DISCONNECT","SERVER_APPROVED","SERVER_DENIED","SERVER_MAIN_MENU","GAME_STARTED","TURN_SWITCH","SERVER_MOVE_REQUEST","GAME_ENDED","SERVER_NO_OPPONENTS","GAME_VIEW","SERVER_OPPONENT_OUT" };
char client_meaasge[][MASSGAE_TYPE_MAX_LEN] = { "CLIENT_REQUEST","CLIENT_VERSUS","CLIENT_PLAYER_MOVE","CLIENT_DISCONNECT" };
char server_massage[NUM_SERVER_TYPES][MASSGAE_TYPE_MAX_LEN] = { "SERVER_APPROVED","SERVER_DENIED","SERVER_MAIN_MENU","GAME_STARTED","TURN_SWITCH","SERVER_MOVE_REQUEST","GAME_ENDED","","GAME_VIEW","SERVER_OPPONENT_OUT" };

//function decleration
void send_to_server(const char* Str, SOCKET m_socket);
message_cell_t* add_message_to_cell_arr(char* p_message, int message_len, message_cell_t* p_cell_head, int wait_for_response);
void MainClient();
void sync_with_srver();
char* readLine();
int user_choose_option();
int create_log_file();
int write_to_log(const char* p_start_message_str, char* p_buffer, const DWORD buffer_len);
DWORD write_to_file(LPCSTR p_file_name, LPVOID p_buffer, const DWORD buffer_len);
HANDLE create_file_simple(LPCSTR p_file_name, char mode);
void client_response();

int main(int argc, char* argv[])
{
	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERROR_CODE;
	}
	g_server_adress = argv[SERVER_IP_IND];
	g_server_port = atoi(argv[SERVER_PORT_IND]);
	strcpy(g_user_name, argv[USERNAME_IND]);
	create_log_file();
	sync_with_srver();
	MainClient();
	
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
		RecvRes = ReceiveString(&AcceptedStr, g_main_socket, g_wait_for_server_event);

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
			wait_code = WaitForSingleObject(g_hads_mutex, INFINITE);
			if (WAIT_OBJECT_0 != wait_code)
			{
				printf("Error when waiting for mutex\n");
				return ERROR_CODE;
			}
			gp_recived_massge_hade = add_message_to_cell_arr(AcceptedStr, get_message_len(AcceptedStr), gp_recived_massge_hade, FALSE);
			ret_val = ReleaseMutex(g_hads_mutex);
			if (FALSE == ret_val)
			{
				printf("unable to release mutex\n");
				return ERROR_CODE;
			}
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
	BOOL ret_val;
	while (1)
	{
		wait_code = WaitForSingleObject(g_send_semapore, INFINITE);
		if (wait_code != WAIT_OBJECT_0)
		{
			printf("Error when waiting for semapore\n");
			return ERROR_CODE;
		}
		//SendRes =
		wait_code = WaitForSingleObject(g_hads_mutex, INFINITE);
		if (WAIT_OBJECT_0 != wait_code)
		{
			printf("Error when waiting for mutex\n");
			return ERROR_CODE;
		}
		char* p_massage_to_send = gp_massage_to_send_hade->p_message;
		int wait_for_response = gp_massage_to_send_hade->wait_for_response;
		if (wait_for_response)
			//reset client_event
			wait_code = WaitForSingleObject(g_wait_for_server_event, 0);// dwmillisecends is 0 so if the g_wait_for_client_event is signaled it reset it 
		SendRes = SendString(p_massage_to_send, g_main_socket);
		//free the client massage to send buffer
		free(p_massage_to_send);
		//remove message from linked list
		message_cell_t* p_temp_cell = gp_massage_to_send_hade->p_next;
		free(gp_massage_to_send_hade);
		gp_massage_to_send_hade = p_temp_cell;
		ret_val = ReleaseMutex(g_hads_mutex);
		if (FALSE == ret_val)
		{
			printf("unable to release mutex\n");
			return ERROR_CODE;
		}
		//TODO choose one or marge
		if (SendRes == TRNS_FAILED)
		{
			printf("Socket error while trying to write data to socket\n");
			return 0x555;
		}
		if (SendRes == TRNS_FAILED)
		{
			printf("Service socket error while writing, closing thread.\n");
			closesocket(g_main_socket);
			return 1;
		}
		if (wait_for_response)
		{
			wait_code = WaitForSingleObject(g_wait_for_server_event, wait_for_response * 1000);//*1000 for secends
			//wait for the server respond
			if (WAIT_TIMEOUT == wait_code)
			{
				g_is_time_out_arr = TRUE;
				printf("server is not rsponding error while trying to write data to socket\n");
				//TODO close graceful
			}
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
	g_main_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	if (g_main_socket == INVALID_SOCKET) {
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
	clientService.sin_addr.s_addr = inet_addr(g_server_adress); //Setting the IP address to connect to
	clientService.sin_port = htons(g_server_port); //Setting the port to connect to.

	/*
		AF_INET is the Internet address family.
	*/


	// Call the connect function, passing the created socket and the sockaddr_in structure as parameters. 
	// Check for general errors.
	BOOL try_to_connect = TRUE;
	while (try_to_connect)
	{
		if (connect(g_main_socket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR)
		{
			int erorr_message_len = 4+ strlen(g_server_adress) + max(log10(g_server_port), 0)+1;//:+.\n+\0+
			char* erorr_message = (char*)malloc(sizeof(char) * erorr_message_len);
			sprintf_s(erorr_message, erorr_message_len, "%s:%d.\n", g_server_adress, g_server_port);
			printf(erorr_message);
			write_to_log("Failed connecting to server on ", erorr_message, strlen(erorr_message));
			free(erorr_message);
			printf("Choose what to do next:\n1. Try to reconnect\n2. Exit\n");
			int user_input = user_choose_option();
			if (1==user_input)
			{
				try_to_connect = TRUE;
				//Try to reconnect

			}
			else if (2 == user_input)
			{
				//Exit
				WSACleanup();
				return;
			}
			else if(BAD_ALOC ==  user_input)
			{
				//TODO close resorce

			}

		}
		else
		{
			try_to_connect = FALSE;
			printf("Connected to server on %s:%d\n", g_server_adress, g_server_port);
		}

	}



	hThread[0] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)SendDataThread,
		NULL,
		0,
		NULL
	);
	//TODO handel erorr
	if (NULL == hThread[0])
	{
		printf("Couldn't create thread\n");
		//free_resource(pg_frame_queue_head, pg_thread_queue_head);
		exit(ERROR_CODE);
	}
	hThread[1] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)RecvDataThread,
		NULL,
		0,
		NULL
	);
	//TODO handel erorr
	if (NULL == hThread[1])
	{
		printf("Couldn't create thread\n");
		//free_resource(pg_frame_queue_head, pg_thread_queue_head);
		exit(ERROR_CODE);
	}
	//send client request
	char message[MASSGAE_TYPE_MAX_LEN+ MAX_USERNAME_LEN+2] = { 0 };//message type + user_name + \n + :
	sprintf_s(message, MASSGAE_TYPE_MAX_LEN + MAX_USERNAME_LEN + 2, "%s%c%s%c", client_meaasge[CLIENT_REQUEST], MASSGAE_TYPE_SEPARATE, g_user_name, MASSGAE_END);
	g_game_on = TRUE;
	//send requset to server
	send_message_to_server(message, get_message_len(message), 2 * MAX_TIME_FOR_TIMEOUT);
	client_response();

	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

	TerminateThread(hThread[0], 0x555);
	TerminateThread(hThread[1], 0x555);

	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	closesocket(g_main_socket);

	WSACleanup();

	return;
}

void send_to_server(const char* Str, SOCKET m_socket)
{
	DWORD wait_res;
	TransferResult_t SendRes;
	//reset_server_event
	wait_res = WaitForSingleObject(g_wait_for_server_event, 0);
	SendRes = SendString(Str, m_socket);
	if (SendRes == TRNS_FAILED)
	{
		printf("Socket error while trying to write data to socket\n");
		return 0x555;
	}
	//TODO if CLIENT_VERSUS wait duble time
	wait_res = WaitForSingleObject(g_wait_for_server_event, MAX_TIME_FOR_TIMEOUT*1000);//*1000 for secends
	//wait for the server respond
	if (WAIT_TIMEOUT == wait_res)
	{
		g_is_time_out_arr = TRUE;
		printf("server is not rsponding error while trying to write data to socket\n");
		//TODO close graceful
	}
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
		MAX_MESSAGES_IN_BUF,		/* Maximum Count */
		NULL); /* un-named */
	if (NULL == g_send_semapore)
	{
		printf("error when Create Semaphore\n");
	}

	g_rcev_semapore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - */
		MAX_MESSAGES_IN_BUF,		/* Maximum Count */
		NULL); /* un-named */
	if (NULL == g_send_semapore)
	{
		printf("error when Create Semaphore\n");
	}

	g_hads_mutex = CreateMutex(
		NULL,	/* default security attributes */
		FALSE,	/* initially not owned */
		NULL);	/* unnamed mutex */
	if (NULL == g_hads_mutex)
	{
		printf("error when Create Semaphore\n");
		//TODO add error handel
		return ERROR_CODE;
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

void client_interface(int stage)
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
		if (STRINGS_ARE_EQUAL(g_server_input_username, g_user_name))
		{
			printf("Your turn!\n");
		}
		else
		{
			printf("%s's turn!\n", g_server_input_username);
		}		
		break;
	case 6:
		printf("Enter the next number or boom:\n");
		break;
	case 7:
		printf("%s won!\n", g_server_input_username);
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
			printf("%s move was %s %s\n", g_server_input_username,BOOM_TEXT, is_end_msg);
		}
		else
		{
			printf("%s move was %d %s\n", g_server_input_username, g_other_player_num, is_end_msg);
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
	int index = 0, c, capacity = USER_INPUT_BASE_SIZE;
	char* buffer;

	if (NULL == (buffer = (char*)malloc(capacity)))
	{
		//TODO handle erorr
		return NULL;
	}

	for (c = getchar(); c != '\n'; c = getchar())
	{
		if (index == capacity - 1)
		{
			char* resize_buffer;
			if (NULL == (resize_buffer = (char*)realloc(buffer, capacity + USER_INPUT_BASE_SIZE)))
			{
				if (buffer != NULL)
					free(buffer);
				return NULL;
			}
			buffer = resize_buffer;
			capacity += USER_INPUT_BASE_SIZE;
		}
		buffer[index++] = c;
	}
	buffer[index] = '\0';
	return buffer;
}

void get_user_command(char* p_command)
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
	while (Str[index] != MASSGAE_TYPE_SEPARATE && Str[index] != MASSGAE_END)
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
			break;
		}
	}
	if (NUM_SERVER_TYPES == type)
	{
		//TODO not good massge
	}
	while (Str[index]!= MASSGAE_END)
	{
		// has name parmeter
		if (TURN_SWITCH == type || GAME_ENDED == type|| GAME_VIEW == type)
		{
			index++;
			int name_ind = 0;
			while (Str[index] != PRAMS_SEPARATE && Str[index] != MASSGAE_END)
			{
				g_server_input_username[name_ind] = Str[index];
				index++;
				name_ind++;
			}
			//TODO is the username includes \0
			g_server_input_username[name_ind] = '\0';
		}
		if (GAME_VIEW == type)
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
				char temp_char = Str[index];
				Str[index] = '\0';
				g_other_player_num = atoi(Str + start_ind);//human readable
				Str[index] = temp_char;

			}
			//is end
			index++;
			char is_end_msg[END_MSG_MAX_LEN] = { 0 };
			int msg_ind = 0;
			while (Str[index] != MASSGAE_END)
			{
				is_end_msg[msg_ind]= Str[index];
				index++;
				msg_ind++;
			}
			is_end_msg[msg_ind]= '\0';
			g_is_game_end = STRINGS_ARE_EQUAL(is_end_msg, END_GAME);
			
			}
	}
	return type;
}


void client_response()
{
	DWORD wait_code;
	BOOL ret_val;
	while (g_game_on)
	{
		wait_code = WaitForSingleObject(g_rcev_semapore, INFINITE);
		if (wait_code != WAIT_OBJECT_0)
		{
			printf("Error when waiting for semapore\n");
			return ERROR_CODE;
		}
		wait_code = WaitForSingleObject(g_hads_mutex, INFINITE);
		if (WAIT_OBJECT_0 != wait_code)
		{
			printf("Error when waiting for mutex\n");
			return ERROR_CODE;
		}
		char* p_massage_from_server = gp_recived_massge_hade->p_message;
		write_to_log(LOG_REC, p_massage_from_server, get_message_len(p_massage_from_server));
		int message_type = read_massage_from_server(p_massage_from_server);
		//free the client massage to send buffer
		free(p_massage_from_server);
		//remove message from linked list
		message_cell_t* p_temp_cell = gp_recived_massge_hade->p_next;
		free(gp_recived_massge_hade);
		gp_recived_massge_hade = p_temp_cell;
		ret_val = ReleaseMutex(g_hads_mutex);
		if (FALSE == ret_val)
		{
			printf("unable to release mutex\n");
			return ERROR_CODE;
		}
		char* p_user_input = NULL;
		if (SERVER_APPROVED == message_type)
		{

		}
		else if (SERVER_DENIED == message_type)
		{
			printf("Server on <ip>:<port> denied the connection request\nChoose what to do next:\n1. Try to reconnect\n2. Exit\n");
			int user_input = user_choose_option();
			if (1 == user_input)
			{

			}
			else if (2 == user_input)
			{
			}

		}
		else if (SERVER_MAIN_MENU == message_type)
		{
			printf("Choose what to do next:\n1. Play against another client\n2. Quit\n");
			int user_input = user_choose_option();
			if (1 == user_input)
			{
				char message[MASSGAE_TYPE_MAX_LEN] = { 0 };//message type + \n
				sprintf_s(message, MASSGAE_TYPE_MAX_LEN, "%s%c", client_meaasge[CLIENT_VERSUS], MASSGAE_END);
				//send to server
				send_message_to_server(message, get_message_len(message), MAX_TIME_FOR_TIMEOUT);
			}
			else if (2 == user_input)
			{
			}
		}
		else if (GAME_STARTED == message_type)
		{
			printf("Game is on!\n");
		}
		else if (TURN_SWITCH == message_type)
		{
			if (STRINGS_ARE_EQUAL(g_server_input_username, g_user_name))
			{
				printf("Your turn!\n");
			}
			else
			{
				printf("%s's turn!\n", g_server_input_username);
			}
		}
		else if (SERVER_MOVE_REQUEST == message_type)
		{
			printf("Enter the next number or boom:\n");
			char* p_user_input = NULL;
			p_user_input = readLine();
			//message type + \n + MASSGAE_TYPE_SEPARATE
			int message_len = MASSGAE_TYPE_MAX_LEN + strlen(p_user_input)+2;//TODO if input as /0 in it need to change
			char *message= (char*)malloc(sizeof(char)*message_len);
			if (NULL == message)
			{
				//TODO add erorr
				return ERROR_CODE;

			}
			sprintf_s(message, message_len, "%s%c%s%c", client_meaasge[CLIENT_PLAYER_MOVE], MASSGAE_TYPE_SEPARATE, p_user_input, MASSGAE_END);
			free(p_user_input);
			//send to server
			send_message_to_server(message, get_message_len(message), MAX_TIME_FOR_TIMEOUT);
			free(message);

		}
		else if (GAME_ENDED == message_type)
		{
			printf("%s won!\n", g_server_input_username);
		}
		else if (GAME_VIEW == message_type)
		{
			char is_end_msg[END_MSG_MAX_LEN] = CONTINUE_GAME;
			if (g_is_game_end)
				strcpy(is_end_msg, END_GAME);
			if (BOOM_VLUE == g_other_player_num)
			{
				printf("%s move was %s %s\n", g_server_input_username, BOOM_TEXT, is_end_msg);
			}
			else
			{
				printf("%s move was %d %s\n", g_server_input_username, g_other_player_num, is_end_msg);
			}
		}
		else if (SERVER_OPPONENT_OUT == message_type)
		{
			printf("Opponent quit .\n");
		}
		else
		{

		}
	}
}

int send_message_to_server( char* p_message, int msg_len, int wait_for_response)
{
	DWORD wait_res;
	BOOL ret_val;
	//write message to log
	write_to_log(LOG_SENT, p_message, msg_len);
	wait_res = WaitForSingleObject(g_hads_mutex, INFINITE);
	if (WAIT_OBJECT_0 != wait_res)
	{
		printf("Error when waiting for mutex\n");
		return ERROR_CODE;
	}
	message_cell_t *p_new_head = add_message_to_cell_arr(p_message, msg_len, gp_massage_to_send_hade, wait_for_response);
	if (NULL == p_new_head)
	{
		return ERROR_CODE;
	}
	gp_massage_to_send_hade = p_new_head;
	ret_val = ReleaseMutex(g_hads_mutex);
	if (FALSE == ret_val)
	{
		printf("unable to release mutex\n");
		return ERROR_CODE;
	}
	ret_val = ReleaseSemaphore(
		g_send_semapore,
		1, 		/* Signal that exactly one cell was emptied */
		&wait_res);
	if (FALSE == ret_val)
	{
		printf("Error when releasing\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}

message_cell_t* add_message_to_cell_arr(char* p_message,int message_len ,message_cell_t* p_cell_head, int wait_for_response)
{
	/// <summary>
	/// 
	/// </summary>
	/// <param name="p_str"></param>
	/// <param name="p_cell_head"></param>
	/// <param name="wait_for_response"></param>
	/// <returns></returns>
	message_cell_t* p_temp_cell = p_cell_head;
	message_cell_t* p_new_cell = (message_cell_t*)malloc(sizeof(message_cell_t));
	if (NULL == p_new_cell)
	{
		printf("unable to alocate memory for message_cell\n");
		return NULL;
	}
	p_new_cell->wait_for_response = wait_for_response;
	p_new_cell->p_next = NULL;
	char* p_temp_buf = (char*)malloc(sizeof(char) * message_len);
	if (NULL == p_temp_buf)
	{
		free(p_new_cell);
		printf("unable to alocate memory for message_cell\n");
		return NULL;
	}
	//copy message
	for (int ind = 0; ind < message_len; ind++)
	{
		p_temp_buf[ind] = p_message[ind];
	}
	p_new_cell->p_message = p_temp_buf;

	if (NULL == p_temp_cell)
	{
		return p_new_cell;
	}
	else
	{
		while (NULL != p_temp_cell->p_next)
		{
			p_temp_cell = p_temp_cell->p_next;
		}
		p_temp_cell->p_next = p_new_cell;
		return p_cell_head;
	}
}


//		gets_s(SendStr, sizeof(SendStr)); //Reading a string from the keyboard
//		if (STRINGS_ARE_EQUAL(SendStr, "quit"))
//return 0x555; //"quit" signals an exit from the client side

int user_choose_option()
{
	char* p_user_input = NULL;
	while (TRUE)
	{
		p_user_input = readLine();
		if (NULL == p_user_input)
		{
			//unable to alocate memory for user input
			return BAD_ALOC;
		}
		if (STRINGS_ARE_EQUAL(p_user_input, "1"))
		{
			free(p_user_input);
			return 1;
		}
		else if (STRINGS_ARE_EQUAL(p_user_input, "2"))
		{
			free(p_user_input);
			return 2;
		}
		else
		{
			free(p_user_input);
			printf("Error:Illegal command\n");
		}

	}
}
int create_log_file()
{
	sprintf_s(g_log_file_name, MAX_USERNAME_LEN+ LOG_FILE_NAME_BASE_LEN, "Client_log_%s.txt", g_user_name);
	//create empty log file, overwrite if exist
	HANDLE h_file = create_file_simple(g_log_file_name, 'd');
	if (INVALID_HANDLE_VALUE == h_file)
	{
		printf("unable to crate log file: %s\n", g_log_file_name);
		return ERROR_CODE;
	}
	CloseHandle(h_file);
	

}
int write_to_log(const char *p_start_message_str,char *p_buffer, const DWORD buffer_len)
{
	int num_byte_written = 0;
	num_byte_written = write_to_file(g_log_file_name, p_start_message_str, strlen(p_start_message_str));
	if (strlen(p_start_message_str) != num_byte_written)
	{
		printf("problem when try to write to log file the line %s%s\n", p_start_message_str, p_buffer);
		return ERROR_CODE;
	}
	num_byte_written = write_to_file(g_log_file_name, p_buffer, buffer_len);
	if (buffer_len != num_byte_written)
	{
		printf("problem when try to write to log file the line %s%s\n", p_start_message_str, p_buffer);
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}

DWORD write_to_file(LPCSTR p_file_name, LPVOID p_buffer, const DWORD buffer_len)
{
	/// <summary>
	///  write message from p_buffer in len buffer_len to file 
	/// </summary>
	/// <param name="file_name">path to file to write the buffer</param>
	/// <param name="p_buffer">pointer to buffer</param>
	/// <param name="buffer_len"> the length of the buffer</param>
	/// <returns> num of bytes written to file</returns>
	HANDLE h_file = create_file_simple(p_file_name, 'w');
	DWORD last_error;
	DWORD n_written = 0;
	if (INVALID_HANDLE_VALUE == h_file)
	{
		return n_written;
	}
	//move file pointer in ofset
	SetFilePointer(
		h_file,	//handle to file
		0, // number of bytes to move the file pointer
		NULL, // 
		FILE_END);
	if (FALSE == WriteFile(h_file,
		p_buffer,
		buffer_len,
		&n_written, NULL))
	{
		last_error = GetLastError();
		printf("Unable to write to file, error: %ld\n", last_error);
	}
	CloseHandle(h_file);
	return n_written;
}
HANDLE create_file_simple(LPCSTR p_file_name, char mode)
{
	/// <summary>
	/// use CreateFile with default params for read or write and return the handle
	/// </summary>
	/// <param name="file_name">file path</param>
	/// <param name="mode">mode : 'r'/'R' for read acsses 'w'/'W' for write </param>
	/// <returns>handle to file</returns>
	HANDLE h_file;
	DWORD last_error;
	DWORD acsees;
	DWORD share;
	DWORD creation_disposition;
	switch (mode)
	{
	case 'r': case 'R':
		acsees = GENERIC_READ;  // open for reading
		share = FILE_SHARE_READ; // share for reading
		creation_disposition = OPEN_EXISTING; // existing file only
		break;
	case 'w':case'W':
		acsees = GENERIC_WRITE;
		share = FILE_SHARE_WRITE;
		creation_disposition = OPEN_ALWAYS; // open if existe or create new if not
		break;
	case 'd':case'D':
		acsees = GENERIC_WRITE;
		share = FILE_SHARE_WRITE;
		creation_disposition = CREATE_ALWAYS; // overwrite if existe or create new if not 
		break;
	default:
		acsees = GENERIC_READ;  // open for reading
		share = FILE_SHARE_READ; // share for reading
		creation_disposition = OPEN_EXISTING;// existing file only
	};
	h_file = CreateFile(p_file_name,  // file to open
		acsees,
		share,
		NULL,                  // default security
		creation_disposition,
		FILE_ATTRIBUTE_NORMAL, // normal file
		NULL);
	if (h_file == INVALID_HANDLE_VALUE)
	{
		printf("Unable to open file %s\n", p_file_name);
	}
	last_error = GetLastError();
	if (last_error == ERROR_FILE_NOT_FOUND)
	{
		printf("did not find file %s\n", p_file_name);
	}
	return h_file;
}