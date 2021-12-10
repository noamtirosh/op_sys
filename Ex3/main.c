//////////////////////////////////////
/*
* Authors – Noam Tirosh 314962945, Daniel Kogan 315786418
* project : Ex3
* Description: TODO add discription
*/
/////////////////////////////////////
#include <stdio.h>
#include <windows.h>
#include "hardcoded.h"
#include <string.h>
#include <math.h>
#include <ctype.h>

struct page_info {
	int start_time;
	int page_number;
	int work_time;
};
typedef struct lookup_table_info {//the number of this struct will be outer structure of page_number we will difine array of g_page[page_number]
	int g_frame_number;
	int valid;
	int end_time;
};

DWORD get_file_len(LPCSTR p_file_name);
DWORD read_from_file(LPCSTR p_file_name, long offset, LPVOID p_buffer, const DWORD buffer_len);
int build_table_for_calcultion(char* p_line, struct page_info pages[]);
int open_thread_for_commands(struct page_info pages[], struct lookup_table_info table[], int num_of_frame, int number_of_command_line);
int count_chars(const char* string, char ch);
int main(int argc, char* argv[])
{
	char* p_full_text;
	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERROR_CODE;
	}
	const char* p_message_file_path = NULL;//the path to open the file
	const char* p_ex_name = NULL;//the name of the exition
	int page_number;//the number of pages
	int frame_number;//the number of frames

	p_message_file_path = argv[MASSAGE_FILE_IND];
	p_ex_name = argv[EX_IND];
	page_number = pow(2, atoi(argv[VIRTUAL_MEMORY_IND]) - 12);//we are looking for number we dont have overflow or double
	frame_number = pow(2, atoi(argv[PHYSICAL_MEMORY_IND]) - 12);

	struct lookup_table_info* pages = (struct lookup_table_info*)malloc(page_number * sizeof(struct lookup_table_info));
	if (NULL == pages)
	{
		printf("Error when allocating memory\n");
		return ERROR_CODE;
	}

	int length_of_text = get_file_len(p_message_file_path);
	p_full_text = (char*)malloc((length_of_text + 1) * sizeof(char));
	if (NULL == p_full_text)
	{
		printf("Error when allocating memory\n");
		return ERROR_CODE;
	}
	DWORD num_bytes_readen = read_from_file(p_message_file_path, 0, p_full_text, length_of_text);
	p_full_text[strlen(p_full_text) - 1] = '\0';
	int number_of_command_line = count_chars(p_full_text, '/n');
	struct page_info* command_lines = (struct page_info*)malloc((number_of_command_line) * sizeof(struct page_info));
	if (NULL == command_lines)
	{
		printf("Error when allocating memory\n");
		return ERROR_CODE;
	}
	int x = build_table_for_calcultion(p_full_text, command_lines);
	int y = open_thread_for_commands(command_lines, pages, frame_number, number_of_command_line);
	free(pages);
	free(p_full_text);
	free(command_lines);//TODO CANNOT FREE THE FILE
	return SUCCESS_CODE;

}

int count_chars(const char* string, char ch)
{
	int count = 0;
	int i;
	int length = strlen(string);
	for (i = 0; i < length; i++)
	{
		if (string[i] == ch)
		{
			count++;
		}
	}

	return count;
}

int build_table_for_calcultion(char* p_line, struct page_info pages[])
{
	/// <summary>
	/// the code will break the text file to the command lines that build him
	/// than we put the info in a array of struct that will conain this info 
	/// </summary>
	/// <param name="p_line">the input text</param>
	/// <param name="pages">the pages where we put the commands input</param>
	/// <returns></returns>
	char seprete[] = " \n";
	char* token_pointer = NULL;
	char* next_token_pointer = NULL;
	token_pointer = strtok_s(p_line, seprete, &next_token_pointer);
	int array[3];//help array to contain the info of each line
	int i = 0;
	int j = 0;
	while (token_pointer != NULL) {
		array[i % 3] = atoi(token_pointer);
		token_pointer = strtok_s(NULL, seprete, &next_token_pointer);
		i = i + 1;
		if ((i >= 3) && (i % 3 == 0))
		{
			pages[j].page_number = array[1] / 4000;//the number of page that corseponds to the adress
			pages[j].start_time = array[0];//the start time
			pages[j].work_time = array[2];//the time the page works

		}
	}
	return SUCCESS_CODE;
}
int open_thread_for_commands(struct page_info pages[], struct lookup_table_info table[], int num_of_frame, int number_of_command_line)
{
	/// <summary>
	/// 
	/// </summary>
	/// <param name="pages">the commands of the input</param>
	/// <param name="table">the table where we will change the values</param>
	/// <param name="num_of_frame">number of frames</param>
	/// <returns></returns>
	HANDLE thread_handle_arr = (HANDLE*)malloc((number_of_command_line) * sizeof(HANDLE));
	if (NULL == thread_handle_arr)
	{
		printf("Error when allocating memory\n");
		return ERROR_CODE;
	}
	int clock = 0;//Our psuado clock ,TODO NOT sure mabey should be global virabale
	int i = 0;
	for (clock = 0; clock <= pages[number_of_command_line - 1].start_time; clock++)
	{
		if (clock == pages[i].start_time)
		{
			thread_handle_arr[i] = CreateThreadSimple(school_thread, school_params, &thread_id);
			i = i + 1;
		}
	}
	return SUCCESS_CODE;
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
DWORD read_from_file(LPCSTR p_file_name, long offset, LPVOID p_buffer, const DWORD buffer_len)
{
	/// </summary>
	/// <param name="file_name">path to file to read</param>
	/// <param name="offset"> offset in bytes from beginning </param>
	/// <param name="p_buffer"> pointer to buffer to put message</param>
	/// <param name="buffer_len">length of message to read</param>
	/// <returns>num of bytes readed from file</returns>
	HANDLE h_file = create_file_simple(p_file_name, 'r');
	DWORD n_readen = 0;
	DWORD last_error;
	if (INVALID_HANDLE_VALUE == h_file)
	{
		return n_readen;
		return n_readen;
	}
	//move file pointer in ofset
	SetFilePointer(
		h_file,	//handle to file
		offset, // number of bytes to move the file pointer
		NULL, // 
		FILE_BEGIN); // provides offset from beginning of the file
	// Read one character less than the buffer size to save room for 
	// the terminating NULL character. 
	if (FALSE == ReadFile(h_file,    //handle to file
		p_buffer,   // pointer to buffer to save data read from file
		buffer_len, //maximum number of bytes to be read
		&n_readen, NULL))
	{
		last_error = GetLastError();
		printf("Unable to read from file, error: %ld\n", last_error);
	}
	CloseHandle(h_file);
	return n_readen;

}
DWORD get_file_len(LPCSTR p_file_name)
{
	HANDLE h_file = create_file_simple(p_file_name, 'r');
	DWORD current_file_position;
	//move file pointer in ofset
	current_file_position = SetFilePointer(
		h_file,	//handle to file
		0,	// number of bytes to move the file pointer
		NULL, // 
		FILE_END);
	CloseHandle(h_file);
	return current_file_position;
}
static HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine,
	LPVOID p_thread_parameters,
	LPDWORD p_thread_id)
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
		exit(ERROR_CODE);
	}

	if (NULL == p_thread_id)
	{
		printf("Error when creating a thread");
		printf("Received null pointer");
		exit(ERROR_CODE);
	}

	thread_handle = CreateThread(
		NULL,                /*  default security attributes */
		0,                   /*  use default stack size */
		p_start_routine,     /*  thread function */
		p_thread_parameters, /*  argument to thread function */
		0,                   /*  use default creation flags */
		p_thread_id);        /*  returns the thread identifier */
	if (NULL == thread_handle)
	{
		printf("Couldn't create thread\n");
		exit(ERROR_CODE);
	}
	return thread_handle;
}
