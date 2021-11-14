#include <stdio.h>
#include <windows.h>
#include "HardCodeData.h"
#include <string.h>

// Types -----------------------------------------------------------------------

typedef struct
{
	int school_num;
} school;


// function declration
static DWORD WINAPI school_thread(LPVOID lpParam);
static HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine,LPVOID p_thread_parameters,	LPDWORD p_thread_id);
DWORD wait_for_remain_schools(const DWORD p_n_open_threads, HANDLE* handle_arr);
void school_function(int school_ind);
DWORD create_new_school_thread(LPVOID school_params, DWORD* p_n_open_threads, HANDLE* handle_arr);

static int real_weight = 0;
static int human_weight = 0;
static int english_weight = 0;
static int school_weight = 0;
static int num_schools = 0;

school* p_school_array  = NULL;


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
	p_school_array = (school*)malloc(num_schools*sizeof(school));
	if (NULL == p_school_array)
	{
		printf("Error when allocating memory\n");
		return ERROR_CODE;
	}
	//create thread for ech school 
	for (int i = 0; i < num_schools; i++)
	{
		(p_school_array + i)->school_num = i;
		if (ERROR_CODE == create_new_school_thread((p_school_array + i), &num_of_open_threads, thread_handle_arr))
		{
			printf("was not able to create new thread for school num %d\n",i);
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
		return ERROR_CODE;
	}
	else
	{
		//close all handle
		for (DWORD i = 0; i < num_open_threads; i++)
		{
			ret_val = GetExitCodeThread(handle_arr[i], &exit_code);
			if (0 == ret_val)
			{
				printf("Error when getting thread exit code\n");
				return ERROR_CODE;
			}
			//if thread retrun exit code of error 
			if(ERROR_CODE == exit_code)
			{
				printf("one of the thread give error exit code\n");
				return ERROR_CODE;
			}
			/* Close thread handle */
			ret_val = CloseHandle(handle_arr[i]);
			if (FALSE == ret_val)
			{
				printf("Error when closing thread handle\n");
				return ERROR_CODE;
			}

		}

	}
	return SUCCESS_CODE;
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
		if (ERROR_CODE == ret_val)
		{
			printf("Error when getting thread exit code\n");
			return ERROR_CODE;
		}
		/* Close thread handle */
		ret_val = CloseHandle(handle_arr[multi_wait_code]);
		if (ERROR_CODE == ret_val)
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
	school* p_current_school = NULL;
	p_current_school = (school*)lpParam;
	school_function(p_current_school->school_num);
	return SUCCESS_CODE;
}

void school_function(int school_ind)
{
	printf("school num: %d\n", school_ind);
}