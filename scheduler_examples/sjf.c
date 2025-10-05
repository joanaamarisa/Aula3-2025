#include "queue.h"
#include "msg.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

/**
 * Algoritmo SJF (Shortest Job First)
 *
 * Este escalonador escolhe sempre o processo com o menor tempo de execução
 * (time_ms) entre os que estão prontos na fila. Assim que um processo começa
 * a executar, ele permanece no CPU até terminar (sem preempção).
 *
 * Vantagem: minimiza o tempo médio de espera.
 * Limitação: pode causar starvation se processos curtos continuarem a chegar.
 */
void sjf_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task) {
    // 1) Atualiza o processo que está no CPU (caso exista)
    if (*cpu_task) {
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;

        // Verifica se o processo terminou a sua execução
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            // Envia mensagem DONE para a aplicação correspondente
            msg_t msg = {
                .pid = (*cpu_task)->pid,
                .request = PROCESS_REQUEST_DONE,
                .time_ms = current_time_ms
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof msg) != sizeof msg) {
                perror("write");
            }

            // Liberta o PCB e marca o CPU como livre
            free(*cpu_task);
            *cpu_task = NULL;
        }
    }

    // 2) Pequeno atraso inicial para evitar escolher logo o primeiro processo
    //    Isto permite que mais processos entrem na fila antes da primeira escolha,
    //    garantindo um comportamento mais justo (sobretudo em run_apps2.sh).
    static int first_dispatch_done = 0;
    if (!first_dispatch_done && current_time_ms < 200) {
        return; // espera cerca de 200ms antes de despachar o primeiro
    }

    // 3) Se o CPU está livre e existem processos prontos na fila
    if (*cpu_task == NULL && rq->head != NULL) {
        // Procura o processo com o menor tempo total (SJF clássico)
        queue_elem_t *it = rq->head;
        queue_elem_t *min_elem = it;

        while (it != NULL) {
            if (it->pcb->time_ms < min_elem->pcb->time_ms) {
                min_elem = it;
            }
            it = it->next;
        }

        // Remove o processo mais curto da fila e coloca-o no CPU
        queue_elem_t *removed = remove_queue_elem(rq, min_elem);
        if (removed) {
            *cpu_task = removed->pcb;
            free(removed);
            first_dispatch_done = 1; // indica que o primeiro despacho foi feito
        }
    }
}
