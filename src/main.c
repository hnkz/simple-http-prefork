#include "exp1.h"
#include "exp1lib.h"
#include "httplib.h"
#include "websocket.h"

#define MAX_PROCESS_SIZE 1

void create_thread(int sock);
void* thread_func(void *param);

int temporarily_move;
int see_other;

int main(int argc, char **argv)
{
    int i;
    int len;
    int pids[MAX_PROCESS_SIZE];
    int sock_listen;
    int sock_client;
    struct sockaddr_in addr;
    int ws_listen;
    int ws_client;
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
        char ws_head[1024];

        ws_listen = exp1_tcp_listen(22222);
        while(1) {
            int ret;
            char buf[1024];
            memset(buf, 0, sizeof(buf));

            ws_client = accept(ws_listen, (struct sockaddr *)&addr, (socklen_t *)&len);
            if(ws_client < 0) {
                perror("cannot accept client");
                close(ws_listen);
                close(ws_client);              
                break;
            }

            ret = recv(ws_client, buf, sizeof(buf), 0);
            if(ret < 0) {
                write(1, "cannnot recv\n", 13);
                close(ws_listen);
                close(ws_client);
                break;
            }

            char *key = create_secret_key(buf);

            memset(ws_head, 0, sizeof(ws_head));
            snprintf(ws_head, sizeof(ws_head), "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", key);
            ret = send(ws_client, ws_head, strlen(ws_head), 0);
            if(ret < 0) {
                write(1, "cannnot send\n", 13);
                close(ws_listen);
                close(ws_client);
                break;
            }
            free(key);
            write(1, ws_head, sizeof(ws_head));

            while(1) {
                memset(buf, 0, sizeof(buf));
                ret = recv(ws_client, buf, sizeof(buf), 0);
                if(ret < 0) {
                    write(1, "cannnot recv\n", 13);
                    close(ws_listen);
                    close(ws_client);
                    break;
                }

                ws_frame *frame;
                frame = (ws_frame *)buf;
                // print_ws_frame(*frame);

                char data[2048];
                int pay_len;
                memset(data, 0, sizeof(data));
                pay_len = get_payload(*frame, data);

                ws_frame send_frame;
                memset((void *)&send_frame, 0, sizeof(ws_frame));
                send_frame.flags[0] = 0x81;
                send_frame.flags[1] = 0x00 | pay_len;
                memset(send_frame.mask_key, 0, 4);
                memcpy(send_frame.payload, data, sizeof(data));

                // print_ws_frame(send_frame);

                // If mask flag set 1
                if(send_frame.flags[1] & 0x80) {
                    ret = send(ws_client, (void *)&send_frame, sizeof(send_frame.flags) + sizeof(send_frame.mask_key) + (send_frame.flags[1] & 0x7f) , 0);
                    if(ret < 0) {
                        write(1, "cannnot send\n", 13);
                        close(ws_listen);
                        close(ws_client);
                        break;
                    }
                } else {
                    // flags
                    ret = send(ws_client, (void *)&send_frame.flags, sizeof(send_frame.flags) , 0);
                    if(ret < 0) {
                        write(1, "cannnot send\n", 13);
                        close(ws_listen);
                        close(ws_client);
                        break;
                    }

                    // payload
                    ret = send(ws_client, (void *)&send_frame.payload, send_frame.flags[1] & 0x7f , 0);
                    if(ret < 0) {
                        write(1, "cannnot send\n", 13);
                        close(ws_listen);
                        close(ws_client);
                        break;
                    }
                }
            }

            close(ws_client);
        }
        _exit(0);
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