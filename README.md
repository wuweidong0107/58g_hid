# Intro
All kinds of embedded linux device control utils

# Usage
example:
```
# 命令模式
$ ./devctl -c ./conf.d/devctl.conf -m 0 -r io

# 交互模式
$ ./devctl -c ./conf.d/devctl.conf -m 1

# 服务模式
$ ./devctl -c ./conf.d/devctl.conf -m 2
```

访问 server:
```
# http server
http://localhost:8000/

# websocket server
ws://localhost:8000/websocket
```

# Reference
https://www.usb.org/sites/default/files/hid1_11.pdf

