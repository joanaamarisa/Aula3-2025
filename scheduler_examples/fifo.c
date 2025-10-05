#include "fifo.h"
#include <stdio.h>
#include <stdlib.h>
#include "msg.h"
#include <unistd.h>

/**
 * Algoritmo de escalonamento FIFO (First-In-First-Out)
 *
 * O FIFO é o método mais simples de escalonamento.
 * Cada processo é colocado na fila de prontos (ready queue)
 * conforme a sua chegada, e será executado apenas quando
 * todos os processos anteriores terminarem.
 * Ou seja, o primeiro a entrar é o primeiro a sair.
 *
 * Quando um processo termina, o próximo da fila é escolhido
 * para ocupar o CPU.
 *
 * Parâmetros:
 *  - current_time_ms: tempo atual da simulação em milissegundos
 *  - rq: ponteiro para a fila de processos prontos
 *  - cpu_task: ponteiro duplo para o processo atualmente no CPU
 */
void fifo_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task) {
    // Verifica se há um processo a ser executado no momento
    if (*cpu_task) {
        // Atualiza o tempo que o processo já utilizou de CPU
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;

        // Caso o tempo de execução atinja o valor total definido
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            // O processo terminou a sua execução

            // Cria uma mensagem para avisar a aplicação de que o processo acabou
            msg_t msg = {
                .pid = (*cpu_task)->pid,
                .request = PROCESS_REQUEST_DONE,
                .time_ms = current_time_ms
            };

            // Envia a mensagem pelo socket associado ao processo
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            }

            // Liberta a memória usada pelo processo (já terminou)
            free((*cpu_task));

            // Indica que o CPU está livre novamente
            (*cpu_task) = NULL;
        }
    }

    // Se o CPU estiver livre (não há processo em execução)
    if (*cpu_task == NULL) {
        // Retira o próximo processo da fila de prontos
        // (FIFO → o primeiro que entrou é o primeiro a ser executado)
        *cpu_task = dequeue_pcb(rq);
    }
}
