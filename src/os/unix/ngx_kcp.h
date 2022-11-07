/*
 * Copyright (C) homqyy
 */

#ifndef _NGX_KCP_H_INCLUDED_
#define _NGX_KCP_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>

#include <ikcp.h>


struct ngx_kcp_s
{
    ikcpcb *ikcp;

    void (*write_handler)(ngx_connection_t *c);
    void (*read_handler)(ngx_connection_t *c);

    ngx_int_t max_waiting_send_number;
    ngx_int_t valve_of_send;

    unsigned close         : 1;
    unsigned error         : 1;
    unsigned waiting_read  : 1;
    unsigned waiting_write : 1;
};

ngx_kcp_t *ngx_create_kcp(ngx_connection_t *c);
ssize_t    ngx_kcp_send(ngx_connection_t *c, u_char *b, size_t size);
ssize_t    ngx_kcp_recv(ngx_connection_t *c, u_char *buf, size_t size);

#endif //!_NGX_KCP_H_INCLUDED_