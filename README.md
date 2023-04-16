# Intro
All kinds of embedded linux device control utils

# Dependency
```
$ libev libusb libreadline
```

# Usage
**example1: io**
```Bash
[root@RK3328:/]# devctl -c /etc/devctl.conf  -q -m cmd -r "io"
Raw memory i/o utility - $Revision: 1.5 $

io -v -1|2|4 -r|w [-l <len>] [-f <file>] <addr> [<value>]

    -v         Verbose, asks for confirmation
    -1|2|4     Sets memory access size in bytes (default byte)
    -l <len>   Length in bytes of area to access (defaults to
               one access, or whole file length)
    -r|w       Read from or Write to memory (default read)
    -f <file>  File to write on memory read, or
               to read on memory write
    <addr>     The memory address to access
    <val>      The value to write (implies -w)

Examples:
    io 0x1000                  Reads one byte from 0x1000
    io 0x1000 0x12             Writes 0x12 to location 0x1000
    io -2 -l 8 0x1000          Reads 8 words from 0x1000
    io -r -f dmp -l 100 200    Reads 100 bytes from addr 200 to file
    io -w -f img 0x10000       Writes the whole of file to memory

Note access size (-1|2|4) does not apply to file based accesses.

[root@RK3328:/]# devctl -q -m cmd -r "io -4 -r 0xFF190000"
ff190000:  00000000
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

