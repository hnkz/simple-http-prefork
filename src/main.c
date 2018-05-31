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

typedef struct {
    unsigned fin:1;
    unsigned rsv1:1;
    unsigned rsv2:1;
    unsigned rsv3:1;
    unsigned opcode:4;
    // unsigned mask:1;
    unsigned payload_len:7;
    char mask_key[4];
    char payload[2048];
} ws_frame;

typedef struct {
    unsigned fin:1;
    unsigned rsv2:1;
    unsigned rsv3:1;
    unsigned rsv1:1;
    unsigned opcode:4;
    unsigned mask:1;
    unsigned payload_len:7;
    char mask_key[4];
    char payload[2048];
} send_ws_frame;


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

int get_payload(ws_frame frame, char *payload) {
    int i;
    for(i = 0; i < frame.payload_len; i++) {
        payload[i] = frame.payload[i] ^ frame.mask_key[i % 4];
    }

    return frame.payload_len;
}

void print_ws_frame(ws_frame frame) {
    // char fin[2];
    // char rsv1[2];
    // char rsv2[2];
    // char rsv3[2];
    char opcode[32];
    // char mask[2];
    char payload_len[32];
    char mask_key[64];
    char payload[2048];

    memset(opcode, 0, sizeof(opcode));
    memset(payload_len, 0, sizeof(payload_len));
    memset(mask_key, 0, sizeof(mask_key));
    memset(payload, 0, sizeof(payload));

    write(1, "fin: ", 5);
    if(frame.fin)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    write(1, "rsv1: ", 6);
    if(frame.rsv1)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    write(1, "rsv2: ", 6);
    if(frame.rsv2) 
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    write(1, "rsv3: ", 6);
    if(frame.rsv3)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    sprintf(opcode, "opcode: %u\n", frame.opcode);
    write(1, opcode, sizeof(opcode));

    // write(1, "mask: ", 6);
    // if(frame.mask)
    //     write(1, "1\n", 2);
    // else
    //     write(1, "0\n", 2);

    sprintf(payload_len, "payload_len: %d\n", frame.payload_len);
    write(1, payload_len, sizeof(payload_len));

    sprintf(mask_key, "mask_key: %u\n", frame.mask_key);
    write(1, mask_key, sizeof(mask_key));

    get_payload(frame, payload);

    write(1, "payload: ", 9);
    write(1, payload, sizeof(payload));
    write(1, "\n\n", 2);
}

void print_send_ws_frame(send_ws_frame frame) {
    // char fin[2];
    // char rsv1[2];
    // char rsv2[2];
    // char rsv3[2];
    char opcode[32];
    // char mask[2];
    char payload_len[32];
    char mask_key[64];
    char payload[2048];

    memset(opcode, 0, sizeof(opcode));
    memset(payload_len, 0, sizeof(payload_len));
    memset(mask_key, 0, sizeof(mask_key));
    memset(payload, 0, sizeof(payload));

    write(1, "fin: ", 5);
    if(frame.fin)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    write(1, "rsv1: ", 6);
    if(frame.rsv1)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    write(1, "rsv2: ", 6);
    if(frame.rsv2) 
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    write(1, "rsv3: ", 6);
    if(frame.rsv3)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    sprintf(opcode, "opcode: %u\n", frame.opcode);
    write(1, opcode, sizeof(opcode));

    write(1, "mask: ", 6);
    if(frame.mask)
        write(1, "1\n", 2);
    else
        write(1, "0\n", 2);

    sprintf(payload_len, "payload_len: %d\n", frame.payload_len);
    write(1, payload_len, sizeof(payload_len));

    sprintf(mask_key, "mask_key: %u\n", frame.mask_key);
    write(1, mask_key, sizeof(mask_key));

    // if(frame.mask)
        // get_payload(frame, payload);
    // else
    memcpy(payload, frame.payload, sizeof(payload));

    write(1, "payload: ", 9);
    write(1, payload, sizeof(payload));
    write(1, "\n\n", 2);
}

void h_to_n_for_send_ws_frame(send_ws_frame *frame) {
    u_int16_t tmp1[1027];
    u_int16_t tmp2[1027];

    memset(tmp1, 0, sizeof(tmp1));
    memset(tmp2, 0, sizeof(tmp2));
    memcpy(tmp1, (char *)frame, sizeof(frame));

    int i;
    for(i = 0; i < 1027 / 2; i++) {
        tmp2[i] = htons(tmp1[i]);
    }

    memcpy(frame, tmp2, sizeof(tmp2));
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
        char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        char key_magic[] = "Sec-WebSocket-Key: ";

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
            char *send_key = base64_encode(hash, sizeof(hash), &send_key_len);

            // write(1, buf, sizeof(buf));

            memset(ws_head, 0, sizeof(ws_head));
            snprintf(ws_head, sizeof(ws_head), "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", send_key);
            ret = send(ws_client, ws_head, strlen(ws_head), 0);
            if(ret < 0) {
                write(1, "cannnot send\n", 13);
                close(ws_listen);
                close(ws_client);
                break;
            }
            free(send_key);
            // write(1, ws_head, sizeof(ws_head));

            while(1) {
                memset(buf, 0, sizeof(buf));
                ret = recv(ws_client, buf, sizeof(buf), 0);
                if(ret < 0) {
                    write(1, "cannnot recv\n", 13);
                    close(ws_listen);
                    close(ws_client);
                    break;
                }
                // write(1, buf, sizeof(buf));

                ws_frame *frame;
                frame = (ws_frame *)buf;
                print_ws_frame(*frame);

                char data[2048];
                int pay_len;
                memset(data, 0, sizeof(data));
                pay_len = get_payload(*frame, data);

                send_ws_frame send_frame;
                memset((void *)&send_frame, 0, sizeof(send_ws_frame));
                send_frame.fin = 1;
                send_frame.rsv1 = 0;
                send_frame.rsv2 = 0;
                send_frame.rsv3 = 0;
                send_frame.opcode = 8;
                send_frame.mask = 0;
                memset(send_frame.mask_key, 0, 4);
                send_frame.payload_len = pay_len;
                memcpy(send_frame.payload, data, sizeof(data));

                print_send_ws_frame(send_frame);

                // ret = send(ws_client, (void *)&send_frame, 57 + pay_len , 0);
                ret = send(ws_client, (void *)&send_frame, sizeof(send_ws_frame) , 0);
                if(ret < 0) {
                    write(1, "cannnot send\n", 13);
                    close(ws_listen);
                    close(ws_client);
                    break;
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
