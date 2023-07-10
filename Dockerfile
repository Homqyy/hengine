FROM homqyy/dev_env_centos8 as compile

RUN yum install -y pcre-devel openssl-devel

WORKDIR /usr/src/hengine

COPY . .

RUN bash ./configure \
    --prefix=/usr/local/hengine \
    --with-http_ssl_module \
    --with-http_sub_module \
    --with-stream_ssl_module \
    --with-stream \
    --with-stream_sni \
    --add-module=./modules/ngx_http_proxy_connect_module \
    && make && make install

FROM centos:8

# update yum repos
RUN rm -f /etc/yum.repos.d/* \
        && cd /etc/yum.repos.d/ \
        && curl http://mirrors.aliyun.com/repo/Centos-8.repo > CentOS-Linux-BaseOS.repo \
        && sed -i 's/\$releasever/8-stream/g' CentOS-Linux-BaseOS.repo \
        && cd - \
        && yum clean all \
        && yum makecache

COPY --from=compile /usr/local/hengine/ /usr/local/hengine/

WORKDIR /usr/local/hengine

CMD [ "/usr/local/hengine/sbin/nginx", "-g", "daemon off;" ]