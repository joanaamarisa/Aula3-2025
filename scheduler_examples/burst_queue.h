#ifndef BURST_QUEUE_H
#define BURST_QUEUE_H

#include "msg.h"

typedef struct {
    uint32_t burst_time_ms;         // Burst time in milliseconds
    uint32_t block_time_ms;         // Burst time in milliseconds
    int nice;                       // Nice value (priority)
    page_info_t pages;
} burst_t;


typedef struct burst_node {
    burst_t *burst;
    struct burst_node* next;
} burst_node_t;

typedef struct burst_queue_st  {
    burst_node_t* head;
    burst_node_t* tail;
} burst_queue_t;

int read_queue_from_file(burst_queue_t* queue, const char* filename);
int enqueue_burst(burst_queue_t* q, const burst_t* burst);
burst_t* dequeue_burst(burst_queue_t* q);


#endif //BURST_QUEUE_H
