#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "queue.h"
#include "msg.h"
#include "fifo.h"
#include "debug.h"

// Protótipos dos diferentes escalonadores
void sjf_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task);
void rr_scheduler (uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task);

// Funções específicas do MLFQ (definidas em mlfq.c)
void mlfq_init(void);
void enqueue_mlfq(pcb_t *pcb);
void mlfq_scheduler(uint32_t current_time_ms, queue_t *rq /*unused*/, pcb_t **cpu_task);

// Enum que representa o escalonador ativo
typedef enum  {
    NULL_SCHEDULER = -1,
    SCHED_FIFO = 0,
    SCHED_SJF,
    SCHED_RR,
    SCHED_MLFQ
} scheduler_en;

static const char *SCHEDULER_NAMES[] = {"FIFO","SJF","RR","MLFQ",NULL};

// ---------------------------------------------------------
// Funções utilitárias
// ---------------------------------------------------------

// Define um descritor de ficheiro como “non-blocking” (não bloqueante)
// Isto permite que o servidor continue a correr mesmo que não haja mensagens.
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Variável global que serve para terminar o programa com Ctrl+C
static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

// ---------------------------------------------------------
// Criação do socket servidor UNIX
// ---------------------------------------------------------
static int make_server_socket(const char *path) {
    // Remove sockets antigos que possam existir
    unlink(path);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

    // Associa o socket ao caminho (bind)
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    // Coloca o socket a “escutar” novas ligações
    if (listen(fd, 32) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    // Define o socket como não bloqueante
    set_nonblocking(fd);
    return fd;
}

// ---------------------------------------------------------
// Leitura de mensagens dos clientes (apps)
// ---------------------------------------------------------
static int read_msg_nonblock(int sockfd, msg_t *out) {
    ssize_t n = recv(sockfd, out, sizeof(*out), MSG_DONTWAIT);
    if (n == 0) {
        // O cliente fechou a ligação
        return 0;
    }
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return -2; // nada para ler agora
        return -1; // erro real
    }
    if ((size_t)n != sizeof(*out)) return -1;
    return 1; // leitura bem sucedida
}

// ---------------------------------------------------------
// Filas usadas no simulador:
//   - command_q: sockets ligados (para receber pedidos)
//   - ready_q:   processos prontos (usado por FIFO/SJF/RR)
//   - blocked_q: processos bloqueados (I/O em curso)
//   - cpu_task:  processo em execução no CPU
// ---------------------------------------------------------

/**
 * Aceita novas ligações e trata mensagens RUN/BLOCK de todas as ligações ativas.
 *
 * RUN  → envia ACK e adiciona o processo à fila certa:
 *          - MLFQ → enqueue_mlfq(p)
 *          - restantes → enqueue_pcb(ready_q, p)
 *
 * BLOCK → envia ACK e coloca o processo em blocked_q.
 *
 * Cada ligação mantém um PCB “de comando” apenas para guardar o socket ativo.
 */
static void check_new_commands(queue_t *command_q,
                               queue_t *blocked_q,
                               queue_t *ready_q,
                               int server_fd,
                               uint32_t now_ms,
                               scheduler_en scheduler)
{
    // 1) Aceitar novas ligações (modo não bloqueante)
    while (1) {
        int client = accept(server_fd, NULL, NULL);
        if (client < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("accept");
            break;
        }
        set_nonblocking(client);

        // Cria um PCB apenas para representar esta ligação (comando)
        pcb_t *cmd = new_pcb(-1, (uint32_t)client, 0);
        if (!cmd) { close(client); continue; }
        cmd->status = TASK_COMMAND;
        enqueue_pcb(command_q, cmd);
        DBG("New client connected (fd=%d)", client);
    }

    // 2) Lê mensagens de todos os sockets ligados (sem remover da queue)
    for (queue_elem_t *it = command_q->head; it != NULL; it = it->next) {
        pcb_t *cmd = it->pcb;
        if (!cmd) continue;

        msg_t msg;
        int r = read_msg_nonblock((int)cmd->sockfd, &msg);
        if (r == -2) continue;     // nada para ler neste tick
        if (r <= 0) {
            if (r == 0) {
                DBG("Client fd=%d closed connection", (int)cmd->sockfd);
            } else {
                perror("read");
            }
            close((int)cmd->sockfd);
            cmd->sockfd = (uint32_t)-1;
            continue;
        }

        // Envia resposta imediata (ACK) a cada pedido recebido
        msg_t ack = {
            .pid = msg.pid,
            .request = PROCESS_REQUEST_ACK,
            .time_ms = now_ms
        };
        if (write((int)cmd->sockfd, &ack, sizeof(ack)) != sizeof(ack)) {
            perror("write(ACK)");
            continue;
        }

        // Tratamento do pedido recebido
        if (msg.request == PROCESS_REQUEST_RUN) {
            // Cria um novo PCB para este burst de execução
            pcb_t *p = new_pcb(msg.pid, cmd->sockfd, msg.time_ms);
            if (!p) continue;
            p->status = TASK_RUNNING;
            p->ellapsed_time_ms = 0;
            p->slice_start_ms = 0;

            if (scheduler == SCHED_MLFQ) {
                enqueue_mlfq(p); // MLFQ gere internamente as suas filas
            } else {
                enqueue_pcb(ready_q, p);
            }

            DBG("Process %d requested RUN for %u ms", p->pid, p->time_ms);
        }
        else if (msg.request == PROCESS_REQUEST_BLOCK) {
            // O processo pediu I/O → vai para a fila de bloqueados
            pcb_t *p = new_pcb(msg.pid, cmd->sockfd, msg.time_ms);
            if (!p) continue;
            p->status = TASK_BLOCKED;
            p->ellapsed_time_ms = 0;
            p->last_update_time_ms = now_ms;
            enqueue_pcb(blocked_q, p);

            DBG("Process %d requested BLOCK for %u ms", p->pid, p->time_ms);
        }
        else {
            // Pedido não reconhecido (segurança extra)
            DBG("Unexpected request from pid=%d type=%d", (int)msg.pid, (int)msg.request);
        }
    }
}

/**
 * Atualiza os processos bloqueados (I/O).
 * Quando o tempo de bloqueio termina, envia uma mensagem DONE ao processo
 * e remove-o da lista de bloqueados.
 */
static void check_blocked_queue(queue_t *blocked_q, uint32_t now_ms) {
    queue_elem_t *it = blocked_q->head;
    while (it) {
        pcb_t *p = it->pcb;
        if (p && p->status == TASK_BLOCKED) {
            p->ellapsed_time_ms += TICKS_MS;

            if (p->ellapsed_time_ms >= p->time_ms) {
                // O processo terminou o I/O → envia DONE
                msg_t done = {
                    .pid = p->pid,
                    .request = PROCESS_REQUEST_DONE,
                    .time_ms = now_ms
                };
                if (write((int)p->sockfd, &done, sizeof(done)) != sizeof(done)) {
                    perror("write(DONE:BLOCK)");
                }

                // Remove da fila sem quebrar o iterador
                queue_elem_t *to_remove = it;
                it = it->next;
                queue_elem_t *removed = remove_queue_elem(blocked_q, to_remove);
                if (removed) {
                    free(removed->pcb);
                    free(removed);
                }
                continue;
            }
        }
        it = it->next;
    }
}

// ---------------------------------------------------------
// Identificação do escalonador a usar
// ---------------------------------------------------------
static scheduler_en get_scheduler(const char *name) {
    if (!name) return NULL_SCHEDULER;
    if (!strcmp(name, "FIFO"))  return SCHED_FIFO;
    if (!strcmp(name, "SJF"))   return SCHED_SJF;
    if (!strcmp(name, "RR"))    return SCHED_RR;
    if (!strcmp(name, "MLFQ"))  return SCHED_MLFQ;
    return NULL_SCHEDULER;
}

// ---------------------------------------------------------
// Função principal do simulador (main)
// ---------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <FIFO|SJF|RR|MLFQ>\n", argv[0]);
        return EXIT_FAILURE;
    }

    scheduler_en scheduler_type = get_scheduler(argv[1]);
    if (scheduler_type == NULL_SCHEDULER) {
        fprintf(stderr, "Invalid scheduler '%s'. Use FIFO, SJF, RR or MLFQ.\n", argv[1]);
        return EXIT_FAILURE;
    }

    signal(SIGINT, on_sigint);

    int server_fd = make_server_socket(SOCKET_PATH);
    if (server_fd < 0) return EXIT_FAILURE;

    printf("Scheduler server listening on %s...\n", SOCKET_PATH);
    printf("Active scheduler: %s\n", SCHEDULER_NAMES[scheduler_type]);

    // Estruturas principais
    queue_t command_queue = {.head=NULL, .tail=NULL};
    queue_t ready_queue   = {.head=NULL, .tail=NULL};
    queue_t blocked_queue = {.head=NULL, .tail=NULL};
    pcb_t *cpu_task = NULL;

    if (scheduler_type == SCHED_MLFQ) {
        mlfq_init(); // inicializa as filas internas do MLFQ
    }

    // Ciclo principal da simulação
    uint32_t current_time_ms = 0;
    uint32_t last_print_s = 0;

    while (!g_stop) {
        // 1) Receber pedidos novos das aplicações
        check_new_commands(&command_queue, &blocked_queue, &ready_queue,
                           server_fd, current_time_ms, scheduler_type);

        // 2) Atualizar a fila de bloqueados
        check_blocked_queue(&blocked_queue, current_time_ms);

        // 3) Executar o escalonador ativo
        switch (scheduler_type) {
            case SCHED_FIFO:
                fifo_scheduler(current_time_ms, &ready_queue, &cpu_task);
                break;
            case SCHED_SJF:
                sjf_scheduler(current_time_ms, &ready_queue, &cpu_task);
                break;
            case SCHED_RR:
                rr_scheduler(current_time_ms, &ready_queue, &cpu_task);
                break;
            case SCHED_MLFQ:
                mlfq_scheduler(current_time_ms, &ready_queue, &cpu_task);
                break;
            default:
                break;
        }

        // 4) Mostrar tempo de simulação uma vez por segundo
        if ((current_time_ms / 1000) != last_print_s) {
            last_print_s = current_time_ms / 1000;
            printf("Current time: %u s\n", last_print_s);
            fflush(stdout);
        }

        // 5) Avançar o tempo da simulação (tick)
        usleep(TICKS_MS * 1000);
        current_time_ms += TICKS_MS;
    }

    // Encerramento e limpeza final
    close(server_fd);
    unlink(SOCKET_PATH);

    // Liberta memória das filas restantes
    while (command_queue.head) free(dequeue_pcb(&command_queue));
    while (ready_queue.head)   free(dequeue_pcb(&ready_queue));
    while (blocked_queue.head) free(dequeue_pcb(&blocked_queue));
    if (cpu_task) free(cpu_task);

    return EXIT_SUCCESS;
}
