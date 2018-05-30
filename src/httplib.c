#include "exp1.h"
#include "exp1lib.h"
#include "httplib.h"

const char *CMD[64] = {
    "GET",
    "POST",
    "Host:",
    "Connection:",
    "Content-Length:"
};

enum cmd_status {
    GET,
    POST,
    Host,
    Connection,
    ContentLength
};


extern int temporarily_move;
extern int see_other;

int http_session(int sock) {
    char buf[2048];
    int recv_size = 0;
    http_info_type info;
    info.code = 0;
    int ret = 0;

    while (ret == 0) {
        memset(buf, 0, sizeof(buf));
        int size = recv(sock, buf + recv_size, 2048, 0);

        if (size == -1) {
            shutdown(sock, SHUT_RDWR);
            close(sock);
            write(1, "cannnot recv\n", 13);
            return -1;
        }

        recv_size += size;
        // printf("%s\n", buf);
        ret = parse_header(buf, recv_size, &info);
    }

    http_reply(sock, &info);

    return 0;
}

int parse_header(char *buf, int size, http_info_type *info) {
    char status[1024];
    int i, j;

    enum state_type {
        PARSE_START,
        PARSE_STATUS,
        PARSE_END
    } state;

    state = PARSE_START;
    j = 0;
    for (i = 0; i < size; i++) {
        switch (state) {
            case PARSE_START:
                if(buf[i] == '\r' && buf[i+1] == '\n') {

                    // POST
                    if(strcmp(info->method, "POST") == 0) {
                        i += 2;
                        int k;
                        for(k = 0; k < info->content_length; k++) {
                            if(buf[i] == '&') {
                                info->param[k] = ' ';
                            } else {
                                info->param[k] = buf[i];
                            }
                            i += 1;
                        }
                        info->param[k] = '\0';
                    }

                    state = PARSE_END;
                    break;
                }
                state = PARSE_STATUS;
                i -= 1;
                break;
            case PARSE_STATUS:
                if (buf[i] == '\r' && buf[i+1] == '\n') {

                    status[j] = '\0';
                    i += 1;
                    j = 0;
                    state = PARSE_START;
                    // print_info(info);
                    parse_status(status, info);
                    check_info(info);
                } else {
                    status[j] = buf[i];
                    j++;
                }
                break;
            case PARSE_END:
            default:
                return 1;
                break;
        }
    }

    return 1;
}

void parse_status(char *status, http_info_type *pinfo) {
    char cmd[1024];
    char value[1024];
    int i, j;

    enum state_type {
        SEARCH_CMD,
        SEARCH_PATH,
        SEARCH_END
    } state;

    state = SEARCH_CMD;
    j = 0;
    for (i = 0; i < strlen(status); i++) {
        switch (state) {
            case SEARCH_CMD:
                if (status[i] == ' ') {
                    cmd[j] = '\0';
                    j = 0;
                    state = SEARCH_PATH;
                } else {
                    cmd[j] = status[i];
                    j++;
                }
                break;
            case SEARCH_PATH:
                if (status[i] == ' ') {
                    value[j] = '\0';
                    j = 0;
                    state = SEARCH_END;
                } else {
                    value[j] = status[i];
                    j++;
                }
                break;
            case SEARCH_END:
            default:
                break;
        }
    }

    strcpy(pinfo->cmd, cmd);
    strcpy(pinfo->value, value);
}

void check_info(http_info_type *info) {
    if(strcmp(info->cmd, CMD[GET]) == 0) {
        strncpy(info->path, info->value, sizeof(info->path));
        strncpy(info->method, "GET", sizeof(info->method));
        check_file(info);
        set_type(info);
    } else if(strcmp(info->cmd, CMD[POST]) == 0) {
        strcpy(info->path, info->value);
        strcpy(info->method, "POST");
        check_file(info);
        set_type(info);
    } else if(strcmp(info->cmd, CMD[Host]) == 0) {
        strncpy(info->hostname, info->value, sizeof(info->hostname));
    
    } else if(strcmp(info->cmd, CMD[ContentLength]) == 0) {
        info->content_length = atoi(info->value);
    } else {

    }
}

void check_file(http_info_type *info) {
    file_check_flags flags;
    char real_path[1024];
    struct stat s;
    int ret;

    flags = init_file_check_flag();

    // exist file & is dir ?
    sprintf(real_path, "html%s", info->path);
    ret = stat(real_path, &s);
    if(S_ISDIR(s.st_mode) && ret == 0) {
        flags.exist_dir = 1;
        if(real_path[strlen(real_path)-1] != '/') {
            flags.moved_permanently = 1;
            sprintf(real_path, "%s/index.html", real_path);
        } else {
            sprintf(real_path, "%sindex.html", real_path);
        }

        ret = stat(real_path, &s);
        if(ret == -1) {
            flags.exist_file = 0;
        } else {
            flags.exist_file = 1;
        }
    } else if(ret == 0) {
        flags.exist_file = 1;
    }

    // exist tmp file
    sprintf(real_path, "html/tmp%s", info->path);
    ret = stat(real_path, &s);
    if(ret == 0)
        flags.exist_tmp_file = 1;

    // 200
    if(
        flags.exist_file && !flags.moved_permanently
    ) {
        info->code = 200;
        if(flags.exist_dir) {
            sprintf(info->real_path, "html%sindex.html", info->path);
            sprintf(info->path, "%sindex.html", info->path);
        } else {
            sprintf(info->real_path, "html%s", info->path);
        }
        ret = stat(info->real_path, &s);
        info->size = (int)s.st_size;
    }
    // 301
    else if(
        flags.exist_file && flags.exist_dir &&
        !flags.temporarily_move && !flags.exist_tmp_file &&
        flags.moved_permanently && !flags.see_other
    ) {
        info->code = 301;
        sprintf(info->real_path, "html%s/index.html", info->path);
    }
    // 302
    else if(
        !flags.exist_file && !flags.exist_dir &&
        flags.temporarily_move && flags.exist_tmp_file &&
        !flags.moved_permanently && !flags.see_other
    ) {
        info->code = 302;
        sprintf(info->real_path, "/tmp%s", info->path);
    } 
    // 303
    else if(
         flags.see_other
    ) {
        info->code = 303;
        sprintf(info->real_path, "/tmp%s", info->path);
    } 
    // 404
    else if(
        !flags.exist_file && !flags.exist_dir &&
        !flags.temporarily_move && !flags.exist_tmp_file &&
        !flags.moved_permanently && !flags.see_other
    ) {
        info->code = 404;
    } 

    // print_flags(flags);
}

void set_type(http_info_type *info) {
    char *pext;

    pext = strstr(info->path, ".");
    if (pext != NULL && strcmp(pext, ".html") == 0) {
        strcpy(info->type, "text/html");
    } else if (pext != NULL && strcmp(pext, ".php") == 0) {
        strcpy(info->type, "text/html");
        info->cgi = 1;
    } else if (pext != NULL && strcmp(pext, ".jpg") == 0) {
        strcpy(info->type, "image/jpeg");
    }
}

void http_reply(int sock, http_info_type *info) {
    print_info(info);
    switch(info->code) {
        case 200:
            send_200(sock, info);
            // printf("200 OK\n");
            break;
        case 301:
            send_301(sock, info);
            // printf("301 Moved Permanently\n");
            break;
        case 302:
            send_302(sock, info);
            // printf("302 Found\n");
            break;
        case 303:
            send_303(sock, info);
            // printf("303 See Other\n");
            break;
        case 404:
            send_404(sock);
            // printf("404 not found %s\n", info->path);
            return;
            break;
        default:
            break;
    }
}

void send_200(int sock, http_info_type *info) {
    FILE *fp;
    char buf[16384];
    int len;
    int ret;

    memset(buf, 0, sizeof(buf));
    if(info->cgi) {
        fp = get_php(info);
        ret = fread(buf, sizeof(char), 16384, fp);
        while(ret > 0) {
            len += ret; 
            ret = fread(buf, sizeof(char), 16384, fp);
        }
        info->size = len;
        pclose(fp);
    }

    memset(buf, 0, sizeof(buf));
    len = sprintf(buf, "HTTP/1.0 200 OK\r\n");
    len += sprintf(buf + len, "Content-Length: %d\r\n", info->size);
    len += sprintf(buf + len, "Content-Type: %s\r\n", info->type);
    len += sprintf(buf + len, "\r\n");

    ret = send(sock, buf, len, 0);
    if (ret < 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        return;
    }

    if(info->cgi) {
        send_php(sock, info);
    } else {
        send_file(sock, info->real_path);
    }
}

void send_301(int sock, http_info_type *info) {
    char buf[16384];
    int len;
    int ret;

    len = sprintf(buf, "HTTP/1.0 301 Moved Permanently\r\n");
    len += sprintf(buf + len, "Location: %s/index.html\r\n", info->path);
    len += sprintf(buf + len, "\r\n");

    ret = send(sock, buf, len, 0);
    if (ret < 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        return;
    }
}

void send_302(int sock, http_info_type *info) {
    char buf[16384];
    int len;
    int ret;

    len = sprintf(buf, "HTTP/1.0 302 Found\r\n");
    len += sprintf(buf + len, "Location: tmp%s\r\n", info->path);
    len += sprintf(buf + len, "\r\n");

    ret = send(sock, buf, len, 0);
    if (ret < 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        return;
    }
}

void send_303(int sock, http_info_type *info) {
    char buf[16384];
    int len;
    int ret;

    len = sprintf(buf, "HTTP/1.0 303 See Other\r\n");
    len += sprintf(buf + len, "Location: tmp%s\r\n", info->path);
    len += sprintf(buf + len, "\r\n");

    ret = send(sock, buf, len, 0);
    if (ret < 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        return;
    }
}

void send_404(int sock) {
    char buf[16384];
    int ret;

    sprintf(buf, "HTTP/1.0 404 Not Found\r\n\r\n");
    printf("%s", buf);
    ret = send(sock, buf, strlen(buf), 0);

    if (ret < 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
}

void send_file(int sock, char *filename) {
    FILE *fp;
    int len;
    char buf[16384];

    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("cannnot open file");
        shutdown(sock, SHUT_RDWR);
        close(sock);
        return;
    }

    len = fread(buf, sizeof(char), 16384, fp);
    while (len > 0) {
        int ret = send(sock, buf, len, 0);
        if (ret < 0) {
            shutdown(sock, SHUT_RDWR);
            close(sock);
            break;
        }
        len = fread(buf, sizeof(char), 1460, fp);
    }

    fclose(fp);
}

void send_php(int sock, http_info_type *info) {
    char buf[16384];
    int len;
    FILE *fp;

    memset(buf, 0, sizeof(buf));
    fp = get_php(info);
    len = fread(buf, sizeof(char), 16384, fp);
    while (len > 0) {
        printf("%s", buf);
        int ret = send(sock, buf, len, 0);
        if (ret < 0) {
            perror("cannnot send");
            shutdown(sock, SHUT_RDWR);
            close(sock);
            break;
        }
        len = fread(buf, sizeof(char), 1460, fp);
    }

    pclose(fp);
}

FILE* get_php(http_info_type *info) {
    FILE *fp;
    char path[256];
    char command[1024];

    getcwd(path, sizeof(path));
    // snprintf(command, sizeof(command)-1,
    //      "REDIRECT_STATUS=true;REQUEST_METHOD=POST;CONTENT_LENGTH=%d;SCRIPT_FILENAME=%s;export REDIRECT_STATUS;export REQUEST_METHOD;export CONTENT_LENGTH;export SCRIPT_FILENAME;php-cgi -q %s", info->content_length, info->real_path, info->param);

    snprintf(command, sizeof(command)-1, "php-cgi -q %s %s", info->real_path, info->param);


    fp = popen(command, "r");
    if(fp == NULL) {
        perror("cannot popen");
        return NULL;
    }
    return fp;
}

void print_info(http_info_type *info) {
    printf("------------------------------------------------------\n");
    printf("\tcmd: %s\n", info->cmd);
    printf("\tvalue: %s\n", info->value);
    printf("\tpath: %s\n", info->path);
    printf("\treal_path: %s\n", info->real_path);
    printf("\ttype: %s\n", info->type);
    printf("\thostname: %s\n", info->hostname);
    printf("\tmethod: %s\n", info->method);
    printf("\tparam: %s\n", info->param);
    printf("\tcode: %d\n", info->code);
    printf("\tsize: %d\n", info->size);
    printf("\tcontent-length: %d\n", info->content_length);
    printf("\tcgi: %d\n", info->cgi);
    printf("------------------------------------------------------\n\n");
}

void print_flags(file_check_flags flags) {
    if(flags.exist_file)
        printf("exist_file | ");
    if(flags.exist_dir)
        printf("exist_dir | ");
    if(flags.temporarily_move)
        printf("temporarily_move | ");
    if(flags.exist_tmp_file)
        printf("exist_tmp_file | ");
    if(flags.moved_permanently)
        printf("moved_permanently | ");
    
    printf("\n");
}

file_check_flags init_file_check_flag() {
    file_check_flags flags;
    flags.exist_file = 0;
    flags.exist_dir = 0;
    flags.exist_tmp_file = 0;
    flags.temporarily_move = temporarily_move;
    flags.moved_permanently = 0;
    flags.see_other = see_other;

    return flags;
}