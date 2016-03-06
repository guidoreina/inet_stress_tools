Internet stress tools
=====================

Internet stress tools are a set of stress tools for TCP/UDP (at the moment only a TCP sender). It runs under Linux.

## tcp\_sender
`tcp_sender` can be used to create and handle many concurrent TCP connections divided between a small number of threads.

```
Usage: bin/tcp_sender [OPTIONS] <address>
  <address>: socket address where to connect to. Possible formats:
    <host>:<port>: IPv4/IPv6 address and port.
    Unix socket: Unix socket as absolute path.

  Options:
    --connections <count> (range: 1 - 32768, default: 1).
    --threads <count> (range: 1 - 32, default: 1).
    --receive <count>: number of bytes to receive (range: 0 - 32768, default: 0).
    --loops <count>: number of iterations send/receive (range: 1 - 18446744073709551615, default: 1).
    --client-sends-first | --server-sends-first: who sends data first? (default: --client-sends-first).
    --set-read-write-event | --do-not-set-read-write-event: should both events be set or only one? (default: --set-read-write-event).
    --processors <processor-list> | "even" | "odd".
      <processor-list> ::= <processor>[,<processor>]*
    --file <filename>: file to be sent. --file <filename> can be used multiple times.
      If not specified, a dummy file of 1400 bytes is sent.
```
