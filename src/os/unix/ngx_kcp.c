/*
 * Copyright (C) homqyy
 */

#include <ngx_config.h>
#include <ngx_core.h>

#include <ngx_event.h>

#define NGX_KCP_INTERVAL_DEFAULT 5000

static void ngx_destroy_kcp(ngx_kcp_t *kcp);
static int  ngx_kcp_output_handler(const char *buf, int len, ikcpcb *ikcp,
                                   void *user);
static void ngx_kcp_log(const char *log, struct IKCPCB *kcp, void *user);
static void ngx_kcp_write_handler(ngx_connection_t *c);
static void ngx_kcp_read_handler(ngx_connection_t *c);

static ssize_t      ngx_kcp_send(ngx_connection_t *c, u_char *b, size_t size);
static ngx_chain_t *ngx_kcp_send_chain(ngx_connection_t *c, ngx_chain_t *in,
                                       off_t limit);
static ssize_t      ngx_kcp_recv(ngx_connection_t *c, u_char *buf, size_t size);
static ssize_t      ngx_kcp_recv_chain(ngx_connection_t *c, ngx_chain_t *in,
                                       off_t limit);


static ngx_chain_t *
ngx_kcp_send_chain(ngx_connection_t *c, ngx_chain_t *in, off_t limit)
{
    ngx_kcp_t   *kcp = c->kcp;
    ngx_event_t *wev;

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp send chain %d",
                   kcp->number);

    wev = c->write;

    if (!wev->ready)
    {
        return in;
    }

#if (NGX_HAVE_KQUEUE)

    if ((ngx_event_flags & NGX_USE_KQUEUE_EVENT) && wev->pending_eof)
    {
        (void)ngx_connection_error(
            c, wev->kq_errno, "kevent() reported about an closed connection");
        wev->error = 1;
        return NGX_CHAIN_ERROR;
    }

#endif

    /* the maximum limit size is the maximum size_t value - the page size */

    if (limit == 0 || limit > (off_t)(NGX_MAX_SIZE_T_VALUE - ngx_pagesize))
    {
        limit = NGX_MAX_SIZE_T_VALUE - ngx_pagesize;
    }

    ssize_t n, size;
    off_t   total;

    for (total = 0; in && total < limit; in = in->next)
    {
        if (ngx_buf_special(in->buf))
        {
            continue;
        }

        if (in->buf->in_file)
        {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "file buf in writev "
                          "t:%d r:%d f:%d %p %p-%p %p %O-%O",
                          in->buf->temporary, in->buf->recycled,
                          in->buf->in_file, in->buf->start, in->buf->pos,
                          in->buf->last, in->buf->file, in->buf->file_pos,
                          in->buf->file_last);

            ngx_debug_point();

            return NGX_CHAIN_ERROR;
        }

        if (!ngx_buf_in_memory(in->buf))
        {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "bad buf in output chain "
                          "t:%d r:%d f:%d %p %p-%p %p %O-%O",
                          in->buf->temporary, in->buf->recycled,
                          in->buf->in_file, in->buf->start, in->buf->pos,
                          in->buf->last, in->buf->file, in->buf->file_pos,
                          in->buf->file_last);

            ngx_debug_point();

            return NGX_CHAIN_ERROR;
        }

        size = in->buf->last - in->buf->pos;

        if (size > limit - total)
        {
            size = limit - total;
        }

        n = ngx_kcp_send(c, in->buf->pos, size);
        if (0 < n)
        {
            in->buf->pos += n;
            total += n;

            if (n != size)
            {
                break;
            }

            continue;
        }
        else if (n == NGX_ERROR)
        {
            return NGX_CHAIN_ERROR;
        }

        /* n == NGX_AGAIN */

        break;
    }

    return in;
}

static ssize_t
ngx_kcp_recv_chain(ngx_connection_t *c, ngx_chain_t *in, off_t limit)
{
    ngx_kcp_t *kcp = c->kcp;
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp recv chain %d",
                   kcp->number);
    return NGX_ERROR;
}

static ssize_t
ngx_kcp_send(ngx_connection_t *c, u_char *b, size_t size)
{
    ngx_kcp_t *kcp = c->kcp;
    int        rc;
    int        n;

    n = ikcp_waitsnd(kcp->ikcp);

    if (kcp->max_waiting_send_number < n)
    {
        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "seed to max number of waiting send");
        return NGX_AGAIN;
    }

    rc = ikcp_send(kcp->ikcp, (const char *)b, size);
    if (rc < 0)
    {
        ngx_log_error(NGX_LOG_ERR, c->log, 0, "ikcp_send() failed: %d", rc);
        return NGX_ERROR;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp send %d:%uz bytes",
                   kcp->number, size);

    ngx_event_kcp_update_timer(c->log, kcp);

    return size;
}


static ssize_t
ngx_kcp_recv(ngx_connection_t *c, u_char *buf, size_t size)
{
    ngx_kcp_t *kcp = c->kcp;
    int        n;


    if (kcp->close)
    {
        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp recv %d: closed",
                       kcp->number);
        return 0;
    }

    n = ikcp_recv(kcp->ikcp, (char *)buf, size);
    if (n < 0)
    {
        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp recv %d: again",
                       kcp->number);
        return NGX_AGAIN;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp recv %d:%d bytes",
                   kcp->number, n);

    return n;
}

static void
ngx_kcp_log(const char *log, struct IKCPCB *kcp, void *user)
{
    ngx_connection_t *c = user;

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp log: %s", log);
}

ngx_kcp_t *
ngx_create_kcp(ngx_connection_t *c)
{
    ngx_kcp_t          *kcp;
    ikcpcb             *ikcp;
    ngx_pool_cleanup_t *cln;
    ssize_t             n;

    kcp = ngx_pcalloc(c->pool, sizeof(ngx_kcp_t));
    if (kcp == NULL) return NULL;

    ikcp = ikcp_create(c->number, c);
    if (ikcp == NULL) return NULL;

    ikcp_setoutput(ikcp, ngx_kcp_output_handler);
    ikcp_nodelay(ikcp, 1, NGX_KCP_INTERVAL_DEFAULT, 2, 1);
    ikcp_wndsize(ikcp, 1024, 1024);

    ikcp->logmask  = 0xfffffff;
    ikcp->writelog = ngx_kcp_log;

    if (c->buffer && (n = ngx_buf_size(c->buffer)))
    {
        /* consume buffer in the connection */
        int rc = ikcp_input(ikcp, (const char *)c->buffer->pos, n);

        if (rc < 0)
        {
            ngx_log_error(NGX_LOG_ERR, c->log, 0,
                          "ikcp_input() error: %d. buffer: %p, size: %z", rc,
                          c->buffer->pos, n);
            return NULL;
        }

        c->buffer->pos += n;
    }

    if (!c->read->active
        && ngx_add_event(c->read, NGX_READ_EVENT, 0) == NGX_ERROR)
    {
        ngx_destroy_kcp(kcp);
        return NULL;
    }

    cln = ngx_pool_cleanup_add(c->pool, 0);
    if (cln == NULL)
    {
        ngx_destroy_kcp(kcp);
        return NULL;
    }

    cln->data    = kcp;
    cln->handler = (ngx_pool_cleanup_pt)ngx_destroy_kcp;

    kcp->number                  = c->number;
    kcp->ikcp                    = ikcp;
    kcp->write_handler           = ngx_kcp_write_handler;
    kcp->read_handler            = ngx_kcp_read_handler;
    kcp->max_waiting_send_number = 2048;
    kcp->valve_of_send           = 64;

    kcp->transport_send       = c->send;
    kcp->transport_send_chain = c->send_chain;
    kcp->transport_recv       = c->recv;
    kcp->transport_recv_chain = c->recv_chain;

    c->send       = ngx_kcp_send;
    c->send_chain = ngx_kcp_send_chain;
    c->recv       = ngx_kcp_recv;
    c->recv_chain = ngx_kcp_recv_chain;

    ngx_event_kcp_add_timer(c->log, kcp);

    return kcp;
}

static void
ngx_kcp_write_handler(ngx_connection_t *c)
{
    ngx_kcp_t *kcp = c->kcp;
    ngx_int_t  n   = NGX_MAX_INT32_VALUE;

    if (c->write->active
        && ngx_del_event(c->write, NGX_WRITE_EVENT, 0) == NGX_ERROR)
    {
        kcp->error = 1;
        goto done;
    }

    ikcp_flush(kcp->ikcp);

    ngx_event_kcp_update_timer(c->log, kcp);

    n = ikcp_waitsnd(kcp->ikcp);

done:

    {
        if (0 < ikcp_peeksize(kcp->ikcp) && c->read->handler)
        {
            c->read->handler(c->read);
        }

        if (kcp->waiting_read && c->read->handler && (kcp->error || kcp->close))
        {
            c->read->handler(c->read);
        }
    }

    /* notify above layer to write */

    if (kcp->waiting_write && c->write->handler
        && (n < kcp->valve_of_send || kcp->error))
    {
        unsigned old = c->write->ready;

        c->write->ready = 1; // set 'ready=1' to notify above layer to write
        c->write->handler(c->write);
        c->write->ready = old; // recover
    }
}

static void
ngx_kcp_read_handler(ngx_connection_t *c)
{
    ngx_kcp_t *kcp = c->kcp;
    ssize_t    n;
    u_char     buffer[65536];

    while (1)
    {
        n = kcp->transport_recv(c, buffer, sizeof(buffer));
        if (0 < n)
        {
            int rc;

            rc = ikcp_input(kcp->ikcp, (const char *)buffer, n);
            if (rc < 0)
            {
                ngx_log_error(NGX_LOG_ERR, c->log, 0, "ikcp_input() error: %d",
                              rc);

                c->error   = 1;
                kcp->error = 1;
                break;
            }

            continue;
        }
        else if (NGX_AGAIN == n)
        {
            break;
        }
        else if (0 == n)
        {
            ngx_log_error(NGX_LOG_INFO, c->log, 0,
                          "connection was be closed by client");

            kcp->close = 1;
            break;
        }

        /* NGX_ERROR */

        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "connection was be closed by client");

        c->error   = 1;
        kcp->error = 1;
        break;
    }

    ngx_event_kcp_update_timer(c->log, kcp);

    /* notify above layer to read */
    {
        unsigned old = c->read->ready;

        c->read->ready = 1; // set 'ready=1' to notify above layer to read

        if (0 < ikcp_peeksize(kcp->ikcp) && c->read->handler)
        {
            c->read->handler(c->read);
        }

        if (kcp->waiting_read && c->read->handler && (kcp->error || kcp->close))
        {
            c->read->handler(c->read);
        }

        c->read->ready = old; // recover
    }
}

static int
ngx_kcp_output_handler(const char *buf, int len, ikcpcb *ikcp, void *user)
{
    ngx_connection_t *c   = user;
    ngx_kcp_t        *kcp = c->kcp;
    ssize_t           n;

    n = kcp->transport_send(c, (u_char *)buf, len);
    if (0 < n)
    {
        if (n != len)
        {
            ngx_log_error(NGX_LOG_ERR, c->log, 0,
                          "kcp_output_handler() send data is incomplete");
            goto error;
        }

        return 0;
    }
    else if (n == NGX_AGAIN)
    {
        if (!c->write->active
            && ngx_add_event(c->write, NGX_WRITE_EVENT, 0) != NGX_ERROR)
        {
            goto error;
        }

        return 0;
    }

    /* n == NGX_ERROR */

error:

    c->error   = 1;
    kcp->error = 1;

    if (c->write->handler) c->write->handler(c->write);

    return -1;
}

static void
ngx_destroy_kcp(ngx_kcp_t *kcp)
{
    ikcp_release(kcp->ikcp);
}