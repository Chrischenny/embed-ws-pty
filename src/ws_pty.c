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

struct io_buf{
    uint8_t *buf;       // buf addr
    size_t buf_len;     // written len
    size_t buf_size;    // total size
};

struct pty_poll_data{
    int pty;
    char username[MAX_USERNAME_LENGTH];
    struct io_buf pty_out_buf;
};

int pts_proc_flags;


/**
 * @brief 
 * 
 * @param ws_msg 
 * @param username 
 * @return size_t 
 */
size_t pty_authenticate(struct mg_ws_message *ws_msg, char *username)
{
    return 1;
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
size_t pty_io_delete(struct io_buf pty_buf, size_t len)
{

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
                mg_ws_upgrade(conn, hm, "login:");
                int pty = -1;
                pid_t pid = forkpty(&pty, NULL, NULL, NULL);
                if (pid < 0 || pty < 0)
                {
                    mg_http_reply(conn, 500, NULL, "Server Internal Error!");
                }else if (pid = 0) // slave pty process
                {
                    close(pty);
                }else{
                     //upgrade protocol to websocket

                }    
            }
        }
    case MG_EV_WS_MSG:
        {
            struct pty_poll_data *pty_data = (struct pty_poll_data *)fn_data;
            int pty = pty_data->pty;
            struct mg_ws_message *ws_msg = (struct mg_ws_message *)ev_data;
            if (pty < 0) //means that not finish authenticate
            {
                pty_authenticate(ws_msg, pty_data->username);
            }else{
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
                    read_pty(pty, pty_data->pty_out_buf.buf);

                    //pty_out_buf to mongoose_out_buf, generally, it will append all data once a time;
                    size_t written_len = mg_ws_send(conn, pty_data->pty_out_buf.buf, pty_data->pty_out_buf.buf_len, WEBSOCKET_OP_TEXT);
                    pty_io_delete(pty_data->pty_out_buf, written_len);
                }
            }
        }
    default:
        break;
    }
}