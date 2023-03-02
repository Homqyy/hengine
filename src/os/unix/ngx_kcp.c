/*
 * Copyright (C) homqyy
 */

#include <ngx_config.h>
#include <ngx_core.h>

#include <ngx_event.h>

#include <ikcp.c>


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
    ngx_event_t *wev;

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp send chain #%d %ud",
                   c->fd, c->kcp->conv);

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
    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp recv chain #%d %ud",
                   c->fd, c->kcp->conv);
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
                      "#%d seed to max number of waiting send", c->fd);
        return NGX_AGAIN;
    }

    rc = ikcp_send(kcp->ikcp, (const char *)b, size);
    if (rc < 0)
    {
        ngx_log_error(NGX_LOG_ERR, c->log, 0, "#%d ikcp_send() failed: %d",
                      c->fd, rc);
        return NGX_ERROR;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp send #%d %ud:%uz bytes",
                   c->fd, kcp->conv, size);

    ikcp_flush(kcp->ikcp);

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
        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "kcp recv #%d %ud: closed", c->fd, kcp->conv);
        return 0;
    }

    n = ikcp_recv(kcp->ikcp, (char *)buf, size);
    if (n < 0)
    {
        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "kcp recv #%d %ud: again", c->fd, kcp->conv);
        return NGX_AGAIN;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp recv #%d %ud:%d bytes",
                   c->fd, kcp->conv, n);

    return n;
}

static void
ngx_kcp_log(const char *log, struct IKCPCB *kcp, void *user)
{
#if (NGX_DEBUG)
    ngx_connection_t *c = user;

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0, "#%d kcp log: %s", c->fd,
                   log);
#endif
}

ngx_kcp_t *
ngx_create_kcp(ngx_connection_t *c, ngx_uint_t conv, ngx_uint_t mode)
{
    ngx_kcp_t          *kcp;
    ikcpcb             *ikcp;
    ngx_pool_cleanup_t *cln;
    ssize_t             n;

    kcp = ngx_pcalloc(c->pool, sizeof(ngx_kcp_t));
    if (kcp == NULL) return NULL;

    kcp->mode                    = mode;
    kcp->log                     = c->log;
    kcp->waiting_read            = c->read->active ? 1 : 0;
    kcp->waiting_write           = c->write->active ? 1 : 0;
    kcp->conv                    = conv;
    kcp->write_handler           = ngx_kcp_write_handler;
    kcp->read_handler            = ngx_kcp_read_handler;
    kcp->max_waiting_send_number = 2048;
    kcp->valve_of_send           = 64;

    kcp->transport_send       = c->send;
    kcp->transport_send_chain = c->send_chain;
    kcp->transport_recv       = c->recv;
    kcp->transport_recv_chain = c->recv_chain;

    ikcp = ikcp_create(conv, c);
    if (ikcp == NULL) return NULL;

    ikcp_setoutput(ikcp, ngx_kcp_output_handler);

    switch (mode)
    {
    case NGX_KCP_NORMAL_MODE: ikcp_nodelay(ikcp, 0, 40, 0, 0); break;
    case NGX_KCP_QUICK_MODE: ikcp_nodelay(ikcp, 1, 10, 2, 1); break;
    }

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
                          "ikcp_input() error: #%d %d. buffer: %p, size: %z",
                          rc, c->fd, c->buffer->pos, n);
            return NULL;
        }

        c->buffer->pos += n;

        if (ikcp_peeksize(ikcp))
        {
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp ready to read");

            c->read->ready = 1;
        }
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

    c->send       = ngx_kcp_send;
    c->send_chain = ngx_kcp_send_chain;
    c->recv       = ngx_kcp_recv;
    c->recv_chain = ngx_kcp_recv_chain;
    c->kcp        = kcp;

    kcp->ikcp = ikcp;

    ngx_event_kcp_add_timer(c->log, kcp);

    ikcp_update(ikcp, ngx_current_msec); // immediately active it

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0,
                   "create kcp %d, and mode is %ud on fd %d", kcp->conv,
                   kcp->mode, c->fd);

    return kcp;
}

static void
ngx_kcp_write_handler(ngx_connection_t *c)
{
    ngx_kcp_t *kcp = c->kcp;
    ngx_int_t  n   = NGX_MAX_INT32_VALUE;

    ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp write handler");

    if (kcp->write_active)
    {
        c->kcp = NULL;

        if (ngx_del_event(c->write, NGX_WRITE_EVENT, 0) == NGX_ERROR)
        {
            kcp->error = 1;

            c->kcp = kcp;
            goto done;
        }

        kcp->write_active = 0;

        c->kcp = kcp;
    }

    ikcp_flush(kcp->ikcp);

    ngx_event_kcp_update_timer(c->log, kcp);

    n = ikcp_waitsnd(kcp->ikcp);

done:

    /* notify above layer to write */

    if (kcp->waiting_write && c->write->handler
        && (n < kcp->valve_of_send || kcp->error))
    {
        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "exec write handler of connection");

        unsigned old = c->write->ready;

        c->write->ready = 1; // set 'ready=1' to notify above layer to write
        c->write->handler(c->write);
        c->write->ready = old; // recover
    }
    else
    {
        ngx_log_debug4(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "don't exec connection write handler: %p, "
                       "waiting_write: %d, waitsnd: %d, "
                       "value_of_send: %d",
                       c->write->handler, kcp->waiting_write, n,
                       kcp->valve_of_send);
    }
}

static void
ngx_kcp_read_handler(ngx_connection_t *c)
{
    ngx_kcp_t *kcp = c->kcp;
    ssize_t    n;
    u_char     buffer[65536];

    ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0, "kcp read handler");

    while (1)
    {
        n = kcp->transport_recv(c, buffer, sizeof(buffer));
        if (0 < n)
        {
            int rc;

            rc = ikcp_input(kcp->ikcp, (const char *)buffer, n);
            if (rc < 0)
            {
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "ikcp_input() error: #%d %d", c->fd, rc);

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
                          "#%d connection was be closed by client", c->fd);

            kcp->close = 1;
            break;
        }

        /* NGX_ERROR */

        ngx_log_error(NGX_LOG_INFO, c->log, 0, "connection #%d error", c->fd);

        c->error   = 1;
        kcp->error = 1;
        break;
    }

    ngx_event_kcp_update_timer(c->log, kcp);

    /* notify above layer to read */
    if (kcp->waiting_read && c->read->handler
        && (0 < ikcp_peeksize(kcp->ikcp) || kcp->error || kcp->close))
    {
        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "exec read handler of connection");

        unsigned old = c->read->ready;

        c->read->ready = 1; // set 'ready=1' to notify above layer to read
        c->read->handler(c->read);
        c->read->ready = old; // recover
    }
    else
    {
        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "don't exec connection read handler: %p, "
                       "waiting_read: %d, waitsize: %d",
                       c->read->handler, kcp->waiting_read,
                       ikcp_peeksize(kcp->ikcp));
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
        ngx_log_error(NGX_LOG_DEBUG, c->log, 0, "#%d kcp ouput %z bytes", c->fd,
                      n);

        if (n != len)
        {
            ngx_log_error(NGX_LOG_ERR, c->log, 0,
                          "#%d kcp_output_handler() send data is incomplete",
                          c->fd);
            goto error;
        }

        return 0;
    }
    else if (n == NGX_AGAIN)
    {
        if (!kcp->write_active)
        {
            c->kcp = NULL;

            if (ngx_add_event(c->write, NGX_WRITE_EVENT, 0) != NGX_ERROR)
            {
                c->kcp = kcp;
                goto error;
            }

            kcp->write_active = 1;

            c->kcp = kcp;
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
    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, kcp->log, 0,
                   "destroy kcp %d, and mode is %ud", kcp->conv, kcp->mode);

    ngx_event_kcp_del_timer(kcp->log, kcp);
    ikcp_release(kcp->ikcp);
}

ngx_uint_t
ngx_get_kcp_conv(u_char *buffer, size_t size)
{
    if (size < IKCP_OVERHEAD)
    {
        return 0;
    }

    return ikcp_getconv(buffer);
}