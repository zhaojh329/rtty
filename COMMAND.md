# Instructions
API Path

    /cmd/:devid?wait=10

The wait parameter is optional and defaults to 30s, or 0 if you do not care about the execution of the command

Request message format

    {
        "username": "test",
        "password": "test",
        "cmd": "echo",
        "params": ["hello rtty"]
    }

The username, cmd in the message must be provided. The password, params are optional. The params is a JSON array.

If the command is executed finish, the server will return the command execution result in json format.
    
    {
        "code": 0,
        "stdout": "aGVsbG8gcnR0eQo=",
        "stderr": ""
    }

The stdout and stderr in the response are base64 encoded.

If any of the steps fail, the server will return an error message in json format.

    {
        "err": 1002,
        "msg": "device offline"
    }


All error codes are as follows

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