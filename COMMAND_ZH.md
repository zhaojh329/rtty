# 说明
API 路径

    /cmd/:devid?wait=10

wait 参数为可选，默认等待 30s，如果不关心命令执行结果，可设置为 0

请求消息格式

    {
        "username": "test",
        "password": "test",
        "cmd": "echo",
        "params": ["hello rtty"]
    }

其中 username、cmd 必须提供。password、params 为可选项。params 为一个 JSON 数组。

如果命令执行完成，服务器将返回 json 格式的命令执行结果：

    {
        "code": 0,
        "stdout": "aGVsbG8gcnR0eQo=",
        "stderr": ""
    }

响应中的 stdout 和 stderr 是经过 base64 编码的。

如果任何一步操作失败，服务器都将返回 json 格式的错误信息：
    
    {
        "err": 1002,
        "msg": "device offline"
    }


所有错误码如下

    1001    invalid format
    1002    device offline
    1003    timeout
    1       operation not permitted
    2       not found
    3       no mem
    4       sys error
    5       stdout+stderr is too big

# Example
## [Shell](/tools/sendcmd.sh)

## Jquery

```javascript
var data = {username: 'test', password: 'test', cmd: 'echo', params: ['hello rtty']};
$.post('http://your-server:5913/cmd/test', JSON.stringify(data), function(r) {
    if (r.stdout) {
        console.log(window.atob(r.stdout))
    } else {
        console.log(r)
    }
});
```

## Axios

```javascript
var data = {username: 'test', password: 'test', cmd: 'echo', params: ['hello rtty']};
axios.post('http://your-server:5913/cmd/test', data).then(function(r) {
    var resp = r.data;
    if (resp.stdout) {
        console.log(window.atob(resp.stdout))
    } else {
        console.log(resp)
    }
});
```