cmake_minimum_required(VERSION 3.25)
project(scheduling C)

set(CMAKE_C_STANDARD 11)

# --- Simulador principal (scheduler) ---
add_executable(scheduler
        ossim.c
        queue.c
        fifo.c
        sjf.c
        rr.c
        mlfq.c
        burst_queue.c
)

# --- Aplicação simples (sem I/O) ---
add_executable(app
        app.c
)

# --- Aplicação com I/O (usa ficheiros CSV) ---
add_executable(app-io
        app-io.c
        burst_queue.c
)
