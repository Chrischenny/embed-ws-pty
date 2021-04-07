/**
 * @file ws_pty.c
 * @author your name (you@domain.com)
 * @brief based on mongoose-websocket to login bash on the embed system
 * @version 0.1
 * @date 2021-04-03
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "ws_pty.h"

#include <pty.h>

#define PTY_IO_SIZE        1024

#define PTY_ERROR_COMMON_SUCCEED   0
#define PTY_ERROR_COMMON_FAIL      -1

struct io_buf{
    uint8_t *buf;       // buf addr
    size_t buf_len;     // written len
    size_t buf_size;    // total size
};

struct pty_poll_data{
    int pty;
    struct io_buf pty_out_buf;
};

enum{
    PTY_AUTU_NAME_TOO_LONG,
    PTY_AUTH_CONTINUE,
    PTY_AUTH_FINISH,

    PTY_AUTH_BUTT,
};

int8_t child_process_flags = false;
pid_t global_pid;
char child_username[MAX_USERNAME_LENGTH] = {0}; //save every pty's loged user


/**
 * @brief 
 * 
 * @param ws_msg 
 * @param username 
 * @return size_t 
 */
int pty_authenticate(struct mg_ws_message *ws_msg, char *username)
{
    size_t curr_name_len = strlen(username);
    char *name_tail = username + curr_name_len;
    char *tmp = ws_msg->data.ptr;
    size_t msg_len = ws_msg->data.len;
    if (curr_name_len + msg_len > MAX_USERNAME_LENGTH -1)
    {
        return PTY_AUTU_NAME_TOO_LONG;
    }
    int i;
    for (i = 0; i < msg_len && curr_name_len < MAX_USERNAME_LENGTH; i++)
    {
        if (*tmp != '\n')// means we not yet finish typing the username
        {
            *name_tail = *tmp;
        }
        else
        {
            return PTY_AUTH_FINISH;
        }
        name_tail++;
        curr_name_len++;
        tmp++;
    }

    return PTY_AUTH_CONTINUE;
}


/**
 * @brief 
 * 
 * @param pty 
 * @param ws_msg 
 * @return int8_t 
 */
int8_t flush_pty(int pty, struct mg_ws_message *ws_msg)
{
    size_t len = 0;

    while (len < ws_msg->data.len)
    {
        int w_len = write(pty, ws_msg->data.ptr, ws_msg->data.len);
        if (w_len > 0)
        {
            len += w_len;
        }
        if (len > ws_msg->data.len)
        {
            return -1;
        }
    }

    return 0;
}


/**
 * @brief 
 * 
 * @param pty_buf 
 * @param len 
 * @return size_t 
 */
size_t pty_io_delete(struct io_buf *buf, size_t len)
{
    memmove(buf->buf, buf->buf+len, buf->buf_len - len);
    buf->buf_len -= len;
    return len;
}


/**
 * @brief 
 * 
 * @param buf 
 * @param new_size 
 * @return size_t 
 */
size_t pty_io_resize(struct io_buf *buf, size_t new_size)
{
    realloc(buf->buf, new_size);
    if (buf->buf == NULL)
    {
        return 0;
    }

    buf->buf_size = new_size;
    return new_size;
}


/**
 * @brief 
 * 
 * @param pty 
 * @param buf 
 * @return size_t 
 */
size_t read_pty(int pty, struct io_buf *buf)
{
    if (buf->buf_size - buf->buf_len < PTY_IO_SIZE)
    {
        if (pty_io_resize(buf, buf->buf_len+PTY_IO_SIZE) == 0)
        {
            return 0;
        }
    }

    size_t read_len = read(pty, buf->buf, buf->buf_size - buf->buf_len);
    if (read_len <= 0)
    {
        return 0;
    }
    buf->buf_len += read_len;
    return read_len;
}


/**
 * @brief 
 * 
 * @param conn 
 */
void free_pty_data(struct mg_connection *conn)
{
    struct mg_mgr *mgr = conn->mgr;
    struct mg_connection *tmp_conn;
    for (tmp_conn = mgr->conns; tmp_conn != NULL; tmp_conn = tmp_conn->next)
    {
        if (tmp_conn->fn_data != NULL)
        {
            struct pty_poll_data *fn_data = (struct pty_poll_data *)tmp_conn->fn_data;
            free(fn_data->pty_out_buf.buf);
            free(fn_data);
            tmp_conn->fn_data = NULL;
        }
    }
}


/**
 * @brief 
 * 
 * @param conn 
 * @param ev 
 * @param ev_data 
 * @param fn_data 
 */
static void mongoose_ws_callback(struct mg_connection *conn, int ev, void *ev_data, void* fn_data)
{
    switch (ev)
    {
    case MG_EV_HTTP_MSG:
        {
            struct mg_http_message *hm = (struct mg_http_message *)ev_data;
            struct mg_str *upgrade = mg_http_get_header(hm, "Upgrade");
            if (strncmp(upgrade->ptr, "websocket", upgrade->len) == 0)
            {
                struct pty_poll_data *pty_data = malloc(sizeof(struct pty_poll_data));
                if (pty_data == NULL)
                {
                    mg_http_reply(conn, 500, NULL, "Sevrer Internal Error");
                }
                memset((void *)pty_data, 0, sizeof(*pty_data));
                pty_data->pty = -1;
                conn->fn_data = (void *)pty_data;
                mg_ws_upgrade(conn, hm, NULL);
                global_pid = forkpty(&pty_data->pty, NULL, NULL, NULL);
                if (global_pid == 0)// child process
                {
                    /*
                        * warning :every incoming ws connection will fork a child process
                        * and copy the variable and the memroy from the father process.
                        * because of this, we need to free data and memory child process
                        * doesn't need. but it is not a good solution, the more incoming
                        * connetions are,the more memory consumption will waste while copy to
                        * child process, cause mongoose will keep every acitve connetion.
                        * so, the less connections are,the better to the embed system; 
                        */

                    /* judge every poll terms if,child stop the mongoose poll
                        * we should exec the /bin/login after the mongoose stop
                        */
                    child_process_flags = true; 
                    //copy the login user to the child global variable
                    close(pty_data->pty);
                    free_pty_data(conn);
                }
                else if(global_pid < 0)
                {
                    mg_ws_send(conn, "Internal Error", sizeof("Internal Error"), WEBSOCKET_OP_CLOSE);
                }
            }
        }
    case MG_EV_WS_MSG:
        {
            struct pty_poll_data *pty_data = (struct pty_poll_data *)fn_data;
            int pty = pty_data->pty;
            struct mg_ws_message *ws_msg = (struct mg_ws_message *)ev_data;
            if (!flush_pty(pty, ws_msg))
            {
                mg_ws_send(conn, "oops, something wrong with server,please reconnect"
                            , sizeof("oops, something wrong with server,please reconnect"), WEBSOCKET_OP_TEXT);
                conn->is_draining = 1;
                //fixed : how to stop pts process？
                close(pty);
            }
            break; 
        }
    case MG_EV_POLL:
        {
            struct pty_poll_data *pty_data = (struct pty_poll_data *)fn_data;
            int pty = pty_data->pty;
            if (pty > 0) //
            {
                fd_set r_set;
                FD_ZERO(&r_set);
                FD_SET(pty, &r_set);

                if(select(pty + 1, &r_set, NULL, NULL, NULL) < 0)
                {
                    break;
                }

                if (FD_ISSET(pty, &r_set))
                {
                    if (0 == read_pty(pty, &pty_data->pty_out_buf))
                    {
                        //fixed: should we use a reread count?
                        break; //no message read,try to read next poll.
                    }

                    //pty_out_buf to mongoose_out_buf, generally, it will append all data once a time;
                    size_t written_len = mg_ws_send(conn, pty_data->pty_out_buf.buf, pty_data->pty_out_buf.buf_len, WEBSOCKET_OP_TEXT);
                    pty_io_delete(&pty_data->pty_out_buf, written_len);
                }
            }
        }
    default:
        break;
    }
}


/**
 * @brief 
 * 
 */
void start_login()
{
    execl("/bin/login", "login", "-p", NULL);
}


/**
 * @brief 
 * 
 * @return int 
 */
int pty_ws_run()
{
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    char host_name[128] = {0};
    gethostname(host_name, 128);
    struct hostent *host = gethostbyname(host_name);
    char *host_addr = inet_ntoa(*(struct in_addr*)host->h_addr_list[0]);
    if (mg_http_listen(&mgr, host_addr, mongoose_ws_callback, NULL) == NULL)
    {
        return PTY_ERROR_COMMON_FAIL;
    }

    for(;;)
    {
        if (!child_process_flags)
        {
            mg_mgr_poll(&mgr, 1000);
        }
        else
        {
            break;
        }
    }

    mg_mgr_free(&mgr);
    if (global_pid == 0) //start exec /bin/login
    {   
        start_login();
    }
    
}