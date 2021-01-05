//
//  proj2_header_rec.h
//
//  NAME: Gabriella Long
//  EMAIL: gabriellalong@g.ucla.edu
//
//  Created by Gabriella Kaarina Long on 5/30/20.
//

#ifndef proj2_header_rec_h
#define proj2_header_rec_h

#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdint.h>

#define headerRecSize           12
#define nullPtr                 0
#define MAX_SEQ_NUMBER          25600
#define TRUE                    1
#define FALSE                   0

#define MAX_FILE_SEGMENT_SIZE   512
#define MAX_FILE_SIZE           ((1024 * 1024) * 10)    // 10MB
#define SEL_RPT_WINDOW_COUNT    10

#define HALF_SECOND_NS          500000000L
#define ONE_SECOND_NS           1000000000L

enum
{
    LOG_RECV = 1,
    LOG_SEND = 2,
    LOG_RESEND = 4,
    LOG_TIMEOUT = 8,
    LOG_SYN = 0x10,
    LOG_FIN = 0x20,
    LOG_ACK = 0x40,
    LOG_DUP_ACK = 0x80
};

enum
{
    UDP_XFER_NO_ERROR = 0,
    UDP_XFER_HANDSHAKE_ERROR,
    UDP_XFER_FILEXFER_ERROR,
    UDP_XFER_END_ERROR
};


typedef struct
{
    uint32_t seqNumber;
    uint32_t ackNumber;
    uint16_t dataOffset;
    uint16_t packetFlags;
} headerRec;


typedef struct
{
    headerRec hdrRec;
    uint8_t payloadData[512];
} frameRec;


typedef struct
{
    frameRec txFrame;
    int txFrameSize;
    struct timespec txTime;
    uint8_t inUseFlag;
} txFrameRec;



#endif /* proj2_header_rec_h */
