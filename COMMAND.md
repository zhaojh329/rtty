# How to operate

You first need to send a command to the server through POST, the message format is as follows.

    {"devid": "test", "username": "test", "password": "test", "cmd": "echo", "params": ["hello rtty"]}

The devid, username, cmd in the message must be provided. Password, params are optional. Params is a JSON array.

Then the server returns a unique token.

    {"token":"7fb8dcfe3fee2129427276b692987338"}

Then use the token as a URL parameter to query the command execution result from the server.
If the command is executed, the server will return the command execution result in json format.
    
    {"code":0,"stdout":"aGVsbG8gcnR0eQo=","stderr":""}

The stdout and stderr in the response are base64 encoded.

If any of the steps fail, the server will return an error message in json format.

    {"err": 1002, "msg":"device offline"}

All error codes are as follows

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