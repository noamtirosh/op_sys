//////////////////////////////////////
/*struct page_info
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

typedef struct row_obj_s {
	int start_time;
	int page_number;
	int work_time;
}row_obj_t;

typedef struct lookup_table_info_s {//the number of this struct will be outer structure of page_number we will difine array of g_page[page_number]
	int g_frame_number;
	int valid;
	int end_time;
}lookup_table_info_t;

typedef struct page_obj_s {
	int g_frame_number;
	int valid;
	int end_time;
} page_obj_t;

typedef struct frame_obj_s {
	int page_number;
	int end_time;
} frame_obj_t;

typedef struct thread_input_s {
	int time;
	int virtual_page_num;
	int physical_frame_num;
	char eviction_placement;
}thread_input_t;

static HANDLE g_out_put_file;
static HANDLE g_page_table_mutex_handle = NULL;
static HANDLE g_can_evict_frame_semapore;
page_obj_t* pg_page_table[];
int num_of_frames = 0;
int frame_lru[]
frame_obj_t frame_table[];
int current_row_ind = 0;
int n_rows = 0;
row_obj_t next_row;


DWORD get_file_len(LPCSTR p_file_name);
DWORD read_from_file(LPCSTR p_file_name, long offset, LPVOID p_buffer, const DWORD buffer_len);
int build_table_for_calcultion(char* p_line, row_obj_t pages[]);
int open_thread_for_commands(row_obj_t pages[], lookup_table_info_t table[], int num_of_frame, int number_of_command_line);
int count_chars(const char* string, char ch);
int main(int argc, char* argv[])
{
	char* p_full_text;
	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERROR_CODE;
	}
	const char* p_input_file_path = argv[INPUT_FILE_PATH_IND];//the path to open the file
	int page_number;//the number of pages
	int frame_number;//the number of frames
	page_number = pow(2, atoi(argv[NUM_BITS_IN_VIRTUAL_MEM_IND]) - 12);//we are looking for number we dont have overflow or double
	frame_number = pow(2, atoi(argv[NUM_BITS_PHYSICAL_MEM_IND]) - 12);
	page_number  = 2 ^ (atoi(argv[NUM_BITS_IN_VIRTUAL_MEM_IND])- MIN_BITS_IN_MEM);
	frame_number = 2 ^ (atoi(argv[NUM_BITS_PHYSICAL_MEM_IND]) - MIN_BITS_IN_MEM);

	 lookup_table_info_t* pages = (lookup_table_info_t*)malloc(page_number * sizeof(lookup_table_info_t)); //TODO free memory when return error 
	if (NULL == pages)
	{
		printf("Error when allocating memory\n");
		return ERROR_CODE;
	}
	
	//read input file
	int length_of_text = get_file_len(p_input_file_path);
	// read all file at once TODO change to line by line
	p_full_text = (char*)malloc((length_of_text + 1) * sizeof(char));
	if (NULL == p_full_text)
	{
		printf("Error when allocating memory\n");
		return ERROR_CODE;
	}
	DWORD num_bytes_readen = read_from_file(p_input_file_path, 0, p_full_text, length_of_text);
	p_full_text[strlen(p_full_text) - 1] = '\0';
	int number_of_command_line = count_chars(p_full_text, '/n');
	//for each line alocate mem for page - page array
	row_obj_t* command_lines = (row_obj_t*)malloc((number_of_command_line) * sizeof(row_obj_t));
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


void run_pages()
{
	BOOL release_res;
	LONG previous_count;
	int current_time = 0;
	g_can_evict_frame_semapore = CreateSemaphore(
		NULL,	/* Default security attributes */
		num_of_frames,		/* Initial Count - all slots are empty */
		num_of_frames,		/* Maximum Count */
		NULL); /* un-named */
	HANDLE *thread_handle_arr = (HANDLE*)malloc((n_rows) * sizeof(HANDLE));
	thread_input_t* thread_input_arr = (thread_input_t*)malloc((n_rows) * sizeof(thread_input_t));
	while (current_row_ind != n_rows)
	{
		//create new thread
		if (next_row.start_time == current_time)
		{
			DWORD thread_id;
			//init data in thread_input
			thread_input_arr[current_row_ind].time = current_time; // TODO mybe dalete
			thread_handle_arr[current_row_ind] = CreateThreadSimple(page_thread, thread_input_arr+ current_row_ind, &thread_id);
			//move to next row TODO get next row
		}
		//check_if_any_frame_end
		for (int frame_ind = 0; frame_ind < num_of_frames; frame_ind++)
		{
			if (frame_table[frame_ind].end_time == current_time)
			{
				//add frame to fifio
				release_res = ReleaseSemaphore(
					g_can_evict_frame_semapore,
					1, 		/* Signal that exactly one cell was emptied */
					&previous_count);
				if (release_res == FALSE)
				{
					//TODO print error
				}
				//TODO check taht in first time not entere condition if semapore if full
			}

		}
		current_time++;
		
	}
	
	//wait for pages in frams to end
	CloseHandle(g_can_evict_frame_semapore);
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

int build_table_for_calcultion(char* p_line, row_obj_t pages[])
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
int open_thread_for_commands(row_obj_t pages[], lookup_table_info_t table[], int num_of_frame, int number_of_command_line)
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
			thread_handle_arr[i] = CreateThreadSimple(page_thread, school_params, &thread_id);
			i = i + 1;
		}
	}
	return SUCCESS_CODE;
}

static DWORD WINAPI page_thread(LPVOID lpParam)
{
	//the thread func input - 
	DWORD wait_code;
	BOOL ret_val;
	thread_input_t *p_current_page_input = (thread_input_t*)lpParam;
	wait_code = WaitForSingleObject(g_page_table_mutex_handle, INFINITE);
	if (WAIT_OBJECT_0 != wait_code)
	{
		printf("Error when waiting for mutex\n");
		return ERROR_CODE;
	}
	/*
	* Critical Section
	* We can now safely access the page_table resource.
	* 
	*/
	// is the page as a active frame?
	if (pg_page_table[p_current_page_input->virtual_page_num]->valid) //TODO make shure virtual_page_num start from 0
	{
		// the page as a active frame 
		//update end_of_use in page table TODO change name to end_of_use
		int new_end_of_use = max(pg_page_table[p_current_page_input->virtual_page_num]->end_time, p_current_page_input->time);
		pg_page_table[p_current_page_input->virtual_page_num]->end_time = new_end_of_use;
		//update end_of_use in frame table
		frame_table[pg_page_table[p_current_page_input->virtual_page_num]->g_frame_number].end_time = new_end_of_use;// no need to print
	}
	ret_val = ReleaseMutex(g_page_table_mutex_handle);
	if (FALSE == ret_val)
	{
		printf("Error when releasing\n");
		return ERROR_CODE;
	}
	wait_code = WaitForSingleObject(g_can_evict_frame_semapore, INFINITE);
	if (wait_code != WAIT_OBJECT_0)
	{
		printf("Error when waiting for mutex\n");
		return ERROR_CODE;
	}
	//there is a empty_frame or frame that can be evicted
	//is the frame that can be evicted contain the page
	if (pg_page_table[p_current_page_input->virtual_page_num]->valid)
	{
		//no need to print
	}
	// is there a empty frame?
	else
	{
		//TODO think how to 
	}
	//need to evicted frame and place the page
	{
	}


	//if (SUCCESS_CODE != 1)
	//{
	//	printf("thread for school: %d crashed\n", *p_current_school);
	//	return ERROR_CODE;
	//}
	return SUCCESS_CODE;
}
DWORD print_to_output_file(int time,int virtual_page_num ,int physical_frame_num, char eviction_placement)
{
	DWORD num_char_written = 0;
	int write_buffer_len = floor(log10(time)) +1+ floor(log10(virtual_page_num)) +1+ floor(log10(physical_frame_num))+1 + 1 + 6; // time+virtual_page_num+physical_frame_num+eviction_placement+"   \r\n"
	char* write_buffer = (char*)malloc(write_buffer_len * sizeof(char));
	//TODO check maloc
	sprintf_s(write_buffer, write_buffer_len, "%d %d %d %c\r\n", time, virtual_page_num, physical_frame_num, eviction_placement);
	num_char_written = write_to_file(g_out_put_file, write_buffer, write_buffer_len);
	free(write_buffer);
	if (num_char_written == write_buffer_len)
	{
		return SUCCESS_CODE;
	}
	return ERROR_CODE;//TODO add print
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
