#include "queue.h"
#include "msg.h"
#include <stdlib.h>
#include <stdio.h>    // para perror
#include <unistd.h>   // para write()

#define TIME_SLICE 500 // quantum fixo de 500 ms para cada processo

/**
 * Algoritmo Round-Robin (RR)
 *
 * Este escalonador atribui a cada processo um tempo máximo de execução (TIME_SLICE).
 * Quando o tempo se esgota, o processo perde o CPU e volta ao fim da fila,
 * garantindo que todos os processos tenham acesso regular à CPU.
 *
 * Resumo do comportamento:
 *  - Se o processo terminar antes de esgotar o slice → envia DONE e é removido.
 *  - Se o slice terminar e houver processos na fila → o processo atual é preemptado e volta ao fim.
 *  - Se o slice terminar e NÃO houver outros prontos → o mesmo processo continua (reinicia o slice).
 */
void rr_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task) {

    // 1) Atualiza o processo que está a usar o CPU (caso exista)
    if (*cpu_task) {
        // Acrescenta o tempo que o processo já executou neste tick
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;

        // 1.a) Verifica se já executou todo o seu tempo total
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            // O processo terminou a execução
            // Envia mensagem DONE para a aplicação correspondente
            msg_t msg = {
                .pid = (*cpu_task)->pid,
                .request = PROCESS_REQUEST_DONE,
                .time_ms = current_time_ms
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof msg) != sizeof msg) {
                perror("write");
            }

            // Liberta a memória do PCB e marca o CPU como livre
            free(*cpu_task);
            *cpu_task = NULL;
        }
        // 1.b) Caso ainda não tenha terminado, verifica se o slice expirou
        else if ((current_time_ms - (*cpu_task)->slice_start_ms) >= TIME_SLICE) {
            // Se não há mais processos prontos, o mesmo processo continua
            if (rq->head == NULL) {
                // Reinicia o contador de slice para o mesmo processo
                (*cpu_task)->slice_start_ms = current_time_ms;
            } else {
                // Há outros processos na fila → preempção
                // Move o processo atual para o fim da fila e liberta o CPU
                enqueue_pcb(rq, *cpu_task);
                *cpu_task = NULL;
                // O slice_start_ms será atualizado quando o processo voltar ao CPU
            }
        }
    }

    // 2) Caso o CPU esteja livre, retira o próximo processo da ready queue
    if (*cpu_task == NULL) {
        *cpu_task = dequeue_pcb(rq);
        if (*cpu_task) {
            // Regista o início do novo slice para o processo agora escolhido
            (*cpu_task)->slice_start_ms = current_time_ms;
        }
    }
}
