
# 安装客户端 rtty

## Linux 发行版

### 1. 安装依赖库

根据您的 Linux 发行版，选择相应的命令安装依赖：

**Ubuntu/Debian**
```bash
sudo apt install -y libev-dev libssl-dev
```

**ArchLinux**
```bash
sudo pacman -S --noconfirm libev openssl
```

**CentOS/RHEL**
```bash
sudo yum install -y libev-devel openssl-devel
```

### 2. 下载源代码
下载最新版本的 rtty 源代码：
```bash
wget https://github.com/zhaojh329/rtty/releases/download/v9.0.0/rtty-RTTY-VERSION.tar.gz
```

### 3. 解压源代码
```bash
tar xvf rtty-RTTY-VERSION.tar.gz
```

### 4. 编译和安装
```bash
cd rtty-RTTY-VERSION
mkdir build
cd build
cmake ..
make
sudo make install
```

## OpenWrt

### 方式一：直接安装软件包

OpenWrt 提供了 4 个不同的 rtty 软件包，请根据您的需求选择：

- **rtty-nossl** - 无 SSL 支持版本
- **rtty-openssl** - 使用 OpenSSL 作为 SSL 后端
- **rtty-mbedtls** - 使用 mbedTLS 作为 SSL 后端（推荐）
- **rtty-wolfssl** - 使用 wolfSSL 作为 SSL 后端

**安装示例：**
```bash
opkg update
opkg install rtty-mbedtls
```

### 方式二：基于源码编译

如果您需要自定义编译，可以按以下步骤操作：

**1. 安装 feed**
```bash
./scripts/feeds update packages
./scripts/feeds install rtty
```

**2. 配置 menuconfig**

在 menuconfig 中选择对应的 rtty 版本，然后重新编译固件：

```
Utilities  --->
  Terminal  --->
    <*> rtty-mbedtls................. Access your terminals from anywhere via the web
    < > rtty-nossl................... Access your terminals from anywhere via the web
    < > rtty-openssl................. Access your terminals from anywhere via the web
    < > rtty-wolfssl................. Access your terminals from anywhere via the web
```

## 其他嵌入式 Linux 系统

对于其他嵌入式 Linux 系统，您需要进行交叉编译。请将以下示例中的交叉编译工具链替换为您环境中的实际工具链。

### 1. 编译 libev 依赖库
```bash
git clone https://github.com/enki/libev.git
cd libev
./configure --host=arm-linux-gnueabi
DESTDIR=/tmp/rtty_install make install
```

### 2. 交叉编译 rtty
```bash
wget https://github.com/zhaojh329/rtty/releases/download/v9.0.0/rtty-RTTY-VERSION.tar.gz
tar xvf rtty-RTTY-VERSION.tar.gz
cd rtty-RTTY-VERSION
cmake . -DCMAKE_C_COMPILER=arm-linux-gnueabi-gcc -DCMAKE_FIND_ROOT_PATH=/tmp/rtty_install
DESTDIR=/tmp/rtty_install make install
```

### 3. 部署到目标设备
将编译好的文件拷贝到设备的对应目录：
```bash
/tmp/rtty_install/
└── usr
    └── local
        ├── bin
        │   └── rtty
        └── lib
            ├── libev.so -> libev.so.4.0.0
            ├── libev.so.4 -> libev.so.4.0.0
            └── libev.so.4.0.0
```

# 安装服务端 rttys

## 方式一：deb 软件包（推荐）

### 1. 下载

根据您的系统架构选择对应的版本：

**Linux x86_64：**
```bash
wget https://github.com/zhaojh329/rttys/releases/download/vRTTYS-VERSION/rttys_RTTYS-VERSION_amd64.deb
```

**Linux ARM64：**
```bash
wget https://github.com/zhaojh329/rttys/releases/download/vRTTYS-VERSION/rttys_RTTYS-VERSION_arm64.deb
```

### 2. 使用 dpkg 命令安装

```bash
sudo dpkg -i rttys_RTTYS-VERSION_amd64.deb
```

## 方式二：使用预编译包

### 1. 下载

根据您的系统架构选择对应的版本：

**Linux x86_64：**
```bash
wget https://github.com/zhaojh329/rttys/releases/download/vRTTYS-VERSION/rttys-RTTYS-VERSION-linux-amd64.tar.bz2
```

**Linux ARM64：**
```bash
wget https://github.com/zhaojh329/rttys/releases/download/vRTTYS-VERSION/rttys-RTTYS-VERSION-linux-arm64.tar.bz2
```

**Windows x86_64：**
```bash
wget https://github.com/zhaojh329/rttys/releases/download/vRTTYS-VERSION/rttys-RTTYS-VERSION-windows-amd64.tar.bz2
```

### 2. 解压安装包
```bash
tar xvf rttys-RTTYS-VERSION-linux-amd64.tar.bz2
```

### 3. 安装程序和配置文件
```bash
sudo cp rttys-RTTYS-VERSION-linux-amd64/rttys /usr/local/bin/
sudo mkdir -p /etc/rttys
sudo cp rttys-RTTYS-VERSION-linux-amd64/rttys.conf /etc/rttys/
```

### 4. 注册系统服务
```bash
sudo cp rttys-RTTYS-VERSION-linux-amd64/rttys.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable rttys
```

## 方式三：源码编译

如果您需要自定义功能或针对特定平台编译，可以选择源码编译方式。

### 1. 下载源码
```bash
wget https://github.com/zhaojh329/rttys/archive/refs/tags/vRTTYS-VERSION.tar.gz -O rttys-RTTYS-VERSION.tar.gz
```

### 2. 解压源码
```bash
tar xvf rttys-RTTYS-VERSION.tar.gz
```

### 3. 编译前端界面
```bash
cd rttys-RTTYS-VERSION/ui
npm install
npm run build
```

### 4. 编译服务端程序
```bash
cd ../
./build.sh linux amd64
```

### 5. 编译结果
编译完成后，会在当前目录生成以下文件：
```bash
rttys-linux-amd64/
├── rttys          # 服务端可执行文件
├── rttys.conf     # 配置文件模板
└── rttys.service  # systemd 服务文件
```

## 方式四：Docker 部署

使用 Docker 可以快速部署 rttys 服务：

```bash
sudo docker run -it -p 5912:5912 -p 5913:5913 -p 5914:5914 \
  zhaojh329/rttys:latest --addr-http-proxy :5914
```

**端口说明：**
- `5912` - 设备连接端口
- `5913` - 用户 Web 管理端口
- `5914` - HTTP 代理端口

# 使用指南

## 命令行参数详解

### rtty 客户端参数

使用以下命令查看 rtty 客户端的所有支持参数：

```bash
$ rtty --help
Usage: rtty [option]
      -g, --group=string       为设备设置分组（最多 16 个字符，不允许空格）
      -I, --id=string          为设备设置 ID（最多 32 个字符，不允许空格）
      -h, --host=string        服务器主机名或 IP 地址（默认为 localhost）
      -p, --port=number        服务器端口（默认为 5912）
      -d, --description=string 添加设备描述（最多 126 字节）
      -a                       自动重连到服务器
      -i number                设置心跳间隔秒数（默认 30 秒）
      -s                       启用 SSL
      -C, --cacert             用于验证对端的 CA 证书
      -x, --insecure           使用 SSL 时允许不安全的服务器连接
      -c, --cert               要使用的证书文件
      -k, --key                要使用的私钥文件
      -D                       在后台运行
      -t, --token=string       授权令牌
      -f username              跳过二次登录认证。详情参见 man login(1)
      -R                       接收文件
      -S file                  发送文件
      -v, --verbose            详细输出
      -V, --version            显示版本
      --help                   显示帮助信息
```

### rttys 服务端参数

使用以下命令查看 rttys 服务端的所有支持参数：

```bash
$ rttys -h
NAME:
   rttys - rtty 的服务端程序

USAGE:
   rttys [global options]

VERSION:
   RTTYS-VERSION

GLOBAL OPTIONS:
   --log string                      日志文件路径（默认："/var/log/rttys.log"）
   --log-level string                日志级别（debug, info, warn, error）（默认："info"）
   --conf string, -c string          要加载的配置文件
   --addr-dev string                 设备监听地址（默认：":5912"）
   --addr-user string                用户监听地址（默认：":5913"）
   --addr-http-proxy string          HTTP 代理监听地址（默认自动）
   --http-proxy-redir-url string     HTTP 代理重定向 URL
   --http-proxy-redir-domain string  HTTP 代理设置 cookie 的域名
   --token string, -t string         使用的令牌
   --dev-hook-url string             设备连接时调用的 URL
   --user-hook-url string            用户访问 API 时调用的 URL
   --local-auth                      本地访问是否需要认证（默认：true）
   --password string                 Web 管理密码
   --allow-origins                   允许跨域请求的所有来源（默认：false）
   --verbose, -V                     更详细的输出（默认：false）
   --help, -h                        显示帮助
   --version, -v                     打印版本
```

## 快速开始

### 启动服务端

使用默认配置启动 rttys 服务端：

```bash
$ rttys
2025-07-06T22:51:34+08:00 |INFO| Go Version: go1.24.4
2025-07-06T22:51:34+08:00 |INFO| Go OS/Arch: linux/amd64
2025-07-06T22:51:34+08:00 |INFO| Rttys Version: RTTYS-VERSION
2025-07-06T22:51:34+08:00 |INFO| Git Commit: 36d270c
2025-07-06T22:51:34+08:00 |INFO| Build Time: 2025-07-06T21:48:57+0800
2025-07-06T22:51:34+08:00 |INFO| Listen devices on: [::]:5912
2025-07-06T22:51:34+08:00 |INFO| Listen http proxy on: [::]:46308
2025-07-06T22:51:34+08:00 |INFO| Listen users on: [::]:5913
2025-07-06T22:51:37+08:00 |INFO| device 'test' registered, group '' proto 5, heartbeat 30s
```

### 连接客户端

在需要远程访问的设备上运行 rtty 客户端：

```bash
$ sudo rtty -I test
2025/07/06 22:51:37 info rtty[68091]: (main.c:278) rtty version RTTY-VERSION
2025/07/06 22:51:37 info rtty[68091]: (rtty.c:690) connected to server
2025/07/06 22:51:37 info rtty[68091]: (rtty.c:498) register success
```

### 访问 Web 管理界面

在浏览器中访问服务器的 Web 管理面板：
```
http://127.0.0.1:5913
```

现在您可以通过 Web 界面远程访问已连接的设备终端了。

## 启用 TLS/SSL 支持

为了确保通信安全，建议在生产环境中启用 TLS 支持。

### 服务端配置

**1. 准备 SSL 证书**

**2. 启动 rttys**

```bash
rttys --sslcert=/etc/rttys/rttys.crt --sslkey=/etc/rttys/rttys.key
```

### 客户端配置

**使用 SSL 连接：**
```bash
sudo rtty -I test -s
```

**如果使用自签名证书，需要添加 `-x` 参数跳过证书验证：**
```bash
sudo rtty -I test -s -x
```

## 生产环境部署

以下是完整的生产环境部署指南，包括域名、SSL 证书配置等。

### Nginx 配置

请将示例中的域名和证书路径替换为您自己的配置。

**用户访问配置（rttys-user.conf）：**

```nginx
# 用户 Web 管理界面
server {
    listen       443 ssl;
    server_name  rttys.net;

    ssl_certificate      /etc/letsencrypt/live/rttys.net/cert.pem;
    ssl_certificate_key  /etc/letsencrypt/live/rttys.net/privkey.pem;

    # WebSocket 连接支持
    location /connect/ {
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_pass http://127.0.0.1:5913;
        proxy_read_timeout 3600s;
    }

    # 其他 HTTP 请求
    location / {
        proxy_pass http://127.0.0.1:5913;
    }
}

# HTTP 代理子域名
server {
    listen       443 ssl;
    server_name  web.rttys.net;

    ssl_certificate      /etc/letsencrypt/live/web.rttys.net/cert.pem;
    ssl_certificate_key  /etc/letsencrypt/live/web.rttys.net/privkey.pem;

    location / {
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_pass http://127.0.0.1:5914;
        proxy_request_buffering off;
        proxy_read_timeout 3600s;
    }
}
```

**在 Nginx 主配置文件中引入配置：**

```nginx
# 在 http 模块中包含用户访问配置
http {
    # ... 其他配置 ...
    
    include /etc/nginx/rttys-user.conf;
}
```

### rttys 服务配置

创建或编辑 rttys 配置文件 `/etc/rttys/rttys.conf`：

```yaml
# 设备连接监听地址
addr-dev: :5912

# 用户 Web 界面监听地址
addr-user: 127.0.0.1:5913

# HTTP 代理监听地址
addr-http-proxy: 127.0.0.1:5914

# HTTP 代理重定向 URL（用于设备的 Web 界面访问）
http-proxy-redir-url: https://web.rttys.net

# 用于设置 cookie 的域名
http-proxy-redir-domain: rttys.net

# Web 管理界面密码
password: rttys

# 设备监听 SSL/TLS
sslkey: /etc/letsencrypt/live/rttys.net/cert.pem;
cacert: /etc/letsencrypt/live/rttys.net/privkey.pem;
```

### 启动服务

**启动 rttys 服务：**
```bash
systemctl restart rttys
systemctl status rttys  # 检查服务状态
journalctl -u rttys -f  # 查看日志
```

**重新加载 Nginx 配置：**
```bash
nginx -t                # 检查配置语法
systemctl reload nginx  # 重新加载配置
```

### 客户端连接

在需要远程管理的设备上运行客户端：

```bash
sudo rtty -I test -h rttys.net -sx
```

**参数说明：**
- `-I test` - 设备 ID 为 "test"
- `-h rttys.net` - 连接到 rttys.net 服务器
- `-s` - 启用 SSL
- `-x` - 允许不安全的 SSL 连接（如果使用自签名证书）

### 访问设备

配置完成后，您可以使用域名在浏览器中访问服务器的 Web 管理面板：

```
https://rttys.net
```
