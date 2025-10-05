#ifndef FIFO_H
#define FIFO_H

#include "queue.h"

void fifo_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task);

#endif //FIFO_H
