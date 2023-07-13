# Hengine

## 介绍

- 基于“Tengine”开发，更新至`tengine-master-2023-7-2`
- 对应的镜像为：[homqyy/hengine](https://hub.docker.com/r/homqyy/hengine)

### 新增特性

#### Proxy Connect Module 支持Basic认证

[ngx_http_proxy_connect_module_cn.md](./docs/modules/ngx_http_proxy_connect_module_cn.md)

#### 协议转换

可以在`tcp`、`udp`和`udp+kcp`之间任意转换协议

#### 支持KCP代理

手册暂未提供

## 计划

- [ ] kcp测试脚本
- [x] 补丁：Proxy Connect + SSL 功能异常
    - [#1794](https://github.com/alibaba/tengine/issues/1794)
    - [#1797](https://github.com/alibaba/tengine/pull/1797/files)
- [x] 自动上传构建docker镜像`homqyy/hengine`
- [ ] 支持 arm64 镜像
- [ ] 对 KCP 功能进行性能测试
- [ ] 基于 README-zh.md 自动生成 README.md
