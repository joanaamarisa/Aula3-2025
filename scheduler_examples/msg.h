#ifndef COMMON_H
#define COMMON_H

// Not really the correct place, but this file is included where it is necessary,
// and it did not feel like making a new file just for this was justified.

#define TICKS_MS 10

#include <stdint.h>
#include <sys/types.h>

#define SOCKET_PATH "/tmp/scheduler.sock"

#define MAX_PAGES 32

// Define process request strings for debugging purposes
static const char PROCESS_REQUEST_STRINGS[][10] = {
    "RUN",
    "BLOCK",
    "ACK",
    "DONE"
};

// Define the types of requests a process can make to the scheduler
typedef enum  {
    PROCESS_REQUEST_RUN = 0,
    PROCESS_REQUEST_BLOCK,
    PROCESS_REQUEST_ACK,
    PROCESS_REQUEST_DONE,
} process_request_t;

// Define the structure for page information
// Note: Not used until we get to memory management, but defined here for completeness
typedef struct {
    uint32_t count;            // Number of pages in the burst
    uint32_t ids[MAX_PAGES];      // Array of pages (up to MAX_PAGES)
} page_info_t;

// Define the message structure for communication between applications and the scheduler
// This structure is sent over the socket
typedef struct {
    pid_t pid;                      // Process ID
    process_request_t request;      // Request type
    uint32_t time_ms;               // Time information
} msg_t;


#endif //COMMON_H
