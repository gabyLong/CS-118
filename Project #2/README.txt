NAME:    Gabriella Long
EMAIL:   gabriellalong@g.ucla.edu

High Level Design

Project 2 is a UDP implementation of a Selective Repeat window-based reliable file transfer mechanism. The server application launches and opens a user-specified port, then waits for a client connection. The client launches on the same user-specified port and performs a handshake with the server to establish the UDP file transfer connection. Once the file transfer connection is established the client begins to send data packets with 512-byte file payloads using the Selective Repeat loss detection and packet recovery mechanism using per-packet timers. As each packet is transmitted from the client a timestamp is established. As ACK packets are received from the server, the Selective Repeat window slot is reused with the next packet. Any packet not acknowledged will timeout and be resent. When the entire file is successfully delivered, the client waits two seconds then performs a connection tear-down and exits. When the server detects and completes the client's connection tear-down, it waits to provide the next client connection and UDP file transfer. The "tc" utility was used to provide packet delivery errors to validate the data loss and recovery mechanism.

Problems

One problem I encountered was attempting to use multiple Posix timers when implementing the Selective Repeat window. Using the timer expiration events I could not get the timers to generate individual signal values. Instead is used clock_gettime() and clock_settime() functions to set a per-packet timestamp then detect when the 0.5 second timeout occurred to trigger a retransmission.

A second problem I encountered was managing out-of-order packet delivery. To address this problem I allocated a global buffer to contain the maximum (10MB) file size. As valid packets were delivered they were copied into the buffer at their correct offset as specified by an offset field in the packet's header.

Acknowledgements

Time difference calculation:
https://raspberry-projects.com/pi/programming-in-c/timing/clock_gettime-for-acurate-timing