#define MAX_QUEUE_SIZE 50

typedef struct {
    int index;
    int row[50];
} Queue_int;

// int queue
Queue_int init_queue_int();
int push_int(Queue_int *queue, int value);
int pop_int(Queue_int *queue);
int is_exist_queue_int(Queue_int queue);