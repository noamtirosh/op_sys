//////////////////////////////////////
/*
* Authors – Noam Tirosh 314962945, Daniel Kogan 315786418
* project : Father
* Description: get message file path and key file path, for ech 16 bytes in the message file, call process Son with 
*	increasing offset.
*/
/////////////////////////////////////

#include "HardCodeData.h"
#include <stdio.h>
#include <windows.h>
#include <string.h>




//function declaration
BOOL CreateProcessSimple(LPTSTR p_commandLine, PROCESS_INFORMATION* p_processInfoPtr);
DWORD create_son_process(LPTSTR p_command);
HANDLE create_file_simple(LPCSTR p_file_name, char mode);
DWORD get_file_len(LPCSTR p_file_name);


/// <summary>
/// open file with message, opens a procces for every 16 bytes of message that contains the message the key 
///and the corrent place to read the bytes stop the work when the it reach the length of the text 
/// </summary>
/// <param name="argc">num of arguments (should be 2 )</param>
/// <param name="argv">name of file contain message to encrypt,name file contain the key</param>
/// <returns>SUCCESS_CODE if all process ran ERR_CODE else</returns>
int main(int argc, char* argv[])
{
	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERR_CODE;
	}
	const char* p_massage_file_path = argv[MASSAGE_FILE_IND];
	const char* p_key_file_path = argv[KEY_FILE_IND];
	char* comannd = NULL;
	DWORD file_len = get_file_len(p_massage_file_path);//get length in bytes of message
	if (0 == file_len)
	{
		printf("the message file is empty\n");
		return SUCCESS_CODE;
	}
	if (file_len % KEY_LEN)
	{
		printf("the message in file: %s does not divisible in %d\n", p_massage_file_path, KEY_LEN);
		return ERR_CODE;
	}
	if (KEY_LEN != get_file_len(p_massage_file_path))
	{
		printf("the key in file: %s does not have %d chars\n", p_massage_file_path, KEY_LEN);
		return ERR_CODE;
	}
	int num_of_dig = file_len % 10;
	int size = strlen(p_massage_file_path) + strlen(p_key_file_path) + strlen(BEGINCOMANND) + num_of_dig + 4;//text + 3X' ' + /0
	comannd = (char*)malloc(size * sizeof(char)); 
	if (NULL == comannd)
	{
		printf("wasnt able to allocate memory\n");
		return ERR_CODE;
	}
	for (int offset = 0; offset < file_len ; offset += KEY_LEN)
	{
		DWORD	exitcode;
		sprintf_s(comannd, size, "%s %s %d %s", BEGINCOMANND, p_massage_file_path, offset, p_key_file_path);
		exitcode = create_son_process(comannd);
		if (SUCCESS_CODE != exitcode)
		{
			printf("son process was failed\n");
			free(comannd);
			return ERR_CODE;
		}
	}
	free(comannd);	
	return SUCCESS_CODE;
}


/// <summary>
/// Gets command line whitch contains argumants for the son,create the process
/// </summary>
/// <param name="command">A pointer which hold the command line for son procces</param>
DWORD create_son_process(LPTSTR p_command)
{

	PROCESS_INFORMATION procinfo;
	DWORD				waitcode;
	DWORD				exitcode;
	BOOL				retVal;

	/*  Start the child process. */
	retVal = CreateProcessSimple(p_command, &procinfo);

	if (retVal == 0)
	{
		printf("Process Creation Failed!\n");
		return;
	}
	waitcode = WaitForSingleObject(
		procinfo.hProcess,
		INFINITE); /* Waiting for prosses to end*/

	printf("WaitForSingleObject output: ");
	switch (waitcode)
	{
	case WAIT_OBJECT_0:
		printf("WAIT_OBJECT_0\n"); break;
	default:
		printf("0x%x\n", waitcode);
	}

	GetExitCodeProcess(procinfo.hProcess, &exitcode);
	CloseHandle(procinfo.hProcess); /* Closing the handle to the process */
	CloseHandle(procinfo.hThread); /* Closing the handle to the main thread of the process */
	return exitcode;
}


BOOL CreateProcessSimple(LPTSTR p_commandLine, PROCESS_INFORMATION* p_processInfoPtr)
{
	STARTUPINFO	startinfo = { sizeof(STARTUPINFO), NULL, 0 }; /* <ISP> here we */
															  /* initialize a "Neutral" STARTUPINFO variable. Supplying this to */
															  /* CreateProcess() means we have no special interest in this parameter. */
															  /* This is equivalent to what we are doing by supplying NULL to most other */
															  /* parameters of CreateProcess(). */

	return CreateProcess(
		NULL, /*  No module name (use command line). */
		p_commandLine,			/*  Command line. */
		NULL,					/*  Process handle not inheritable. */
		NULL,					/*  Thread handle not inheritable. */
		FALSE,					/*  Set handle inheritance to FALSE. */
		NORMAL_PRIORITY_CLASS,	/*  creation/priority flags. */
		NULL,					/*  Use parent's environment block. */
		NULL,					/*  Use parent's starting directory. */
		&startinfo,				/*  Pointer to STARTUPINFO structure. */
		p_processInfoPtr			/*  Pointer to PROCESS_INFORMATION structure. */
	);
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

/// <summary>
/// return the length in bytes of given file
/// </summary>
/// <param name="p_file_name"> path to file</param>
/// <returns>the length of the file</returns>
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
