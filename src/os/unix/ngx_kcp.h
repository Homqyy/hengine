/*
 * Copyright (C) homqyy
 */

#ifndef _NGX_KCP_H_INCLUDED_
#define _NGX_KCP_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>

#include <ikcp.h>

#define NGX_KCP_NORMAL_MODE 0
#define NGX_KCP_QUICK_MODE  1

struct ngx_kcp_s
{
    ngx_log_t        *log;
    ikcpcb           *ikcp;
    ngx_uint_t        conv;
    ngx_uint_t        mode;
    ngx_rbtree_node_t timer;
    ngx_int_t         max_waiting_send_number;
    ngx_int_t         valve_of_send;

    ngx_send_pt       transport_send;
    ngx_send_chain_pt transport_send_chain;
    ngx_recv_pt       transport_recv;
    ngx_recv_chain_pt transport_recv_chain;

    void (*write_handler)(ngx_connection_t *c);
    void (*read_handler)(ngx_connection_t *c);

    unsigned close         : 1;
    unsigned error         : 1;
    unsigned waiting_read  : 1;
    unsigned waiting_write : 1;
    unsigned write_active  : 1;
};

ngx_kcp_t *ngx_create_kcp(ngx_connection_t *c, ngx_uint_t conv,
                          ngx_uint_t mode);
ngx_uint_t ngx_get_kcp_conv(u_char *buffer, size_t size);
#define ngx_kcp_get_conv(kcp) (kcp->conv)
#define ngx_kcp_get_mode(kcp) (kcp->mode)

#endif //!_NGX_KCP_H_INCLUDED_