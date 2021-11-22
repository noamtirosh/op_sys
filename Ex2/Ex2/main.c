#include <stdio.h>
#include <windows.h>
#include "HardCodeData.h"
#include <string.h>

int school_num;



// function declration
static DWORD WINAPI school_thread(LPVOID lpParam);
static HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine,LPVOID p_thread_parameters,	LPDWORD p_thread_id);
DWORD wait_for_remain_schools(const DWORD p_n_open_threads, HANDLE* handle_arr);
DWORD school_function(int school_ind);
DWORD create_new_school_thread(LPVOID school_params, DWORD* p_n_open_threads, HANDLE* handle_arr);
DWORD get_file_len(LPCSTR p_file_name);
DWORD write_to_file(LPCSTR p_file_name, LPVOID p_buffer, const DWORD buffer_len);
DWORD read_from_file(LPCSTR p_file_name, long offset, LPVOID p_buffer, const DWORD buffer_len);
int read_and_write_schools(char** p_files_names, int school_ind);


static int real_weight = 0;
static int human_weight = 0;
static int english_weight = 0;
static int school_weight = 0;
static int num_schools = 0;


int* p_school_array  = NULL;


int main(int argc, char* argv[])
{
	HANDLE thread_handle_arr[MAX_NUM_OF_THREADS];
	DWORD wait_code;
	DWORD num_of_open_threads = 0;

	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERROR_CODE;
	}
	//create Results dir
	if (0 == CreateDirectoryA(RESULT_DIR_PATH, NULL))
	{
		printf("Error when create folder : %s\n", RESULT_DIR_PATH);
		return ERROR_CODE;
	}
	//get global params
	num_schools = atoi(argv[NUM_OF_SCHOOLS_IND]);
	real_weight = atoi(argv[REAL_CLASS_WEIGHT_IND]);
	human_weight = atoi(argv[HUMAN_CLASS_WEIGHT_IND]);
	english_weight = atoi(argv[ENGLISH_CLASS_WEIGHT_IND]);
	school_weight = atoi(argv[SCHOLL_WEIGHT_IND]);

	//create tread for ech school
	//Allocate memory for thread parameters
	p_school_array = (int*)malloc(num_schools*sizeof(int));
	if (NULL == p_school_array)
	{
		printf("Error when allocating memory\n");
		return ERROR_CODE;
	}
	//create thread for ech school 
	for (int i = 0; i < num_schools; i++)
	{
		*(p_school_array + i) = i;
		if (ERROR_CODE == create_new_school_thread((p_school_array + i), &num_of_open_threads, thread_handle_arr))
		{
			printf("was not able to create new thread for school num %d\n",i);
			//wait all threds close or trminate remain
			wait_for_remain_schools(num_of_open_threads, thread_handle_arr);
			return ERROR_CODE;
		}
	}
	wait_code = wait_for_remain_schools(num_of_open_threads, thread_handle_arr);
	if (ERROR_CODE == wait_code)
	{
		printf("error wen waitting for all threads to finsh\n");
		return ERROR_CODE;
	}
	//free memory for thread parameters
	free(p_school_array);
	return SUCCESS_CODE;


}

/// <summary>
/// wait until all the threads that open became signaled
/// and cloose akk their handls if they gave success exit code.
/// </summary>
/// <param name="num_open_threads"> num of open threads</param>
/// <param name="handle_arr"> array of handles for the threads </param>
/// <returns> SUCCESS_CODE if finsh to close all handles ERROR_CODE else</returns>
DWORD wait_for_remain_schools(const DWORD num_open_threads,HANDLE* handle_arr)
{
	DWORD exit_code;
	BOOL ret_val;
	BOOL return_val = SUCCESS_CODE;
	DWORD multi_wait_code;
	multi_wait_code = WaitForMultipleObjects(num_open_threads,// num of objects to wait for
		handle_arr, //array of handels to wait for
		TRUE, // wait until all of the objects became signaled
		INFINITE // no time out
	);
	if (WAIT_OBJECT_0 != multi_wait_code)
	{	
		//TODO if get error do we need to close threads?
		printf("Error when waiting\n");
		return_val =  ERROR_CODE;
	}
	//close all handle
	for (DWORD i = 0; i < num_open_threads; i++)
	{
		ret_val = GetExitCodeThread(handle_arr[i], &exit_code);
		if (ERROR_RET == ret_val || STILL_ACTIVE == exit_code)
		{
			TerminateThread(handle_arr[i], ERROR_CODE);
			printf("Error when getting thread exit code\n");
			return_val = ERROR_CODE;
		}
		//if thread retrun exit code of error 
		if(ERROR_CODE == exit_code)
		{
			printf("one of the thread give error exit code\n");
			return_val =  ERROR_CODE;
		}
		/* Close thread handle */
		ret_val = CloseHandle(handle_arr[i]);
		if (ERROR_RET == ret_val)
		{
			printf("Error when closing thread handle\n");
			return_val = ERROR_CODE;
		}

	}
	return return_val;
}

void force_close(const DWORD num_open_threads, HANDLE* handle_arr)
{

}

/// <summary>
/// get params input for create new threads and num of open threads and handls array 
/// create new thread and put it handle in the arry if num of open threads not exide MAX_NUM_OF_THREADS
/// else wait for one of the threads to be signaled and replace its handle.
/// </summary>
/// <param name="school_params"></param>
/// <param name="p_n_open_threads"></param>
/// <param name="handle_arr"></param>
/// <returns></returns>
DWORD create_new_school_thread(LPVOID school_params,DWORD *p_n_open_threads, HANDLE *handle_arr)
{
	DWORD multi_wait_code;
	DWORD exit_code;
	BOOL ret_val;
	DWORD thread_id;
	if (*p_n_open_threads < MAX_NUM_OF_THREADS)
	{
		(*p_n_open_threads)++;
		handle_arr[(*p_n_open_threads)-1] = CreateThreadSimple(school_thread, school_params, &thread_id);
		if (NULL == handle_arr[(*p_n_open_threads) - 1])
		{
			printf("Error when creating thread\n");
			return ERROR_CODE;
		}
	}
	else
	{
		multi_wait_code = WaitForMultipleObjects(*p_n_open_threads,// num of objects to wait for
			handle_arr, //array of handels to wait for
			FALSE, // wait until one of the objects became signaled
			INFINITE // no time out
		);
		//handle errors  with WaitForMultipleObjects
		if (WAIT_FAILED == multi_wait_code)
		{
			printf("Error when waitting for multiple threads \n");
			return ERROR_CODE;
		}
		//for one of the threads finsh

		/* Check the DWORD returned by MathThread */
		ret_val = GetExitCodeThread(handle_arr[multi_wait_code], &exit_code);
		if (ERROR_RET == ret_val)
		{
			printf("Error when getting thread exit code\n");
			return ERROR_CODE;
		}
		//if thread retrun exit code of error 
		if (ERROR_CODE == exit_code)
		{
			printf("one of the thread give error exit code\n");
			return ERROR_CODE;
		}
		/* Close thread handle */
		ret_val = CloseHandle(handle_arr[multi_wait_code]);
		if (ERROR_RET == ret_val)
		{
			printf("Error when closing\n");
			return ERROR_CODE;
		}
		handle_arr[multi_wait_code] = CreateThreadSimple(school_thread, school_params, &thread_id);
		if (NULL == handle_arr[multi_wait_code])
		{
			printf("Error when creating thread\n");
			return ERROR_CODE;
		}
	}
	return SUCCESS_CODE;



}



static HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine,
	LPVOID p_thread_parameters,
	LPDWORD p_thread_id)
{
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



static DWORD WINAPI school_thread(LPVOID lpParam)
{
	int* p_current_school = NULL;
	p_current_school = (int*)lpParam;
	return school_function(*p_current_school);
}

/// <summary>
/// the function work on a single school it gets the number of the school and the grade componnet and build
/// text file witch hold for each student the final grades
/// </summary>
/// <param name="school_ind">the number of the school</param>
/// <param name="school_grade_waight_commponents">the weight of each grade</param>
DWORD school_function(const int school_ind)
{
	const char file_names[][MAX_SIZE_NAME] = { "Real","Human","Eng","Eval" };//array with the names of the four sybjects
	int path_len = 2 * MAX_SIZE_NAME + 3 + num_schools / 10 + 1 + strlen(TXT_STRING) + 1;// +./ +file_names +/+file_names+TXT_STRING+/0
	char* p_file_name;//name of the files
	char* p_files_names[NUM_OF_GRADE_COMPONENTS];
	for (int i = 0; i < NUM_OF_GRADE_COMPONENTS; i++)
	{
		p_file_name = (char*)malloc(path_len * sizeof(char));//aloction of the names 
		if (NULL == p_file_name)
		{
			printf("wasnt able to allocate memory\n");
			i--;
			for (; i > 0; i--)
			{
				free(p_files_names[i]);
			}
			return ERROR_CODE;
		}
		p_files_names[i] = p_file_name;
		sprintf_s(p_file_name, path_len, "./%s/%s%d%s", file_names[i], file_names[i], school_ind, TXT_STRING);
	}
	if (ERROR_CODE == read_and_write_schools(p_files_names, school_ind))
	{
		printf("wasnt able to read or write to files\n");
		for (int i = 0; i < NUM_OF_GRADE_COMPONENTS; i++)
		{
			free(p_files_names[i]);
		}
		return ERROR_CODE;

	}
	for (int i = 0; i < NUM_OF_GRADE_COMPONENTS; i++)
	{
		free(p_files_names[i]);
	}
	printf("finsh school %d successfully\n", school_ind);
	return SUCCESS_CODE;
}

/// <summary>
/// the file opens for read the four grades files calculate the finale grade and than writes 
/// the final grade in result<n>.txt file
/// </summary>
/// <param name="p_file_names">array that contanins the all the file names</param>
/// <param name="array_grade_eval">array that contains the waight of the grades</param>
/// <returns>If the function sucssesful return SUCCESS_CODE else return ERROR_CODE</returns>
int read_and_write_schools(char** p_files_names, int school_ind)
{
	DWORD files_offset[NUM_OF_GRADE_COMPONENTS] = { 0 };//the ofset of each of the four files from beginnig
	DWORD file_len = get_file_len(p_files_names[0]);//gets one file length to the while condition
	int result_file_name_len = OUTPUT_FILE_NAME_LEN + (num_schools / 10) + 2;// len of"./Results/Results.txt" + num of dig + \0
	char* p_result_file_name;
	p_result_file_name = (char*)malloc(result_file_name_len * sizeof(char));//aloction of the names 
	const int school_grade_waight_commponents[] = { real_weight ,human_weight,english_weight,school_weight };
	float temp_grade = 0;
	char result_grade[FILE_BUFFER] = { 0 };
	char read_buffer[FILE_BUFFER] = { 0 };

	if (NULL == p_result_file_name)
	{
		printf("wasnt able to allocate memory\n");
		return ERROR_CODE;
	}
	while (file_len > files_offset[0])
	{
		char* p_help_word;//the text we are readinga after the first \n
		int next_line_ind = 0;
		temp_grade = 0;
		int grades_per_student[NUM_OF_GRADE_COMPONENTS]={0};
		for (int i = 0; i < NUM_OF_GRADE_COMPONENTS; i++) {
			DWORD num_bytes_readen = read_from_file(p_files_names[i], files_offset[i], read_buffer, FILE_BUFFER);
			if (FILE_BUFFER != num_bytes_readen )
			{
				if ((get_file_len(p_files_names[i]) - files_offset[i]) == num_bytes_readen)
				{
					//finsh file
					files_offset[i] = get_file_len(p_files_names[i]);

				}
				else
				{
					printf("unable to read message from file: %s\n", p_files_names[i]);
					free(p_result_file_name);
					return ERROR_CODE;
				}

			}
			else
			{
				p_help_word = strchr(read_buffer, '\n'); // TODO if end of file and not end with \n
				*p_help_word = '\0';
			}
			next_line_ind = strlen(read_buffer);
			files_offset[i] += (next_line_ind + 1);		
			temp_grade += atoi(read_buffer) * school_grade_waight_commponents[i];
			//the grade of students 
		}
		temp_grade = temp_grade / 100;
		sprintf_s(result_grade, FILE_BUFFER, "%d%s", (int)temp_grade, "\r\n");
		sprintf_s(p_result_file_name, result_file_name_len, "%s/%s%d%s", RESULT_DIR_PATH, RESULT_FILE_NAME, school_ind, TXT_STRING);
		DWORD line_len = strlen(result_grade);
		if (line_len != write_to_file(p_result_file_name, result_grade, line_len))
		{
			printf("unable write message to file: %s\n", p_result_file_name);
			free(p_result_file_name);
			return ERROR_CODE;
		}
		
	}
	free(p_result_file_name);
	return SUCCESS_CODE;
}


/// <summary>
/// use CreateFile with default params for read or write and return the handle
/// </summary>
/// <param name="file_name">file path</param>
/// <param name="mode">mode : 'r'/'R' for read acsses 'w'/'W' for write </param>
/// <returns>handle to file</returns>
HANDLE create_file_simple(LPCSTR p_file_name, char mode)
{
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

/// </summary>
/// <param name="file_name">path to file to read</param>
/// <param name="offset"> offset in bytes from beginning </param>
/// <param name="p_buffer"> pointer to buffer to put message</param>
/// <param name="buffer_len">length of message to read</param>
/// <returns>num of bytes readed from file</returns>
DWORD read_from_file(LPCSTR p_file_name, long offset, LPVOID p_buffer, const DWORD buffer_len)
{
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

/// <summary>
///  write message from p_buffer in len buffer_len to file 
/// </summary>
/// <param name="file_name">path to file to write the buffer</param>
/// <param name="p_buffer">pointer to buffer</param>
/// <param name="buffer_len"> the length of the buffer</param>
/// <returns> num of bytes written to file</returns>
DWORD write_to_file(LPCSTR p_file_name, LPVOID p_buffer, const DWORD buffer_len)
{
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

DWORD get_file_len(LPCSTR p_file_name)
{
	HANDLE h_file = create_file_simple(p_file_name, 'r');
	DWORD current_file_position;
	//move file pointer in ofset
	current_file_position = SetFilePointer(
		h_file,	//handle to file
		0, // number of bytes to move the file pointer
		NULL, // 
		FILE_END);
	CloseHandle(h_file);
	return current_file_position;
}