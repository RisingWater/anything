
# Anything

一个面向Linux桌面系统的快速文件搜索工具

此工程包含一个基于 Qt 的桌面客户端（用于发起搜索、管理扫描目录）和一个运行在后台的服务端组件（负责文件索引、数据库管理与提供 HTTP API）。同时提供一个与 auditd 集成的插件，用于触发增量索引或扫描更新。

## 运行效果

<image src="https://raw.githubusercontent.com/Risingwater/Anything/master/images/sample.png" width="100%" />

## 构建项目

### docker准备

```bash
docker build -t anything-build .
```

### 进入docker

```
docker run -it -d anything-build --name <container_name> /bin/bash
docker exec -it <container_name> /bin/bash
```

### 构建代码

```bash
mkdir build
cd build
cmake .. -G Ninja
ninja package
```

构建完成后，在 `build` 目录下生成 `Anything-版本号.deb` 包。

## 安装运行

### 安装

```bash
sudo dpkg -i Anything-版本号.deb
```

## 使用

- 开始菜单寻找`Anything`运行
- 开机会自动启动于托盘图标处

