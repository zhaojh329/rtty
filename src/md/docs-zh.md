
# 安装 C 客户端 rtty

## Linux 发行版

### 1. 安装依赖库

根据您的 Linux 发行版，选择相应的命令安装依赖：

**Ubuntu/Debian**
```bash
sudo apt install -y build-essential cmake pkg-config libev-dev libinih-dev libssl-dev
```

**ArchLinux**
```bash
sudo pacman -S --noconfirm base-devel cmake pkgconf libev inih openssl
```

**CentOS/RHEL**
```bash
sudo yum install -y gcc make cmake pkgconfig libev-devel inih-devel openssl-devel
```

### 2. 下载源代码
下载最新版本的 rtty 源代码：
```bash
wget https://github.com/zhaojh329/rtty/releases/download/vRTTY-VERSION/rtty-RTTY-VERSION.tar.gz
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

### 1. 交叉编译 libev

```bash
git clone https://github.com/enki/libev.git
cd libev
./configure --host=aarch64-linux-gnu
DESTDIR=/tmp/rtty_install make install
```

### 2. 交叉编译 inih

克隆代码
```bash
git clone https://github.com/benhoyt/inih.git
cd inih
```

创建交叉编译配置文件 aarch64-linux-gnu.ini
```ini
[binaries]
c = 'aarch64-linux-gnu-gcc'
ar = 'aarch64-linux-gnu-ar'
strip = 'aarch64-linux-gnu-strip'

[host_machine]
system = 'linux'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'

[properties]
needs_exe_wrapper = true
```

编译
```bash
meson setup build --cross-file aarch64-linux-gnu.ini -Dwith_INIReader=false
DESTDIR=/tmp/rtty_install meson install -C build
```

### 2. 交叉编译 rtty

克隆代码
```bash
wget https://github.com/zhaojh329/rtty/releases/download/vRTTY-VERSION/rtty-RTTY-VERSION.tar.gz
tar xvf rtty-RTTY-VERSION.tar.gz
cd rtty-RTTY-VERSION
```

编译
```bash
PKG_CONFIG_PATH= \
    PKG_CONFIG_LIBDIR=/tmp/rtty_install/usr/local/lib/pkgconfig \
    PKG_CONFIG_SYSROOT_DIR=/tmp/rtty_install \
    cmake -S . -B build \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_FIND_ROOT_PATH=/tmp/rtty_install \
    -DCMAKE_EXE_LINKER_FLAGS="-L/tmp/rtty_install/usr/local/lib" \
    -DSSL_SUPPORT=OFF
cmake --build build
DESTDIR=/tmp/rtty_install cmake --install build
```

### 3. 部署到目标设备
将编译好的文件拷贝到设备的对应目录：
```bash
/tmp/rtty_install/
└── usr
    └── local
        ├── bin
        │   └── rtty
        ├── lib
        │   ├── libev.so -> libev.so.4.0.0
        │   ├── libev.so.4 -> libev.so.4.0.0
        │   ├── libev.so.4.0.0
        │   ├── libinih.so -> libinih.so.0
        │   ├── libinih.so.0
```

# 安装 Go 客户端 rtty-go

在仓库根目录构建，Go 版本以仓库 `go.mod` 为准。生成的程序仍名为 `rtty`，与 C 客户端二选一安装。

```bash
git clone https://github.com/zhaojh329/rtty-go.git
cd rtty-go
go install ./cmd/rtty
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
./scripts/build.sh linux amd64
```

### 5. 编译结果
编译完成后，会在当前目录生成以下文件：
```bash
rttys-RTTYS-VERSION-linux-amd64/
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

### C 客户端参数

使用以下命令查看 rtty 客户端的所有支持参数：

```bash
$ rtty --help
Usage: rtty [option]
      --conf=file              从 INI 配置文件加载选项
      -g, --group=string       为设备设置分组（最多 16 个字符，不允许空格）
      -I, --id=string          为设备设置 ID（最多 32 个字符，不允许空格）
      -h, --host=string        服务器主机名或 IP 地址（默认为 localhost）
      -p, --port=number        服务器端口（默认为 5912）
      -d, --description=string 添加设备描述（最多 126 字节）
      -a                       自动重连到服务器
      -i number                设置心跳间隔秒数（默认 30 秒）
      --http-timeout=number    HTTP 空闲超时秒数（默认 30 秒，范围 5–255）
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
   --log-level string                日志级别（debug, info, warn, error）（默认："info"）
   --conf string, -c string          要加载的配置文件
   --addr-dev string                 设备监听地址（默认：":5912"）
   --addr-user string                用户监听地址（默认：":5913"）
   --addr-http-proxy string          HTTP 代理监听地址（默认自动）
   --share-bind-host string         临时分享监听地址（默认所有接口）
   --share-public-host string       分享连接信息中的公网主机名
   --share-port-start int           分享端口范围起点（默认 20000）
   --share-port-end int             分享端口范围终点（默认 21000）
   --share-host-key string          分享所用的 SSH 主机密钥路径
   --http-proxy-redir-url string     HTTP 代理重定向 URL
   --http-proxy-redir-domain string  HTTP 代理设置 cookie 的域名
   --token string, -t string         使用的令牌
   --dev-hook-url string             设备连接时调用的 URL
   --user-hook-url string            用户访问 API 时调用的 URL
   --local-auth                      本地访问是否需要认证（默认：true）
   --password string                 Web 管理密码
   --allow-origins                   允许跨域请求的所有来源（默认：false）
   --sslcert string                 设备连接的 TLS 证书
   --sslkey string                  设备连接的 TLS 私钥
   --cacert string                  验证设备证书的 CA（mTLS）
   --pprof string                   启用 pprof 并监听指定地址
   --verbose, -V                     更详细的输出（默认：false）
   --help, -h                        显示帮助
   --version, -v                     打印版本
```

## 快速开始

### 启动服务端

使用默认配置启动 rttys 服务端：

```bash
rttys
```

### 连接客户端

在需要远程访问的设备上运行 rtty 客户端：

```bash
sudo rtty -I test
```

### 访问 Web 管理界面

在浏览器中访问服务器的 Web 管理面板：
```
http://127.0.0.1:5913
```

现在您可以通过 Web 界面远程访问已连接的设备终端了。

## C 客户端配置与权限

通过 `rtty --conf /etc/rtty/rtty.ini` 显式加载 INI 文件。配置优先级依次为：默认值、配置文件、命令行参数，后者覆盖前者。

```ini
[rtty]
id = test
host = 127.0.0.1
port = 5912
reconnect = true
heartbeat = 30
http-timeout = 30

[ssl]
enabled = false
```

使用 TLS 时，`[ssl]` 中还可设置 `cacert`、`cert`、`key` 和 `insecure`。

## 文件传输

在通过 Web 终端打开的设备 Shell 中执行：

```bash
# 从浏览器接收文件到设备当前目录
rtty -R
# 将设备文件发送到浏览器
rtty -S /path/to/file
```

## 串口终端

在 Web 界面中打开设备的串口操作入口，选择串口，并设置波特率、数据位、校验和停止位后连接。

客户端进程必须有打开串口的权限，且串口处于可用状态。串口会话不会启动设备 Shell，也不支持上述 Shell 文件传输命令。

## 临时 SSH 与 TCP 分享

在设备的分享入口选择**终端**、**串口**或 **TCP**，再选择对外端口和空闲超时。端口填 `0` 时，服务端从配置范围内选择可用端口。

- **终端 / 串口**：使用创建后显示的临时密码，通过 `ssh -p PORT share@HOST` 连接。串口分享需要先选择串口参数。请在密码显示时保存，分享列表不会再次返回密码。终端分享仍会打开设备的登录会话。
- **TCP**：填写设备能够访问的目标 IPv4 地址及端口，然后连接返回的服务端地址和端口。这是原始 TCP 转发，对外 TCP 监听不额外提供分享密码或加密，认证和加密由目标服务负责。

空闲超时范围为 1–60 分钟，默认 1 分钟。这里的空闲指没有活动连接，而非没有键盘输入或网络流量；存在活动连接时不会因该计时器回收。在分享列表中结束分享会立即断开其用户连接。设备断线或服务端重启也会结束分享。

在 `/etc/rttys/rttys.conf` 中配置分享监听：

```yaml
share-bind-host: 0.0.0.0
share-public-host: access.example.com
share-port-start: 20000
share-port-end: 21000
share-host-key: /var/lib/rttys/ssh_host_ed25519_key
```

首次创建主机密钥时，服务进程需要对密钥保存位置有写权限；重启后应保留同一密钥。服务端防火墙和 NAT 需允许访问配置的分享端口；这些是独立 TCP 监听，不是下方 Nginx 配置代理的 HTTP 路由。

使用 Docker 时，还需映射相同的 TCP 端口范围、设置对外主机名，并持久化 SSH 主机密钥，例如：

```bash
sudo docker run -it -p 5912:5912 -p 5913:5913 -p 5914:5914 \
  -p 20000-21000:20000-21000/tcp -v rttys-data:/var/lib/rttys \
  zhaojh329/rttys:latest --addr-http-proxy :5914 \
  --share-public-host access.example.com \
  --share-port-start 20000 --share-port-end 21000 \
  --share-host-key /var/lib/rttys/ssh_host_ed25519_key
```

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

**使用私有 CA 或自签名证书时，通过 `--cacert` 指定信任的证书，并确保连接主机名与证书匹配：**
```bash
sudo rtty -I test -h device-server.example.com -s --cacert /etc/rtty/ca.pem
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

    ssl_certificate      /etc/letsencrypt/live/rttys.net/fullchain.pem;
    ssl_certificate_key  /etc/letsencrypt/live/rttys.net/privkey.pem;

    # WebSocket 连接支持
    location /api/connect/ {
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

    ssl_certificate      /etc/letsencrypt/live/web.rttys.net/fullchain.pem;
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
sslcert: /etc/letsencrypt/live/rttys.net/fullchain.pem
sslkey: /etc/letsencrypt/live/rttys.net/privkey.pem
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
sudo rtty -I test -h rttys.net -s
```

**参数说明：**
- `-I test` - 设备 ID 为 "test"
- `-h rttys.net` - 连接到 rttys.net 服务器
- `-s` - 启用 SSL

### 访问设备

配置完成后，您可以使用域名在浏览器中访问服务器的 Web 管理面板：

```
https://rttys.net
```
