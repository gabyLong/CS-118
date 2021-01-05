//
//  proj2_client.c
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
#include <time.h>
#include <signal.h>
#include "proj2_header_rec.h"
#include "proj2_client.h"


struct sockaddr_in serverAddr;
socklen_t addressLen;
int socketFd;
int clientSeqNumber;
txFrameRec txFrameArray[SEL_RPT_WINDOW_COUNT];


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


int performHandshake(int socketFd, frameRec* xferFrameRecPtr)
{
    int result = UDP_XFER_NO_ERROR;
    int handshakeFlag = FALSE;
    int resendFlag = FALSE;
    struct timespec txTime;
    struct timespec currentTime;
    long secondCount;
    long nsecCount;
    int timeoutFlag = FALSE;

    xferFrameRecPtr->hdrRec.seqNumber = clientSeqNumber++;
    xferFrameRecPtr->hdrRec.ackNumber = 0;
    xferFrameRecPtr->hdrRec.packetFlags = LOG_SYN;
    xferFrameRecPtr->hdrRec.dataOffset = 0;

    while (handshakeFlag == FALSE)
    {
        clock_gettime(CLOCK_REALTIME, &txTime);

        sendto(socketFd, (const char*)xferFrameRecPtr, sizeof(frameRec),
               MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if (resendFlag == FALSE)
        {
            logOutput(LOG_SEND, xferFrameRecPtr);
        }
        else
        {
            logOutput(LOG_RESEND, xferFrameRecPtr);
        }

        while (TRUE)
        {
            ssize_t rxSize = recvfrom(socketFd, xferFrameRecPtr, sizeof(frameRec), MSG_DONTWAIT, (struct sockaddr*)&serverAddr, &addressLen);
            if (rxSize > 0)
            {
                logOutput(LOG_RECV, xferFrameRecPtr);
                resendFlag = FALSE;
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
                    resendFlag = TRUE;
                    break;
                }
            }
        }

        if (resendFlag == FALSE)
        {
            xferFrameRecPtr->hdrRec.ackNumber = xferFrameRecPtr->hdrRec.seqNumber + 1;
            xferFrameRecPtr->hdrRec.seqNumber = clientSeqNumber;
            xferFrameRecPtr->hdrRec.packetFlags = LOG_ACK;
            xferFrameRecPtr->hdrRec.dataOffset = 0;

            sendto(socketFd, (const char*)xferFrameRecPtr, sizeof(frameRec),
                   MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));
            logOutput(LOG_SEND, xferFrameRecPtr);

            handshakeFlag = 1;
        }
    }

    return result;
}


int testResend()
{
    struct timespec currentTime;
    uint64_t secondCount;
    uint64_t nsecCount;
    int timeoutFlag = FALSE;
    frameRec* xferFrameRecPtr;
    int testIndex;

    clock_gettime(CLOCK_REALTIME, &currentTime);

    for (testIndex = 0;testIndex < SEL_RPT_WINDOW_COUNT;testIndex++)
    {
        if (txFrameArray[testIndex].inUseFlag == TRUE)
        {
            secondCount = (uint64_t)currentTime.tv_sec - (uint64_t)txFrameArray[testIndex].txTime.tv_sec;
            nsecCount = (uint64_t)currentTime.tv_nsec - (uint64_t)txFrameArray[testIndex].txTime.tv_nsec;
            if (txFrameArray[testIndex].txTime.tv_nsec > currentTime.tv_nsec)
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
                logOutput(LOG_TIMEOUT, &txFrameArray[testIndex].txFrame);

                //printf("*** timeout  sec: %lld  nsec: %lld  data: (0x%X) ***\n", secondCount, nsecCount, dataValue);
                //fflush(stdout);

                // Reset time
                clock_gettime(CLOCK_REALTIME, &txFrameArray[testIndex].txTime);

                xferFrameRecPtr = &txFrameArray[testIndex].txFrame;

                int dataSize = txFrameArray[testIndex].txFrameSize;

                xferFrameRecPtr->hdrRec.packetFlags = LOG_RESEND;

                sendto(socketFd, (const char*)xferFrameRecPtr, dataSize, MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));
                logOutput(LOG_RESEND, xferFrameRecPtr);
            }
        }
    }

    if (++testIndex >= SEL_RPT_WINDOW_COUNT)
    {
        testIndex = 0;
    }

    return testIndex;
}


void freeFrameRec(frameRec* xferFrameRecPtr)
{
    int loopIndex;

    for (loopIndex = 0;loopIndex < SEL_RPT_WINDOW_COUNT;loopIndex++)
    {
        if ((txFrameArray[loopIndex].txFrame.hdrRec.seqNumber == xferFrameRecPtr->hdrRec.ackNumber) && 
            (txFrameArray[loopIndex].txFrame.hdrRec.dataOffset == xferFrameRecPtr->hdrRec.dataOffset) && 
            (txFrameArray[loopIndex].inUseFlag == TRUE))
        {
            txFrameArray[loopIndex].inUseFlag = FALSE;
            break;
        }
    }
}


int countFrameRec()
{
    int loopIndex;
    int frameRecCount = 0;

    for (loopIndex = 0;loopIndex < SEL_RPT_WINDOW_COUNT;loopIndex++)
    {
        if (txFrameArray[loopIndex].inUseFlag == TRUE)
        {
            frameRecCount++;
        }
    }

    return frameRecCount;
}


int transferFile(int socketFd, frameRec* frameRecPtr, FILE* transferFile, long fileLen)
{
    int result = UDP_XFER_NO_ERROR;
    int avblSlotIndex;
    int doneFlag = FALSE;
    long fileOffset = 0;
    frameRec* xferFrameRecPtr;

    while (doneFlag == FALSE)
    {
        for (avblSlotIndex = 0;avblSlotIndex < SEL_RPT_WINDOW_COUNT;avblSlotIndex++)
        {
            if (fileOffset < fileLen)
            {
                if (txFrameArray[avblSlotIndex].inUseFlag == FALSE)
                {
                    txFrameArray[avblSlotIndex].inUseFlag = TRUE;

                    clock_gettime(CLOCK_REALTIME, &txFrameArray[avblSlotIndex].txTime);

                    int readLen = ((fileLen - fileOffset) > MAX_FILE_SEGMENT_SIZE) ? MAX_FILE_SEGMENT_SIZE : (fileLen - fileOffset);

                    fread(txFrameArray[avblSlotIndex].txFrame.payloadData, 1, readLen, transferFile);

                    clientSeqNumber += readLen;

                    if (clientSeqNumber > MAX_SEQ_NUMBER)
                    {
                        clientSeqNumber = readLen;
                    }

                    xferFrameRecPtr = &txFrameArray[avblSlotIndex].txFrame;

                    xferFrameRecPtr->hdrRec.seqNumber = clientSeqNumber;
                    xferFrameRecPtr->hdrRec.ackNumber = 0;
                    xferFrameRecPtr->hdrRec.packetFlags = 0;
                    xferFrameRecPtr->hdrRec.dataOffset = (uint16_t)(fileOffset / 512);

                    int dataSize = readLen + sizeof(headerRec);

                    txFrameArray[avblSlotIndex].txFrameSize = dataSize;

                    //// Force lost frame
                    //if (xferFrameRecPtr->hdrRec.seqNumber != 9576)
                    //{
                    //    sendto(socketFd, (const char*)xferFrameRecPtr, dataSize, MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));
                    //    logOutput(LOG_SEND, xferFrameRecPtr);
                    //}

                    sendto(socketFd, (const char*)xferFrameRecPtr, dataSize, MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));
                    logOutput(LOG_SEND, xferFrameRecPtr);

                    fileOffset += readLen;
                }
            }
        }

        testResend();

        if (countFrameRec() != 0)
        {
            ssize_t rxSize = recvfrom(socketFd, frameRecPtr, sizeof(frameRec), MSG_DONTWAIT, (struct sockaddr*)&serverAddr, &addressLen);
            if (rxSize > 0)
            {
                logOutput(LOG_RECV, frameRecPtr);
                freeFrameRec(frameRecPtr);
            }
        }
        else
        {
            doneFlag = TRUE;
        }
    }

    fclose(transferFile);

    return result;
}


int endTransfer(int socketFd, frameRec* frameRecPtr)
{
    int result = UDP_XFER_NO_ERROR;
    int doneFlag = FALSE;
    int resendFlag = FALSE;
    struct timespec txTime;
    struct timespec currentTime;
    long secondCount;
    long nsecCount;
    int timeoutFlag = FALSE;
    int rxCount = 0;

    int currentSeq = frameRecPtr->hdrRec.seqNumber;

    frameRecPtr->hdrRec.seqNumber = frameRecPtr->hdrRec.ackNumber + 1;
    frameRecPtr->hdrRec.ackNumber = currentSeq + 1;
    frameRecPtr->hdrRec.packetFlags = LOG_FIN;
    frameRecPtr->hdrRec.dataOffset = 0;

    while (doneFlag == FALSE)
    {
        clock_gettime(CLOCK_REALTIME, &txTime);

        sendto(socketFd, (const char*)frameRecPtr, sizeof(frameRec), MSG_CONFIRM, (const struct sockaddr*)&serverAddr, sizeof(serverAddr));
        if (resendFlag == FALSE)
        {
            logOutput(LOG_SEND, frameRecPtr);
        }
        else
        {
            logOutput(LOG_RESEND, frameRecPtr);
        }

        while (TRUE)
        {
            ssize_t rxSize = recvfrom(socketFd, frameRecPtr, sizeof(frameRec), MSG_DONTWAIT, (struct sockaddr*)&serverAddr, &addressLen);
            if (rxSize > 0)
            {
                logOutput(LOG_RECV, frameRecPtr);

                if (++rxCount >= 2)
                {
                    doneFlag = TRUE;
                    break;
                }
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
                    resendFlag = TRUE;
                    break;
                }
            }
        }
    }

    return result;
}


int transferFileToServer(int socketFd, char* fileName)
{
    int result = 0;
    FILE* fileStream;
    long fileLen;
    frameRec xferFrameRec;
    char msgBuffer[100];

    clientSeqNumber = rand() % MAX_SEQ_NUMBER;

    addressLen = sizeof(serverAddr);

    fileStream = fopen(fileName, "rb");

    if (fileStream != 0)
    {
        if (fseek(fileStream, 0, SEEK_END) == 0)
        {
            fileLen = ftell(fileStream);
            fseek(fileStream, 0, SEEK_SET);

            //printf("*** transferFileToServer  fileLen: %ld\n", fileLen);
            //fflush(stdout);

            if (performHandshake(socketFd, &xferFrameRec) == UDP_XFER_NO_ERROR)
            {
                if (transferFile(socketFd, &xferFrameRec, fileStream, fileLen) == UDP_XFER_NO_ERROR)
                {
                    if (endTransfer(socketFd, &xferFrameRec) != UDP_XFER_NO_ERROR)
                    {
                        sprintf(msgBuffer, "End transfer error\n");
                        write(STDERR_FILENO, msgBuffer, strlen(msgBuffer));
                        exit(1);
                    }

                    sleep(FIN_DELAY);
                }
                else
                {
                    sprintf(msgBuffer, "Transfer error\n");
                    write(STDERR_FILENO, msgBuffer, strlen(msgBuffer));
                    exit(1);
                }
            }
            else
            {
                sprintf(msgBuffer, "Handshake error\n");
                write(STDERR_FILENO, msgBuffer, strlen(msgBuffer));
                exit(1);
            }
        }
    }

    return result;
}


int setupClientConnection(int udpPortValue)
{
    int socketFd = -1;

    if ((socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("UDP socket failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(udpPortValue);
    }

    return socketFd;
}


int main(int argc, char** argv)
{
    //int loopIndex;
    //for (loopIndex = 0;loopIndex < argc;loopIndex++)
    //{
    //    printf("*** Client  argv[%d]: %s ***\n", loopIndex, argv[loopIndex]);
    //}

    if (argc != 4)
    {
        printf("*** Parameter count error argc: %d ***\n", argc);
        exit(1);
    }

    socketFd = setupClientConnection(atoi(argv[2]));

    transferFileToServer(socketFd, argv[3]);

    close(socketFd);

    return 0;
}


