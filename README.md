# Hengine

For Chinese, please refer to [README-zh.md](./README-zh.md).

## Introduction

- Developed based on "Tengine", updated to `tengine-master-2023-7-2`
- The corresponding image is: [homqyy/hengine](https://hub.docker.com/r/homqyy/hengine)

### New Features

#### Proxy Connect Module supports Basic Authentication

[ngx_http_proxy_connect_module_cn.md](./docs/modules/ngx_http_proxy_connect_module_cn.md)

#### Protocol Conversion

You can freely convert protocols between `tcp`, `udp` and `udp+kcp`.

#### Support for KCP Proxy

The manual is not yet available.

## Plans

- [ ] KCP test script
- [x] Patch: Proxy Connect + SSL functionality is abnormal
    - [#1794](https://github.com/alibaba/tengine/issues/1794)
    - [#1797](https://github.com/alibaba/tengine/pull/1797/files)
- [x] Automatically upload and build the docker image `homqyy/hengine`
- [ ] Support arm64 image
- [ ] Test performance for kcp
- [ ] Automatically genete README.md based on README-zh.md
