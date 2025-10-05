#include "queue.h"
#include "msg.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_QUEUES 3        // número de níveis de prioridade
#define TIME_SLICE 500      // tempo máximo por fatia (500 ms)

// Estrutura de cada nível de prioridade (uma fila por nível)
typedef struct {
    queue_t queue;
} mlfq_level_t;

// Vetor de filas — nível 0 tem a maior prioridade
static mlfq_level_t levels[NUM_QUEUES];

/**
 * Inicializa as filas do MLFQ, garantindo que todas começam vazias.
 */
void mlfq_init(void) {
    for (int i = 0; i < NUM_QUEUES; i++) {
        levels[i].queue.head = NULL;
        levels[i].queue.tail = NULL;
    }
}

/**
 * Adiciona um processo à fila mais prioritária (nível 0).
 *
 * Esta função é chamada sempre que um processo:
 *  - entra no sistema pela primeira vez, ou
 *  - regressa de uma operação de I/O.
 *
 * Ao reiniciar, o processo volta ao topo (nível 0),
 * com os contadores de tempo e fatia (slice) a zero.
 */
void enqueue_mlfq(pcb_t *pcb) {
    pcb->priority_level = 0;       // começa no nível mais alto
    pcb->ellapsed_time_ms = 0;     // reinicia o tempo total de CPU
    pcb->slice_start_ms = 0;       // reinicia o contador do slice atual
    enqueue_pcb(&levels[0].queue, pcb);
}

/**
 * Escalonador MLFQ (Multi-Level Feedback Queue)
 *
 * Funcionamento geral:
 *  - Existem várias filas com diferentes níveis de prioridade.
 *  - Processos novos começam no nível mais alto.
 *  - Se não terminam dentro do time-slice → descem um nível.
 *  - Se terminam (DONE) → são removidos.
 *  - A escolha do próximo processo é sempre feita da fila mais prioritária que tiver tarefas.
 */
void mlfq_scheduler(uint32_t current_time_ms, queue_t *rq /*unused*/, pcb_t **cpu_task) {
    // 1) Atualiza o processo atualmente em execução (se existir)
    if (*cpu_task) {
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;

        // 1.a) Caso tenha terminado o burst, envia DONE para a aplicação
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            msg_t msg = {
                .pid = (*cpu_task)->pid,
                .request = PROCESS_REQUEST_DONE,
                .time_ms = current_time_ms
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            }
            free(*cpu_task);
            *cpu_task = NULL;
        }
        // 1.b) Caso o processo ainda não tenha terminado, verifica o time-slice
        else if ((current_time_ms - (*cpu_task)->slice_start_ms) >= TIME_SLICE) {
            // Se não está na última fila, desce de prioridade
            if ((*cpu_task)->priority_level < NUM_QUEUES - 1) {
                (*cpu_task)->priority_level++;
            }
            // Volta para a nova fila de acordo com a prioridade atual
            enqueue_pcb(&levels[(*cpu_task)->priority_level].queue, *cpu_task);
            *cpu_task = NULL;
        }
    }

    // 2) Se o CPU estiver livre, escolhe o próximo processo
    if (*cpu_task == NULL) {
        for (int i = 0; i < NUM_QUEUES; i++) {
            pcb_t *next = dequeue_pcb(&levels[i].queue);
            if (next) {
                *cpu_task = next;
                // Marca o início de um novo time-slice
                (*cpu_task)->slice_start_ms = current_time_ms;
                break;
            }
        }
    }
}
