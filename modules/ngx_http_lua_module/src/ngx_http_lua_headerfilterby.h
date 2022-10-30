
/*
 * Copyright (C) Yichun Zhang (agentzh)
 */


#ifndef _NGX_HTTP_LUA_HEADERFILTERBY_H_INCLUDED_
#define _NGX_HTTP_LUA_HEADERFILTERBY_H_INCLUDED_


#include "ngx_http_lua_common.h"


extern ngx_http_output_header_filter_pt ngx_http_lua_next_filter_header_filter;


ngx_int_t ngx_http_lua_header_filter_init(void);

ngx_int_t ngx_http_lua_header_filter_by_chunk(lua_State          *L,
                                              ngx_http_request_t *r);

ngx_int_t ngx_http_lua_header_filter_inline(ngx_http_request_t *r);

ngx_int_t ngx_http_lua_header_filter_file(ngx_http_request_t *r);


#endif /* _NGX_HTTP_LUA_HEADERFILTERBY_H_INCLUDED_ */

/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
