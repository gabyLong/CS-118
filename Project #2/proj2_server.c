//
//  proj2_server.c
//
//  NAME: Gabriella Long
//  EMAIL: gabriellalong@g.ucla.edu
//
//  Created by Gabriella Kaarina Long on 5/30/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "proj2_header_rec.h"

struct sockaddr_in serverAddr;
struct sockaddr_in clientAddr;
socklen_t addressLen;
uint8_t* fileBufferPtr;
int fileIndex = 0;
int serverSeqNumber;
unsigned int fileLen = 0;


void outputFlags(char* msgBufferPtr, frameRec* frameRecPtr)
{
    if ((frameRecPtr->hdrRec.packetFlags & LOG_SYN) == LOG_SYN)
    {
        strcat(msgBufferPtr, " SYN");
    }

    if ((frameRecPtr->hdrRec.packetFlags & LOG_FIN) == LOG_FIN)
    {
        strcat(msgBufferPtr, " FIN");
    }

    if ((frameRecPtr->hdrRec.packetFlags & LOG_ACK) == LOG_ACK)
    {
        strcat(msgBufferPtr, " ACK");
    }

    if ((frameRecPtr->hdrRec.packetFlags & LOG_DUP_ACK) == LOG_DUP_ACK)
    {
        strcat(msgBufferPtr, " DUP-ACK");
    }
}


int logOutput(int logType, frameRec* frameRecPtr)
{
    char msgBuffer[80];
    char numStr[40];

    switch (logType)
    {
    case LOG_RECV:
        strcpy(msgBuffer, "RECV");
        sprintf(numStr," %d %d", frameRecPtr->hdrRec.seqNumber, frameRecPtr->hdrRec.ackNumber);
        strcat(msgBuffer, numStr);
        outputFlags(msgBuffer, frameRecPtr);
        break;
    case LOG_SEND:
        strcpy(msgBuffer, "SEND");
        sprintf(numStr," %d %d", frameRecPtr->hdrRec.seqNumber, frameRecPtr->hdrRec.ackNumber);
        strcat(msgBuffer, numStr);
        outputFlags(msgBuffer, frameRecPtr);
        break;
    case LOG_RESEND:
        strcpy(msgBuffer, "RESEND");
        sprintf(numStr," %d", frameRecPtr->hdrRec.seqNumber);
        strcat(msgBuffer, numStr);
        outputFlags(msgBuffer, frameRecPtr);
        break;
    case LOG_TIMEOUT:
        strcpy(msgBuffer, "TIMEOUT");
        sprintf(numStr," %d", frameRecPtr->hdrRec.seqNumber);
        strcat(msgBuffer, numStr);
        break;
    }

    strcat(msgBuffer, "\n");

    write(STDOUT_FILENO, msgBuffer, strlen(msgBuffer));

    return UDP_XFER_NO_ERROR;
}


int setupServerConnection(int udpPortValue)
{
    int socketFd = -1;

    if ((socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(udpPortValue);

    if (bind(socketFd, (const struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("bind function failure");
        exit(EXIT_FAILURE);
    }

    return socketFd;
}


int performHandshake(int socketFd, frameRec* xferFrameRecPtr)
{
    int result = UDP_XFER_HANDSHAKE_ERROR;
    int handshakeFlag = 0;
    int resendFlag = 0;
    struct timespec txTime;
    struct timespec currentTime;
    long secondCount;
    long nsecCount;
    int timeoutFlag = FALSE;

    recvfrom(socketFd, (char *)xferFrameRecPtr, sizeof(frameRec), MSG_WAITALL, (struct sockaddr*)&clientAddr, &addressLen);

    if (xferFrameRecPtr->hdrRec.packetFlags == LOG_SYN)
    {
        logOutput(LOG_RECV, xferFrameRecPtr);

        result = UDP_XFER_NO_ERROR;

        serverSeqNumber = rand() % 0x100;

        xferFrameRecPtr->hdrRec.ackNumber = xferFrameRecPtr->hdrRec.seqNumber + 1;
        xferFrameRecPtr->hdrRec.seqNumber = serverSeqNumber++;
        xferFrameRecPtr->hdrRec.packetFlags = LOG_ACK | LOG_SYN;
        xferFrameRecPtr->hdrRec.dataOffset = 0;

        while (handshakeFlag == 0)
        {
            clock_gettime(CLOCK_REALTIME, &txTime);

            sendto(socketFd, (const char*)xferFrameRecPtr, sizeof(frameRec), MSG_CONFIRM, (const struct sockaddr*)&clientAddr, addressLen);

            if (resendFlag == 0)
            {
                logOutput(LOG_SEND, xferFrameRecPtr);
            }
            else
            {
                logOutput(LOG_RESEND, xferFrameRecPtr);
            }

            while (TRUE)
            {
                ssize_t rxSize = recvfrom(socketFd, xferFrameRecPtr, sizeof(frameRec), MSG_DONTWAIT, (struct sockaddr*)&clientAddr, &addressLen);
                if (rxSize > 0)
                {
                    logOutput(LOG_RECV, xferFrameRecPtr);
                    resendFlag = 0;
                    break;
                }
                else
                {
                    clock_gettime(CLOCK_REALTIME, &currentTime);

                    secondCount = currentTime.tv_sec - txTime.tv_sec;
                    nsecCount = currentTime.tv_nsec - txTime.tv_nsec;
                    if (txTime.tv_nsec > currentTime.tv_nsec)
                    {
                        secondCount--;
                        nsecCount += ONE_SECOND_NS;
                    }

                    if (secondCount > 0)
                    {
                        timeoutFlag = TRUE;
                    }
                    else
                    {
                        if (nsecCount > HALF_SECOND_NS)
                        {
                            timeoutFlag = TRUE;
                        }
                    }

                    if (timeoutFlag == TRUE)
                    {
                        resendFlag = 1;
                        break;
                    }
                }
            }

            if (resendFlag == 0)
            {
                break;
            }
        }
    }

    return result;
}


int transferFile(int socketFd, frameRec* frameRecPtr)
{
    int result = UDP_XFER_NO_ERROR;

    fileLen = 0;

    // loop until FIN flag received
    while (TRUE)
    {
        int rxSize = (int)recvfrom(socketFd, frameRecPtr, sizeof(frameRec), 0, (struct sockaddr*)&serverAddr, &addressLen);
        logOutput(LOG_RECV, frameRecPtr);

        if ((frameRecPtr->hdrRec.packetFlags & LOG_FIN) != LOG_FIN)
        {
            unsigned int dataSize = rxSize - sizeof(headerRec);
            unsigned int destOffset = (unsigned int)(frameRecPtr->hdrRec.dataOffset * 512);
            memcpy(&fileBufferPtr[destOffset], frameRecPtr->payloadData, dataSize);

            if ((destOffset + dataSize) > fileLen)
            {
                fileLen = destOffset + dataSize;
            }

            frameRecPtr->hdrRec.ackNumber = frameRecPtr->hdrRec.seqNumber;
            frameRecPtr->hdrRec.seqNumber = ++serverSeqNumber;
            frameRecPtr->hdrRec.packetFlags = LOG_ACK;

            sendto(socketFd, (const char*)frameRecPtr, sizeof(frameRec), MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));
            logOutput(LOG_SEND, frameRecPtr);
        }
        else
        {
            break;
        }
    }

    return result;
}


int endTransfer(int socketFd, frameRec* frameRecPtr)
{
    int result = UDP_XFER_NO_ERROR;
    char fileName[40];

    int currentSeq = frameRecPtr->hdrRec.seqNumber;

    frameRecPtr->hdrRec.seqNumber = ++serverSeqNumber;
    frameRecPtr->hdrRec.ackNumber = currentSeq + 1;
    frameRecPtr->hdrRec.packetFlags = LOG_ACK;
    frameRecPtr->hdrRec.dataOffset = 0;

    sendto(socketFd, (const char*)frameRecPtr, sizeof(frameRec), MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));
    logOutput(LOG_SEND, frameRecPtr);

    frameRecPtr->hdrRec.ackNumber = 0;
    frameRecPtr->hdrRec.packetFlags = LOG_FIN;

    sendto(socketFd, (const char*)frameRecPtr, sizeof(frameRec), MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));
    logOutput(LOG_SEND, frameRecPtr);

    sprintf(fileName, "%d.file", ++fileIndex);

    FILE* outFile = (FILE*)fopen(fileName, "wb");

    if (outFile != nullPtr)
    {
        fwrite(fileBufferPtr, 1, fileLen, outFile);
        fclose(outFile);
    }

    return result;
}


int main(int argc, char** argv)
{
    frameRec xferFrameRec;
    char msgBuffer[100];

    //int loopIndex;
    //for (loopIndex = 0;loopIndex < argc;loopIndex++)
    //{
    //    printf("*** SERVER  argv[%d]: %s ***\n", loopIndex, argv[loopIndex]);
    //}

// $ ./server 5000

    if (argc != 2)
    {
        sprintf(msgBuffer, "Parameter count error argc: %d\n", argc);
        write(STDERR_FILENO, msgBuffer, strlen(msgBuffer));
        exit(1);
    }

    fileBufferPtr = (uint8_t*)malloc(MAX_FILE_SIZE);
    if (fileBufferPtr == (uint8_t*)0)
    {
        sprintf(msgBuffer, "Memory allocation error\n");
        write(STDERR_FILENO, msgBuffer, strlen(msgBuffer));
        exit(1);
    }

    int socketFd = setupServerConnection(atoi(argv[1]));

    addressLen = sizeof(clientAddr);

    while (TRUE)
    {
        if (performHandshake(socketFd, &xferFrameRec) == UDP_XFER_NO_ERROR)
        {
            if (transferFile(socketFd, &xferFrameRec) == UDP_XFER_NO_ERROR)
            {
                endTransfer(socketFd, &xferFrameRec);
            }
        }
    }

    return 0;
}


