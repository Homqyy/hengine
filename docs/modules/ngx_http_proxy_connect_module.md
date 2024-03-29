name
====

This module provides support for the CONNECT HTTP method after Tengine version 2.3.0.  
This method is mainly used to [tunnel SSL requests](https://en.wikipedia.org/wiki/HTTP_tunnel#HTTP_CONNECT_tunneling) through proxy servers.

Table of Contents
=================

   * [name](#name)
   * [Example](#example)
      * [configuration example](#configuration-example)
      * [example for curl](#example-for-curl)
   * [Install](#install)
   * [Error Log](#error-log)
   * [Directive](#directive)
      * [proxy_connect](#proxy_connect)
      * [proxy_connect_allow](#proxy_connect_allow)
      * [proxy_connect_auth](#proxy_connect_auth)
      * [proxy_connect_connect_timeout](#proxy_connect_connect_timeout)
      * [proxy_connect_data_timeout](#proxy_connect_data_timeout)
      * [proxy_connect_read_timeout(deprecated)](#proxy_connect_read_timeout)
      * [proxy_connect_send_timeout(deprecated)](#proxy_connect_send_timeout)
      * [proxy_connect_address](#proxy_connect_address)
      * [proxy_connect_bind](#proxy_connect_bind)
      * [proxy_connect_response](#proxy_connect_response)
   * [Variables](#variables)
      * [$connect_host](#connect_host)
      * [$connect_port](#connect_port)
      * [$connect_addr](#connect_addr)
      * [$proxy_connect_connect_timeout](#proxy_connect_connect_timeout-1)
      * [$proxy_connect_read_timeout](#proxy_connect_read_timeout-1)
      * [$proxy_connect_send_timeout(deprecated)](#proxy_connect_send_timeout-1)
      * [$proxy_connect_resolve_time](#proxy_connect_resolve_time)
      * [$proxy_connect_connect_time](#proxy_connect_connect_time)
      * [$proxy_connect_first_byte_time](#proxy_connect_first_byte_time)
      * [$proxy_connect_response](#proxy_connect_response-1)
   * [Known Issues](#known-issues)

Example
=======

Configuration Example
---------------------

```nginx
 server {
     listen                         3128;

     # dns resolver used by forward proxying
     resolver                       8.8.8.8;

     # forward proxy for CONNECT request
     proxy_connect;
     proxy_connect_allow            443 563;
     proxy_connect_connect_timeout  10s;
     proxy_connect_data_timeout     10s;

     # forward proxy for non-CONNECT request
     location / {
         proxy_pass http://$host;
         proxy_set_header Host $host;
     }
 }
```

Example for curl
----------------

With above configuration, you can get any https website via HTTP CONNECT tunnel.
A simple test with command `curl` is as following:

```
$ curl https://github.com/ -v -x 127.0.0.1:3128
*   Trying 127.0.0.1...                                           -.
* Connected to 127.0.0.1 (127.0.0.1) port 3128 (#0)                | curl creates TCP connection with nginx (with proxy_connect module).
* Establish HTTP proxy tunnel to github.com:443                   -'
> CONNECT github.com:443 HTTP/1.1                                 -.
> Host: github.com:443                                         (1) | curl sends CONNECT request to create tunnel.
> User-Agent: curl/7.43.0                                          |
> Proxy-Connection: Keep-Alive                                    -'
>
< HTTP/1.0 200 Connection Established                             .- nginx replies 200 that tunnel is established.
< Proxy-agent: nginx                                           (2)|  (The client is now being proxied to the remote host. Any data sent
<                                                                 '-  to nginx is now forwarded, unmodified, to the remote host)

* Proxy replied OK to CONNECT request
* TLS 1.2 connection using TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256  -.
* Server certificate: github.com                                   |
* Server certificate: DigiCert SHA2 Extended Validation Server CA  | curl sends "https://github.com" request via tunnel,
* Server certificate: DigiCert High Assurance EV Root CA           | proxy_connect module will proxy data to remote host (github.com).
> GET / HTTP/1.1                                                   |
> Host: github.com                                             (3) |
> User-Agent: curl/7.43.0                                          |
> Accept: */*                                                     -'
>
< HTTP/1.1 200 OK                                                 .-
< Date: Fri, 11 Aug 2017 04:13:57 GMT                             |
< Content-Type: text/html; charset=utf-8                          |  Any data received from remote host will be sent to client
< Transfer-Encoding: chunked                                      |  by proxy_connect module.
< Server: GitHub.com                                           (4)|
< Status: 200 OK                                                  |
< Cache-Control: no-cache                                         |
< Vary: X-PJAX                                                    |
...                                                               |
... <other response headers & response body> ...                  |
...                                                               '-
```

The sequence diagram of above example is as following:

```
  curl                     nginx (proxy_connect)            github.com
    |                             |                          |
(1) |-- CONNECT github.com:443 -->|                          |
    |                             |                          |
    |                             |----[ TCP connection ]--->|
    |                             |                          |
(2) |<- HTTP/1.1 200           ---|                          |
    |   Connection Established    |                          |
    |                             |                          |
    |                                                        |
    ========= CONNECT tunnel has been established. ===========
    |                                                        |
    |                             |                          |
    |                             |                          |
    |   [ SSL stream       ]      |                          |
(3) |---[ GET / HTTP/1.1   ]----->|   [ SSL stream       ]   |
    |   [ Host: github.com ]      |---[ GET / HTTP/1.1   ]-->.
    |                             |   [ Host: github.com ]   |
    |                             |                          |
    |                             |                          |
    |                             |                          |
    |                             |   [ SSL stream       ]   |
    |   [ SSL stream       ]      |<--[ HTTP/1.1 200 OK  ]---'
(4) |<--[ HTTP/1.1 200 OK  ]------|   [ < html page >    ]   |
    |   [ < html page >    ]      |                          |
    |                             |                          |
```

Install
=======

* Build Tengine with this module from source:

```
$ ./configure --add-module=./modules/ngx_http_proxy_connect_module
$ make && make install
```

Error Log
=========

This module logs its own error message beginning with `"proxy_connect:"` string.  
Some typical error logs are shown as following:

* The proxy_connect module tries to establish tunnel connection with backend server, but the TCP connection timeout occurs.

```
2019/08/07 17:27:20 [error] 19257#0: *1 proxy_connect: upstream connect timed out (peer:216.58.200.4:443) while connecting to upstream, client: 127.0.0.1, server: , request: "CONNECT www.google.com:443 HTTP/1.1", host: "www.google.com:443"
```

Directive
=========

proxy_connect
-------------

Syntax: **proxy_connect**  
Default: `none`  
Context: `server`  

Enable "CONNECT" HTTP method support.

proxy_connect_allow
-------------------

Syntax: **proxy_connect_allow `all | [port ...] | [port-range ...]`**  
Default: `443 563`  
Context: `server`  

This directive specifies a list of port numbers or ranges to which the proxy CONNECT method may connect.  
By default, only the default https port (443) and the default snews port (563) are enabled.  
Using this directive will override this default and allow connections to the listed ports only.

The value `all` will allow all ports to proxy.

The value `port` will allow specified port to proxy.

The value `port-range` will allow specified range of port to proxy, for example:

```
proxy_connect_allow 1000-2000 3000-4000; # allow range of port from 1000 to 2000, from 3000 to 4000.
```

proxy_connect_auth
------------------

Syntax: **proxy_connect_auth `<on|off>`**  
Default: `off`  
Context: `server`  

Enable "Proxy Authorication" feature. Now, only `Basic` is supported.

Configure example:

```
 server {
     listen                         3128;

     # dns resolver used by forward proxying
     resolver                       8.8.8.8;

     # forward proxy for CONNECT request
     proxy_connect;
     proxy_connect_allow            443 563;
     proxy_connect_connect_timeout  10s;
     proxy_connect_read_timeout     10s;
     proxy_connect_send_timeout     10s;

     # proxy auth
     proxy_connect_auth     on;                 # open proxy auth
     auth_basic             web_proxy;
     auth_basic_user_file   user_list.htpasswd;

     # forward proxy for non-CONNECT request
     location / {
         auth_basic off;                        # close basic auth

         proxy_pass http://$host;
         proxy_set_header Host $host;
     }
 }
```

proxy_connect_connect_timeout
-----------------------------

Syntax: **proxy_connect_connect_timeout `time`**  
Default: `none`  
Context: `server`  

Defines a timeout for establishing a connection with a proxied server.

proxy_connect_data_timeout
--------------------------

Syntax: **proxy_connect_data_timeout `time`**  
Default: `60s`  
Context: `server`  

Sets the timeout between two successive read or write operations on client or proxied server connections. If no data is transmitted within this time, the connection is closed.

proxy_connect_read_timeout
--------------------------

Syntax: **proxy_connect_read_timeout `time`**  
Default: `60s`  
Context: `server`  

Deprecated.

It has the same function as the directive `proxy_connect_data_timeout` for compatibility. You can configure only one of the directives (`proxy_connect_data_timeout` or `proxy_connect_read_timeout`).

proxy_connect_send_timeout
--------------------------

Syntax: **proxy_connect_send_timeout `time`**  
Default: `60s`  
Context: `server`  

Deprecated.

It has no function.

proxy_connect_address
---------------------

Syntax: **proxy_connect_address `address | off`**  
Default: `none`  
Context: `server`  

Specifiy an IP address of the proxied server. The address can contain variables.  
The special value off is equal to none, which uses the IP address resolved from host name of CONNECT request line.  

proxy_connect_bind
------------------

Syntax: **proxy_connect_bind `address [transparent] | off`**  
Default: `none`  
Context: `server`  

Makes outgoing connections to a proxied server originate from the specified local IP address with an optional port.  
Parameter value can contain variables. The special value off is equal to none, which allows the system to auto-assign the local IP address and port.

The transparent parameter allows outgoing connections to a proxied server originate from a non-local IP address, for example, from a real IP address of a client:

```
proxy_connect_bind $remote_addr transparent;

```

In order for this parameter to work, it is usually necessary to run nginx worker processes with the [superuser](http://nginx.org/en/docs/ngx_core_module.html#user) privileges. On Linux it is not required (1.13.8) as if the transparent parameter is specified, worker processes inherit the CAP_NET_RAW capability from the master process. It is also necessary to configure kernel routing table to intercept network traffic from the proxied server.

proxy_connect_response
----------------------

Syntax: **proxy_connect_response `CONNECT response`**  
Default: `HTTP/1.1 200 Connection Established\r\nProxy-agent: nginx\r\n\r\n`  
Context: `server`

Set the response of CONNECT request.

Note that it is only used for CONNECT request, it cannot modify the data flow over CONNECT tunnel.

For example:

```
proxy_connect_response "HTTP/1.1 200 Connection Established\r\nProxy-agent: nginx\r\nX-Proxy-Connected-Addr: $connect_addr\r\n\r\n";

```

The `curl` command test case with above config is as following:

```
$ curl https://github.com -sv -x localhost:3128
* Connected to localhost (127.0.0.1) port 3128 (#0)
* allocate connect buffer!
* Establish HTTP proxy tunnel to github.com:443
> CONNECT github.com:443 HTTP/1.1
> Host: github.com:443
> User-Agent: curl/7.64.1
> Proxy-Connection: Keep-Alive
>
< HTTP/1.1 200 Connection Established            --.
< Proxy-agent: nginx                               | custom CONNECT response
< X-Proxy-Connected-Addr: 13.229.188.59:443      --'
...

```


Variables
=========

$connect_host
-------------

host name from CONNECT request line.

$connect_port
-------------

port from CONNECT request line.

$connect_addr
-------------

IP address and port of the remote host, e.g. "192.168.1.5:12345".
IP address is resolved from host name of CONNECT request line.

$proxy_connect_connect_timeout
------------------------------

Get or set timeout of [`proxy_connect_connect_timeout` directive](#proxy_connect_connect_timeout).

For example:

```nginx
# Set default value

proxy_connect_connect_timeout   10s;
proxy_connect_data_timeout      10s;

# Overlap default value

if ($host = "test.com") {
    set $proxy_connect_connect_timeout  "10ms";
    set $proxy_connect_data_timeout     "10ms";
}
```

$proxy_connect_data_timeout
---------------------------

Get or set a timeout of [`proxy_connect_data_timeout` directive](#proxy_connect_data_timeout).

$proxy_connect_read_timeout
---------------------------

Deprecated. 
It still can get or set a timeout of [`proxy_connect_data_timeout` directive](#proxy_connect_data_timeout) for compatibility.

$proxy_connect_send_timeout
---------------------------

Deprecated.
It has no function.

$proxy_connect_resolve_time
---------------------------

Keeps time spent on name resolving; the time is kept in seconds with millisecond resolution.

* Value of "" means this module does not work on this request.
* Value of "-" means name resolving failed.


$proxy_connect_connect_time
---------------------------

Keeps time spent on establishing a connection with the upstream server; the time is kept in seconds with millisecond resolution.

* Value of "" means this module does not work on this request.
* Value of "-" means name resolving or connecting failed.


$proxy_connect_first_byte_time
---------------------------

Keeps time to receive the first byte of data from the upstream server; the time is kept in seconds with millisecond resolution.

* Value of "" means this module does not work on this request.
* Value of "-" means name resolving, connecting or receving failed.


$proxy_connect_response
---------------------------

Get or set the response of CONNECT request.  
The default response of CONNECT request is "HTTP/1.1 200 Connection Established\r\nProxy-agent: nginx\r\n\r\n".

Note that it is only used for CONNECT request, it cannot modify the data flow over CONNECT tunnel.

For example:

```nginx

# modify default Proxy-agent header
set $proxy_connect_response "HTTP/1.1 200\r\nProxy-agent: nginx/1.19\r\n\r\n";
```


Known Issues
============

* In HTTP/2, the CONNECT method is not supported. It only supports the CONNECT method request in HTTP/1.x and HTTPS.

See Also
========

* [HTTP tunnel - Wikipedia](https://en.wikipedia.org/wiki/HTTP_tunnel)
* [CONNECT method in HTTP/1.1](https://tools.ietf.org/html/rfc7231#section-4.3.6)
* [CONNECT method in HTTP/2](https://httpwg.org/specs/rfc7540.html#CONNECT)

