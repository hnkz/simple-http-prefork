typedef struct {
    unsigned char flags[2];
    unsigned char mask_key[4];
    unsigned char payload[2048];
} ws_frame;

int get_payload(ws_frame frame, char *payload);
char *create_secret_key(char *buf);
void print_ws_frame(ws_frame frame);
int send_response_head(int ws_client, char *buf);
void create_text_frame(ws_frame *send_frame, char *data, int pay_len);