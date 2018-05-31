#include "exp1.h"
#include "exp1lib.h"
#include "httplib.h"
#include "queue.h"
#include <openssl/sha.h>

#define MAX_PROCESS_SIZE 1

#define READ 0
#define WRITE 1

static char encoding_table[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};
static int mod_table[] = {0, 2, 1};

int temporarily_move;
int see_other;

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


char *base64_encode(const unsigned char *data, size_t input_length, size_t *output_length) {
    *output_length = 4 * ((input_length + 2) / 3);

    char *encoded_data = malloc(*output_length);
    if (encoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {

        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[*output_length - 1 - i] = '=';

    return encoded_data;
}

typedef struct {
    unsigned char flags[2];
    unsigned char mask_key[4];
    unsigned char payload[2048];
} ws_frame;

int get_payload(ws_frame frame, char *payload) {
    int i;
    for(i = 0; i < (frame.flags[1] & 0x7f); i++) {
        payload[i] = frame.payload[i] ^ frame.mask_key[i % 4];
    }

    return frame.flags[1] & 0x7f;
}

char *create_secret_key(char *buf) {
    char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char key_magic[] = "Sec-WebSocket-Key: ";

    char *secret_key = strstr(buf, key_magic);
    secret_key += strlen(key_magic);
    char *end = strchr(secret_key, '\r');
    *end = '\0';

    char hash_data[512];
    memset(hash_data, 0, sizeof(hash_data));
    sprintf(hash_data, "%s%s", secret_key, magic);

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)hash_data, strlen(hash_data), hash);

    size_t send_key_len;
    return base64_encode(hash, sizeof(hash), &send_key_len);
}

void print_ws_frame(ws_frame frame) {
    char opcode[32];
    char payload_len[32];
    char mask_key[64];
    char payload[2048];

    memset(opcode, 0, sizeof(opcode));
    memset(payload_len, 0, sizeof(payload_len));
    memset(mask_key, 0, sizeof(mask_key));
    memset(payload, 0, sizeof(payload));

    write(1, "fin: ", 5);
    if(frame.flags[0] & 0x80)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    write(1, "rsv1: ", 6);
    if(frame.flags[0] & 0x40)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    write(1, "rsv2: ", 6);
    if(frame.flags[0] & 0x20) 
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    write(1, "rsv3: ", 6);
    if(frame.flags[0] & 0x10)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    sprintf(opcode, "opcode: %u\n", frame.flags[0] & 0x0f);
    write(1, opcode, sizeof(opcode));

    write(1, "mask: ", 6);
    if(frame.flags[1] & 0x80)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    sprintf(payload_len, "payload_len: %d\n", frame.flags[1] & 0x7f);
    write(1, payload_len, sizeof(payload_len));

    sprintf(mask_key, "mask_key: %x%x%x%x\n", frame.mask_key[0], frame.mask_key[1], frame.mask_key[2], frame.mask_key[3]);
    write(1, mask_key, sizeof(mask_key));

    get_payload(frame, payload);

    write(1, "payload: ", 9);
    write(1, payload, sizeof(payload));
    write(1, "\n\n", 2);
}

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
                print_ws_frame(*frame);

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

                print_ws_frame(send_frame);

                if(send_frame.flags[1] & 0x80) {

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
