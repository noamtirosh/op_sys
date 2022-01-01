
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

typedef struct message_cell_s {
	struct message_cell_s* p_next;
	char* p_message;
	BOOL wait_for_response;
}message_cell_t;

typedef struct thread_input_s {
	int client_num;
	SOCKET client_socket;
	enum thread_mode client_mode;
}thread_input_t;

HANDLE g_wait_for_client_event[MAX_NUM_PLAYERS] = { NULL };
char g_player_name_arr[MAX_NUM_PLAYERS][MAX_USERNAME_LEN] = { NULL };
int  g_player_num_arr[MAX_NUM_PLAYERS] = { 0 };
int g_num_of_clients = 0;
int g_game_turn_num= 0;
int g_game_state = 0;
message_cell_t *gp_recived_massge_hade[MAX_NUM_CLIENTS] = { NULL };
message_cell_t* gp_massage_to_send_hade[MAX_NUM_CLIENTS] = { NULL };

BOOL g_is_time_out_arr[MAX_NUM_CLIENTS] = { FALSE };
static HANDLE g_send_semapore_arr[MAX_NUM_CLIENTS] = { NULL };
static HANDLE g_rcev_semapore_arr[MAX_NUM_CLIENTS] = { NULL };

HANDLE g_thread_handles_arr[MAX_NUM_CLIENTS];
thread_input_t g_thread_inputs_arr[MAX_NUM_CLIENTS];
char client_meaasge[NUM_CLIENT_TYPES][MASSGAE_TYPE_MAX_LEN] = { "CLIENT_REQUEST","CLIENT_VERSUS","CLIENT_PLAYER_MOVE","CLIENT_DISCONNECT" };
char server_massage[NUM_SERVER_TYPES][MASSGAE_TYPE_MAX_LEN] = { "SERVER_APPROVED","SERVER_DENIED","SERVER_MAIN_MENU","GAME_STARTED","TURN_SWITCH","SERVER_MOVE_REQUEST","GAME_ENDED","SERVER_NO_OPPONENTS","GAME_VIEW","SERVER_OPPONENT_OUT" };

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
		g_thread_handles_arr[Ind] = NULL;

	printf("Waiting for a client to connect...\n");

	for (Loop = 0; Loop < MAX_NUM_CLIENTS; Loop++)
	{
		SOCKET AcceptSocket = accept(MainSocket, NULL, NULL);
		if (AcceptSocket == INVALID_SOCKET)
		{
			printf("Accepting connection with client failed, error %ld\n", WSAGetLastError());
			//goto server_cleanup_3;
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
			g_thread_inputs_arr[Ind].client_socket = AcceptSocket; // shallow copy: don't close 
											  // AcceptSocket, instead close 
											  // ThreadInputs[Ind] when the
											  // time comes.
			g_thread_handles_arr[Ind] = CreateThread(
				NULL,
				0,
				(LPTHREAD_START_ROUTINE)ServiceThread,
				&(g_thread_inputs_arr[Ind]),
				0,
				NULL
			);
		}
	} // for ( Loop = 0; Loop < MAX_LOOPS; Loop++ )

}



//Reading data coming from the server
static DWORD WINAPI recive_from_client_thread(LPVOID t_socket)
{
	thread_input_t* p_current_client_input = (thread_input_t*)t_socket;
	TransferResult_t RecvRes;
	int client_num = p_current_client_input->client_num;
	while (1)
	{
		DWORD wait_code;
		BOOL ret_val;
		char* AcceptedStr = NULL;
		RecvRes = ReceiveString(&AcceptedStr, p_current_client_input->client_socket, g_wait_for_client_event[client_num]);
		//TODO choose one or marge
		if (RecvRes == TRNS_FAILED)
		{
			printf("Socket error while trying to write data to socket\n");
			return 0x555;
		}
		if (RecvRes == TRNS_FAILED)
		{
			printf("Service socket error while writing, closing thread.\n");
			closesocket(p_current_client_input->client_socket);
			return 1;
		}
		else if (RecvRes == TRNS_DISCONNECTED)
		{
			printf("Server closed connection. Bye!\n");
			return 0x555;
		}
		else
		{
			add_message_to_cell_arr(AcceptedStr, gp_recived_massge_hade[client_num],FALSE);
			//release semaphore for next thred
			ret_val = ReleaseSemaphore(
				g_rcev_semapore_arr[client_num],
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
static DWORD WINAPI send_to_client_thread(LPVOID t_socket)
{
	thread_input_t* p_current_client_input = (thread_input_t*)t_socket;
	int client_num = p_current_client_input->client_num;
	TransferResult_t SendRes;
	DWORD wait_code;

	while (1)
	{

		wait_code = WaitForSingleObject(g_send_semapore_arr[client_num], INFINITE);
		if (wait_code != WAIT_OBJECT_0)
		{
			printf("Error when waiting for semapore\n");
			return ERROR_CODE;
		}
		char* p_massage_to_send = gp_massage_to_send_hade[client_num]->p_message;
		message_cell_t* p_temp_cell = gp_massage_to_send_hade[client_num]->p_next;
		BOOL wait_for_response = gp_massage_to_send_hade[client_num]->wait_for_response;
		if (wait_for_response)
			//reset client_event
			wait_code = WaitForSingleObject(g_wait_for_client_event[client_num],0);
		SendRes = SendString(p_massage_to_send, p_current_client_input->client_socket);
		//free the client massage to send buffer
		free(p_massage_to_send);
		free(gp_massage_to_send_hade[client_num]);
		gp_massage_to_send_hade[client_num] = p_temp_cell;
		//TODO choose one or marge
		if (SendRes == TRNS_FAILED)
		{
			printf("Socket error while trying to write data to socket\n");
			return 0x555;
		}
		if (SendRes == TRNS_FAILED)
		{
			printf("Service socket error while writing, closing thread.\n");
			closesocket(p_current_client_input->client_socket);
			return 1;
		}
		if (wait_for_response)
		{
			wait_code = WaitForSingleObject(g_wait_for_client_event[client_num], MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
			//wait for the server respond
			if (WAIT_TIMEOUT == wait_code)
			{
				g_is_time_out_arr[client_num] = TRUE;
				printf("server is not rsponding error while trying to write data to socket\n");
				//TODO close graceful
			}
		}
	}
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

int server_send_response(int type,int client_ind)
{

	int message_len = 0;
	int other_player_ind = client_ind ? client_ind - 1 : client_ind + 1;
	DWORD wait_res;
	switch (type)
	{
	case CLIENT_REQUEST://CLIENT_REQUEST
		if (g_num_of_clients == MAX_NUM_PLAYERS)
		{
			message_len = MASSGAE_TYPE_MAX_LEN + 1;//for add \n
			send_message_to_client(message_len, client_ind, SERVER_DENIED, NULL, FALSE);
			return SERVER_DENIED; //SERVER_DENIED
			//TODO close_thread and socket
		}
		else
		{
			g_num_of_clients++;
			message_len = MASSGAE_TYPE_MAX_LEN + 1;//for add \n
			send_message_to_client(message_len, client_ind, SERVER_APPROVED, NULL, FALSE);
			send_message_to_client(message_len, client_ind, SERVER_MAIN_MENU, NULL, FALSE);
			return SERVER_APPROVED; // SERVER_APPROVED
		}
	case CLIENT_VERSUS: 
		//TODO wait for another player
		//both players ask to play
		if (MAX_NUM_PLAYERS == g_num_of_clients)
		{
			wait_res = WaitForSingleObject(g_wait_for_client_event, MAX_TIME_FOR_TIMEOUT * 1000);//*1000 for secends
		}
		if (MAX_NUM_PLAYERS != g_num_of_clients || WAIT_TIMEOUT == wait_res)
		{
			message_len = MASSGAE_TYPE_MAX_LEN + 1;//for add \n
			send_message_to_client(message_len, client_ind, SERVER_NO_OPPONENTS, NULL, FALSE);
			return SERVER_NO_OPPONENTS;
		}
		if (MAX_NUM_PLAYERS == g_game_state)
		{
			//start game
			g_game_state++;
			message_len = MASSGAE_TYPE_MAX_LEN + 1;//for add \n
			//send both client GAME_STARTED	
			send_message_to_client(message_len, client_ind, GAME_STARTED, NULL, FALSE);
			send_message_to_client(message_len, other_player_ind, GAME_STARTED, NULL, FALSE);
			//TODO make shure g_player_name_arr end with \n or chenge send_message_to_client
			//we start the game with the player how first connect to server
			message_len = MASSGAE_TYPE_MAX_LEN + 1 + MAX_USERNAME_LEN;
			send_message_to_client(message_len, client_ind, TURN_SWITCH, g_player_name_arr[0], FALSE);
			send_message_to_client(message_len, other_player_ind, TURN_SWITCH, g_player_name_arr[0], FALSE);
			message_len = MASSGAE_TYPE_MAX_LEN + 1;
			//reset g_wait_for_client_event for client 0 
			WaitForSingleObject(g_wait_for_client_event[0], 0);//event on ato
			send_message_to_client(message_len, 0, SERVER_MOVE_REQUEST ,NULL, TRUE);
			return GAME_STARTED;
		}

		else if(TRUE)
		{
	
		}
		else
		{
			return SERVER_NO_OPPONENTS;
		}
	case CLIENT_PLAYER_MOVE: 

	case CLIENT_DISCONNECT:


	default:
		break;
	}


}

void init_sync_obj()
{
	for (int i = 0; i < MAX_NUM_CLIENTS; i++)
	{
		//create semapore
		g_send_semapore_arr[i] = CreateSemaphore(
			NULL,	/* Default security attributes */
			0,		/* Initial Count - */
			MAX_MESSAGES_IN_BUF,		/* Maximum Count */
			NULL); /* un-named */
		if (NULL == g_send_semapore_arr)
		{
			printf("error when Create Semaphore\n");
		}
		g_rcev_semapore_arr[i] = CreateSemaphore(
			NULL,	/* Default security attributes */
			0,		/* Initial Count - */
			MAX_MESSAGES_IN_BUF,		/* Maximum Count */
			NULL); /* un-named */
		if (NULL == g_send_semapore_arr)
		{
			printf("error when Create Semaphore\n");
		}

	}
	for (int i = 0; i < MAX_NUM_PLAYERS; i++)
	{
		//create event to track time out
		/* Parameters for CreateEvent */
		static const char P_EVENT_NAME[EVENT_NAME_LEN] = { 0 };

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
			//TODO add error handel
		}
	}


}

// TODO make more general
int read_massage_from_client(char* Str,int client_ind)
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
	for (int i = 0; i < NUM_CLIENT_TYPES; i++)
	{
		if (STRINGS_ARE_EQUAL(client_meaasge[client_ind][i], massage_type))
		{
			type = i;
		}
	}
	if (CLIENT_VERSUS == type)
	{
		g_game_state++;
	}
	while (Str[index] != MASSGAE_END)
	{

		// has name parmeter
		if (CLIENT_REQUEST == type )
		{
			if (client_ind < MAX_NUM_PLAYERS)
			{
				int name_ind = 0;
				while (Str[index] != PRAMS_SEPARATE && Str[index] != MASSGAE_END)
				{
					g_player_name_arr[client_ind][name_ind] = Str[index];
					index++;
					name_ind++;
				}
				//TODO is the username includes \0
				g_player_name_arr[client_ind][name_ind] = '\0';
				//reset wait for client event
				WaitForSingleObject(g_wait_for_client_event[client_ind], 0);//event on ato
			}
		}
		if(CLIENT_PLAYER_MOVE == type)
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

			}
		}

		index++;
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
	/// <returns></returns>
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
	for (int part_num = input_num; part_num < 10; part_num /= 10)
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
	DWORD client_ind;
	client_ind = WaitForMultipleObjects(g_num_of_clients,// num of objects to wait for
		g_rcev_semapore_arr, //array of handels to wait for
		FALSE, // wait until one of the objects became signaled
		INFINITE // no time out
	);
	if (WAIT_FAILED == client_ind)
	{

	}
	else
	{
		int massage_type = read_massage_from_client(gp_recived_massge_hade[client_ind]->p_message, client_ind);
		server_send_response(massage_type);
		if (CLIENT_PLAYER_MOVE == massage_type)
		{
			if (seven_boom(g_player_num_arr[client_ind]))
			{

			}
		}
			
	}

}


message_cell_t* add_message_to_cell_arr(char *p_str, message_cell_t *p_cell_head,BOOL wait_for_response)
{
	message_cell_t* p_temp_cell = p_cell_head;
	message_cell_t* p_new_cell = (message_cell_t*)malloc(sizeof(message_cell_t));
	if(NULL==p_new_cell)
	{
	}
	p_new_cell->wait_for_response = wait_for_response;
	p_new_cell->p_next = NULL;
	p_new_cell->p_message = p_str;

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
	DWORD wait_res;
	BOOL ret_val;
	 char* p_message_to_add = (char*)malloc(sizeof(char) * msg_len);
	 if (NULL == p_message_to_add)
	 {
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
		for (int ind = 0; p_params[ind] != MASSGAE_END; ind++)
		{
			p_message_to_add[last_ind + ind] = p_params[ind];
			if (last_ind + ind == msg_len)
			{
				//TODO error no MASSGAE_END
			}
		}
	}
	gp_massage_to_send_hade[client_ind] = add_message_to_cell_arr(p_message_to_add, gp_massage_to_send_hade[client_ind], wait_for_response);
	ret_val = ReleaseSemaphore(
		g_send_semapore_arr[client_ind],
		1, 		/* Signal that exactly one cell was emptied */
		&wait_res);
	if (FALSE == ret_val)
	{
		printf("Error when releasing\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}