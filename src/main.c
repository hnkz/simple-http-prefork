#include "exp1.h"
#include "exp1lib.h"
#include "httplib.h"
#include "queue.h"

#define MAX_PROCESS_SIZE 1

#define READ 0
#define WRITE 1

int temporarily_move = 0;

void* thread_func(void *param) {
    int* psock;
    int sock;

    pthread_detach(pthread_self());
    psock = (int*) param;
    sock = *psock;
    free(psock);
    
    http_session(sock);
    
    shutdown(sock, SHUT_RDWR);
    close(sock);

    return 0;
}

void create_thread(int sock) {
  int *psock;
  pthread_t th;
  psock = malloc(sizeof(int));
  *psock = sock;

  pthread_create(&th, NULL, thread_func, psock);
}

int main(int argc, char **argv)
{
    int i;
    int my_id = -1;
    int len;
    int pids[MAX_PROCESS_SIZE];
    int sock_listen;
    int sock_client;
    struct sockaddr_in addr;

    if(argc == 2) {
        temporarily_move = atoi(argv[1]);
    }

    sock_listen = exp1_tcp_listen(12345);

    // create process
    for(i = 0; i < MAX_PROCESS_SIZE; i++) {
        pids[i] = fork();

        if(pids[i] == -1) {
            perror("cannot fork");
            return -1;
        }

        if(pids[i] == 0) {
            break;
        } else {

        }
    }

    while(1) {
        sock_client = accept(sock_listen, (struct sockaddr *)&addr, (socklen_t *)&len);

        if(sock_client < 0) {
            perror("cannot accept client");
            continue;
        }

        create_thread(sock_client);
    }
}
