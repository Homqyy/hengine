# Name #

**ngx\_stream\_upstream\_module**


# Examples #

```
upstream backend {
    hash $remote_addr consistent;

    server backend1.example.com:12345  weight=5;
    server backend2.example.com:12345;
    server backend3.example.com:12345 udp;
    server backend4.example.com:12345 udp kcp=5;
    server backend5.example.com:12345 udp kcp=6 kcp_mode=quick;
    server unix:/tmp/backend3;

    server backup1.example.com:12345                          backup;
    server backup2.example.com:12345                          backup;
    server backup3.example.com:12345 udp                      backup;
    server backup4.example.com:12345 udp kcp=5                backup;
    server backup5.example.com:12345 udp kcp=6 kcp_mode=quick backup;
}

server {
    listen 12346;
    proxy_pass backend;
}
```

# 指令 #

## server ##

Syntax: **server** `address [udp [kcp=conv [kcp_mode=normal|quick]]] [weight=number] [max_conns=number] [max_fails=number] [fail_timeout=time] [backup] [down];`

Default: -

Context: `upstream`

该指令用来配置后端服务器的地址`address`、连接类型和负载均衡策略。`address`可以是携带端口的域名或IP地址，或则用`unix:`前缀来表示使用Unix-domain套接字。

指令后面的参数意义是：

* `udp`：后端服务器的连接类型为UDP
    - `kcp`：后端服务器的连接类型为KCP（UDP+KCP），并设置代理的会话ID`conv`；通常情况下，两侧的协议应当是一致的，无需用此配置进行设置，而是在`listen`中指定即可，即仅当进行连接类型转换时（例如：tcp转kcp）才需要此配置；可在配置编译选项时开启此功能：`./configure --with-kcp`
        - `kcp_mode`：设置KCP的模式，`normal`为正常模式，`quick`为极速模式。