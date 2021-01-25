#!/bin/sh

# Your server's host and port
host=localhost
port=5913

# Your linux device's username and password
username="test"
password="test"

# Default wait 30s.
# If you don't care about the results, you can set it to 0, that is, don't wait
#wait=0

devid="test"
cmd="echo"
params='["Hello, Rtty"]'


resp=$(curl "http://$host:$port/cmd/$devid?wait=$wait" -d "{\"cmd\":\"$cmd\", \"params\": $params, \"username\": \"$username\", \"password\": \"$password\"}" 2>/dev/null)

[ "$wait" = "0" ] && exit 0

err=$(echo "$resp" | jq -r '.err')

if [ "$err" != "null" ];
then
    msg=$(echo "$resp" | jq -r '.msg')
    echo "err: $err"
    echo "msg: $msg"
else
    code=$(echo "$resp" | jq -r '.code')
    stdout=$(echo "$resp" | jq -r '.stdout' | base64 -d)
    stderr=$(echo "$resp" | jq -r '.stderr' | base64 -d)

    echo "code: $code"
    echo "stdout: $stdout"
    echo "stderr: $stderr"
fi
