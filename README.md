# Hengine

## 介绍

- 基于“Tengine”的`v2.3.3`
- 对应的镜像为：[homqyy/hengine](https://hub.docker.com/r/homqyy/hengine)

## 使用

1. 配置：`./configure --prefix=/path/to/install/`
2. 编译：`make -j`
3. 安装：`make install`
4. 运行：`/path/to/install/sbin/nginx`

## 目录结构

```
hengine/
├── auto/                       # 编译脚本
├── build.sh                    # 一件构建脚本
├── CHANGES.cn.md               # 版本说明
├── conf/                       # 默认配置文件
├── configure                   # 配置脚本
├── contrib/                    # 贡献者
├── docker/                     # Docker相关文件
├── Dockerfile                  # 构建的容器配置文件
├── docs/                       # 说明文档
├── html/                       # 默认页面
├── LICENSE
├── man/                        # manpage
├── modules/                    # 可选的nginx模块
├── packages/
├── README.md
├── src/                        # nginx框架和内置模块代码
├── tests/                      # 测试脚本
└── tools-dev/
```