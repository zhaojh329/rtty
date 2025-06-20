# rtty-site

This template should help get you started developing with Vue 3 in Vite.

## Recommended IDE Setup

[VSCode](https://code.visualstudio.com/) + [Volar](https://marketplace.visualstudio.com/items?itemName=Vue.volar) (and disable Vetur).

## Customize configuration

See [Vite Configuration Reference](https://vite.dev/config/).

## Project Setup

```sh
npm install
```

### Compile and Hot-Reload for Development

```sh
npm run dev
```

### Compile and Minify for Production

```sh
npm run build
```

### Lint with [ESLint](https://eslint.org/)

```sh
npm run lint
```

## OpenResty

/etc/openresty/nginx.conf

```nginx
http {
    ...
    include /etc/openresty/rtty-site.conf;
}
```

/etc/openresty/rtty-site.conf

```nginx
server {
    listen       443 ssl;
    server_name  rttys.net;

    ssl_certificate      /etc/openresty/ssl/rttys.net/cert.pem;
    ssl_certificate_key  /etc/openresty/ssl/rttys.net/cert.key;

    location = /rtty-version.json {
        root assets;
    }

    location /dl/ {
        root assets;
    }

    location / {
        root html/rtty-site;
    }
}
```

/usr/local/openresty/nginx/assets/rtty-version.json

```json
{"rtty":"9.0.0","rttys":"5.1.0"}
```

/usr/local/openresty/nginx/html/rtty-site/

/usr/local/openresty/nginx/assets/dl/
