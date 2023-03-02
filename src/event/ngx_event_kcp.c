/*
 * Copyright (C) homqyy
 */

#include <ngx_config.h>
#include <ngx_core.h>

#include <ngx_event.h>

ngx_msec_t
ngx_event_kcp_process_connections(ngx_cycle_t *cycle)
{
    ngx_kcp_t         *kcp;
    ngx_rbtree_node_t *node, *root, *sentinel;

    sentinel = &cycle->kcp_sentinel;

    for (;;)
    {
        root = cycle->kcp_rbtree.root;

        if (root == sentinel)
        {
            return NGX_TIMER_INFINITE;
        }

        node = ngx_rbtree_min(root, sentinel);

        /* node->key > ngx_current_msec */

        if ((ngx_msec_int_t)(node->key - ngx_current_msec) > 0)
        {
            break;
        }

        kcp = (ngx_kcp_t *)((char *)node - offsetof(ngx_kcp_t, timer));

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0, "kcp %ud update",
                       kcp->conv);

        ikcp_update(kcp->ikcp, ngx_current_msec);

        ngx_event_kcp_update_timer(cycle->log, kcp);

        if (kcp->timer.key == ngx_current_msec)
        {
            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0, "kcp %ud flush",
                           kcp->conv);
            ikcp_flush(kcp->ikcp); // immediately flush
            ngx_event_kcp_update_timer(cycle->log, kcp);
        }
    }

    root = cycle->kcp_rbtree.root;

    node = ngx_rbtree_min(root, sentinel);

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                   "kcp next timer: %M, current time: %M", node->key,
                   ngx_current_msec);

    return node->key - ngx_current_msec;
}

void
ngx_event_kcp_update_timer(ngx_log_t *log, ngx_kcp_t *kcp)
{
    ngx_msec_t timer = ikcp_check(kcp->ikcp, ngx_current_msec);

    if (kcp->timer.key == timer)
    {
        return;
    }

    ngx_rbtree_delete((ngx_rbtree_t *)&ngx_cycle->kcp_rbtree, &kcp->timer);

    kcp->timer.key = timer;

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, log, 0,
                   "kcp timer update: %ud:%M, current time: %M", kcp->conv,
                   kcp->timer.key, ngx_current_msec);

    ngx_rbtree_insert((ngx_rbtree_t *)&ngx_cycle->kcp_rbtree, &kcp->timer);
}

void
ngx_event_kcp_add_timer(ngx_log_t *log, ngx_kcp_t *kcp)
{
    kcp->timer.key = ngx_current_msec;
    ngx_rbtree_insert((ngx_rbtree_t *)&ngx_cycle->kcp_rbtree, &kcp->timer);

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, log, 0, "kcp timer add: %ud:%M",
                   kcp->conv, kcp->timer.key);
}

void
ngx_event_kcp_del_timer(ngx_log_t *log, ngx_kcp_t *kcp)
{
    ngx_rbtree_delete((ngx_rbtree_t *)&ngx_cycle->kcp_rbtree, &kcp->timer);

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, log, 0, "kcp timer del: %ud:%M",
                   kcp->conv, kcp->timer.key);
}

void
ngx_event_kcp_handler(ngx_event_t *ev)
{
    ngx_connection_t *c   = ev->data;
    ngx_kcp_t        *kcp = c->kcp;

    ngx_log_debug6(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "kcp handler: %ud, close: %d, error: %d, waiting_read: %d, "
                   "waiting_write: %d, fd: %d",
                   kcp->conv, kcp->close, kcp->error, kcp->waiting_read,
                   kcp->waiting_write, c->fd);

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