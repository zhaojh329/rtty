#!/bin/sh

host=localhost
port=5912
devid="test"
cmd="echo"
params='["hello rtty"]'
username="test"
password="test"

resp=$(curl "http://$host:$port/cmd" -d "{\"devid\":\"$devid\", \"cmd\":\"$cmd\", \"params\": $params, \"username\": \"$username\", \"password\": \"$password\"}" 2>/dev/null)

token=$(echo "$resp" | jq -r '.token')

[ "$token" = "null" ] && {
    echo "$resp"
    exit 1
}


while [ true ]
do
    resp=$(curl "http://$host:$port/cmd?token=$token" 2>/dev/null)
    err=$(echo "$resp" | jq -r '.err')
    [ "$err" = "1005" ] && {
        echo "Pending..."
        sleep 1
        continue
    }

    [ -n "err" ] && {
        msg=$(echo "$resp" | jq -r '.msg')
        echo "err: $err"
        echo "msg: $msg"
        break
    }

    code=$(echo "$resp" | jq -r '.code')
    stdout=$(echo "$resp" | jq -r '.stdout' | base64 -d)
    stderr=$(echo "$resp" | jq -r '.stderr' | base64 -d)

    echo "code: $code"
    echo "stdout: $stdout"
    echo "stderr: $stderr"
    break
done
