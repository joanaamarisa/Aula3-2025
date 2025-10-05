#include <ctype.h>
#include <stdint.h>

#include "burst_queue.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LEN 1024

int parse_burst_line(const char* line, burst_t* burst) {
    if (!line || !burst) return -1;

    char* line_copy = strdup(line);
    if (!line_copy) return -1;

    char* endptr;
    char* token = strtok(line_copy, ",");

    // Parse required burst_time_ms
    if (!token) {
        fprintf(stderr, "Missing burst time\n");
        free(line_copy);
        return -1;
    }

    long burst_time = strtol(token, &endptr, 10);
    if (*endptr != '\0' || burst_time < 0 || burst_time > INT_MAX) {
        fprintf(stderr, "Invalid burst time: %s\n", token);
        free(line_copy);
        return -1;
    }
    burst->burst_time_ms = (int)burst_time;

    // Optional: block time
    token = strtok(NULL, ",\r\n");
    if (token) {
        long block_time_ms = strtol(token, &endptr, 10);
        if (*endptr != '\0' || block_time_ms < INT_MIN || block_time_ms > INT_MAX) {
            fprintf(stderr, "Invalid block time value: %s\n", token);
            free(line_copy);
            return -1;
        }
        burst->block_time_ms = (int)block_time_ms;
    }

    // Optional: parse nice
    token = strtok(NULL, ",\r\n");
    if (token) {
        long nice_value = strtol(token, &endptr, 10);
        if (*endptr != '\0' || nice_value < INT_MIN || nice_value > INT_MAX) {
            fprintf(stderr, "Invalid nice value: %s\n", token);
            free(line_copy);
            return -1;
        }
        burst->nice = (int)nice_value;
    }


    // Optional: parse pages list
    burst->pages.count = 0;
    token = strtok(NULL, "[");
    if (token) token = strtok(NULL, "]");
    if (token) {
        char* page_token = strtok(token, ",");
        while (page_token &&  burst->pages.count< MAX_PAGES) {
            long page = strtol(page_token, &endptr, 10);
            if (*endptr != '\0' || page < 0 || page > INT_MAX) {
                fprintf(stderr, "Invalid page number: %s\n", page_token);
                free(line_copy);
                return -1;
            }
            burst->pages.ids[burst->pages.count++] = (int)page;
            page_token = strtok(NULL, ",");
        }
    }

    free(line_copy);
    return 0;
}


int read_queue_from_file(burst_queue_t* queue, const char* filename) {
    if (!queue || !filename) return -1;

    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    char line[MAX_LINE_LEN];
    int success_count = 0;

    while (fgets(line, sizeof(line), file)) {
        // Trim leading whitespace
        char* trimmed = line;
        while (isspace(*trimmed)) ++trimmed;

        if (*trimmed == '#' || *trimmed == '\0') continue;

        burst_t burst = {0}; // Initialize burst structure
        if (parse_burst_line(trimmed, &burst) == 0) {
            if (enqueue_burst(queue, &burst)) {
                success_count++;
            } else {
                fprintf(stderr, "Queue full or allocation failed\n");
                break;
            }
        } else {
            fprintf(stderr, "Skipping malformed line: %s", line);
        }
    }

    fclose(file);
    return success_count;
}


int enqueue_burst(burst_queue_t* q, const burst_t* burst) {
    burst_node_t* node = malloc(sizeof(burst_node_t));
    if (!node) return 0;

    node->burst = malloc(sizeof(burst_t));
    if (!node->burst) {
        free(node);
        return 0;
    }

    *node->burst = *burst; // Deep copy the struct
    node->next = NULL;

    if (q->tail) {
        q->tail->next = node;
    } else {
        q->head = node;
    }
    q->tail = node;
    return 1;
}

burst_t* dequeue_burst(burst_queue_t* q) {
    if (!q || !q->head) return NULL;

    burst_node_t* node = q->head;
    burst_t* result = node->burst;

    q->head = node->next;
    if (!q->head)
        q->tail = NULL;

    free(node);
    return result;
}
