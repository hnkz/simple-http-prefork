#include "exp1.h"
#include "queue.h"

Queue_int init_queue_int() {
    Queue_int queue;
    queue.index = 0;

    return queue;
}

int push_int(Queue_int *queue, int value) {

    // queue size is max
    if(queue->index >= MAX_QUEUE_SIZE)
        return -1;
    
    queue->row[(queue->index)++] = value;
    return 0;
}

int pop_int(Queue_int *queue) {
    if(queue->index == 0) {
        fprintf(stderr, "Queue is empty\n");
        _exit(-1);
    }

    return queue->row[--queue->index];
}

int is_exist_queue_int(Queue_int queue) {
    if(queue.index == 0)
        return 0;
    
    return 1;
}