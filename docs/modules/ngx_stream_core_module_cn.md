# Name #

**ngx\_stream\_core\_module**

该模块是`stream`定义和驱动stream框架的模块，在配置编译选项的时候开启：`./configure --with-stream`

# Examples #

```
worker_processes auto;

error_log /var/log/nginx/error.log info;

events {
    worker_connections  1024;
}

stream {
    upstream backend {
        hash $remote_addr consistent;

        server backend1.example.com:12345 weight=5;
        server 127.0.0.1:12345            max_fails=3 fail_timeout=30s;
        server unix:/tmp/backend3;
    }

    upstream dns {
       server 192.168.0.1:53535;
       server dns.example.com:53;
    }

    server {
        listen 12345;
        proxy_connect_timeout 1s;
        proxy_timeout 3s;
        proxy_pass backend;
    }

    server {
        listen 127.0.0.1:53 udp reuseport;
        proxy_timeout 20s;
        proxy_pass dns;
    }

    server {
        listen 127.0.0.1:53 udp kcp=quick reuseport;
        proxy_timeout 20s;
        proxy_pass dns;
    }

    server {
        listen [::1]:12345;
        proxy_pass unix:/tmp/stream.socket;
    }
}
```

# 指令 #

## listen ##

Syntax: **listen** `address:port [ssl] [udp [kcp=normal|quick]] [proxy_protocol] [fastopen=number] [backlog=number] [rcvbuf=size] [sndbuf=size] [bind] [ipv6only=on|off] [reuseport] [so_keepalive=on|off|[keepidle]:[keepintvl]:[keepcnt]];`

Default: -

Context: `server`

该指令用来配置流代理监听的地址`address`和端口`port`

指令后面的参数意义是：

* `udp`：监听的连接类型为UDP
    - `kcp`：设置监听的连接类型为KCP（UDP + KCP），参数是用来设置KCP的模式的。可在配置编译选项时开启此功能：`./configure --with-kcp`
        - `normal`：正常模式
        - `quick`：极速模式