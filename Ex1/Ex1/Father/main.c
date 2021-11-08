#include "HardCodeData.h"
#include <stdio.h>
#include <windows.h>
#include <string.h>



//father

//function declaration
BOOL CreateProcessSimple(LPTSTR p_commandLine, PROCESS_INFORMATION* p_processInfoPtr);
DWORD create_son_process(LPTSTR p_command);
HANDLE create_file_simple(LPCSTR p_file_name, char mode);
DWORD get_file_len(LPCSTR p_file_name);

int main(int argc, char* argv[])
{
	/// <summary>
	/// open file with message, opens a procces for every 16 bytes of message that contains the message the key 
	///and the corrent place to read the bytes stop the work when the it reach the length of the text 
	/// </summary>
	/// <param name="argc">num of arguments (should be 2 )</param>
	/// <param name="argv">name of file contain message to encrypt,name file contain the key</param>
	/// <returns></returns>
	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERR_CODE;
	}
	const char* p_massage_file_path = argv[MASSAGE_FILE_IND];
	const char* p_key_file_path = argv[KEY_FILE_IND];
	char* comannd = NULL;
	DWORD file_len = get_file_len("plaintext.txt");
	if (0 == file_len)
	{
		printf("the message file is empty\n");
		return SUCCESS_CODE;
	}
	if (file_len % 16)
	{
		printf("The line does not divisible in 16 \n");
		return ERR_CODE;
	}
	int num_of_dig = file_len % 10;
	int size = strlen(p_massage_file_path) + strlen(p_key_file_path) + strlen(BEGINCOMANND) + num_of_dig + 4;// 3 ' ' + /0
	comannd = (char*)malloc(size * sizeof(char)); 
	if (NULL == comannd)
	{
		printf("wasnt able to allocate memory\n");
		return ERR_CODE;
	}
	for (int offset = 0; offset < file_len ; offset +=16)
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

/**
* Demonstrates win32 process creation and termination.
*/
DWORD create_son_process(LPTSTR p_command)
{
	/// <summary>
	/// Gets command line whitch contains argumants for the son,create the process
	/// </summary>
	/// <param name="command">A pointer which hold the command line for son procces</param>
	PROCESS_INFORMATION procinfo;
	DWORD				waitcode;
	DWORD				exitcode;
	BOOL				retVal;
	 /* TCHAR is a win32
													generic char which may be either a simple (ANSI) char or a unicode char,
													depending on behind-the-scenes operating system definitions. Type LPTSTR
													is a string of TCHARs. Type LPCTSTR is a const string of TCHARs. */

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
	/* Note: process is still being tracked by OS until we release handles */
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
