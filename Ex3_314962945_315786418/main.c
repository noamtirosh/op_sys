//////////////////////////////////////
/*struct page_info
* Authors – Noam Tirosh 314962945, Daniel Kogan 315786418
* project : Ex3
* Description: The code implements system to mange an Paging system for virtual memory,
* the code will map pages to phisical frames and will mange the entry time of each page
* according to the avilability of the frames the system have in the spsyfic moment in time
* show us the time those frame begin to hold the page,the number of frame we used for the wonted
* page and the total time the frame hold the page.
*/
/////////////////////////////////////
#include <stdio.h>
#include <windows.h>
#include "hardcoded.h"
#include <string.h>
#include <math.h>

// structures
typedef struct row_obj_s {
	int start_time;
	int physical_frame_num;
	int work_time;
}row_obj_t;

typedef struct page_obj_s {
	int frame_number;
	int valid;
	int end_of_use;
} page_obj_t;

typedef struct frame_obj_s {
	int page_number;
	int end_time;
} frame_obj_t;

typedef struct thread_input_s {
	int call_time;
	int virtual_page_num;
	int work_time;
	int row_num;
}thread_input_t;

typedef struct queue_cell_s {
	struct queue_cell_s* p_next;
	int index;
}queue_cell_t;

typedef struct mem_queue_cell_s {
	struct mem_queue_cell_s* p_next;
	void* p_pointer;
}mem_queue_cell_t;



static HANDLE g_page_table_mutex_handle = NULL;
static HANDLE g_main_semapore = NULL;
static HANDLE *pg_thread_semaphore_arr = NULL;
static HANDLE *pg_thread_handle_arr = NULL;
static page_obj_t *pg_page_table = NULL;
static frame_obj_t *pg_frame_table = NULL;
static char* pg_full_text;
static int g_num_of_frames = 0;
static int g_num_of_pages = 0;
static queue_cell_t *pg_frame_queue_head =NULL;
static queue_cell_t* pg_thread_queue_head = NULL;
static mem_queue_cell_t* pg_resource_head = NULL;
static int g_avilable_frame = 0;
static int g_num_rows = 0;
static const char *pg_output_file_path = NULL;
static int g_current_time = 0;



//read input functions
int count_chars(const char* p_string, char ch);
char* get_next_line(char* p_line, row_obj_t* p_next_line_params);

// table functions
int create_tabels();
void initial_tables();
int init_sync_obj();
void init_new_thread(HANDLE* p_thread, thread_input_t* p_thread_in_arr, int row_num, int current_time, int work_time, int virtual_page_num);

//queue functions

int get_empty_frame_ind();
void remove_frame_from_queue(int frame_num);

int add_to_frame_queue(int frame_ind);

int release_waiting_threads();

int print_remains_frames();
void evicte_and_place(int placement_time, int work_time, int page_ind);
void update_frame_in_table(int page_ind, int placement_time, int page_work_time);
int update_empty_frame(int page_ind, int placement_time, int page_work_time);
int add_to_thread_queue(int thread_ind);

//memory mange functions
void* get_mem_resource(size_t size, const char* error_msg, int* p_return_value);
void free_resource(queue_cell_t* p_queue_head_1, queue_cell_t* p_queue_head_2);
int close_handels();
//file functions
HANDLE create_file_simple(LPCSTR p_file_name, char mode);
DWORD get_file_len(LPCSTR p_file_name);
DWORD read_from_file(LPCSTR p_file_name, long offset, LPVOID p_buffer, const DWORD buffer_len);
DWORD write_to_file(LPCSTR p_file_name, LPVOID p_buffer, const DWORD buffer_len);
LPCSTR get_output_path(LPCSTR p_input_path);
int print_to_output_file(int time, int virtual_page_num, int physical_frame_num, char eviction_placement);

static DWORD WINAPI page_thread(LPVOID lpParam); 
static HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine, LPVOID p_thread_parameters, LPDWORD p_thread_id);

int main(int argc, char* argv[])
{
	/// <summary>
	/// the number of bits in physical memory adress and the number of bits in  virtual adress and path to input file
	/// we use the input file to simulate virtual page request,
	/// the program build a lookup table for virtual pages that tell us witch pages are open with witch frames and
	/// the work time of page and will tell us when we start using and when we stoped using the frame
	/// </summary>
	/// <param name="argc">contains the number of argumants in our case 3</param>
	/// <param name="argv"> the number of bits in physical memory adress and the number of bits in  virtual adress and path to input file </param>
	/// <returns></returns>

	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERROR_CODE;
	}
	const char* p_input_file_path = argv[INPUT_FILE_PATH_IND];//the path to open the file
	pg_output_file_path = get_output_path(p_input_file_path);
	g_num_of_pages = (int)pow(2,((double)atoi(argv[NUM_BITS_IN_VIRTUAL_MEM_IND])- MIN_BITS_IN_MEM));
	g_num_of_frames = (int)pow(2,((double)atoi(argv[NUM_BITS_PHYSICAL_MEM_IND]) - MIN_BITS_IN_MEM));
	if (ERROR_CODE == create_tabels())
	{
		return ERROR_CODE;
	}
	initial_tables();
	//read input file
	int length_of_text = get_file_len(p_input_file_path);
	// read all file at once TODO change to line by line
	int memory_aloc_check = 0;
	pg_full_text = (char*)get_mem_resource((length_of_text + 1) * sizeof(char), "Error when allocating memory for input file text\n", &memory_aloc_check);
	if (NULL == pg_full_text)
	{
		free_resource(pg_frame_queue_head, pg_thread_queue_head);
		return ERROR_CODE;
	}
	DWORD num_bytes_readen = read_from_file(p_input_file_path, 0, pg_full_text, length_of_text);//read the text from input file 
	if (num_bytes_readen != length_of_text)
	{
		printf("Error when try to read input file %s\n", p_input_file_path);
		free_resource(pg_frame_queue_head, pg_thread_queue_head);
		return ERROR_CODE;
	}
	pg_full_text[length_of_text] = '\0';
	g_num_rows = count_chars(pg_full_text, '\n');
	if ('\n' != pg_full_text[length_of_text - 1])
	{
		g_num_rows++;
	}
	if (ERROR_CODE == init_sync_obj())
	{
		free_resource(pg_frame_queue_head, pg_thread_queue_head);
		return ERROR_CODE;
	}
	if (ERROR_CODE == run_pages())
	{
		free_resource(pg_frame_queue_head, pg_thread_queue_head);
		return ERROR_CODE;
	}
	free_resource(pg_frame_queue_head, pg_thread_queue_head);
	return SUCCESS_CODE;

}


DWORD run_pages()
{
	/// <summary>
	/// this function run over the lines of the imput file
	/// for each line it open athread to handle the request for virtual page 
	/// build the table and prints the info to the output file
	/// </summary>
	/// <returns></returns>
	DWORD wait_code;
	BOOL ret_val;
	int resource_aloc_check = 0;
	row_obj_t next_row_input;
	char* p_next_line = NULL;
	int current_row_ind = 0;

	//alocate memory for each thread for each row
	pg_thread_handle_arr = (HANDLE*)get_mem_resource((g_num_rows) * sizeof(HANDLE), "Error when allocating memory for thread_array\n", &resource_aloc_check);
	if (NULL == pg_thread_handle_arr)
	{
		return ERROR_CODE;
	}
	//init all handles to NULL
	for (int handle_ind = 0; handle_ind < g_num_rows; handle_ind++)
		pg_thread_handle_arr[handle_ind] = NULL;
	//for each line alocate mem for page - page array
	thread_input_t* p_thread_input_arr = (thread_input_t*)get_mem_resource((g_num_rows) * sizeof(thread_input_t), "Error when allocating memory for thread_input_array\n", &resource_aloc_check);
	if (NULL == p_thread_input_arr)
	{
		return ERROR_CODE;
	}
	p_next_line = get_next_line(pg_full_text, &next_row_input);
	while (current_row_ind != g_num_rows || NULL != pg_thread_queue_head)
	{
		//create new thread
		if (next_row_input.start_time < g_current_time && current_row_ind != g_num_rows)
		{
			if (next_row_input.start_time == g_current_time - 1)
			{
				g_current_time--;
			}
			else
			{
				printf("the line in the input file are not in the right order\n");
				return ERROR_CODE;
			}
		}
		if (next_row_input.start_time == g_current_time)
		{

			init_new_thread(pg_thread_handle_arr + current_row_ind, p_thread_input_arr + current_row_ind, current_row_ind, g_current_time, next_row_input.work_time, next_row_input.physical_frame_num);
			wait_code = WaitForSingleObject(g_main_semapore, INFINITE);
			if (WAIT_OBJECT_0 != wait_code)
			{
				printf("Error when waiting for mutex\n");
				return ERROR_CODE;
			}
			//move to next row
			if (!strlen(p_next_line))
			{
				if (g_num_rows - 1 == current_row_ind)
				{
					current_row_ind++;
				}
				else
				{

					printf("unable to read line num %d\n", current_row_ind);
					return ERROR_CODE;
				}
			}
			else
			{
				p_next_line = get_next_line(p_next_line, &next_row_input);
				current_row_ind++;
			}
		}
		
		wait_code = WaitForSingleObject(g_page_table_mutex_handle, INFINITE);
		if (WAIT_OBJECT_0 != wait_code)
		{
			printf("Error when waiting for mutex\n");
			return ERROR_CODE;
		}
		//check_if_any_frame_end
		for (int frame_ind = 0; frame_ind < g_num_of_frames; frame_ind++)
		{
			if (pg_frame_table[frame_ind].end_time == g_current_time) 
			{
				if (ERROR_CODE == add_to_frame_queue(frame_ind))
				{
					return ERROR_CODE;
				}
			}

		}
		
		//cheack if there is a thread waiting
		if (ERROR_CODE == release_waiting_threads())
		{
			return ERROR_CODE;
		}

		ret_val = ReleaseMutex(g_page_table_mutex_handle);
		if (FALSE == ret_val)
		{
			printf("Error when releasing\n");
			return ERROR_CODE;
		}
		g_current_time++;
		
	}

	//wait for pages in frams to end
	DWORD multi_wait_code;
	multi_wait_code = WaitForMultipleObjects(g_num_rows,// num of objects to wait for
		pg_thread_handle_arr, //array of handels to wait for
		TRUE, // wait until all of the objects became signaled
		INFINITE // no time out
	);
	if (WAIT_OBJECT_0 != multi_wait_code)
	{
		printf("Error when waiting for threads to finsh\n");
		return ERROR_CODE;
	}
	return print_remains_frames();
}

//thread functions
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
		free_resource(pg_frame_queue_head, pg_thread_queue_head);
		exit(ERROR_CODE);
	}
	if (NULL == p_thread_id)
	{
		printf("Error when creating a thread");
		printf("Received null pointer");
		free_resource(pg_frame_queue_head, pg_thread_queue_head);
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
		free_resource(pg_frame_queue_head, pg_thread_queue_head);
		exit(ERROR_CODE);
	}
	return thread_handle;
}

static DWORD WINAPI page_thread(LPVOID lpParam)
{
	/// <summary>
	/// This function wait for a frame , if its not empty , evict it (from the page table) and then put the page data in table wite the frame index  
	/// </summary>
	/// <param name="lpParam">a pointer witch contains the info to the thread </param>
	/// <returns></returns>
	//the thread func input 
	DWORD wait_code;
	BOOL ret_val;
	BOOL update_table = 0;
	thread_input_t *p_current_page_input = (thread_input_t*)lpParam;
	int placement_time = p_current_page_input->call_time;
	int page_ind = p_current_page_input->virtual_page_num;
	int page_work_time = p_current_page_input->work_time;
	wait_code = WaitForSingleObject(g_page_table_mutex_handle, INFINITE);
	if (WAIT_OBJECT_0 != wait_code)
	{
		printf("Error when waiting for mutex\n");
		return ERROR_CODE;
	}
	/*
	* Critical Section
	* We can now safely access the page_table resource.	*/
	// is the page as a active frame?
	if (pg_page_table[page_ind].valid) 
	{
		update_frame_in_table(page_ind, placement_time, page_work_time);
		update_table = 1;
	}
	// is there a empty frame?
	else if (-1 != get_empty_frame_ind())
	{
		if (ERROR_CODE == update_empty_frame(page_ind, placement_time, page_work_time))
		{
			printf("was not able to updateempt empty frame\n");
			return ERROR_CODE;
		}
		update_table = 1;
	}
	if (!update_table)
	{
		//add thread to thread queue 
		if (ERROR_CODE == add_to_thread_queue(p_current_page_input->row_num))
		{
			return ERROR_CODE;
		}
	}
	ret_val = ReleaseMutex(g_page_table_mutex_handle);
	if (FALSE == ret_val)
	{
		printf("Error when releasing mutex\n");
		return ERROR_CODE;
	}
	//release semaphore for next thred
	ret_val = ReleaseSemaphore(
		g_main_semapore,
		1, 		/* Signal that exactly one cell was emptied */
		&wait_code);
	if (FALSE == ret_val)
	{
		printf("Error when releasing\n");
		return ERROR_CODE;
	}
	if (update_table)
	{
		return SUCCESS_CODE;
	}
	// we need to add the page to fram table - wait untill we can
	wait_code = WaitForSingleObject(pg_thread_semaphore_arr[p_current_page_input->row_num], INFINITE);
	if (wait_code != WAIT_OBJECT_0)
	{
		printf("Error when waiting for mutex\n");
		return ERROR_CODE;
	}
	//there is a empty_frame or frame that can be evicted
	wait_code = WaitForSingleObject(g_page_table_mutex_handle, INFINITE);
	//is the page we wont is in the frames that can be evicted
	if (pg_page_table[page_ind].valid)
	{
		update_frame_in_table(page_ind, placement_time, page_work_time);
	}
	//need to evicted frame and place the page
	else
	{
		evicte_and_place(placement_time, page_work_time, page_ind);
		g_avilable_frame--;
	}
	ret_val = ReleaseSemaphore(
		g_main_semapore,
		1, 		/* Signal that exactly one cell was emptied */
		&wait_code);
	if (FALSE == ret_val)
	{
		printf("Error when releasing\n");
		return ERROR_CODE;
	}
	ret_val = ReleaseMutex(g_page_table_mutex_handle);
	if (FALSE == ret_val)
	{
		printf("Error when releasing mutex\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}

//file functions
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

LPCSTR get_output_path(LPCSTR p_input_path)
{
	/// <summary>
	/// given a input path, take its dir path and add a default output file name and return as a output path
	/// </summary>
	/// <param name="p_input_path"></param>
	/// <returns></returns>
	char *new_line = NULL;
	char *next_line = NULL;
	char* output_path = NULL;
	int out_phath_len = strlen(OUTPUT_PATH) + strlen(p_input_path);
	int mem_retrun = 0;
	if (count_chars(p_input_path, '/'))
	{
		output_path = (char*)get_mem_resource(out_phath_len * sizeof(char),"error when alocate memory for output_path",&mem_retrun);
		if(NULL == output_path)
		{
			free_resource(pg_frame_queue_head, pg_thread_queue_head);
			return ERROR_CODE;
		}
		strcpy_s(output_path, out_phath_len, p_input_path);
		for (int ind = strlen(p_input_path); ind >0; ind--)
		{
			if (output_path[ind] == '/')
			{
				output_path[ind + 1] = '\0';
				strcat_s(output_path, out_phath_len, OUTPUT_PATH);
				return(output_path);
			}
		}
	}
	if (count_chars(p_input_path, '\\'))
	{
		output_path = (char*)get_mem_resource((strlen(OUTPUT_PATH) + strlen(p_input_path)) * sizeof(char), "error when alocate memory for output_path", &mem_retrun);
		if (NULL == output_path)
		{
			free_resource(pg_frame_queue_head, pg_thread_queue_head);
			return ERROR_CODE;
		}
		strcpy_s(output_path, out_phath_len, p_input_path);
		for (int ind = strlen(p_input_path); ind > 0; ind--)
		{
			if (output_path[ind] == '\\')
			{
				output_path[ind + 1] = '\0';
				strcat_s(output_path, out_phath_len, OUTPUT_PATH);
				return(output_path);
			}
		}
	}
	return OUTPUT_PATH;
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

int print_to_output_file(int time, int virtual_page_num, int physical_frame_num, char eviction_placement)
{
	/// <summary>
	/// print to output file log line from when ever we put new page or evict page from the page table 
	/// </summary>
	/// <param name="time of placement/eviction in frame</param>
	/// <param name="virtual_page_num"><virtual page number</param>
	/// <param name="physical_frame_num">number frame physical </param>
	/// <param name="eviction_placement">'P' for  placement and 'E' for evict</param>
	/// <returns></returns>
	DWORD num_char_written = 0;
	int write_buffer_len = floor(max(log10(time), 0)) + 1 + floor(max(log10(virtual_page_num), 0)) + 1 + floor(max(log10(physical_frame_num), 0)) + 1 + 1 + 6; // time+virtual_page_num+physical_frame_num+eviction_placement+"   \r\n"
	char* p_write_buffer = (char*)malloc(write_buffer_len * sizeof(char));
	if (NULL == p_write_buffer)
	{
		printf("Error when allocate memory for buffer to write to output\n");
		return ERROR_CODE;
	}
	sprintf_s(p_write_buffer, write_buffer_len, "%d %d %d %c\r\n", time, virtual_page_num, physical_frame_num, eviction_placement);
	num_char_written = write_to_file(pg_output_file_path, p_write_buffer, write_buffer_len - 1);
	free(p_write_buffer);
	if (num_char_written == write_buffer_len - 1)
	{
		return SUCCESS_CODE;
	}
	printf("Error when writing to output text\n");
	return ERROR_CODE;
}

//read input functions
int count_chars(const char* p_string, char ch)
{
	long count = 0;
	long i;
	long length = strlen(p_string);
	for (i = 0; i < length; i++)
	{
		if (p_string[i] == ch)
		{
			count++;
		}
	}

	return count;
}

char* get_next_line(char* p_line, row_obj_t* p_next_line_params)
{
	/// <summary>
	/// take the p_line text and cut the first line of it find the argumants in the line and set them in p_next_line_params struct
	/// </summary>
	/// <param name="p_line">string of text</param>
	/// <param name="next_line_params">pointer of the struct witch will hold the info of the command</param>
	/// <returns>pointer to the string text that have the ramaining commands</returns>
	const char seprete[] = "\n";
	char* p_new_line = NULL;
	char* p_next_line = NULL;
	char* p_input_param = NULL;
	int input_arr[NUM_ROW_PARAMS] = { 0 };
	p_new_line = strtok_s(p_line, seprete, &p_next_line);
	for (int param_ind = 0; param_ind < NUM_ROW_PARAMS; param_ind++)
	{
		p_input_param = strtok_s(p_new_line, " ", &p_new_line);
		input_arr[param_ind] = atoi(p_input_param);
	}
	p_next_line_params->start_time = input_arr[0];//the start time
	p_next_line_params->physical_frame_num = input_arr[1] / PAGE_SIZE;//the number of page that corseponds to the adress
	p_next_line_params->work_time = input_arr[2];//the time the page works
	return p_next_line;
};

// table functions
int create_tabels()
{
	int memory_aloc_check = 0;
	//alocate memory for page table and frame table
	pg_page_table = (page_obj_t*)get_mem_resource(g_num_of_pages * sizeof(page_obj_t), "Error when allocating memory for page_table\n", &memory_aloc_check);
	if (ERROR_CODE == memory_aloc_check)
	{
		free_resource(pg_frame_queue_head, pg_thread_queue_head);
		return ERROR_CODE;
	}
	pg_frame_table = (frame_obj_t*)get_mem_resource(g_num_of_frames * sizeof(frame_obj_t), "Error when allocating memory for farme_table\n", &memory_aloc_check);
	if (ERROR_CODE == memory_aloc_check)
	{
		free_resource(pg_frame_queue_head, pg_thread_queue_head);
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}
void initial_tables()
{
	for (int frame_ind = 0; frame_ind < g_num_of_frames; frame_ind++)
	{
		pg_frame_table[frame_ind].end_time = NOT_USE_FRAME_END_TIME;
	}
	for (int page_ind = 0; page_ind < g_num_of_pages; page_ind++)
	{
		pg_page_table[page_ind].valid = NOT_VALID;
	}
}
int init_sync_obj()
{
	int retrn_val = SUCCESS_CODE;
	g_main_semapore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - */
		1,		/* Maximum Count */
		NULL); /* un-named */  
	if (NULL == g_main_semapore)
	{
		printf("error when Create Semaphore\n");
		retrn_val = ERROR_CODE;
	}
	g_page_table_mutex_handle = CreateMutex(
		NULL,	/* default security attributes */
		FALSE,	/* initially not owned */
		NULL);	/* unnamed mutex */  
	if (NULL == g_page_table_mutex_handle)
	{
		printf("error when Create Semaphore\n");
		retrn_val = ERROR_CODE;
	}
	int resource_aloc_check = 0;
	pg_thread_semaphore_arr = (HANDLE*)get_mem_resource((g_num_rows) * sizeof(HANDLE), "Error when allocating memory for semaphore_arr\n", &resource_aloc_check);
	if (NULL == pg_thread_semaphore_arr)
	{
		printf("Error when allocating memory for semaphore_arr\n");
		free_resource(pg_frame_queue_head, pg_thread_queue_head);
		return ERROR_CODE;
	}
	//init all semaphore
	for (int ind = 0; ind < g_num_rows; ind++)
	{
		if (ERROR_CODE == retrn_val)
		{
			pg_thread_semaphore_arr[ind] = NULL;
		}
		else
		{
			pg_thread_semaphore_arr[ind] = CreateSemaphore(
				NULL,	/* Default security attributes */
				0,		/* Initial Count - */
				1,		/* Maximum Count */
				NULL); /* un-named */ 
			if (NULL == pg_thread_semaphore_arr[ind])
			{
				printf("error when Create Semaphore\n");
				retrn_val = ERROR_CODE;
			}
		}
	}
	return retrn_val;
}

void init_new_thread(HANDLE *p_thread,thread_input_t *p_thread_in_arr,int row_num,int current_time,int work_time, int virtual_page_num)
{
	DWORD thread_id;
	//init data in thread_input
	p_thread_in_arr->row_num = row_num;
	p_thread_in_arr->call_time = current_time;
	p_thread_in_arr->work_time = work_time;
	p_thread_in_arr->virtual_page_num = virtual_page_num;
	*p_thread = CreateThreadSimple(page_thread, p_thread_in_arr, &thread_id);

}


//queue functions

int get_empty_frame_ind()
{
	for (int frame_ind = 0; frame_ind < g_num_of_frames; frame_ind++)
	{
		if (pg_frame_table[frame_ind].end_time == NOT_USE_FRAME_END_TIME) 
		{
			return frame_ind;
		}
	}
		return -1;
}

void remove_frame_from_queue(int frame_num)
{
	//if in queue remove it
	if (NULL != pg_frame_queue_head)
	{
		if (pg_frame_queue_head->index == frame_num)
		{
			queue_cell_t* p_temp_cell = pg_frame_queue_head;
			pg_frame_queue_head = pg_frame_queue_head->p_next;
			free(p_temp_cell);
			g_avilable_frame--;

		}
		else
		{
			queue_cell_t* p_perv_cell = pg_frame_queue_head;
			queue_cell_t* p_itr_cell = pg_frame_queue_head->p_next;
			while (NULL != p_itr_cell)
			{
				if (p_itr_cell->index == frame_num)
				{
					p_perv_cell->p_next = p_itr_cell->p_next;
					g_avilable_frame--;
					free(p_itr_cell);
					break;
				}
				p_perv_cell = p_itr_cell;
				p_itr_cell = p_itr_cell->p_next;
			}
		}
	}
	
}

int add_to_frame_queue(int frame_ind)
{
	//update queue 
	queue_cell_t* p_queue_new_cell = (queue_cell_t*)malloc(sizeof(queue_cell_t));
	if (NULL == p_queue_new_cell)
	{
		printf("Error when allocating memory for new queue_cell\n");
		return ERROR_CODE;
	}
	p_queue_new_cell->index = frame_ind;
	p_queue_new_cell->p_next = NULL;
	//if head not init init it
	if (NULL == pg_frame_queue_head)
	{
		pg_frame_queue_head = p_queue_new_cell;
	}
	//add frame cell at the end of the queue
	else
	{
		//
		queue_cell_t* p_temp_cell = pg_frame_queue_head;
		while (NULL != p_temp_cell->p_next)
		{
			p_temp_cell = p_temp_cell->p_next;
		}
		p_temp_cell->p_next = p_queue_new_cell;
	}
	g_avilable_frame++;
	return SUCCESS_CODE;
}

int release_waiting_threads()
{
	BOOL release_res;
	LONG previous_count;
	DWORD wait_code;
	BOOL ret_val;
	while (NULL != pg_thread_queue_head && g_avilable_frame > 0)
	{
		release_res = ReleaseSemaphore(
			pg_thread_semaphore_arr[pg_thread_queue_head->index],
			1, 		/* Signal that exactly one cell was emptied */
			&previous_count);
		if (release_res == FALSE)
		{
			printf("Error when realsing the thread semaphore\n");
			return ERROR_CODE;
		}
		queue_cell_t* p_temp_cell = pg_thread_queue_head->p_next;
		free(pg_thread_queue_head);
		pg_thread_queue_head = p_temp_cell;
		ret_val = ReleaseMutex(g_page_table_mutex_handle);
		if (FALSE == ret_val)
		{
			printf("unable to release mutex\n");
			return ERROR_CODE;
		}
		wait_code = WaitForSingleObject(g_main_semapore, INFINITE);
		if (WAIT_OBJECT_0 != wait_code)
		{
			printf("Error when waiting for mutex\n");
			return ERROR_CODE;
		}
		wait_code = WaitForSingleObject(g_page_table_mutex_handle, INFINITE);
		if (WAIT_OBJECT_0 != wait_code)
		{
			printf("Error when waiting for mutex\n");
			return ERROR_CODE;
		}
	}
	return SUCCESS_CODE;

}

int print_remains_frames()
{
	//find last end_time
	int last_end_time = 0;
	for (int frame_ind = 0; frame_ind < g_num_of_frames; frame_ind++)
	{
		last_end_time = max(pg_frame_table[frame_ind].end_time, last_end_time);
	}
	//print all the end time in frame tabel
	for (int frame_ind = 0; frame_ind < g_num_of_frames; frame_ind++)
	{
		if (-1 != pg_frame_table[frame_ind].end_time)
		{
			if (ERROR_CODE == print_to_output_file(last_end_time, pg_frame_table[frame_ind].page_number, frame_ind, EVICT_CODE))
			{
				return ERROR_CODE;

			}
		}
	}
	return SUCCESS_CODE;
}

void evicte_and_place(int placement_time,int work_time, int page_ind)
{
	//need to evicted frame and place the page
	
	int frame_to_evict = pg_frame_queue_head->index;
	int evict_time = max(pg_frame_table[frame_to_evict].end_time, placement_time);
	int page_to_unvalid = pg_frame_table[frame_to_evict].page_number;
	print_to_output_file(evict_time, page_to_unvalid, frame_to_evict, EVICT_CODE);
	//updeate frame tabel
	pg_frame_table[frame_to_evict].end_time = evict_time + work_time;
	pg_frame_table[frame_to_evict].page_number = page_ind;
	//update page tabel
	pg_page_table[page_to_unvalid].valid = NOT_VALID;
	pg_page_table[page_ind].end_of_use = evict_time + work_time;
	pg_page_table[page_ind].valid = VALID;
	pg_page_table[page_ind].frame_number = frame_to_evict;
	print_to_output_file(evict_time, page_ind, frame_to_evict, PLACEMENT_CODE);
	//remove frame from queue
	queue_cell_t* p_temp_queue_head = pg_frame_queue_head->p_next;
	free(pg_frame_queue_head);
	pg_frame_queue_head = p_temp_queue_head;
}

void update_frame_in_table(int page_ind, int placement_time,int page_work_time)
{
	//update end_of_use in page table TODO change name to end_of_use
	int new_end_of_use = max(pg_page_table[page_ind].end_of_use, placement_time + page_work_time);
	if (new_end_of_use  ==  g_current_time)
	{
		new_end_of_use = new_end_of_use + page_work_time;
	}
	pg_page_table[page_ind].end_of_use = new_end_of_use;
	//update end_of_use in frame table
	pg_frame_table[pg_page_table[page_ind].frame_number].end_time = new_end_of_use;
	//if in queue remove it
	remove_frame_from_queue(pg_page_table[page_ind].frame_number);
	// no need to print
}

int update_empty_frame(int page_ind, int placement_time, int page_work_time)
{
	int empty_frame_ind = get_empty_frame_ind();
	if (-1 == empty_frame_ind)
	{
		return ERROR_CODE;
	}
	int end_time = placement_time + page_work_time;
	//updeate frame tabel
	pg_frame_table[empty_frame_ind].end_time = end_time;
	pg_frame_table[empty_frame_ind].page_number = page_ind;
	//update page tabel
	pg_page_table[page_ind].end_of_use = end_time;
	pg_page_table[page_ind].valid = VALID;
	pg_page_table[page_ind].frame_number = empty_frame_ind;
	return print_to_output_file(placement_time, page_ind, empty_frame_ind, PLACEMENT_CODE);
}

int add_to_thread_queue(int thread_ind)
{
	//add thread to thread queue 
	queue_cell_t* p_new_thread_queue_cell = (queue_cell_t*)malloc(sizeof(queue_cell_t));
	if (NULL == p_new_thread_queue_cell)
	{
		printf("Error when allocating memory for new thread queue_cell\n");
		return ERROR_CODE; 
	}
	p_new_thread_queue_cell->index = thread_ind;
	p_new_thread_queue_cell->p_next = NULL;
	//put in the end of the thread cain
	if (NULL == pg_thread_queue_head)
	{
		pg_thread_queue_head = p_new_thread_queue_cell;
	}
	else
	{
		queue_cell_t* p_temp_cell = pg_thread_queue_head;
		while (NULL != p_temp_cell->p_next)
		{
			p_temp_cell = p_temp_cell->p_next;
		}
		p_temp_cell->p_next = p_new_thread_queue_cell;
	}
	return SUCCESS_CODE;
}

//memory mange functions

void* get_mem_resource(size_t size, const char* error_msg, int* p_return_value)
{
	/// <summary>
	///  aloocated memory and add it to a linked list
	/// </summary>
	/// <param name="size">the size we will use for our alloction</param>
	/// <param name="p_error_msg">if the alloction fail return this message </param>
	/// <param name="p_return_valu">set the value of return_value to ERROR_CODE or SUCCESS_CODE </param>
	/// <returns> pointer to the aloocated memory</returns>
	void* p_resource = malloc(size);
	if (NULL == p_resource)
	{
		printf(error_msg);
		*p_return_value = ERROR_CODE;
	}
	else
	{
		mem_queue_cell_t* new_resource_cell = (mem_queue_cell_t*)malloc(sizeof(mem_queue_cell_t));
		if (NULL == new_resource_cell)
		{
			printf("unable to alocate memory for mem_queue_cell_t struct\n ");
			free(p_resource);
			p_resource = NULL;
			*p_return_value = ERROR_CODE;
		}
		else
		{
			new_resource_cell->p_pointer = p_resource;
			new_resource_cell->p_next = pg_resource_head;
			pg_resource_head = new_resource_cell;
			*p_return_value = SUCCESS_CODE;
		}
	}
	return p_resource;
}


void free_resource(queue_cell_t *p_queue_head_1, queue_cell_t *p_queue_head_2)
{
	//free threads and handels
	if (ERROR_CODE == close_handels())
	{
		printf("unable to close all  threads and handels\n");
	}
	//free memory resource
	mem_queue_cell_t* p_temp_resource_cell = NULL;
	while (NULL != pg_resource_head)
	{
		p_temp_resource_cell = pg_resource_head;
		pg_resource_head = pg_resource_head->p_next;
		free(p_temp_resource_cell->p_pointer);
		free(p_temp_resource_cell);
	}
	queue_cell_t* temp_queue_cell = NULL;
	while (NULL != p_queue_head_1)
	{
		temp_queue_cell = p_queue_head_1;
		p_queue_head_1 = p_queue_head_1->p_next;
		free(temp_queue_cell);
	}
	while (NULL != p_queue_head_2)
	{
		temp_queue_cell = p_queue_head_2;
		p_queue_head_2 = p_queue_head_2->p_next;
		free(temp_queue_cell);
	}

}

int close_handels()
{
	/// <summary>
	/// this code will close all the handles and threads we used
	/// </summary>
	/// <returns></returns>
	BOOL ret_val = TRUE;
	DWORD exit_code;
	DWORD thread_out_val = SUCCESS_CODE;
	if (NULL != g_page_table_mutex_handle)
	{
		ret_val = ret_val && CloseHandle(g_page_table_mutex_handle);
	}
	if (NULL != g_main_semapore)
	{
		ret_val = ret_val && CloseHandle(g_main_semapore);
	}
	for (int ind = 0; ind < g_num_rows; ind++)
	{
		//close the semaphore array
		if (NULL != pg_thread_semaphore_arr[ind])
		{
			ret_val = ret_val && CloseHandle(pg_thread_semaphore_arr[ind]);
		}
		if (NULL != pg_thread_handle_arr[ind])
		{

			ret_val = GetExitCodeThread(pg_thread_handle_arr[ind], &exit_code);
			if (ERROR_RET == ret_val || STILL_ACTIVE == exit_code)
			{
				//if thread didnt end terminate it
				TerminateThread(pg_thread_handle_arr[ind], ERROR_CODE);
				printf("Error when getting thread exit code\n terminate thread\n");
				thread_out_val = ERROR_CODE;
			}
			//if thread retrun exit code of error 
			if (ERROR_CODE == exit_code)
			{
				printf("one of the thread give error exit code\n");
				thread_out_val = ERROR_CODE;
			}
			/* Close thread handle */
			ret_val = CloseHandle(pg_thread_handle_arr[ind]);
			if (ERROR_RET == ret_val)
			{
				printf("Error when closing thread handle\n");
				thread_out_val = ERROR_CODE;
			}
		}
	}
	if (ERROR_RET == ret_val || ERROR_CODE == thread_out_val)
	{
		printf("Error when closing handles\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;

}