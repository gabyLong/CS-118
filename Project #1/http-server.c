

// Server side C program to demonstrate HTTP Server programming
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#define true 1
#define false 0

#define FILE_BUFFER_SIZE 1048576
#define HTTP_PORT 12345


enum
{
    GET_CMD,
//    //HEAD_CMD,
//    //POST_CMD,
//    //PUT_CMD,
//    //DELETE_CMD,
//    //TRACE_CMD,
//    //OPTIONS_CMD,
//    //CONNECT_CMD,
//    //PATCH_CMD,
    MAX_CMD
};

char* responseHeader = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: ";


char* fileBufferPtr;
int socketFD;


char* httpCmds[] =
{
    "GET ",
//    "HEAD",
};


void processGetItemStr(char* cmdTextPtr, char* destBufferPtr)
{
    int destOffset = 0;

    while (true)
    {
        if (cmdTextPtr[destOffset] != ' ')
        {
            destBufferPtr[destOffset] = cmdTextPtr[destOffset];
            destOffset++;
        }
        else
        {
            break;
        }
    }
}


void processGetCmd(char* cmdTextPtr)
{
    char outputBuffer[200] = {0};
    FILE* fileStream;
    long fileLen;

    if (cmdTextPtr != 0)
    {
        processGetItemStr(cmdTextPtr, outputBuffer);

        fileStream = fopen(outputBuffer, "rb");

        if (fileStream != 0)
        {
            if (fseek(fileStream, 0, SEEK_END) == 0)
            {
                fileLen = ftell(fileStream);
                fseek(fileStream, 0, SEEK_SET);
                fread(fileBufferPtr, fileLen, 1, fileStream);

                sprintf(outputBuffer, "%s%d\n\n", responseHeader, (int)fileLen);

                write(socketFD, outputBuffer, strlen(outputBuffer));
                write(socketFD, fileBufferPtr, fileLen);
            }

            fclose(fileStream);
        }
    }
}


void testLine(char* inputPtr)
{
    int loopIndex;
    int strLength;

    for (loopIndex = GET_CMD;loopIndex < MAX_CMD;loopIndex++)
    {
        strLength = strlen(httpCmds[loopIndex]);

        if (strncmp(inputPtr, httpCmds[loopIndex], strLength) == 0)
        {
            switch (loopIndex)
            {
            case GET_CMD:
                processGetCmd(&inputPtr[strLength]);
                break;
            }
        }
    }
}


int nextLine(char* inputPtr, int inputLen, int inputOffset)
{
    while (inputOffset < inputLen)
    {
        if ((inputPtr[inputOffset] == '\r') && (inputPtr[inputOffset + 1] == '\n'))
        {
            inputOffset += 2;
            break;
        }
        else
        {
            inputOffset++;
        }
    }

    return inputOffset;
}


void parseHttpStrings(char* inputPtr, int inputLen)
{
    int bufferOffset = 0;

    while (bufferOffset < inputLen)
    {
        testLine(&inputPtr[bufferOffset]);

        bufferOffset = nextLine(inputPtr, inputLen, bufferOffset);
    }
}


int main(/* int argc, char const *argv[] */)
{
    int serverFD;
    long requestLength;
    struct sockaddr_in socketAddress;
    int socketAddressLen = sizeof(socketAddress);
    //int loopIndex;

    //for (loopIndex = 0;loopIndex < argc;loopIndex++)
    //{
    //    printf("%s\n", argv[loopIndex]);
    //}

    memset(&socketAddress, 0, sizeof(socketAddress));

    fileBufferPtr = (char*)malloc(FILE_BUFFER_SIZE);
    if (fileBufferPtr == 0)
    {
        perror("\n*** MEMORY ALLOCATION ERROR ***\n");
        exit(EXIT_FAILURE);
    }

    if ((serverFD = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("*** Error creating socket ***\n");
        exit(EXIT_FAILURE);
    }

    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;
    socketAddress.sin_port = htons(HTTP_PORT);

    if (bind(serverFD, (struct sockaddr*)&socketAddress, sizeof(socketAddress)) < 0)
    {
        perror("*** Error binding socket ***n");
        exit(EXIT_FAILURE);
    }

    if (listen(serverFD, 100) < 0)
    {
        perror("*** Error listening ***n");
        exit(EXIT_FAILURE);
    }

    while (true)
    {
        printf("\nWaiting for HTTP request...\n");
        socketFD = accept(serverFD, (struct sockaddr*)&socketAddress, (socklen_t*)&socketAddressLen);
        if (socketFD < 0)
        {
            perror("*** Error accepting connection ***\n");
            exit(EXIT_FAILURE);
        }

        // read request from client
        requestLength = read(socketFD, fileBufferPtr, FILE_BUFFER_SIZE);

        // process client's request
        parseHttpStrings(fileBufferPtr, (int)requestLength);

        close(socketFD);
    }

    return 0;
}

