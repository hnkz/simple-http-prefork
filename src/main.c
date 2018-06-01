#include "exp1.h"
#include "exp1lib.h"
#include "httplib.h"
#include "websocket.h"

#define MAX_PROCESS_SIZE 10
#define MAX_CLIENTS 200

void create_thread(int sock);
void* thread_func(void *param);

int temporarily_move;
int see_other;

int g_clients[MAX_CLIENTS];

void exp1_init_clients() {
    int i;

    for(i = 0; i < MAX_CLIENTS; i++){
        g_clients[i] = 0;
    }
}

void exp1_set_fds(fd_set* pfds, int accept_sock) {
    int i;

    FD_ZERO(pfds);
    FD_SET(accept_sock, pfds);

    for(i = 0; i < MAX_CLIENTS; i++){
        if(g_clients[i] == 1){
            FD_SET(i, pfds);
        }
    }
}

void exp1_add(int sock) {
    if(sock < MAX_CLIENTS){
        g_clients[sock] = 1;
    }else{
        printf("connection overflow\n");
        exit(-1);
    }
}

void exp1_remove(int id) {
    g_clients[id] = 0;
}

int exp1_get_max_sock() {
    int i;
    int max_sock = 0;

    for(i = 0; i < MAX_CLIENTS; i++){
        if(g_clients[i] == 1){
            max_sock = i;
        }
    }
    return max_sock;
}

int exp1_broadcast(ws_frame frame) {
    char data[2048];
    int pay_len;
    int ret;

    memset(data, 0, sizeof(data));
    pay_len = get_payload(frame, data);
    
    ws_frame send_frame;

    // create ws_frame for text message
    create_text_frame(&send_frame, data, pay_len);

    int i;
    for(i = 0; i < MAX_CLIENTS; i++){
        if(g_clients[i] == 1){
            // If mask flag set 1
            if(send_frame.flags[1] & 0x80) {
                ret = send(i, (void *)&send_frame, sizeof(send_frame.flags) + sizeof(send_frame.mask_key) + (send_frame.flags[1] & 0x7f) , 0);
                if(ret < 0) {
                    write(1, "cannnot send\n", 13);
                    exp1_remove(i);
                    break;
                }
            } else {
                // flags
                ret = send(i, (void *)&send_frame.flags, sizeof(send_frame.flags) , 0);
                if(ret < 0) {
                    write(1, "cannnot send\n", 13);
                    exp1_remove(i);
                    break;
                }

                // payload
                ret = send(i, (void *)&send_frame.payload, send_frame.flags[1] & 0x7f , 0);
                if(ret < 0) {
                    write(1, "cannnot send\n", 13);
                    exp1_remove(i);
                    break;
                }
            }
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    int i;
    int pids[MAX_PROCESS_SIZE];
    int sock_listen;
    int sock_client;
    int ws_pid;

    if(argc == 3) {
        temporarily_move = atoi(argv[1]);
        see_other = atoi(argv[2]);
    } else {
        printf("usage: %s <temporarily_move> <see_other>\n", argv[0]);
        return 0;
    }

    // websocket
    ws_pid = fork();
    if(ws_pid == 0) {
        fd_set fds;
        int ws_listen;
        int max_sock;
        int ret;

        ws_listen = exp1_tcp_listen(22222);

        while(1) {
            exp1_set_fds(&fds, ws_listen);
            max_sock = exp1_get_max_sock();

            if(max_sock < ws_listen) {
                max_sock = ws_listen;
            }

            select(max_sock + 1, &fds, NULL, NULL, NULL);

            if(FD_ISSET(ws_listen, &fds) != 0){
                int sock;
                char buf[1024];
                
                sock = accept(ws_listen, NULL, NULL);
                if(sock < 0) {
                    perror("accept()");
                }
                memset(buf, 0, sizeof(buf));
                ret = recv(sock, buf, sizeof(buf), 0);
                if(ret < 0) {
                    write(1, "cannnot recv\n", 13);
                    close(ws_listen);
                }

                // send response head
                ret = send_response_head(sock, buf);
                if(ret == -1) {
                    close(ws_listen);
                }
                exp1_add(sock);
            }

            for(i = 0; i < MAX_CLIENTS; i++){
                if(g_clients[i] == 0){
                    continue;
                }

                if(FD_ISSET(i, &fds) != 0){
                    char buf[1024];
                    memset(buf, 0, sizeof(buf));
                    ret = recv(i, buf, sizeof(buf), 0);
                    
                    ws_frame *frame;
                    frame = (ws_frame *)buf;

                    if(ret > 0){
                        if(frame->flags[0] & 0x08) {
                            exp1_remove(i);
                        } else {
                            exp1_broadcast(*frame);
                        }
                    }else{
                        exp1_remove(i);
                    }
                }
            }
        }
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
        // sock_client = accept(sock_listen, (struct sockaddr *)&addr, (socklen_t *)&len);
        sock_client = accept(sock_listen, NULL, NULL);

        if(sock_client < 0) {
            perror("accept()");
            continue;
        }

        create_thread(sock_client);
    }
}

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