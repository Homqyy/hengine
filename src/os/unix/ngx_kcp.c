/*
 * Copyright (C) homqyy
 */

#include <ngx_config.h>
#include <ngx_core.h>

#include <ngx_event.h>


static void ngx_destroy_kcp(ngx_kcp_t *kcp);
static int  ngx_kcp_output_handler(const char *buf, int len, ikcpcb *ikcp,
                                   void *user);
static void ngx_kcp_write_handler(ngx_connection_t *c);
static void ngx_kcp_read_handler(ngx_connection_t *c);


ssize_t
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

    return size;
}


ssize_t
ngx_kcp_recv(ngx_connection_t *c, u_char *buf, size_t size)
{
    ngx_kcp_t *kcp = c->kcp;
    int        n;

    if (kcp->close)
    {
        return 0;
    }

    n = ikcp_recv(kcp->ikcp, (char *)buf, size);
    if (n < 0)
    {
        return NGX_EAGAIN;
    }

    return n;
}

ngx_kcp_t *
ngx_create_kcp(ngx_connection_t *c)
{
    ngx_kcp_t          *kcp;
    ikcpcb             *ikcp;
    ngx_pool_cleanup_t *cln;

    kcp = ngx_pcalloc(c->pool, sizeof(ngx_kcp_t));
    if (kcp == NULL) return NULL;

    ikcp = ikcp_create(c->number, c);
    if (ikcp == NULL) return NULL;

    ikcp->output = ngx_kcp_output_handler;

    ikcp_nodelay(ikcp, 0, 40, 0, 0); // normal mode

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

    kcp->ikcp                    = ikcp;
    kcp->write_handler           = ngx_kcp_write_handler;
    kcp->read_handler            = ngx_kcp_read_handler;
    kcp->max_waiting_send_number = 100;
    kcp->valve_of_send           = 25;

    c->kcp = kcp;

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

    n = ikcp_waitsnd(kcp->ikcp);

done:

    if (kcp->waiting_write && (n < kcp->valve_of_send || kcp->error))
    {
        kcp->waiting_write = 0;

        if (c->write->handler) c->write->handler(c->write);
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
        n = c->recv(c, buffer, sizeof(buffer));
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

    if (kcp->waiting_read
        && (0 < ikcp_peeksize(kcp->ikcp) || kcp->error || kcp->close))
    {
        kcp->waiting_read = 0;

        if (c->read->handler) c->read->handler(c->read);
    }
}

static int
ngx_kcp_output_handler(const char *buf, int len, ikcpcb *ikcp, void *user)
{
    ngx_connection_t *c   = user;
    ngx_kcp_t        *kcp = c->kcp;
    ssize_t           n;

    n = c->send(c, (u_char *)buf, len);
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

    c->write->handler(c->write);

    return -1;
}

static void
ngx_destroy_kcp(ngx_kcp_t *kcp)
{
    ikcp_release(kcp->ikcp);
}