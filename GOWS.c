#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int ListenerSocket = 0,
		   ListenerSocketOptions = 1,
		   ListenerSocketLen = sizeof(struct sockaddr_in),
		   ListenerPort = 80;

static struct sockaddr_in ListenerSocketAddr;

static unsigned char RunServer = 1;

static const char ServerRootStr[] = "/srv/http/";
static const int ServerRootStrlen = sizeof(ServerRootStr);

static const char IndexFileStr[] = "/index.html";
static const int IndexFileStrlen = sizeof(IndexFileStr);

/* Response messages */
static const char InvalidMethodStr[] = "HTTP/1.1 418 I'm a teapot!\r\n\r\nThis server only accepts GET requests.\r\n";
static const int InvalidMethodStrlen = sizeof(InvalidMethodStr);

static const char LFIErrorStr[] = "HTTP/1.1 418 I'm a teapot!\r\n\r\nI don't appreciate your attempted LFI.\r\n";
static const int LFIErrorStrlen = sizeof(LFIErrorStr);

static const char InvalidFileMessageStr[] = "HTTP/1.1 404 Not Found\r\n\r\nThe file could not be found!\r\n";
static const int InvalidFileMessageStrlen = sizeof(InvalidFileMessageStr);

static const char OKHeaderStr[] = "HTTP/1.1 200 OK\r\n\r\n";
static const int OKHeaderStrlen = sizeof(InvalidFileMessageStr);

static const char IndexRedirectStr[] = "HTTP/1.1 301 Moved Permanently\r\nLocation: /index.html\r\n\r\n";
static const int IndexRedirectStrlen = sizeof(IndexRedirectStr);

/* Code */
int SetupListener(void)
{
	ListenerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (ListenerSocket == 0)
	{
		return 1;
	}

	if (setsockopt(
				ListenerSocket,
				SOL_SOCKET,
				SO_REUSEADDR | SO_REUSEPORT,
				&ListenerSocketOptions,
				sizeof(ListenerSocketOptions)))
	{
		return 1;
	}

	ListenerSocketAddr.sin_family = AF_INET;
	ListenerSocketAddr.sin_addr.s_addr = INADDR_ANY;
	ListenerSocketAddr.sin_port = htons(ListenerPort);

	if (
			bind(
				ListenerSocket,
				(struct sockaddr *)&ListenerSocketAddr,
				sizeof(struct sockaddr)) < 0)
	{
		return 1;
	}

	if (listen(ListenerSocket, 3) < 0)
	{
		return 1;
	}
}

int main(int argc, char **argv)
{
	char 
		RequestStr[513],
		*RequestStrPtr = NULL,
		*RequestFilePathBuffer = NULL,
		*ResponseBuffer = NULL;

		unsigned int
			ResponseBufferSize = 512,
							   RequestFilePathBufferSize = 512,
							   FileLength = 0;

		int ConnectingSocket;

		if (SetupListener())
		{
			fprintf(stderr,"Could not create listener. Do we have sudo permissions for port %i?\n", ListenerPort);
			return 1;
		}

		/* Setup buffers */
		RequestStrPtr = &RequestStr[0];


		ResponseBuffer = (char*)malloc(ResponseBufferSize + 1);
		strcpy(ResponseBuffer, OKHeaderStr);

		RequestFilePathBuffer = (char*)malloc(RequestFilePathBufferSize + 1);
		strcpy(RequestFilePathBuffer, ServerRootStr);

		while (RunServer)
		{

			/* GET THE CLIENT */
			ConnectingSocket = 
				accept(
						ListenerSocket,
						(struct sockaddr*)&ListenerSocketAddr,
						(socklen_t *)&ListenerSocketLen
					  );

			if (ConnectingSocket < 0)
			{
				continue;
			}

			/* READ THE REQUEST */
			recv(ConnectingSocket, RequestStr, 512, 0);
			while (*RequestStrPtr != 0)
			{
				if (*RequestStrPtr == ' ')
				{
					*RequestStrPtr = 0;
				}
				RequestStrPtr++;
			}
			RequestStrPtr = &RequestStr[0];

			/* ENSURE THE GIVEN METHOD IS GET */
			char *RequestMethod = &RequestStr[0];
			if (strcmp(RequestMethod, "GET") != 0)
			{
				/* NOT A GET REQUEST */
				send(ConnectingSocket, InvalidMethodStr, InvalidMethodStrlen, 0);
				shutdown(ConnectingSocket, 2);
				continue;
			}

			/* GET THE REQUESTED FILE PATH */
			char *RequestedFilePath = &RequestStr[0] + strlen(RequestMethod) + 1;
			/* printf("RequestedFilePath = %s\n", RequestedFilePath); */

			/* Protect against LFI */
			char *tmp = RequestedFilePath;
			char IsLFIAttempt = 0;
			while (*tmp != 0)
			{
				while (*tmp == '/')
				{
					tmp++;
				}
				if (strncmp(tmp, "..", 2) == 0)
				{
					/* Attempted LFI */
					IsLFIAttempt = 1;
					break; /* while (*tmp != 0) */
				}
				tmp++;
			}

			if (IsLFIAttempt)
			{
				send(ConnectingSocket, LFIErrorStr, LFIErrorStrlen, 0);
				shutdown(ConnectingSocket, 2);
				continue;
			}

			/* Ensure the buffer is of adequite size */
			while (
					strlen(RequestedFilePath) + ServerRootStrlen 
					> RequestFilePathBufferSize
				  )
			{
				RequestFilePathBufferSize += 512;
				RequestFilePathBuffer = 
					realloc(
							RequestFilePathBuffer,
							RequestFilePathBufferSize + 1
						   );
			}

			/* Check if the file is the index
			 * Then copy the file path to the buffer
			 */
			printf("RFP: %s\n", RequestedFilePath);
			if (strcmp(RequestedFilePath, "/") == 0)
			{
				/* Request for index */
				send(ConnectingSocket, IndexRedirectStr, IndexRedirectStrlen, 0);
				shutdown(ConnectingSocket, 2);
				continue;
			}

			strcpy(RequestFilePathBuffer+ServerRootStrlen-1, RequestedFilePath);

			/* Open the requested file */
			printf("Getting file: %s\n", RequestFilePathBuffer); 
			FILE *F = fopen(RequestFilePathBuffer, "r");

			/* Check for errors */
			if (!F)
			{
				send(
						ConnectingSocket,
						InvalidFileMessageStr,
						InvalidFileMessageStrlen,
						0
					);
				shutdown(ConnectingSocket, 2);
				continue;
			}

			/* Get the size of the file */
			fseek(F, 0, SEEK_END);
			FileLength = ftell(F);
			rewind(F);

			/* Ensure the response buffer can hold the file contents */
			while (ResponseBufferSize < FileLength + OKHeaderStrlen)
			{
				ResponseBufferSize += 512;
				ResponseBuffer = realloc(ResponseBuffer, ResponseBufferSize+1);
			}
			strcpy(ResponseBuffer, OKHeaderStr);
			/* Read the file into the buffer */
			fread(ResponseBuffer, 1, FileLength, F);
			send(
					ConnectingSocket,
					ResponseBuffer,
					FileLength + OKHeaderStrlen-1,
					0
				);

			/* Done, cleanup */
			fclose(F);
			shutdown(ConnectingSocket, 2);
		}

		/* Exit */

		free(ResponseBuffer);
		free(RequestFilePathBuffer);
		return 0;
}
