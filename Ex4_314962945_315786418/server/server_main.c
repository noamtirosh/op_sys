
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <string.h>
#include <Windows.h> // TODO if not work to remove
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "SocketShared.h"
#include "SocketSendRecvTools.h"
#include "serverhardcoded.h"
#pragma comment(lib, "Ws2_32.lib")


enum thread_mode { RECIVE, TRANSMIT};
enum client_types{ CLIENT_REQUEST,CLIENT_VERSUS,CLIENT_PLAYER_MOVE,CLIENT_DISCONNECT};
enum server_types{ SERVER_APPROVED,SERVER_DENIED,SERVER_MAIN_MENU,GAME_STARTED,TURN_SWITCH,SERVER_MOVE_REQUEST,GAME_ENDED,SERVER_NO_OPPONENTS,GAME_VIEW,SERVER_OPPONENT_OUT};
enum game_state { NO_REQUEST_STATE, ONE_PLAYER_REQUST, TWO_PLAYER_REQUST, GAME_ON_STATE, GAME_END_STATE};
enum release_stage { NOT_NEED, RELEASE_SOCKET, RELEASE_SYNC, RELEASE_THREADS, RELEASE_LINK_LIST};

typedef struct message_cell_s {
	struct message_cell_s* p_next;
	char* p_message;
	BOOL wait_for_response;
}message_cell_t;

typedef struct thread_input_s {
	int client_num;
	SOCKET client_socket;
}thread_input_t;

HANDLE g_wait_for_client_event[MAX_NUM_PLAYERS] = { NULL };

char g_client_name_arr[MAX_NUM_CLIENTS][MAX_USERNAME_LEN] = { NULL };
char g_log_file_name[MAX_NUM_CLIENTS][MAX_USERNAME_LEN + LOG_FILE_NAME_BASE_LEN] = { 0 };

int  g_player_num_arr[MAX_NUM_PLAYERS] = { 0 };
int g_game_turn_num= 0;
int g_game_state = 0;
BOOL g_server_on = TRUE;
message_cell_t *gp_recived_massge_hade[MAX_NUM_CLIENTS] = { NULL };
message_cell_t* gp_massage_to_send_hade[MAX_NUM_CLIENTS] = { NULL };

BOOL g_is_time_out[MAX_NUM_CLIENTS] = { FALSE };
static HANDLE g_game_end_semapore_arr[MAX_NUM_CLIENTS] = { NULL };
static HANDLE g_send_semapore[MAX_NUM_CLIENTS] = { NULL };
static HANDLE g_hads_mutex[MAX_NUM_CLIENTS] = { NULL };
static HANDLE g_log_file_mutex[MAX_NUM_CLIENTS] = { NULL };

// Initialize all thread handles to NULL, to mark that they have not been initialized
HANDLE g_main_thread_handle = NULL;
HANDLE g_recive_thread_handles_arr[MAX_NUM_CLIENTS] = { NULL };
HANDLE g_send_thread_handles_arr[MAX_NUM_CLIENTS] = { NULL };

thread_input_t g_thread_inputs_arr[MAX_NUM_CLIENTS];
BOOL g_is_thread_active[MAX_NUM_CLIENTS] = { FALSE };
BOOL g_player_vs_req[MAX_NUM_PLAYERS] = { FALSE };
BOOL g_start_graceful_shutdown[MAX_NUM_CLIENTS] = { FALSE };
SOCKET g_main_socket = INVALID_SOCKET;
SOCKET g_client_socket[MAX_NUM_CLIENTS] = { INVALID_SOCKET };


char client_meaasge[NUM_CLIENT_TYPES][MASSGAE_TYPE_MAX_LEN] = { "CLIENT_REQUEST","CLIENT_VERSUS","CLIENT_PLAYER_MOVE","CLIENT_DISCONNECT" };
char server_massage[NUM_SERVER_TYPES][MASSGAE_TYPE_MAX_LEN] = { "SERVER_APPROVED","SERVER_DENIED","SERVER_MAIN_MENU","GAME_STARTED","TURN_SWITCH","SERVER_MOVE_REQUEST","GAME_ENDED","SERVER_NO_OPPONENTS","GAME_VIEW","SERVER_OPPONENT_OUT" };





//function decleration
int connect_to_sokcet(int port_num);
int close_socket(SOCKET socket_to_close);
static DWORD WINAPI send_to_client_thread(int* t_socket);
static DWORD WINAPI recive_from_client_thread(int* t_socket);
static DWORD WINAPI socket_thread(void);
static HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine,LPVOID p_thread_parameters);
int graceful_shutdown(int client_num);
message_cell_t* add_message_to_cell_arr(char* p_message, int message_len, message_cell_t* p_cell_head, BOOL wait_for_response);
int init_sync_obj();
int init_socket_thread();
void logic_fun();
HANDLE create_file_simple(LPCSTR p_file_name, char mode);
DWORD write_to_file(LPCSTR p_file_name, LPVOID p_buffer, const DWORD buffer_len);
int write_to_log(int client_num, const char* p_start_message_str, char* p_buffer, const DWORD buffer_len);
int create_log_file(int client_num);
int close_sync_object();
int relase_link_list();
int close_socket_thread();



int main(int argc, char* argv[])
{
	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERROR_CODE;
	}
	int port_num = atoi(argv[PORT_NUM_IND]);
	connect_to_sokcet(port_num);
	init_sync_obj();
	init_socket_thread();
	while (TRUE)
	{
		DWORD multi_wait_code;
		multi_wait_code = WaitForMultipleObjects(MAX_NUM_CLIENTS, g_game_end_semapore_arr, FALSE, INFINITE);
		if (WAIT_FAILED == multi_wait_code)
		{
			printf("Error when waitting for multiple threads \n");
			//TODO release
			return ERROR_CODE;
		}
		else
		{
			DWORD exit_code;
			//if(!GetExitCodeThread(handle_arr[multi_wait_code], &exit_code))
			//{
			//	printf("Error when getting thread %d exit code\n", handle_arr[multi_wait_code]);

			//}
		}
	}

}




static DWORD WINAPI socket_thread(void)
{
	while (g_server_on)
	{
		SOCKET AcceptSocket = accept(g_main_socket, NULL, NULL);
		if (!g_server_on)
		{
			return SUCCESS_CODE;
		}
		if (AcceptSocket == INVALID_SOCKET)
		{
			printf("Accepting connection with client failed, error %ld\n", WSAGetLastError());
			//goto server_cleanup_3;
		}
		printf("Client Connected.\n");
		//find empty thread handel 
		int client_ind = 0;
		for (int i = 0; i < MAX_NUM_CLIENTS; i++)
		{
			if (!g_is_thread_active[i])
			{
				client_ind = i;
				break;
			}
		}
		if (client_ind == MAX_NUM_CLIENTS)
		{
			//TODO send DENIED_SERVE
			closesocket(AcceptSocket); //Closing the socket, dropping the connection.
		}
		else
		{
			g_is_thread_active[client_ind] = TRUE;
			g_client_socket[client_ind] = AcceptSocket;
		}

		//TODO add mutex
		//creats send_to_client_thread
		g_send_thread_handles_arr[client_ind] = CreateThreadSimple(send_to_client_thread, &client_ind,NULL);
		if (NULL == g_send_thread_handles_arr[client_ind])
		{
			printf("problem when try to create thread for client\n");
			return ERROR_CODE;
		}
		//we asume we dont have more then 3 clients
		//creats send_to_client_thread
		g_recive_thread_handles_arr[client_ind] = CreateThreadSimple(recive_from_client_thread, &client_ind, NULL);
		if (NULL == g_send_thread_handles_arr[client_ind])
		{
			printf("problem when try to create thread for client\n");
			return ERROR_CODE;
		}
	}
	return SUCCESS_CODE;

}

//Reading data coming from the server
static DWORD WINAPI recive_from_client_thread(int *t_socket)
{
	int client_num = *t_socket;
	SOCKET client_socket = g_client_socket[client_num];

	TransferResult_t RecvRes;
	while (g_is_thread_active[client_num])
	{
		DWORD wait_code;
		BOOL ret_val;
		char* AcceptedStr = NULL;
		RecvRes = ReceiveString(&AcceptedStr, client_socket, g_wait_for_client_event[client_num]);
		//TODO choose one or marge
		if (RecvRes == TRNS_FAILED)
		{
			if (NULL != AcceptedStr)
				free(AcceptedStr);
			printf("Socket error while trying to read data from socket\n");
			return ERROR_CODE;
		}
		else if (RecvRes == TRNS_DISCONNECTED)
		{
			if (NULL != AcceptedStr)
				free(AcceptedStr);
			if (!g_start_graceful_shutdown[client_num])
			{
				g_is_thread_active[client_num] = FALSE;
				//TODO send final transmssion
				shutdown(client_socket, SD_SEND);//will stop sending info
			}
			if (close_socket(client_socket))
				return ERROR_CODE;
			//TODO send chenge text transmssion
			//TODO do we need to print someting?
			return SUCCESS_CODE;
		}
		else
		{
			int massage_type = read_massage_from_client(AcceptedStr, client_num);
			if (NULL != AcceptedStr)
				free(AcceptedStr);
			int response_type = server_send_response(massage_type, client_num);

		}

	}

	return SUCCESS_CODE;
}

//Sending data to the server
static DWORD WINAPI send_to_client_thread(int *t_socket)
{
	int client_num =  *t_socket;
	SOCKET client_socket = g_client_socket[client_num];
	TransferResult_t SendRes;
	DWORD wait_code;
	while (g_is_thread_active[client_num])
	{

		wait_code = WaitForSingleObject(g_send_semapore[client_num], INFINITE);
		if (g_start_graceful_shutdown[client_num])
		{
			//try graceful_shutdown 
			shutdown(client_socket, SD_SEND);//will stop sending info
			wait_code = WaitForSingleObject(g_recive_thread_handles_arr[client_num], MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
			//wait for client  shutdown response
			if (WAIT_OBJECT_0 != wait_code)
			{
				g_is_time_out[client_num] = TRUE;
				printf("client is not rsponding to graceful_shutdown\n");
				return ERROR_CODE;
			}
			else
			{
				DWORD exit_code;
				if (!GetExitCodeThread(g_recive_thread_handles_arr[client_num], &exit_code))
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
		else
		{
			if (wait_code != WAIT_OBJECT_0)
			{
				printf("Error when waiting for semapore\n");
				return ERROR_CODE;
			}
			BOOL ret_val;
			wait_code = WaitForSingleObject(g_hads_mutex[client_num], INFINITE);
			if (WAIT_OBJECT_0 != wait_code)
			{
				printf("Error when waiting for mutex\n");
				return ERROR_CODE;
			}
			char* p_massage_to_send = gp_massage_to_send_hade[client_num]->p_message;
			message_cell_t* p_temp_cell = gp_massage_to_send_hade[client_num]->p_next;
			BOOL wait_for_response = gp_massage_to_send_hade[client_num]->wait_for_response;
			if (wait_for_response)
			{				
				//reset client_event
				wait_code = WaitForSingleObject(g_wait_for_client_event[client_num], 0);// dwmillisecends is 0 so if the g_wait_for_client_event is signaled it reset it 
				if (WAIT_FAILED == wait_code)
				{
					printf("Error when waiting for client_event\n");
					return ERROR_CODE;
				}
			}
			SendRes = SendString(p_massage_to_send, client_socket);
			if (TRNS_SUCCEEDED != SendRes)
			{
				printf("Socket error while trying to write data to socket\n");
				return ERROR_CODE;
			}
			//free the client massage to send buffer
			free(p_massage_to_send);
			free(gp_massage_to_send_hade[client_num]);
			gp_massage_to_send_hade[client_num] = p_temp_cell;
			ret_val = ReleaseMutex(g_hads_mutex[client_num]);
			if (FALSE == ret_val)
			{
				printf("unable to release mutex\n");
				return ERROR_CODE;
			}
			//TODO choose one or marge
			if (wait_for_response)
			{
				wait_code = WaitForSingleObject(g_wait_for_client_event[client_num], MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
				//wait for the server respond
				if (WAIT_TIMEOUT == wait_code)
				{
					g_is_time_out[client_num] = TRUE;
					printf("client is not rsponding error while trying to write data to socket\n");
					//TODO close graceful
				}
			}
		}
	}
	return ERROR_CODE;

}


int connect_to_sokcet(int port_num)
{
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
	g_main_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (g_main_socket == INVALID_SOCKET)
	{
		printf("Error at socket( ): %ld\n", WSAGetLastError());
		clean_server();
		return ERROR_CODE;
	}
	// Bind the socket.
	// Create a sockaddr_in object and set its values.
	// Declare variables
	Address = inet_addr(SERVER_ADDRESS_STR);
	if (Address == INADDR_NONE)
	{
		printf("The string \"%s\" cannot be converted into an ip address. ending program.\n",
			SERVER_ADDRESS_STR);
		release_socket();
		return ERROR_CODE;
	}
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = Address;
	service.sin_port = htons(port_num); //The htons function converts a u_short from host to TCP/IP network byte order 
									   //( which is big-endian ).

	// Call the bind function, passing the created socket and the sockaddr_in structure as parameters. 
	// Check for general errors.
	bindRes = bind(g_main_socket, (SOCKADDR*)&service, sizeof(service));
	if (bindRes == SOCKET_ERROR)
	{
		printf("bind( ) failed with error %ld. Ending program\n", WSAGetLastError());
		release_socket();
		return ERROR_CODE;
	}

	// Listen on the Socket.
	ListenRes = listen(g_main_socket, SOMAXCONN);
	if (ListenRes == SOCKET_ERROR)
	{
		printf("Failed listening on socket, error %ld.\n", WSAGetLastError());
		release_socket();
		return ERROR_CODE;
	}
	return SUCCESS_CODE;


}

int server_send_response(int type,int client_ind)
{

	int message_len = 0;
	int other_player_ind = client_ind ? client_ind - 1 : client_ind + 1;
	DWORD wait_res = 0;

		if (CLIENT_REQUEST == type)
		{
			//CLIENT_REQUEST
			if (client_ind == MAX_NUM_PLAYERS)
			{
				message_len = MASSGAE_TYPE_MAX_LEN + 1;//for add \n
				send_message_to_client(message_len, client_ind, SERVER_DENIED, NULL, FALSE);
				return SERVER_DENIED; //SERVER_DENIED
				//TODO close_thread and socket
			}
			else
			{
				message_len = MASSGAE_TYPE_MAX_LEN + 1;//for add \n
				send_message_to_client(message_len, client_ind, SERVER_APPROVED, NULL, FALSE);
				send_message_to_client(message_len, client_ind, SERVER_MAIN_MENU, NULL, FALSE);
				return SERVER_APPROVED; // SERVER_APPROVED
			}
		}
		else if (CLIENT_VERSUS == type)
		{
			g_player_vs_req[client_ind] = TRUE;
			//TODO wait for another player
			//both players ask to play
			if(!g_player_vs_req[other_player_ind])
			{
				wait_res = WaitForSingleObject(g_wait_for_client_event[client_ind], 0);//*1000 for secends
				wait_res = WaitForSingleObject(g_wait_for_client_event[client_ind], MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
			}
			else
			{
				message_len = MASSGAE_TYPE_MAX_LEN + 1;//for add \n
				//send both client GAME_STARTED	
				send_message_to_client(message_len, client_ind, GAME_STARTED, NULL, FALSE);
				send_message_to_client(message_len, other_player_ind, GAME_STARTED, NULL, FALSE);
				//TODO make shure g_player_name_arr end with \n or chenge send_message_to_client
				//we start the game with the player how first connect to server
				message_len = MASSGAE_TYPE_MAX_LEN + 1 + MAX_USERNAME_LEN;
				char params[1 + MAX_USERNAME_LEN] = { 0 };
				sprintf_s(params, 1 + MAX_USERNAME_LEN, "%s%c", g_client_name_arr[0], MASSGAE_END);
				send_message_to_client(message_len, client_ind, TURN_SWITCH, params, FALSE);
				send_message_to_client(message_len, other_player_ind, TURN_SWITCH, params, FALSE);
				message_len = MASSGAE_TYPE_MAX_LEN + 1;
				send_message_to_client(message_len, 0, SERVER_MOVE_REQUEST, NULL, TRUE);
				return GAME_STARTED;
			}
			if (!g_is_thread_active[other_player_ind] || WAIT_TIMEOUT == wait_res)
			{
				g_player_vs_req[client_ind] = FALSE;
				message_len = MASSGAE_TYPE_MAX_LEN + 1;//for add \n
				send_message_to_client(message_len, client_ind, SERVER_NO_OPPONENTS, NULL, FALSE);
				send_message_to_client(message_len, client_ind, SERVER_MAIN_MENU, NULL, FALSE);
				return SERVER_NO_OPPONENTS;
			}

		}
		else if (CLIENT_PLAYER_MOVE == type)
		{
			char end_text[END_MSG_MAX_LEN] = CONTINUE_GAME;
			if (!seven_boom(g_player_num_arr[client_ind]))
			{
				strcpy(end_text, END_GAME);
				g_game_state = GAME_END_STATE;
				
			}
			int params_len = MAX_USERNAME_LEN + END_MSG_MAX_LEN + 2;//+2*PRAMS_SEPARATE;
			if (BOOM_VLUE == g_player_num_arr[client_ind])
				params_len += strlen(BOOM_TEXT);
			else
				params_len += floor(max(log10(g_player_num_arr), 0)) + 1;//num dig of the move
			message_len = MASSGAE_TYPE_MAX_LEN + 1 + params_len;//for add \n
			char* p_prams_string_buffer = (char*)malloc(sizeof(char) * params_len);
			if (BOOM_VLUE == g_player_num_arr[client_ind])
				sprintf_s(p_prams_string_buffer, params_len, "%s%c%s%c%s%c", g_client_name_arr[client_ind], PRAMS_SEPARATE, BOOM_TEXT, PRAMS_SEPARATE, end_text, MASSGAE_END);
			else
			{
				sprintf_s(p_prams_string_buffer, params_len, "%s%c%d%c%s%c", g_client_name_arr[client_ind], PRAMS_SEPARATE, g_player_num_arr[client_ind], PRAMS_SEPARATE, end_text, MASSGAE_END);
			}
			send_message_to_client(message_len, other_player_ind, GAME_VIEW, p_prams_string_buffer, FALSE);
			if (GAME_END_STATE == g_game_state)
			{
				message_len = MASSGAE_TYPE_MAX_LEN + 1 + MAX_USERNAME_LEN;
				char winner_name[MAX_USERNAME_LEN] = { 0 };
				sprintf_s(winner_name, MAX_USERNAME_LEN, "%s%c", g_client_name_arr[other_player_ind], MASSGAE_END);
				send_message_to_client(message_len, client_ind, GAME_ENDED, winner_name, FALSE);
				send_message_to_client(message_len, other_player_ind, GAME_ENDED, winner_name, FALSE);
				reset_game();
				send_message_to_client(message_len, client_ind, SERVER_MAIN_MENU, NULL, FALSE);
				send_message_to_client(message_len, other_player_ind, SERVER_MAIN_MENU, NULL, FALSE);
			}
			else
			{
				message_len = MASSGAE_TYPE_MAX_LEN + 1 + MAX_USERNAME_LEN;
				char params[1 + MAX_USERNAME_LEN] = { 0 };
				sprintf_s(params, 1 + MAX_USERNAME_LEN, "%s%c", g_client_name_arr[other_player_ind], MASSGAE_END);
				send_message_to_client(message_len, client_ind, TURN_SWITCH, params, FALSE);
				send_message_to_client(message_len, other_player_ind, TURN_SWITCH, params, FALSE);
				message_len = MASSGAE_TYPE_MAX_LEN + 1;
				send_message_to_client(message_len, other_player_ind, SERVER_MOVE_REQUEST, NULL, TRUE);

			}
		}
		else if (CLIENT_DISCONNECT == type)
		{
			//when the client use CLIENT_DISCONNECT it should use graceful_shutdown 	 
			//send_message_to_client(message_len, client_ind, SERVER_OPPONENT_OUT, NULL, FALSE);
			//g_game_end_semapore_arr
			return CLIENT_DISCONNECT;
		}
		else if (SERVER_NO_OPPONENTS == type)
		{

		}

}

int init_sync_obj()
{
	for (int i = 0; i < MAX_NUM_CLIENTS; i++)
	{
		//create semapore
		g_send_semapore[i] = CreateSemaphore(
			NULL,	/* Default security attributes */
			0,		/* Initial Count - */
			MAX_MESSAGES_IN_BUF,		/* Maximum Count */
			NULL); /* un-named */
		if (NULL == g_send_semapore[i])
		{
			printf("error when Create Semaphore\n");
			return ERROR_CODE;

		}
		g_game_end_semapore_arr[i] = CreateSemaphore(
			NULL,	/* Default security attributes */
			0,		/* Initial Count - */
			1,		/* Maximum Count */
			NULL); /* un-named */
		if (NULL == g_game_end_semapore_arr[i])
		{
			printf("error when Create Semaphore\n");
			return ERROR_CODE;

		}
		//create mutex
		g_hads_mutex[i] = CreateMutex(
			NULL,	/* default security attributes */
			FALSE,	/* initially not owned */
			NULL);	/* unnamed mutex */
		if (NULL == g_hads_mutex[i])
		{
			printf("error when Create mutex\n");
			//TODO add error handel
			return ERROR_CODE;
		}
		g_log_file_mutex[i] = CreateMutex(
			NULL,	/* default security attributes */
			FALSE,	/* initially not owned */
			NULL);	/* unnamed mutex */
		if (NULL == g_log_file_mutex[i])
		{
			printf("error when Create mutex\n");
			//TODO add error handel
			return ERROR_CODE;
		}
		

	}
	for (int i = 0; i < MAX_NUM_PLAYERS; i++)
	{
		//create event to track time out
		/* Parameters for CreateEvent */
		char P_EVENT_NAME[EVENT_NAME_LEN] = { 0 };

		sprintf_s(P_EVENT_NAME, EVENT_NAME_LEN, "WAIT_FOR_7_BOOM_CLIENT_EVENT%d",i);

		DWORD last_error;

		/* Get handle to event by name. If the event doesn't exist, create it */
		g_wait_for_client_event[i] = CreateEvent(
			NULL, /* default security attributes */
			FALSE,        /* Auto-reset event */
			FALSE,      /* initial state is non-signaled */
			P_EVENT_NAME);         /* name */
		/* Check if succeeded and handle errors */
		last_error = GetLastError();
		if (ERROR_SUCCESS != last_error)
		{
			printf("error when Create client_event\n");
			return ERROR_CODE;
			//TODO add error handel
		}
	}
	return SUCCESS_CODE;

}


int init_socket_thread()
{
	g_main_thread_handle = CreateThreadSimple(socket_thread, NULL, NULL);
	if (NULL == g_main_thread_handle)
	{
		printf("problem when try to create thread for client\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}
// TODO make more general
int read_massage_from_client(char* Str,int client_ind)
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
	for (int i = 0; i < NUM_CLIENT_TYPES; i++)
	{
		if (STRINGS_ARE_EQUAL(client_meaasge[i], massage_type))
		{
			type = i;
			break;
		}
	}
	if (NUM_CLIENT_TYPES == type)
	{
		//TODO add problem not soportes type
	}
	while (Str[index] != MASSGAE_END)
	{

		// has name parmeter
		if (CLIENT_REQUEST == type )
		{
			index++;
			if (client_ind < MAX_NUM_PLAYERS)
			{
				int name_ind = 0;
				while (Str[index] != PRAMS_SEPARATE && Str[index] != MASSGAE_END)
				{
					g_client_name_arr[client_ind][name_ind] = Str[index];
					index++;
					name_ind++;
				}
				//TODO is the username includes \0
				g_client_name_arr[client_ind][name_ind] = '\0';
				create_log_file(client_ind);
			}
		}
		else if(CLIENT_PLAYER_MOVE == type)
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
				g_player_num_arr[client_ind] = BOOM_VLUE;
			}
			else
			{
				while (Str[index] != PRAMS_SEPARATE && Str[index] != MASSGAE_END)
				{
					index++;
				}
				Str[index] = '\0';
				g_player_num_arr[client_ind] = atoi(Str + start_ind);//human readable
				Str[index] = MASSGAE_END;
			}
		}
		else
		{
			//TODO add problem not soportes params for spesific type
		}
		write_to_log(client_ind, LOG_REC, Str, get_message_len(Str));
	}
	return type;
}

int seven_boom(int player_input)
{
	/// <summary>
	/// the function will get input string 
	/// and will that can be an int type number or the string 'boom'
	/// using the law of the game the will dicade the condition of the player  
	/// </summary>
	/// <param name="p_command_line">the command we get to the game</param>
	/// <returns> 0 if wrong ansser and 1 if currect</returns>
	g_game_turn_num++;
	//need to boom?
	if (look_for_seven(g_game_turn_num) || g_game_turn_num % 7 == 0)
	{
		if (BOOM_VLUE == player_input)
			return 1;
		return 0;
	}
	else
	{
		if (player_input == g_game_turn_num)
			return 1;
		return 0;
	}
}
int look_for_seven(int input_num)
{
	/// <summary>
	/// break the number to find if there is seven in it 
	/// </summary>
	/// <returns>if condition is true return 1 else return 0</returns>
	for (int part_num = input_num; part_num > 10; part_num /= 10)
	{
		if (part_num % 10 == 7)
		{
			return 1;
		}
	}
	return 0;
}

void logic_fun()
{
	//wait for clients requests
	// TODO remove next part
	//DWORD client_ind;
	//client_ind = WaitForMultipleObjects(g_num_of_clients,// num of objects to wait for
	//	g_rcev_semapore, //array of handels to wait for
	//	FALSE, // wait until one of the objects became signaled
	//	INFINITE // no time out
	//);
	int client_ind = 0;
	if (WAIT_FAILED == client_ind)
	{

	}
	else
	{
		int massage_type = read_massage_from_client(gp_recived_massge_hade[client_ind]->p_message, client_ind);
		server_send_response(massage_type, client_ind);
		if (CLIENT_PLAYER_MOVE == massage_type)
		{
			if (seven_boom(g_player_num_arr[client_ind]))
			{

			}
		}
			
	}

}


void remove_client(int client_id)
{

}
int graceful_shutdown( int client_num)
{
	//if return SUCCESS_CODE both threads are signaled and client socket is 
	DWORD wait_res;
	BOOL ret_val;
	g_start_graceful_shutdown[client_num] = TRUE;
	ret_val = ReleaseSemaphore(
		g_send_semapore[client_num],
		1, 		/* Signal that exactly one cell was emptied */
		&wait_res);
	if (FALSE == ret_val)
	{
		printf("Error when releasing semapore\n");
		return ERROR_CODE;
	}
	wait_res = WaitForSingleObject(g_send_thread_handles_arr[client_num], 2*MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
	if (WAIT_OBJECT_0 == wait_res)
	{
		DWORD exit_code;
		if (!GetExitCodeThread(g_send_thread_handles_arr[client_num], &exit_code))
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
		g_is_time_out[client_num] = TRUE;
		printf("client is not rsponding to graceful_shutdown\n");
		return ERROR_CODE;

	}

}


message_cell_t* add_message_to_cell_arr(char* p_message, int message_len, message_cell_t* p_cell_head, BOOL wait_for_response)
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

int send_message_to_client(int msg_len,int client_ind,int type,char  *p_params, BOOL wait_for_response)
{
	/// <summary>
	/// 
	/// </summary>
	/// <param name="msg_len"></param>
	/// <param name="client_ind"></param>
	/// <param name="type"></param>
	/// <param name="p_params">with need to have MASSGAE_END in the end</param>
	/// <param name="wait_for_response">if true the thread how send the message wait for response for </param>
	/// <returns></returns>
	DWORD wait_res;
	BOOL ret_val;
	 char* p_message_to_add = (char*)malloc(sizeof(char) * msg_len);
	 if (NULL == p_message_to_add)
	 {
		 printf("unable to alocate memory for message to client\n");
		 return ERROR_CODE;
		 //TODO add erorr
	 }
	 int last_ind = strlen(server_massage[type]);
	strcpy(p_message_to_add, server_massage[type]);
	if (NULL == p_params)
	{
		p_message_to_add[last_ind] = MASSGAE_END;//TODO make shure it set the right char
	}
	else
	{
		p_message_to_add[last_ind] = MASSGAE_TYPE_SEPARATE;//TODO make shure it set the right char
		last_ind++;
		int ind = 0;
		for (; p_params[ind] != MASSGAE_END; ind++)
		{
			p_message_to_add[last_ind + ind] = p_params[ind];
			if (last_ind + ind == msg_len)
			{
				//TODO error no MASSGAE_END
			}
		}
		p_message_to_add[last_ind + ind] = MASSGAE_END;
		//relase params
	}

	write_to_log(client_ind, LOG_SENT, p_message_to_add, get_message_len(p_message_to_add));
	wait_res = WaitForSingleObject(g_hads_mutex[client_ind], INFINITE);
	if (WAIT_OBJECT_0 != wait_res)
	{
		printf("Error when waiting for mutex\n");
		return ERROR_CODE;
	}
	gp_massage_to_send_hade[client_ind] = add_message_to_cell_arr(p_message_to_add, get_message_len(p_message_to_add),gp_massage_to_send_hade[client_ind], wait_for_response);
	ret_val = ReleaseMutex(g_hads_mutex[client_ind]);
	if (FALSE == ret_val)
	{
		printf("unable to release mutex\n");
		return ERROR_CODE;
	}
	free(p_message_to_add);
	ret_val = ReleaseSemaphore(
		g_send_semapore[client_ind],
		1, 		/* Signal that exactly one cell was emptied */
		&wait_res);
	if (FALSE == ret_val)
	{
		printf("Error when releasing\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}

static HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine,
	LPVOID p_thread_parameters)
{
	/// <summary>
	///create a thread and return the handle
	/// </summary>
	/// <param name="p_start_routine">thread function</param>
	/// <param name="p_thread_parameters">the argumants of thread function</param>
	/// <param name="p_thread_id">pointre tahte give us the id number of the thread</param>
	/// <returns>HANDLE to the thread</returns>
	HANDLE thread_handle;
	if (NULL == p_start_routine)
	{
		printf("Error when creating a thread");
		printf("Received null pointer");
		return NULL;
	}
	thread_handle = CreateThread(
		NULL,                /*  default security attributes */
		0,                   /*  use default stack size */
		p_start_routine,     /*  thread function */
		p_thread_parameters, /*  argument to thread function */
		0,                   /*  use default creation flags */
		NULL);        /*  returns the thread identifier */
	if (NULL == thread_handle)
	{
		printf("Couldn't create thread\n");
		//TODO free resource
		exit(ERROR_CODE);
	}
	return thread_handle;
}

int create_log_file(int client_num)
{
	//sprintf_s(g_log_file_name[client_num], MAX_USERNAME_LEN + LOG_FILE_NAME_BASE_LEN, "Client_log_%s.txt", g_client_name_arr[client_num]);
	sprintf_s(g_log_file_name[client_num], MAX_USERNAME_LEN + LOG_FILE_NAME_BASE_LEN, "Thread_log_%s.txt", g_client_name_arr[client_num]);
	//create empty log file, overwrite if exist
	HANDLE h_file = create_file_simple(g_log_file_name[client_num], 'd');
	if (INVALID_HANDLE_VALUE == h_file)
	{
		printf("unable to crate log file: %s\n", g_log_file_name[client_num]);
		return ERROR_CODE;
	}
	CloseHandle(h_file);
	return SUCCESS_CODE;

}
int write_to_log(int client_num, const char* p_start_message_str, char* p_buffer, const DWORD buffer_len)
{
	DWORD wait_res = 0;
	BOOL ret_val;
	wait_res = WaitForSingleObject(g_log_file_mutex[client_num], INFINITE);
	if (WAIT_OBJECT_0 != wait_res)
	{
		printf("Error when waiting for mutex\n");
		return ERROR_CODE;
	}
	int num_byte_written = 0;
	num_byte_written = write_to_file(g_log_file_name[client_num], p_start_message_str, strlen(p_start_message_str));
	ret_val = ReleaseMutex(g_log_file_mutex[client_num]);
	if (FALSE == ret_val)
	{
		printf("unable to release mutex\n");
		return ERROR_CODE;
	}
	if (strlen(p_start_message_str) != num_byte_written)
	{
		printf("problem when try to write to log file the line %s%s\n", p_start_message_str, p_buffer);
		return ERROR_CODE;
	}
	wait_res = WaitForSingleObject(g_log_file_mutex[client_num], INFINITE);
	if (WAIT_OBJECT_0 != wait_res)
	{
		printf("Error when waiting for mutex\n");
		return ERROR_CODE;
	}
	num_byte_written = write_to_file(g_log_file_name[client_num], p_buffer, buffer_len);
	ret_val = ReleaseMutex(g_log_file_mutex[client_num]);
	if (FALSE == ret_val)
	{
		printf("unable to release mutex\n");
		return ERROR_CODE;
	}
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

int release_source(int stage)
{

	int release_result = SUCCESS_CODE;

	switch (stage)
	{
	case RELEASE_LINK_LIST:
		for (int i = 0; i < MAX_NUM_CLIENTS; i++)
		{
			release_result += relase_link_list(i);
		}
	case RELEASE_THREADS:
		for (int i = 0; i < MAX_NUM_CLIENTS; i++)
		{
			release_result += close_client_threads(i);
		}
		release_result += close_socket_thread();
	case RELEASE_SYNC:
		release_result += close_sync_object();
	case RELEASE_SOCKET:
		release_result += release_socket();
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
	/// this code will close all the handles
	/// </summary>
	/// <returns></returns>
	BOOL ret_val = TRUE;
	DWORD exit_code;
	DWORD thread_out_val = SUCCESS_CODE;
	for (int i = 0; i < MAX_NUM_CLIENTS; i++)
	{
		if (NULL != g_send_semapore[i])
		{
			ret_val = ret_val && CloseHandle(g_send_semapore[i]);
		}
		if (NULL != g_game_end_semapore_arr[i])
		{
			ret_val = ret_val && CloseHandle(g_game_end_semapore_arr[i]);
		}
		if (NULL != g_hads_mutex[i])
		{
			ret_val = ret_val && CloseHandle(g_hads_mutex[i]);
		}
		if (NULL != g_log_file_mutex[i])
		{
			ret_val = ret_val && CloseHandle(g_log_file_mutex[i]);
		}
	}
	for (int i = 0; i < MAX_NUM_PLAYERS; i++)
	{
		if (NULL != g_wait_for_client_event[i])
		{
			ret_val = ret_val && CloseHandle(g_wait_for_client_event[i]);
		}
	}
	if (!ret_val)
	{
		printf("Error when closing sync objects handles\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;

}

int release_socket()
{
	int ret_val = SUCCESS_CODE;
	ret_val += close_socket(g_main_socket);
	ret_val += clean_server();
	if (ret_val)
		return ERROR_CODE;
	return SUCCESS_CODE;

}

int close_socket(SOCKET socket_to_close)
{
	if (closesocket(socket_to_close) == SOCKET_ERROR)
	{
		printf("Failed to close MainSocket, error %ld. Ending program\n", WSAGetLastError());
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}

int clean_server()
{
	if (WSACleanup() == SOCKET_ERROR)
	{
		printf("WSA Cleanup failed:, error %ld. Ending program.\n", WSAGetLastError());
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}

int relase_link_list(int client_num)
{
	DWORD wait_code;
	wait_code = WaitForSingleObject(g_hads_mutex[client_num], INFINITE);
	if (WAIT_OBJECT_0 != wait_code)
	{
		printf("Error when waiting for mutex\n");
		return ERROR_CODE;
	}
	message_cell_t* p_temp_cell;
	while (NULL != gp_recived_massge_hade[client_num])
	{
		p_temp_cell = gp_recived_massge_hade[client_num];
		gp_recived_massge_hade[client_num] = gp_recived_massge_hade[client_num]->p_next;
		free(p_temp_cell->p_message);
		free(p_temp_cell);
	}
	while (NULL != gp_massage_to_send_hade[client_num])
	{
		p_temp_cell = gp_massage_to_send_hade[client_num];
		gp_massage_to_send_hade[client_num] = gp_massage_to_send_hade[client_num]->p_next;
		free(p_temp_cell->p_message);
		free(p_temp_cell);
	}
	if (FALSE == ReleaseMutex(g_hads_mutex[client_num]))
	{
		printf("unable to release mutex\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;

}

int close_client_threads(int client_num)
{
	BOOL ret_val = TRUE;
	DWORD wait_res;
	DWORD exit_code;
	int clean_result = SUCCESS_CODE;
	// close send threads
	//check if close
	ret_val = GetExitCodeThread(g_send_thread_handles_arr[client_num], &exit_code);
	if (STILL_ACTIVE == exit_code)
	{
			g_is_thread_active[client_num] = FALSE;
		ret_val = ret_val && ReleaseSemaphore(
			g_send_semapore[client_num],
			1, 		/* Signal that exactly one cell was emptied */
			&wait_res);
		if (ret_val)
		{
			wait_res = WaitForSingleObject(g_send_thread_handles_arr[client_num], MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
			if (WAIT_OBJECT_0 == wait_res)
			{
				ret_val = GetExitCodeThread(g_send_thread_handles_arr[client_num], &exit_code);
			}
		}
	}
	if (!ret_val || STILL_ACTIVE == exit_code)
	{
		clean_result = ERROR_CODE;
		printf("Error when releasing\n");
		TerminateThread(g_send_thread_handles_arr[client_num], ERROR_CODE);
	}
	if (ERROR_CODE == exit_code)
	{
		printf("one of the thread give error exit code\n");
		clean_result = ERROR_CODE;
	}
	ret_val = CloseHandle(g_send_thread_handles_arr[client_num]);
	if (!ret_val)
	{
		printf("Error when closing thread handle\n");
		clean_result = ERROR_CODE;
	}

	//close rec threads
	g_is_thread_active[client_num] = FALSE;
	wait_res = WaitForSingleObject(g_recive_thread_handles_arr[client_num], MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
	if (WAIT_OBJECT_0 == wait_res)
	{
		ret_val = GetExitCodeThread(g_recive_thread_handles_arr[client_num], &exit_code);
	}
	if (!ret_val || STILL_ACTIVE == exit_code)
	{
		clean_result = ERROR_CODE;
		printf("Error when releasing\n");
		TerminateThread(g_recive_thread_handles_arr[client_num], ERROR_CODE);
	}
	if (ERROR_CODE == exit_code)
	{
		printf("one of the thread give error exit code\n");
		clean_result = ERROR_CODE;
	}
	ret_val = CloseHandle(g_recive_thread_handles_arr[client_num]);
	if (!ret_val)
	{
		printf("Error when closing thread handle\n");
		clean_result = ERROR_CODE;
	}
	return clean_result;
}
int close_socket_thread()
{
	BOOL ret_val = TRUE;
	DWORD wait_res;
	DWORD exit_code = 0;
	//close socket_thread
	g_server_on = FALSE;
	wait_res = WaitForSingleObject(g_main_thread_handle, MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
	if (WAIT_OBJECT_0 == wait_res)
	{
		ret_val = GetExitCodeThread(g_main_thread_handle, &exit_code);
	}
	if (!ret_val || STILL_ACTIVE == exit_code)
	{
		printf("Error when releasing\n");
		TerminateThread(g_main_thread_handle, ERROR_CODE);
		return ERROR_CODE;
	}
	if (ERROR_CODE == exit_code)
	{
		printf("socket thread give error exit code\n");
		return ERROR_CODE;
	}
	ret_val = CloseHandle(g_main_thread_handle);
	if (!ret_val)
	{
		printf("Error when closing thread handle\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}

int reset_client(int client_num)
{
	DWORD wait_code;
	int reset_result = SUCCESS_CODE;
	if (client_num >= MAX_NUM_CLIENTS)
	{
		return ERROR_CODE;
	}
	if (MAX_NUM_PLAYERS != client_num)
	{
		//reset game
		reset_game();
		//reset client_event
		wait_code = WaitForSingleObject(g_wait_for_client_event[client_num], 0);// dwmillisecends is 0 so if the g_wait_for_client_event is signaled it reset it 

	}

	//relase_link_list
	reset_result += relase_link_list(client_num);
	g_is_time_out[client_num] = FALSE;
	//reset semapores
	wait_code = WaitForSingleObject(g_send_semapore[client_num], 0);// dwmillisecends is 0 so if the g_wait_for_client_event is signaled it reset it 
	while (WAIT_OBJECT_0 == wait_code)
	{
		wait_code = WaitForSingleObject(g_send_semapore[client_num], 0);// dwmillisecends is 0 so if the g_wait_for_client_event is signaled it reset it 
	}
	wait_code = WaitForSingleObject(g_game_end_semapore_arr[client_num], 0);// dwmillisecends is 0 so if the g_wait_for_client_event is signaled it reset it 
	g_is_thread_active[client_num] = FALSE;
	g_start_graceful_shutdown[client_num] = FALSE;
	reset_result += close_client_threads(client_num);
	// set thread handles to NULL, to mark that they have not been initialized
	g_recive_thread_handles_arr[client_num] = NULL;
	g_send_thread_handles_arr[client_num] = NULL;
	//TODO its need to be after graceful_shutdown
	close_socket(g_client_socket[client_num]);
	g_client_socket[client_num] = INVALID_SOCKET;
	if (reset_result)
		return ERROR_CODE;
	return SUCCESS_CODE;
}

int reset_game()
{
	g_game_turn_num = 0;
	g_game_state = 0;
	for (int i = 0; i < MAX_NUM_PLAYERS; i++)
	{
		g_player_num_arr[i] = 0;
		g_player_vs_req[i] =  FALSE ;
	}

}

