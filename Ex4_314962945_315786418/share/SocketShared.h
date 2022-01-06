/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/
/* 
 This file was written for instruction purposes for the 
 course "Introduction to Systems Programming" at Tel-Aviv
 University, School of Electrical Engineering, Winter 2011, 
 by Amnon Drory.
*/
/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

#ifndef SOCKET_SHARED_H
#define SOCKET_SHARED_H

/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

#define SERVER_ADDRESS_STR "127.0.0.1"
#define SERVER_PORT 2345

#define STRINGS_ARE_EQUAL( Str1, Str2 ) ( strcmp( (Str1), (Str2) ) == 0 )

#define  CLIENT_REQ "CLIENT_REQUEST"
#define MASSGAE_TYPE_MAX_LEN 20
#define MAX_USERNAME_LEN 20
#define NUM_SERVER_TYPES 10
#define NUM_CLIENT_TYPES 4
#define MAX_TIME_FOR_TIMEOUT 15
#define MASSGAE_END '\n'
#define PRAMS_SEPARATE ';'
#define MASSGAE_TYPE_SEPARATE ':'
#define BOOM_TEXT "boom"
#define END_GAME "END"
#define CONTINUE_GAME "CONT"
#define END_MSG_MAX_LEN 5
#define BOOM_VLUE -10 
#define MAX_MESSAGES_IN_BUF 10

/*oOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoOoO*/

#endif // SOCKET_SHARED_H