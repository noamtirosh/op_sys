
#include <stdio.h>
#include <windows.h>
#include <string.h>


#define TIMEOUT_IN_MILLISECONDS 5000
#define BRUTAL_TERMINATION_CODE 0x55
static const int ERROR_CODE = -1;


/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

/**
* CreateProcessSimple uses the win32 API to create a process that runs the
* command in 'CommandLine'. it uses the win32 API function CreateProcess()
* using default values for most parameters.
*
* Accepts:
* --------
* CommandLine - a windows generic string containing the command that the new
*               process performs. ( See CreateProcess( documentation for more ).
* ProcessInfoPtr - an output parameter, used to return a PROCESS_INFORMATION
*					structure containing data about the process that was created.
*					( See CreateProcess() documentation for more ).
*
* Returns:
* --------
* the output of CreateProcess().
*/
BOOL CreateProcessSimple(LPTSTR CommandLine, PROCESS_INFORMATION* ProcessInfoPtr);
void create_son_process(void);

/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

int main()
{
	DWORD file_len = get_file_len("plaintext.txt");
	if (file_len % 16)
	{
		printf("problem");
		//TODO add comment
	}
	char* p_arr = NULL;
	int num_of_dig = file_len % 10;
	int size = strlen("plaintext.txt") + num_of_dig + 2;// + /0
	p_arr = (char*)malloc(size * sizeof(char)); //TODO add check and memory relase
	if (NULL == p_arr)
	{
		return ERROR_CODE;
	}
	sprintf_s(p_arr, size, "%s %s %d %s","a","b",1,"c");
	printf("%s",p_arr);
	for (int i = 0; i < file_len ;i+=16)
	{
		create_son_process();
	}
	free(p_arr);
	
}

/**
* Demonstrates win32 process creation and termination.
*/
void create_son_process(void)
{
	PROCESS_INFORMATION procinfo;
	DWORD				waitcode;
	DWORD				exitcode;
	BOOL				retVal;
	TCHAR				command[] = ("Son.exe plaintext.txt 0 key.txt"); /* TCHAR is a win32
													generic char which may be either a simple (ANSI) char or a unicode char,
													depending on behind-the-scenes operating system definitions. Type LPTSTR
													is a string of TCHARs. Type LPCTSTR is a const string of TCHARs. */

													/*  Start the child process. */
	retVal = CreateProcessSimple(command, &procinfo);


	if (retVal == 0)
	{
		printf("Process Creation Failed!\n");
		return;
	}
	waitcode = WaitForSingleObject(
		procinfo.hProcess,
		TIMEOUT_IN_MILLISECONDS); /* Waiting 5 secs for the process to end */

	printf("WaitForSingleObject output: ");
	switch (waitcode)
	{
	case WAIT_TIMEOUT:
		printf("WAIT_TIMEOUT\n"); break;
	case WAIT_OBJECT_0:
		printf("WAIT_OBJECT_0\n"); break;
	default:
		printf("0x%x\n", waitcode);
	}

	if (waitcode == WAIT_TIMEOUT) /* Process is still alive */
	{
		printf("Process was not terminated before timeout!\n"
			"Terminating brutally!\n");
		TerminateProcess(
			procinfo.hProcess,
			BRUTAL_TERMINATION_CODE); /* Terminating process with an exit code of 55h */
		Sleep(10); /* Waiting a few milliseconds for the process to terminate,
					note the above command may also fail, so another WaitForSingleObject is required.
					We skip this for brevity */
	}

	GetExitCodeProcess(procinfo.hProcess, &exitcode);

	printf("The exit code for the process is 0x%x\n", exitcode);

	/* Note: process is still being tracked by OS until we release handles */
	CloseHandle(procinfo.hProcess); /* Closing the handle to the process */
	CloseHandle(procinfo.hThread); /* Closing the handle to the main thread of the process */
}


BOOL CreateProcessSimple(LPTSTR CommandLine, PROCESS_INFORMATION* ProcessInfoPtr)
{
	STARTUPINFO	startinfo = { sizeof(STARTUPINFO), NULL, 0 }; /* <ISP> here we */
															  /* initialize a "Neutral" STARTUPINFO variable. Supplying this to */
															  /* CreateProcess() means we have no special interest in this parameter. */
															  /* This is equivalent to what we are doing by supplying NULL to most other */
															  /* parameters of CreateProcess(). */

	return CreateProcess(
		NULL, /*  No module name (use command line). */
		CommandLine,			/*  Command line. */
		NULL,					/*  Process handle not inheritable. */
		NULL,					/*  Thread handle not inheritable. */
		FALSE,					/*  Set handle inheritance to FALSE. */
		NORMAL_PRIORITY_CLASS,	/*  creation/priority flags. */
		NULL,					/*  Use parent's environment block. */
		NULL,					/*  Use parent's starting directory. */
		&startinfo,				/*  Pointer to STARTUPINFO structure. */
		ProcessInfoPtr			/*  Pointer to PROCESS_INFORMATION structure. */
	);
}

HANDLE CreateFileSimple(LPCSTR file_name, char mode)
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
		creation_disposition = OPEN_EXISTING;
		break;
	case 'w':case'W':
		acsees = GENERIC_WRITE;
		share = FILE_SHARE_WRITE;
		creation_disposition = OPEN_ALWAYS; // open if existe or create new if not
		break;
	case 'a':
		acsees = GENERIC_READ | GENERIC_WRITE;
		share = FILE_SHARE_WRITE | FILE_SHARE_READ;
		creation_disposition = OPEN_ALWAYS; // open if existe or create new if not
		break;
	default:
		acsees = GENERIC_READ;  // open for reading
		share = FILE_SHARE_READ; // share for reading
		creation_disposition = OPEN_EXISTING;
	};
	h_file = CreateFile(file_name,  // file to open
		acsees,
		share,
		NULL,                  // default security
		creation_disposition,  // existing file only
		FILE_ATTRIBUTE_NORMAL, // normal file
		NULL);
	if (h_file == INVALID_HANDLE_VALUE)
	{
		printf("problem");
		//TODO hendle problems when open file
	}
	last_error = GetLastError();
	if (last_error == ERROR_FILE_NOT_FOUND)
	{
		printf("problem");

	}
	return h_file;
}

DWORD get_file_len(LPCSTR file_name)
{
	HANDLE h_file = CreateFileSimple(file_name, 'r');
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
