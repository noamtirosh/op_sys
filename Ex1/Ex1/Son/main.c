#include "HardCodedData.h"
#include <stdio.h>
#include <Windows.h>
#include <stdlib.h>

//son
//function declaration TODO add functions
void encode_massage(char* massage, const char* key, int massge_len);
HANDLE CreateFileSimple(LPCSTR file_name, char mode);

int main(int argc, char* argv[])
{
	/// <summary>
	/// TODO add function summary
	/// </summary>
	/// <param name="argc"></param>
	/// <param name="argv"></param>
	/// <returns></returns>
	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments");
		return ERR_CODE;
	}
	const char* massage_file_path = argv[MASSAGE_FILE_IND];
	const char* key_file_path = argv[KEY_FILE_IND];
	char read_buffer[FILE_BUFFER+1] = { 0 };
	char key_buffer[FILE_BUFFER+1] = { 0 };
	long offset = atoi(argv[OFFSET_IND]); //TODO check if it's ok to use long
	read_from_file(massage_file_path, offset, read_buffer, FILE_BUFFER);
	read_from_file(key_file_path,0, key_buffer, FILE_BUFFER);
	encode_massage(read_buffer, key_buffer, FILE_BUFFER);
	write_to_file(OUT_FILE_NAME, read_buffer, FILE_BUFFER);
	return(SUCCESS_CODE);

}


long read_from_file(LPCSTR file_name ,long offset, LPVOID p_buffer, const DWORD buffer_len)
{
	HANDLE h_file = CreateFileSimple(file_name, 'r');
	DWORD n_written = 0;
	DWORD last_error ;

	//move file pointer in ofset
	SetFilePointer(
		h_file,	//handle to file
		offset, // number of bytes to move the file pointer
		NULL, // 
		FILE_BEGIN); // provides offset from beginning of the file
	// Read one character less than the buffer size to save room for 
	// TODO check if we add extra char dose it change to /0
	// the terminating NULL character. 
	if (FALSE == ReadFile(h_file,    //handle to file
		p_buffer,   // pointer to buffer to save data read from file
		buffer_len, //maximum number of bytes to be read
		&n_written, NULL))
	{
		last_error = GetLastError();
		printf("problem");
		//TODO handle problem in read file
	}
	CloseHandle(h_file);
	return n_written;

}
long write_to_file(LPCSTR file_name, LPVOID p_buffer,const DWORD buffer_len)
{
	HANDLE h_file = CreateFileSimple(file_name, 'w');
	DWORD last_error;
	DWORD n_written = 0;
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
		printf("problem");
		//TODO handle problem in read file
	}
	if (n_written == buffer_len)
	{
		printf("write all buffer");
	}
	CloseHandle(h_file);
	return n_written;
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
	return current_file_position;
}

HANDLE CreateFileSimple(LPCSTR file_name,char mode)
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

void encode_massage(char *massage, const char *key,int massge_len)
{
	for (int i = 0; i < massge_len; i++)
	{
		massage[i] = massage[i] ^ key[i];
	}
}
