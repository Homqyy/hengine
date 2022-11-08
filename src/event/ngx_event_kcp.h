/*
 * Copyright (C) homqyy
 */

#ifndef _NGX_EVENT_KCP_H_INCLUDED_
#define _NGX_EVENT_KCP_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

void       ngx_event_kcp_handler(ngx_event_t *ev);
ngx_msec_t ngx_event_kcp_process_connections(ngx_cycle_t *cycle);
void       ngx_event_kcp_update_timer(ngx_log_t *log, ngx_kcp_t *kcp);
void       ngx_event_kcp_add_timer(ngx_log_t *log, ngx_kcp_t *kcp);

#endif //!_NGX_EVENT_KCP_H_INCLUDED_