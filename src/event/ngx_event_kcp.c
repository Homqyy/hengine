/*
 * Copyright (C) homqyy
 */

#include <ngx_config.h>
#include <ngx_core.h>

#include <ngx_event.h>

ngx_msec_t
ngx_event_kcp_process_connections(ngx_cycle_t *cycle)
{
    // foeach kcps

    return 0;
}

void
ngx_event_kcp_handler(ngx_event_t *ev)
{
    ngx_connection_t *c   = ev->data;
    ngx_kcp_t        *kcp = c->kcp;

    if (ev->timedout || c->error || c->close)
    {
        ev->handler(ev);
        return;
    }

    if (ev->write)
    {
        kcp->write_handler(c);
        return;
    }

    // for read
    kcp->read_handler(c);
}