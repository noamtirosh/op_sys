
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

HANDLE g_wait_for_server_event =NULL;
SOCKET g_main_socket = INVALID_SOCKET;
HANDLE g_socket_thread[NUM_THREADS_FOR_CLIENT] = { NULL };

char* g_server_adress = NULL;
int g_server_port = 0;
BOOLEAN g_is_game_end = FALSE;
BOOLEAN is_trans_failed = FALSE;
BOOLEAN g_is_thread_avilibale[NUM_THREADS_FOR_CLIENT] = { FALSE };
BOOL g_start_graceful_shutdown = { FALSE };


static HANDLE g_hads_mutex = NULL;
message_cell_t* gp_recived_massge_hade =  NULL;
message_cell_t* gp_massage_to_send_hade = NULL;
static HANDLE g_send_semapore = NULL;
static HANDLE g_rcev_semapore = NULL;

BOOL g_game_on = FALSE;
BOOL g_connect_server = TRUE;

char  g_user_name[MAX_USERNAME_LEN] = { 0 };
char g_log_file_name[MAX_USERNAME_LEN + LOG_FILE_NAME_BASE_LEN] = { 0 };
char g_server_input_username[MAX_USERNAME_LEN] = { 0 };
int g_other_player_num = 0;
enum client_types { CLIENT_REQUEST, CLIENT_VERSUS, CLIENT_PLAYER_MOVE, CLIENT_DISCONNECT };
enum server_types { SERVER_APPROVED, SERVER_DENIED, SERVER_MAIN_MENU, GAME_STARTED, TURN_SWITCH, SERVER_MOVE_REQUEST, GAME_ENDED, SERVER_NO_OPPONENTS, GAME_VIEW, SERVER_OPPONENT_OUT, ERROR_TYPE };
enum release_stage { NOT_NEED, RELEASE_SYNC, RELEASE_SOCKET, RELEASE_THREADS, RELEASE_LINK_LIST ,RELEASE_ALL,AFTER_GRACEFUL,DENIED_BY_SERVER};

char message_type[][MASSGAE_TYPE_MAX_LEN] = { "CLIENT_REQUEST","CLIENT_VERSUS","CLIENT_PLAYER_MOVE","CLIENT_DISCONNECT","SERVER_APPROVED","SERVER_DENIED","SERVER_MAIN_MENU","GAME_STARTED","TURN_SWITCH","SERVER_MOVE_REQUEST","GAME_ENDED","SERVER_NO_OPPONENTS","GAME_VIEW","SERVER_OPPONENT_OUT" };
char client_meaasge[][MASSGAE_TYPE_MAX_LEN] = { "CLIENT_REQUEST","CLIENT_VERSUS","CLIENT_PLAYER_MOVE","CLIENT_DISCONNECT" };
char server_massage[NUM_SERVER_TYPES][MASSGAE_TYPE_MAX_LEN] = { "SERVER_APPROVED","SERVER_DENIED","SERVER_MAIN_MENU","GAME_STARTED","TURN_SWITCH","SERVER_MOVE_REQUEST","GAME_ENDED","SERVER_NO_OPPONENTS","GAME_VIEW","SERVER_OPPONENT_OUT" };
int g_release_sorce_stage = 0;


//function decleration
message_cell_t* add_message_to_cell_arr(char* p_message, int message_len, message_cell_t* p_cell_head, int wait_for_response);
int init_sync_objects();
char* readLine();
int user_choose_option();
int create_log_file();
int write_to_log(const char* p_start_message_str, char* p_buffer, const DWORD buffer_len);
DWORD write_to_file(LPCSTR p_file_name, LPVOID p_buffer, const DWORD buffer_len);
HANDLE create_file_simple(LPCSTR p_file_name, char mode);
int client_response();
int connect_to_server();
int close_sync_object();
int init_threads();
int send_first_request_to_server();
int close_socket();
int graceful_shutdown();

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
	if (create_log_file())
	{
		return ERROR_CODE;
	}
	if (init_sync_objects())
	{
		release_source(RELEASE_SYNC);
		return ERROR_CODE;
	}
	while (g_connect_server)
	{

		if (connect_to_server())
		{
			release_source(RELEASE_SOCKET);
			return ERROR_CODE;
		}
		if (init_threads())
		{
			release_source(RELEASE_THREADS);
			return ERROR_CODE;
		}
		if (send_first_request_to_server())
		{
			release_source(RELEASE_LINK_LIST);
			return ERROR_CODE;
		}
		if (client_response())
		{
			release_source(RELEASE_ALL);
			return ERROR_CODE;
		}
	}
	release_source(RELEASE_ALL);
	return SUCCESS_CODE;
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
		if (!g_is_thread_avilibale[REC_THREAD])
			return SUCCESS_CODE;
		if (RecvRes == TRNS_FAILED)
		{
			printf("Server disconnected. Exiting.");
			if(NULL != AcceptedStr)
				free(AcceptedStr);
			ret_val = ReleaseSemaphore(
				g_rcev_semapore,
				1, 		/* Signal that exactly one cell was emptied */
				&wait_code);
			if (FALSE == ret_val)
			{
				printf("Error when releasing\n");
				exit(ERROR_CODE);
			}
			is_trans_failed = TRUE;
			return ERROR_CODE;
		}
		else if (RecvRes == TRNS_DISCONNECTED)
		{
			if (!g_start_graceful_shutdown)
			{
				if (NULL != AcceptedStr)
					free(AcceptedStr);
				g_is_thread_avilibale[REC_THREAD] = FALSE;
				//TODO send final transmssion
				shutdown(g_main_socket, SD_SEND);//will stop sending info
			}
			if (close_socket(g_main_socket))
				return ERROR_CODE;
			//TODO do we need to print someting?
			//TODO do i need to close other thread now?
			g_is_thread_avilibale[SEND_THREAD] = FALSE;
			return SUCCESS_CODE;
		}
		else
		{
			wait_code = WaitForSingleObject(g_hads_mutex, INFINITE);
			if (WAIT_OBJECT_0 != wait_code)
			{
				printf("Error when waiting for mutex\n");
				if (NULL != AcceptedStr)
					free(AcceptedStr);
				return ERROR_CODE;
			}
			gp_recived_massge_hade = add_message_to_cell_arr(AcceptedStr, get_message_len(AcceptedStr), gp_recived_massge_hade, FALSE);
			if (NULL != AcceptedStr)
				free(AcceptedStr);
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

	return SUCCESS_CODE;
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
		if (!g_is_thread_avilibale[SEND_THREAD])
			return SUCCESS_CODE;
		wait_code = WaitForSingleObject(g_hads_mutex, INFINITE);
		if (WAIT_OBJECT_0 != wait_code)
		{
			printf("Error when waiting for mutex\n");
			return ERROR_CODE;
		}
		if (NULL == gp_massage_to_send_hade && g_start_graceful_shutdown)
		{
			if (!ReleaseMutex(g_hads_mutex))
			{
				printf("unable to release mutex\n");
				return ERROR_CODE;
			}
			//try graceful_shutdown 
			shutdown(g_main_socket, SD_SEND);//will stop sending info
			wait_code = WaitForSingleObject(g_socket_thread[REC_THREAD], MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
			//wait for client  shutdown response
			if (WAIT_OBJECT_0 != wait_code)
			{
				printf("client is not rsponding to graceful_shutdown\n");
				return ERROR_CODE;
			}
			else
			{
				DWORD exit_code;
				if (!GetExitCodeThread(g_socket_thread[REC_THREAD], &exit_code))
				{
					printf("problem when try to get exit code\n");
					return ERROR_CODE;
				}
				if (SUCCESS_CODE == exit_code)
				{
					//TODO do we need to print someting?
					return SUCCESS_CODE;
				}
				else
				{
					printf("rec thread didnt close socket\n");
					return ERROR_CODE;
				}
			}
		}
		if (NULL == gp_massage_to_send_hade)
		{
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
		if (SendRes == TRNS_FAILED)
		{
			printf("Socket error while trying to write data to socket\n");
			return ERROR_CODE;
		}
	}
}

int connect_to_server()
{
	SOCKADDR_IN clientService;

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
		g_main_socket = INVALID_SOCKET;
		return ERROR_CODE;
	}



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
	//create ditels for log
	int connect_params_len = 5+ strlen(g_server_adress) + max(log10(g_server_port), 0) + 1;//:+.\n+\0+
	char* p_connect_params = (char*)malloc(sizeof(char) * connect_params_len);
	if (NULL == p_connect_params)
	{
		printf("unable to alocate memory for erorr_message\n");
		WSACleanup();
		g_main_socket = INVALID_SOCKET;
		return ERROR_CODE;
	}
	sprintf_s(p_connect_params, connect_params_len, "%s:%d.\n", g_server_adress, g_server_port);
	//try to connect to server
	while (try_to_connect)
	{
		if (connect(g_main_socket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR)
		{
			printf("Failed connecting to server on %s", p_connect_params);
			write_to_log("Failed connecting to server on ", p_connect_params, strlen(p_connect_params));
			printf("Choose what to do next:\n1. Try to reconnect\n2. Exit\n");
			int user_input = user_choose_option();
			if (1 == user_input)
			{
				try_to_connect = TRUE;
				//Try to reconnect

			}
			else if (2 == user_input)
			{
				//Exit
				WSACleanup();
				g_main_socket = INVALID_SOCKET;
				free(p_connect_params);
				return ERROR_CODE;
			}
			else if (BAD_ALOC == user_input)
			{
				//TODO close resorce
				free(p_connect_params);
			}

		}
		else
		{
			try_to_connect = FALSE;
			printf("Connected to server on %s", p_connect_params);
			write_to_log("Connected to server on ", p_connect_params, strlen(p_connect_params));
			free(p_connect_params);

		}

	}
	return SUCCESS_CODE;

}

int init_threads()
{
	g_is_thread_avilibale[REC_THREAD] = TRUE;
	g_socket_thread[REC_THREAD] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)SendDataThread,
		NULL,
		0,
		NULL
	);
	if (NULL == g_socket_thread[REC_THREAD])
	{
		printf("Couldn't create thread\n");
		return ERROR_CODE;
	}
	g_is_thread_avilibale[SEND_THREAD] = TRUE;
	g_socket_thread[SEND_THREAD] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)RecvDataThread,
		NULL,
		0,
		NULL
	);
	//TODO handel erorr
	if (NULL == g_socket_thread[SEND_THREAD])
	{
		printf("Couldn't create thread\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}

int send_first_request_to_server()
{
	//send client request
	char message[MASSGAE_TYPE_MAX_LEN + MAX_USERNAME_LEN + 2] = { 0 };//message type + user_name + \n + :
	sprintf_s(message, MASSGAE_TYPE_MAX_LEN + MAX_USERNAME_LEN + 2, "%s%c%s%c", client_meaasge[CLIENT_REQUEST], MASSGAE_TYPE_SEPARATE, g_user_name, MASSGAE_END);
	g_game_on = TRUE;
	//send requset to server
	return send_message_to_server(message, get_message_len(message), 2 * MAX_TIME_FOR_TIMEOUT);
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

int init_sync_objects()
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
		return ERROR_CODE;
	}
	g_rcev_semapore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - */
		MAX_MESSAGES_IN_BUF,		/* Maximum Count */
		NULL); /* un-named */
	if (NULL == g_send_semapore)
	{
		printf("error when Create Semaphore\n");
		return ERROR_CODE;

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
		NULL);         /* name */
	/* Check if succeeded and handle errors */
	last_error = GetLastError();
	if (ERROR_SUCCESS != last_error)
	{
		return ERROR_CODE;
		//TODO add error handel
	}
	return SUCCESS_CODE;

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
		printf("unable to alocate memory for read user input\n");
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
		printf("get message from server in unknown format\n");
		return ERROR_TYPE;
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


int client_response()
{
	DWORD wait_code;
	BOOL ret_val;
	int time_out = 0;
	while (g_game_on)
	{
		wait_code = WaitForSingleObject(g_rcev_semapore, INFINITE);
		if (is_trans_failed)
		{
			return ERROR_CODE;
		}
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
			printf("Server on %s:%d denied the connection request\nChoose what to do next:\n1. Try to reconnect\n2. Exit\n", g_server_adress, g_server_port);
			int user_input = user_choose_option();
			if (1 == user_input)
			{
				//TODO reconnect
				release_source(DENIED_BY_SERVER);
				return SUCCESS_CODE;

			}
			else if (2 == user_input)
			{
				// if server denied the server do the graceful_shutdown
				//TODO release_source
				g_connect_server = FALSE;
				return SUCCESS_CODE;
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
				time_out += send_message_to_server(message, get_message_len(message), 2*MAX_TIME_FOR_TIMEOUT);
			}
			else if (2 == user_input)
			{
				char message[MASSGAE_TYPE_MAX_LEN] = { 0 };//message type + \n
				sprintf_s(message, MASSGAE_TYPE_MAX_LEN, "%s%c", client_meaasge[CLIENT_DISCONNECT], MASSGAE_END);
				//send to server
				time_out += send_message_to_server(message, get_message_len(message), 0);
				//TODO close client and resource
				g_connect_server = FALSE;
				if (graceful_shutdown())
				{
					return ERROR_CODE;
				}
				return SUCCESS_CODE;
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
				printf("unable to alocate memory for message\n");
				return ERROR_CODE;

			}
			sprintf_s(message, message_len, "%s%c%s%c", client_meaasge[CLIENT_PLAYER_MOVE], MASSGAE_TYPE_SEPARATE, p_user_input, MASSGAE_END);
			free(p_user_input);
			//send to server
			time_out += send_message_to_server(message, get_message_len(message), MAX_TIME_FOR_TIMEOUT);
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
		else if (SERVER_NO_OPPONENTS == message_type)
		{
			printf("did not find opponent.\n");
		}
		else if (SERVER_OPPONENT_OUT == message_type)
		{
			printf("Opponent quit .\n");
		}
		else if(ERROR_TYPE == message_type)
		{

		}

		if (time_out)
		{

		}
	}
	return SUCCESS_CODE;

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
		
	}
	if (wait_for_response)
	{
		wait_res = WaitForSingleObject(g_wait_for_server_event, wait_for_response * 1000);//*1000 for secends
		//wait for the server respond
		if (WAIT_TIMEOUT == wait_res)
		{
			printf("server is not rsponding error while trying to write data to socket\n");
			return ERROR_CODE;
		}
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

int user_choose_option()
{
	char* p_user_input = NULL;
	while (TRUE)
	{
		p_user_input = readLine();
		if (NULL == p_user_input)
		{
			printf("unable to alocate memory for user input\n");
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
			write_to_log("Error:Illegal command", "\n", 1);
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
	if (!CloseHandle(h_file))
	{
		printf("Error when closing handle\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;

	

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
	if (!CloseHandle(h_file))
	{
		printf("Error when closing handle\n");
		return 0;
	}
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


//{ NOT_NEED, RELEASE_SYNC, RELEASE_SOCKET,RELEASE_THREADS
int release_source(int stage)
{
	int release_result = SUCCESS_CODE;
	switch (stage)
	{
	case DENIED_BY_SERVER:
		release_result += relase_link_list();
		release_result += close_client_threads();
		release_result += close_socket();
	case AFTER_GRACEFUL:
		release_result += relase_link_list();
		release_result += close_client_threads();
		release_result += close_sync_object();
		break;
	case RELEASE_ALL:
	case RELEASE_LINK_LIST:
		release_result += relase_link_list();
	case RELEASE_THREADS:
		release_result += close_client_threads();
	case RELEASE_SOCKET:
		release_result += close_socket();
	case RELEASE_SYNC:
		release_result += close_sync_object();
	case NOT_NEED:
		break;
	}
	if (SUCCESS_CODE == release_result)
		return SUCCESS_CODE;
	return ERROR_CODE;
}

int close_sync_object()
{
	/// <summary>
	/// this code will close all the handles and threads we used
	/// </summary>
	/// <returns></returns>
	BOOL ret_val = TRUE;
	DWORD exit_code;
	DWORD thread_out_val = SUCCESS_CODE;
	if (NULL != g_send_semapore)
	{
		ret_val = ret_val && CloseHandle(g_send_semapore);
		g_send_semapore = NULL;
	}
	if (NULL != g_rcev_semapore)
	{
		ret_val = ret_val && CloseHandle(g_rcev_semapore);
		g_rcev_semapore = NULL;
	}
	if (NULL != g_hads_mutex)
	{
		ret_val = ret_val && CloseHandle(g_hads_mutex);
		g_hads_mutex = NULL;
	}
	if (NULL != g_wait_for_server_event)
	{
		ret_val = ret_val && CloseHandle(g_wait_for_server_event);
		g_wait_for_server_event = NULL;
	}
	if (ERROR_RET == ret_val)
	{
		printf("Error when closing sync objects handles\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;

}

int close_client_threads()
{
	BOOL ret_val = TRUE;
	DWORD wait_res;
	DWORD exit_code;
	int clean_result = SUCCESS_CODE;
	if (NULL != g_socket_thread[SEND_THREAD])
	{
		// close send threads
		//check if close

		ret_val = GetExitCodeThread(g_socket_thread[SEND_THREAD], &exit_code);
		if (STILL_ACTIVE == exit_code)
		{
			g_is_thread_avilibale[SEND_THREAD] = FALSE;
			ret_val = ret_val && ReleaseSemaphore(
				g_send_semapore,
				1, 		/* Signal that exactly one cell was emptied */
				&wait_res);
			if (ret_val)
			{
				wait_res = WaitForSingleObject(g_socket_thread[SEND_THREAD], MAX_TIME_FOR_TIMEOUT);
				ret_val = GetExitCodeThread(g_socket_thread[SEND_THREAD], &exit_code);
			}
		}
		if (ERROR_RET == ret_val || STILL_ACTIVE == exit_code)
		{
			clean_result = ERROR_CODE;
			printf("Error when releasing\n");
			TerminateThread(g_socket_thread[SEND_THREAD], ERROR_CODE);
		}
		if (ERROR_CODE == exit_code)
		{
			printf("one of the thread give error exit code\n");
			clean_result = ERROR_CODE;
		}
		ret_val = CloseHandle(g_socket_thread[SEND_THREAD]);
		if (!ret_val)
		{
			printf("Error when closing thread handle\n");
			clean_result = ERROR_CODE;
		}
		g_socket_thread[SEND_THREAD] = NULL;
	}
	if (NULL != g_socket_thread[REC_THREAD])
	{
		// close rec threads
		wait_res = WaitForSingleObject(g_socket_thread[REC_THREAD], MAX_TIME_FOR_TIMEOUT);
		ret_val = GetExitCodeThread(g_socket_thread[REC_THREAD], &exit_code);
		if (ERROR_RET == ret_val || STILL_ACTIVE == exit_code)
		{
			clean_result = ERROR_CODE;
			printf("Error when releasing\n");
			TerminateThread(g_socket_thread[REC_THREAD], ERROR_CODE);
		}
		else if (ERROR_CODE == exit_code)
		{
			printf("one of the thread give error exit code\n");
			clean_result = ERROR_CODE;
		}
		ret_val = CloseHandle(g_socket_thread[REC_THREAD]);
		if (!ret_val)
		{
			printf("Error when closing thread handle\n");
			clean_result = ERROR_CODE;
		}
		g_socket_thread[REC_THREAD] = NULL;
		//TODO add to recive thread option to close
		return clean_result;
	}
}

int close_socket()
{
	int ret_val = SUCCESS_CODE;
	if (g_main_socket != INVALID_SOCKET)
	{
		if (SOCKET_ERROR == closesocket(g_main_socket))
		{
			printf("Failed to close MainSocket, error %ld. Ending program\n", WSAGetLastError());
			ret_val = ERROR_CODE;
		}
		if (SOCKET_ERROR == WSACleanup())
		{
			printf("Failed to close Winsocket, error %ld. Ending program.\n", WSAGetLastError());
			ret_val = ERROR_CODE;
		}
		g_main_socket = INVALID_SOCKET;
	}
	return ret_val;
}

int relase_link_list()
{
	DWORD wait_code;
	wait_code = WaitForSingleObject(g_hads_mutex, INFINITE);
	if (WAIT_OBJECT_0 != wait_code)
	{
		printf("Error when waiting for mutex\n");
		return ERROR_CODE;
	}
	message_cell_t* p_temp_cell;
	while (NULL != gp_recived_massge_hade)
	{
		p_temp_cell = gp_recived_massge_hade;
		gp_recived_massge_hade = gp_recived_massge_hade->p_next;
		free(p_temp_cell->p_message);
		free(p_temp_cell);
	}
	while (NULL != gp_massage_to_send_hade)
	{
		p_temp_cell = gp_massage_to_send_hade;
		gp_massage_to_send_hade = gp_massage_to_send_hade->p_next;
		free(p_temp_cell->p_message);
		free(p_temp_cell);
	}
	if (FALSE == ReleaseMutex(g_hads_mutex))
	{
		printf("unable to release mutex\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;

}

int graceful_shutdown()
{
	//if return SUCCESS_CODE both threads are signaled and client socket is 
	DWORD wait_res;
	BOOL ret_val;
	g_start_graceful_shutdown = TRUE;
	ret_val = ReleaseSemaphore(
		g_send_semapore,
		1, 		/* Signal that exactly one cell was emptied */
		&wait_res);
	if (FALSE == ret_val)
	{
		printf("Error when releasing semapore\n");
		return ERROR_CODE;
	}
	wait_res = WaitForSingleObject(g_socket_thread[SEND_THREAD], 2 * MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
	if (WAIT_OBJECT_0 == wait_res)
	{
		DWORD exit_code;
		if (!GetExitCodeThread(g_socket_thread[SEND_THREAD], &exit_code))
		{
			printf("problem when try to get exit code\n");
			return ERROR_CODE;
		}
		if (SUCCESS_CODE == exit_code)
		{
			//TODO do we need to print someting?
			return SUCCESS_CODE;
		}
	}
	else
	{
		printf("client is not rsponding to graceful_shutdown\n");
		return ERROR_CODE;

	}

}

