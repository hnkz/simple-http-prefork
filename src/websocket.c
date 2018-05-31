#include "exp1.h"
#include "exp1lib.h"
#include "httplib.h"
#include "base64.h"
#include "websocket.h"
#include <openssl/sha.h>


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

int send_response_head(int ws_client, char *buf) {
    int ret;
    char ws_head[1024];
    char *key = create_secret_key(buf);
    
    memset(ws_head, 0, sizeof(ws_head));
    snprintf(ws_head, sizeof(ws_head), "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", key);
    ret = send(ws_client, ws_head, strlen(ws_head), 0);
    if(ret < 0) {
        perror("cannot send");
        return -1;
    }
    free(key);

    write(1, ws_head, sizeof(ws_head));

    return 0;
}

void create_text_frame(ws_frame *send_frame, char *data, int pay_len) {
    memset((void *)send_frame, 0, sizeof(ws_frame));
    send_frame->flags[0] = 0x81;
    send_frame->flags[1] = 0x00 | pay_len;
    memset(send_frame->mask_key, 0, 4);
    memcpy(send_frame->payload, data, strlen(data));
}