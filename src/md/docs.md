# Install the C Client rtty

## Linux Distributions

### 1. Install Dependencies

Choose the appropriate command for your Linux distribution to install dependencies:

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

### 2. Download Source Code
Download the latest version of the rtty source code:
```bash
wget https://github.com/zhaojh329/rtty/releases/download/vRTTY-VERSION/rtty-RTTY-VERSION.tar.gz
```

### 3. Extract Source Code
```bash
tar xvf rtty-RTTY-VERSION.tar.gz
```

### 4. Compile and Install
```bash
cd rtty-RTTY-VERSION
mkdir build
cd build
cmake ..
make
sudo make install
```

## OpenWrt

### Method 1: Install Package Directly

OpenWrt provides 4 different rtty packages. Please choose according to your needs:

- **rtty-nossl** - Version without SSL support
- **rtty-openssl** - Uses OpenSSL as the SSL backend
- **rtty-mbedtls** - Uses mbedTLS as the SSL backend (Recommended)
- **rtty-wolfssl** - Uses wolfSSL as the SSL backend

**Installation Example:**
```bash
opkg update
opkg install rtty-mbedtls
```

### Method 2: Compile from Source

If you need to customize the compilation, follow these steps:

**1. Install feed**
```bash
./scripts/feeds update packages
./scripts/feeds install rtty
```

**2. Configure menuconfig**

Select the corresponding rtty version in menuconfig, then recompile the firmware:

```
Utilities  --->
  Terminal  --->
    <*> rtty-mbedtls................. Access your terminals from anywhere via the web
    < > rtty-nossl................... Access your terminals from anywhere via the web
    < > rtty-openssl................. Access your terminals from anywhere via the web
    < > rtty-wolfssl................. Access your terminals from anywhere via the web
```

## Other Embedded Linux Systems

For other embedded Linux systems, you need to cross-compile. Replace the cross-compilation toolchain in the following examples with the toolchain for your target platform.

### 1. Cross-compile libev

```bash
git clone https://github.com/enki/libev.git
cd libev
./configure --host=aarch64-linux-gnu
DESTDIR=/tmp/rtty_install make install
```

### 2. Cross-compile inih

Download the source code:
```bash
git clone https://github.com/benhoyt/inih.git
cd inih
```

Create the cross-compilation configuration file `aarch64-linux-gnu.ini`:
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

Build and install:
```bash
meson setup build --cross-file aarch64-linux-gnu.ini -Dwith_INIReader=false
DESTDIR=/tmp/rtty_install meson install -C build
```

### 2. Cross-compile rtty

Download the source code:
```bash
wget https://github.com/zhaojh329/rtty/releases/download/vRTTY-VERSION/rtty-RTTY-VERSION.tar.gz
tar xvf rtty-RTTY-VERSION.tar.gz
cd rtty-RTTY-VERSION
```

Build and install:
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

### 3. Deploy to the Target Device
Copy the compiled files to the corresponding directories on the device:
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

# Install the Go Client rtty-go

Build from the repository root using the Go version required by `go.mod`. The binary is also named `rtty`; install either the C or Go implementation.

```bash
git clone https://github.com/zhaojh329/rtty-go.git
cd rtty-go
go install ./cmd/rtty
```

# Install rttys Server

## Method 1: Use deb Package (Recommended)

### 1. Download

Choose the corresponding version for your system architecture:

**Linux x86_64：**
```bash
wget https://github.com/zhaojh329/rttys/releases/download/vRTTYS-VERSION/rttys_RTTYS-VERSION_amd64.deb
```

**Linux ARM64：**
```bash
wget https://github.com/zhaojh329/rttys/releases/download/vRTTYS-VERSION/rttys_RTTYS-VERSION_arm64.deb
```

### 2. Install using the dpkg command

```bash
sudo dpkg -i rttys_RTTYS-VERSION_amd64.deb
```

## Method 2: Use Pre-compiled Package

### 1. Download

Choose the corresponding version for your system architecture:

**Linux x86_64:**
```bash
wget https://github.com/zhaojh329/rttys/releases/download/vRTTYS-VERSION/rttys-RTTYS-VERSION-linux-amd64.tar.bz2
```

**Linux ARM64:**
```bash
wget https://github.com/zhaojh329/rttys/releases/download/vRTTYS-VERSION/rttys-RTTYS-VERSION-linux-arm64.tar.bz2
```

**Windows x86_64：**
```bash
wget https://github.com/zhaojh329/rttys/releases/download/vRTTYS-VERSION/rttys-RTTYS-VERSION-windows-amd64.tar.bz2
```

### 2. Extract the Package
```bash
tar xvf rttys-RTTYS-VERSION-linux-amd64.tar.bz2
```

### 3. Install Program and Configuration File
```bash
sudo cp rttys-RTTYS-VERSION-linux-amd64/rttys /usr/local/bin/
sudo mkdir -p /etc/rttys
sudo cp rttys-RTTYS-VERSION-linux-amd64/rttys.conf /etc/rttys/
```

### 4. Register System Service
```bash
sudo cp rttys-RTTYS-VERSION-linux-amd64/rttys.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable rttys
```

## Method 3: Compile from Source

If you need to customize functions or compile for a specific platform, you can choose to compile from source.

### 1. Download Source Code
```bash
wget https://github.com/zhaojh329/rttys/archive/refs/tags/vRTTYS-VERSION.tar.gz -O rttys-RTTYS-VERSION.tar.gz
```

### 2. Extract Source Code
```bash
tar xvf rttys-RTTYS-VERSION.tar.gz
```

### 3. Compile Frontend Interface
```bash
cd rttys-RTTYS-VERSION/ui
npm install
npm run build
```

### 4. Compile Server Program
```bash
cd ../
./scripts/build.sh linux amd64
```

### 5. Compilation Result
After compilation, the following files will be generated in the current directory:
```bash
rttys-RTTYS-VERSION-linux-amd64/
├── rttys          # Server executable
├── rttys.conf     # Configuration file template
└── rttys.service  # systemd service file
```

## Method 4: Docker Deployment

You can quickly deploy the rttys service using Docker:

```bash
sudo docker run -it -p 5912:5912 -p 5913:5913 -p 5914:5914 \
  zhaojh329/rttys:latest --addr-http-proxy :5914
```

**Port Description:**
- `5912` - Device connection port
- `5913` - User Web management port
- `5914` - HTTP proxy port

# Usage Guide

## Command-line Parameter Details

### C Client Parameters

Use the following command to view all supported parameters for the rtty client:

```bash
$ rtty --help
Usage: rtty [option]
      --conf=file              Load options from an INI configuration file
      -g, --group=string       Set a group for the device(max 16 chars, no spaces allowed)
      -I, --id=string          Set an ID for the device(max 32 chars, no spaces allowed)
      -h, --host=string        Server's host or ipaddr(Default is localhost)
      -p, --port=number        Server port(Default is 5912)
      -d, --description=string Add a description to the device(Maximum 126 bytes)
      -a                       Auto reconnect to the server
      -i number                Set heartbeat interval in seconds(Default is 30s)
      --http-timeout=number    HTTP idle timeout in seconds (default 30, range 5–255)
      -s                       SSL on
      -C, --cacert             CA certificate to verify peer against
      -x, --insecure           Allow insecure server connections when using SSL
      -c, --cert               Certificate file to use
      -k, --key                Private key file to use
      -D                       Run in the background
      -t, --token=string       Authorization token
      -f username              Skip a second login authentication. See man login(1) about the details
      -R                       Receive file
      -S file                  Send file
      -v, --verbose            verbose
      -V, --version            Show version
      --help                   Show usage
```

### rttys Server Parameters

Use the following command to view all supported parameters for the rttys server:

```bash
$ rttys -h
NAME:
   rttys - The server side for rtty

USAGE:
   rttys [global options]

VERSION:
   RTTYS-VERSION

GLOBAL OPTIONS:
   --log-level string                log level(debug, info, warn, error) (default: "info")
   --conf string, -c string          config file to load
   --addr-dev string                 address to listen device (default: ":5912")
   --addr-user string                address to listen user (default: ":5913")
   --addr-http-proxy string          address to listen for HTTP proxy (default auto)
   --share-bind-host string         temporary share bind address (default all interfaces)
   --share-public-host string       public hostname for share connection details
   --share-port-start int           first temporary share port (default 20000)
   --share-port-end int             last temporary share port (default 21000)
   --share-host-key string          SSH host key path for shares
   --http-proxy-redir-url string     url to redirect for HTTP proxy
   --http-proxy-redir-domain string  domain for HTTP proxy set cookie
   --token string, -t string         token to use
   --dev-hook-url string             called when the device is connected
   --user-hook-url string            called when the user accesses APIs
   --local-auth                      need auth for local (default: true)
   --password string                 web management password
   --allow-origins                   allow all origins for cross-domain request (default: false)
   --sslcert string                 TLS certificate for device connections
   --sslkey string                  TLS private key for device connections
   --cacert string                  CA for verifying device certificates (mTLS)
   --pprof string                   enable pprof on the specified address
   --verbose, -V                     more detailed output (default: false)
   --help, -h                        show help
   --version, -v                     print the version
```

## Quick Start

### Start the Server

Start the rttys server with the default configuration:

```bash
rttys
```

### Connect the Client

Run the rtty client on the device you want to access remotely:

```bash
sudo rtty -I test
```

### Access the Web Management Interface

Access the server's web management panel in your browser:
```
http://127.0.0.1:5913
```

Now you can remotely access the connected device's terminal through the web interface.

## C Client Configuration and Permissions

Load an INI file explicitly with `rtty --conf /etc/rtty/rtty.ini`. Values are applied in this order: defaults, configuration file, command-line options.

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

For TLS connections, the `[ssl]` section also accepts `cacert`, `cert`, `key` and `insecure`.

## File Transfer

Run these commands inside a device shell opened through the web terminal:

```bash
# Receive a file from the browser into the current device directory
rtty -R
# Send a device file to the browser
rtty -S /path/to/file
```

## Serial Console

Use the device's serial action in the web interface, select a port, then set baud rate, data bits, parity and stop bits before connecting.

The client must have permission to open the port, and the port must be available. Serial sessions do not run a device shell and do not support the shell file-transfer commands above.

## Temporary SSH and TCP Shares

Open the device's share action, choose **Terminal**, **Serial** or **TCP**, then select the public port and idle timeout. Port `0` selects an available port from the server's configured range.

- **Terminal / Serial:** connect with `ssh -p PORT share@HOST` and the temporary password shown after creation. For serial shares, select the serial settings first. Save the password when it is shown; the share list does not return it. A terminal share still opens the device's login session.
- **TCP:** enter a target IPv4 address and port reachable from the device, then connect to the returned server address and port. This is raw TCP forwarding, with no additional share password or encryption on the public TCP listener; authentication and encryption depend on the target service.

Idle timeout is 1–60 minutes (default 1 minute). It counts time with no active connections, not time without keyboard input or traffic; active connections keep the share open. End a share from the shares list to disconnect its users immediately. Device disconnection or server restart also ends shares.

Configure the server's share listeners in `/etc/rttys/rttys.conf`:

```yaml
share-bind-host: 0.0.0.0
share-public-host: access.example.com
share-port-start: 20000
share-port-end: 21000
share-host-key: /var/lib/rttys/ssh_host_ed25519_key
```

The service needs write access to the host-key location when creating the key. Preserve the key across restarts. Make the configured share ports reachable through the server firewall/NAT; these are independent TCP listeners, not HTTP routes handled by the Nginx configuration below.

When using Docker, publish the same TCP port range, set the public hostname, and persist the SSH host key, for example:

```bash
sudo docker run -it -p 5912:5912 -p 5913:5913 -p 5914:5914 \
  -p 20000-21000:20000-21000/tcp -v rttys-data:/var/lib/rttys \
  zhaojh329/rttys:latest --addr-http-proxy :5914 \
  --share-public-host access.example.com \
  --share-port-start 20000 --share-port-end 21000 \
  --share-host-key /var/lib/rttys/ssh_host_ed25519_key
```

## Enable TLS/SSL Support

To ensure secure communication, it is recommended to enable TLS support in a production environment.

### Server Configuration

**1. Prepare SSL certificate**

**2. Start the rttys Service**

```bash
rttys --sslcert=/etc/rttys/rttys.crt --sslkey=/etc/rttys/rttys.key
```

### Client Configuration

**Use SSL connection:**
```bash
sudo rtty -I test -s
```

**For a private CA or self-signed certificate, specify the trusted certificate with `--cacert` and connect using a hostname matching the certificate:**
```bash
sudo rtty -I test -h device-server.example.com -s --cacert /etc/rtty/ca.pem
```

## Production Environment Deployment

The following is a complete guide for production environment deployment, including domain names, SSL certificate configuration, etc.

### Nginx Configuration

Please replace the domain names and certificate paths in the example with your own configuration.

**User Access Configuration (rttys-user.conf):**

```nginx
# User Web Management Interface
server {
    listen       443 ssl;
    server_name  rttys.net;

    ssl_certificate      /etc/letsencrypt/live/rttys.net/fullchain.pem;
    ssl_certificate_key  /etc/letsencrypt/live/rttys.net/privkey.pem;

    # WebSocket connection support
    location /api/connect/ {
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_pass http://127.0.0.1:5913;
        proxy_read_timeout 3600s;
    }

    # Other HTTP requests
    location / {
        proxy_pass http://127.0.0.1:5913;
    }
}

# HTTP proxy subdomain
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

**Include configurations in the main Nginx configuration file:**

```nginx
# Include user access configuration in the http module
http {
    # ... other configurations ...
    
    include /etc/nginx/rttys-user.conf;
}
```

### rttys Service Configuration

Create or edit the rttys configuration file `/etc/rttys/rttys.conf`:

```yaml
# Address to listen for device connections
addr-dev: :5912

# Address to listen for the user Web interface
addr-user: 127.0.0.1:5913

# Address to listen for the HTTP proxy
addr-http-proxy: 127.0.0.1:5914

# Redirect URL for the HTTP proxy (for device Web interface access)
http-proxy-redir-url: https://web.rttys.net

# Domain for setting cookies
http-proxy-redir-domain: rttys.net

# Web management interface password
password: rttys

# SSL/TLS for listen device
sslcert: /etc/letsencrypt/live/rttys.net/fullchain.pem
sslkey: /etc/letsencrypt/live/rttys.net/privkey.pem
```

### Start Services

**Start the rttys service:**
```bash
systemctl restart rttys
systemctl status rttys  # Check service status
journalctl -u rttys -f  # View logs
```

**Reload Nginx configuration:**
```bash
nginx -t                # Check configuration syntax
systemctl reload nginx  # Reload configuration
```

### Client Connection

Run the client on the device you want to manage remotely:

```bash
sudo rtty -I test -h rttys.net -s
```

**Parameter Description:**
- `-I test` - Set device ID to "test"
- `-h rttys.net` - Connect to the rttys.net server
- `-s` - Enable SSL

### Accessing the Device

After configuration, you can access it through:

```
https://rttys.net
```
