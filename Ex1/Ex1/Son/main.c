#include "HardCodedData.h"
#include <stdio.h>
#include <Windows.h>
#include <stdlib.h>

//son
//function declaration
void encode_massage(char* p_massage, const char* p_key, int massge_len);
HANDLE create_file_simple(LPCSTR p_file_name, char mode);
long read_from_file(LPCSTR p_file_name, long offset, LPVOID p_buffer, const DWORD buffer_len);
long write_to_file(LPCSTR p_file_name, LPVOID p_buffer, const DWORD buffer_len);

int main(int argc, char* argv[])
{
	/// <summary>
	/// open file with message read 16 bytes after given offset, open key from file,encrypt massge with key
	///  and write to the end of diffulet file [Encrypte_message.txt]
	/// </summary>
	/// <param name="argc">num of arguments (should be 3 )</param>
	/// <param name="argv">name of file contain message to encrypt ,offset in bytes,name file contain the key</param>
	/// <returns></returns>
	if (argc != NUM_OF_INPUTS + 1)
	{
		printf("ERROR: Not enough input arguments\n");
		return ERR_CODE;
	}
	//get relevent params 
	const char* message_file_path = argv[MASSAGE_FILE_IND];
	const char* key_file_path = argv[KEY_FILE_IND];
	DWORD offset = atoi(argv[OFFSET_IND]);
	//init buffers 
	char read_buffer[FILE_BUFFER+1] = { 0 };//TODO do we need etra 1 char for /0 ?
	char key_buffer[FILE_BUFFER+1] = { 0 };
	
	if (FILE_BUFFER != read_from_file(message_file_path, offset, read_buffer, FILE_BUFFER))
	{
		printf("was not able to read message from file: %s\n", message_file_path);
		return ERR_CODE;
	}
	if (FILE_BUFFER != read_from_file(key_file_path,0, key_buffer, FILE_BUFFER))
	{
		printf("was not able to read key from file: %s\n", key_file_path);
		return ERR_CODE;
	}
	encode_massage(read_buffer, key_buffer, FILE_BUFFER);
	if (FILE_BUFFER != write_to_file(OUT_FILE_NAME, read_buffer, FILE_BUFFER))
	{
		printf("was not able to write encrypte message from file: %s\n", OUT_FILE_NAME);
	}
	return(SUCCESS_CODE);

}

long read_from_file(LPCSTR p_file_name ,long offset, LPVOID p_buffer, const DWORD buffer_len)
{
	/// <summary>
	///  read message in given length from givven offset in bytes from beginning
	// from given file to buffer 
	/// </summary>
	/// <param name="file_name">path to file to read</param>
	/// <param name="offset"> offset in bytes from beginning </param>
	/// <param name="p_buffer"> pointer to buffer to put message</param>
	/// <param name="buffer_len">length of message to read</param>
	/// <returns>num of bytes readed from file</returns>
	HANDLE h_file = CreateFileSimple(p_file_name, 'r');
	DWORD n_written = 0;
	DWORD last_error ;
	if (INVALID_HANDLE_VALUE == h_file)
	{
		return n_written;
	}
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
		printf("Unable to read from file, error: %ld\n", last_error);
	}
	CloseHandle(h_file);
	return n_written;

}
long write_to_file(LPCSTR p_file_name, LPVOID p_buffer,const DWORD buffer_len)
{
	/// <summary>
	///  write message from p_buffer in len buffer_len to file 
	/// </summary>
	/// <param name="file_name">path to file to write the buffer</param>
	/// <param name="p_buffer">pointer to buffer</param>
	/// <param name="buffer_len"> the length of the buffer</param>
	/// <returns> num of bytes written to file</returns>
	HANDLE h_file = CreateFileSimple(p_file_name, 'w');
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
		printf("Unable to write to file, error: %ld\n",last_error);
	}
	if (n_written == buffer_len)
	{
		printf("write all buffer\n");
	}
	CloseHandle(h_file);
	return n_written;
}
HANDLE create_file_simple(LPCSTR p_file_name,char mode)
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
void encode_massage(char *p_massage, const char *p_key,int massge_len)
{
	for (int i = 0; i < massge_len; i++)
	{
		p_massage[i] = p_massage[i] ^ p_key[i];
	}
}
