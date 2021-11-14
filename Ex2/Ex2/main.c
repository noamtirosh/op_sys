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

static int real_weight = 0;
static int human_weight = 0;
static int english_weight = 0;
static int school_weight = 0;
static int num_schools = 0;

school* p_school_array  = NULL;


int main(int argc, char* argv[])
{
	//TODO remove thos local vars
	DWORD thread_id;
	HANDLE thread_handle_arr[MAX_NUM_OF_THREADS];
	DWORD wait_code;
	DWORD multi_wait_code;
	DWORD exit_code;
	BOOL ret_val;

	DWORD num_of_open_threads = 0;

	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
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
		printf("Error when allocating memory");
		return ERROR_CODE;
	}
	//TODO to think about MAX_NUM_OF_THREADS 
	//this loop only open all threads
	for (int i = 0; i < num_schools; i++)
	{
		(p_school_array + i)->school_num = i;
		if (ERROR_CODE == create_new_school_thread((p_school_array + i), &num_of_open_threads, thread_handle_arr))
		{
			//TODO add error if create new school fail
		}
	}
	wait_code = wait_for_remain_schools(num_of_open_threads, thread_handle_arr);
	if (ERROR_CODE == wait_code)
	{
		//TODO add error if wait for thread to finsh fail
	}

	free(p_school_array);



}

DWORD wait_for_remain_schools(const DWORD p_n_open_threads,HANDLE* handle_arr)
{
	DWORD exit_code;
	BOOL ret_val;
	DWORD multi_wait_code;
	multi_wait_code = WaitForMultipleObjects(p_n_open_threads,// num of objects to wait for
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
		for (int i = 0; i < p_n_open_threads; i++)
		{
			multi_wait_code = WaitForMultipleObjects(p_n_open_threads,// num of objects to wait for
				handle_arr, //array of handels to wait for
				FALSE, // wait until all of the objects became signaled
				INFINITE // no time out
			);
			/* Check the DWORD returned by MathThread */
			ret_val = GetExitCodeThread(handle_arr[multi_wait_code], &exit_code);
			if (0 == ret_val)
			{
				printf("Error when getting thread exit code\n");
				//TODO do we need to add return ERROR_CODE;
			}
			//if thread retrun exit code of error TODO
			if(ERROR_CODE == exit_code)
			{

			}
			/* Close thread handle */
			ret_val = CloseHandle(handle_arr[multi_wait_code]);
			if (FALSE == ret_val)
			{
				printf("Error when closing\n");
				return ERROR_CODE;
			}


		}

	}
	return SUCCESS_CODE;
}

DWORD create_new_school_thread(LPVOID school_params,DWORD *p_n_open_threads, HANDLE *handle_arr)
{
	DWORD multi_wait_code;
	DWORD wait_code;
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

		}
		//for one of the threads finsh

		/* Check the DWORD returned by MathThread */
		ret_val = GetExitCodeThread(handle_arr[multi_wait_code], &exit_code);
		if (0 == ret_val)
		{
			printf("Error when getting thread exit code\n");
			//TODO do we need to add return ERROR_CODE;
		}
		/* Close thread handle */
		ret_val = CloseHandle(handle_arr[multi_wait_code]);
		if (FALSE == ret_val)
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