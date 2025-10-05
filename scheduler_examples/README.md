# Scheduler Simulator

The objective of this project is to simulate various CPU scheduling algorithms and
analyze their performance based on different metrics. The simulator allows users to
input a set of processes with their respective arrival times, burst times, and priorities,
and then applies different scheduling algorithms to determine the order of execution.

## Message Format
Each message sends the application PID, the message request type and a time parameter.
Since we are using Unix Domain Sockets (sender and receiver on the same machine), we can
safely pass structs between the application and the simulator.

### Messages from the application to the simulator:
The messages from the application to the simulator (RUN/BLOCK) send the time in ms
that the process requests the CPU or the I/O device.
Although this is not completely realistic, it simplifies the implementation of the simulator
and allows us to focus on the scheduling algorithms.

### Messages from the simulator to the application:
The messages from the simulator to the application (ACK/EXIT) send the current time in ms
in the simulation ("wall clock"). This allows the application to keep track of the time even if
we take some time debugging the code.

## Time Diagram
The time diagram below illustrates the interaction between the application and the simulator:

```
Simulator                           Applications
   |                                    |
   | <---- App1 RUN (run time) -------- |
   |                                    |
   | ---- App1 ACK (current time) ----> |
   |                                    |
   | <---- App2 RUN (run time) -------- |
   |                                    |
   | ---- App2 ACK (current time) ----> |
   |                                    |
   | ---- App1 DONE (current time) ---> |   --> App 1 prints its statistics and terminates
   |                                    |
   | ---- App2 DONE (current time) ---> |   --> App 2 prints its statistics and terminates
   |
 (keeps running)
   ```

## Scheduling Algorithms

### FIFO (First In First Out)
The FIFO scheduling algorithm processes tasks in the order they arrive. The first task to arrive is the
first to be executed.
This is already implemented in the simulator.

### SJF (Shortest Job First)
The SJF scheduling algorithm selects the task with the shortest burst time to execute next.

### Round Robin
The Round Robin scheduling algorithm assigns a fixed time slice to each task in the queue. Each task
is executed for a maximum of the time slice before being moved to the back of the queue.
In the simulator, create a first version of Round Robin with a time slice of 0.5s.

### MLFQ (Multi-Level Feedback Queue)
The MLFQ scheduling algorithm uses multiple queues with different priority levels. The app to be used
here is app-pre, which not only sends burst times, but also block times. The app-pre has a filename as
command line argument, which contains on each line the burst time and the block time (in ms) of each cycle.
Start by using time-slices of 0.5s.

Hint: The diagram used here is slightly different from the one used in class, as it includes not only RUN
messages, but also BLOCK messages. The BLOCK messages are used to simulate I/O operations.

```
Simulator                           Applications
   |                                    |
   | <---- App1 RUN (run time) -------- |
   |                                    |
   | ---- App1 ACK (current time) ----> |
   |                                    |
   | <---- App2 RUN (run time) -------- |
   |                                    |
   | ---- App2 ACK (current time) ----> |
   |                                    |
   | ---- App1 DONE (current time) ---> | 
   |                                    |
   | <---- App1 BLOCK (run time) ------ |
   |                                    |
   | ---- App1 ACK (current time) ----> |
   |                                    |
   | ---- App2 DONE (current time) ---> | 
   |                                    |
   | <---- App2 BLOCK (run time) ------ |
   |                                    |
   | ---- App2 ACK (current time) ----> |
   |                                    |
   | ---- App1 DONE (current time) ---> | 
   |                                    |
   | ---- App2 DONE (current time) ---> | 
```

