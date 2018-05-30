#include <stdio.h>

typedef struct {
    char cmd[64];
    char value[512];
    char path[256];
    char real_path[256];
    char type[64];
    char hostname[256];
    char method[64];
    char param[512];
    int code;
    int size;
    int content_length;
    int cgi;
} http_info_type;

typedef struct {
    int exist_file;
    int exist_dir;
    int exist_tmp_file;
    int moved_permanently;
    int temporarily_move;
    int see_other;
} file_check_flags;

file_check_flags init_file_check_flag();

int http_session(int sock);
int parse_header(char *buf, int size, http_info_type *info);
void parse_status(char *status, http_info_type *pinfo);

void check_info(http_info_type *info);
void check_file(http_info_type *info);
void set_type(http_info_type *info);

void http_reply(int sock, http_info_type *info);

void send_200(int sock, http_info_type *info);
void send_301(int sock, http_info_type *info);
void send_302(int sock, http_info_type *info);
void send_303(int sock, http_info_type *info);
void send_404(int sock);

void send_file(int sock, char *filename);
void send_php(int sock, http_info_type *info);
FILE* get_html_from_php_get(http_info_type *info);
FILE* get_html_from_php_post(http_info_type *info);

void print_info(http_info_type *info);
void print_flags(file_check_flags flags);