# 如何操作

首先要通过POST方式向服务器发送一条命令,消息格式如下所示：

    {"devid": "test", "username": "test", "password": "test", "cmd": "echo", "params": ["hello rtty"]}

其中devid、username、cmd必须提供。password、params为可选项。params为一个JSON数组。

然后服务器返回一个唯一的token：

    {"token":"7fb8dcfe3fee2129427276b692987338"}

然后以该token作为参数向服务器查询命令执行结果。如果命令被执行，服务器将返回json格式的命令执行结果：

    {"code":0,"stdout":"aGVsbG8gcnR0eQo=","stderr":""}

响应中的stdout和stderr是经过base64编码的。

如果任何一步操作失败，服务器都将返回json格式的错误信息：
    
    {"err": 1002, "msg":"device offline"}


所有错误码如下

    1001    invalid format
    1002    device offline
    1003    server is busy
    1004    timeout
    1005    pending
    1006    invalid token
    1       operation not permitted
    2       not found
    3       no mem
    4       sys error
    5       stdout+stderr is too big

# Example
## [Shell](/tools/sendcmd.sh)

## Jquery

    function queryCmdResp(token) {
        $.getJSON('https://your-server:5912/cmd?token=' + token, function(r) {
            if (r.stdout) {
                console.log(window.atob(r.stdout))
            } else {
                console.log(r)
            }
        });
    }

    var data = {devid: 'test', username: 'test', password: 'test', cmd: 'echo', params: ['hello rtty']};
    $.post('http://your-server:5912/cmd', JSON.stringify(data), function(r) {
        if (!r.token) {
            console.log(r)
            return;
        }
        
        setTimeout(function () {
            queryCmdResp(r.token);
        }, 100);
    });

## Axios

    function queryCmdResp(token) {
        axios.get('http://your-server:5912/cmd?token=' + token).then(function(r) {
            var resp = r.data;
            if (resp.stdout) {
                console.log(window.atob(resp.stdout))
            } else {
                console.log(resp)
            }
        });
    }

    var data = {devid: 'test', username: 'test', password: 'test', cmd: 'echo', params: ['hello rtty']};
    axios.post('http://your-server:5912/cmd', data).then(function(r) {
        var resp = r.data;
        if (!resp.token) {
            console.log(r)
            return;
        }
        
        setTimeout(function () {
            queryCmdResp(resp.token);
        }, 100);
    });