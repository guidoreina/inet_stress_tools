Internet stress tools
=====================

Internet stress tools are a set of stress tools for TCP/UDP (at the moment a TCP sender, a TCP receiver and a UDP sender). It runs under Linux.

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
    --thread-loops <count>:
      Number of iterations per thread
      (range: 1 - 18446744073709551615, default: 1).
    --connection-loops <count>:
      Number of iterations send/receive per connection
      (range: 1 - 18446744073709551615, default: 1).
    --client-sends-first | --server-sends-first:
      Who sends data first? (default: --client-sends-first).
    --set-read-write-event | --do-not-set-read-write-event:
      Should both events be set or only one? (default: --set-read-write-event).
    --processors <processor-list> | "even" | "odd".
      <processor-list> ::= <processor>[,<processor>]*
    --file <filename>: file to be sent. --file <filename> can be used multiple times.
      If not specified, a dummy file of 1400 bytes is sent.
```


## tcp\_receiver
`tcp_receiver` can be used to handle many concurrent TCP connections divided between a small number of threads.

```
Usage: bin/tcp_receiver [OPTIONS] <address>
	<address>: socket address where to bind to. Possible formats:
		<host>:<port>: IPv4/IPv6 address and port.
		Unix socket: Unix socket as absolute path.

	Options:
		--threads <count> (range: 1 - 32, default: 1).
		--receive <count>: number of bytes to receive (range: 1 - 32768, default: 1400).
		--connection-loops <count>:
			Number of iterations send/receive per connection
			(range: 1 - 18446744073709551615, default: 1).
		--client-sends-first | --server-sends-first:
			Who sends data first? (default: --client-sends-first).
		--set-read-write-event | --do-not-set-read-write-event:
			Should both events be set or only one? (default: --set-read-write-event).
		--single-listener | --one-listener-per-thread:
			Same listener for all the threads or a listener per thread?
			(default: --one-listener-per-thread).
		--single-epoll-fd | --one-epoll-fd-per-thread:
			Same epoll fd for all the threads or a epoll fd per thread?
			(default: --one-epoll-fd-per-thread).
		--processors <processor-list> | "even" | "odd".
			<processor-list> ::= <processor>[,<processor>]*
		--file <filename>: file to be sent. --file <filename> can be used multiple times.
```

## udp\_sender
`udp_sender` can be used to send many UDP messages divided between a small number of threads.

```
Usage: bin/udp_sender [OPTIONS] <address>
  <address>: socket address where to send messages to. Possible formats:
    <host>:<port>: IPv4/IPv6 address and port.
    Unix socket: Unix socket as absolute path.

  Options:
    --messages-per-send <count> (range: 1 - 4294967295, default: 1).
    --sends <count> (range: 1 - 18446744073709551615, default: 1).
    --delay <microseconds> (range: 0 - 1000000, default: 0).
    --threads <count> (range: 1 - 32, default: 1).
    --processors <processor-list> | "even" | "odd".
      <processor-list> ::= <processor>[,<processor>]*
    --file <filename>: file to be sent. --file <filename> can be used multiple times.
      If not specified, a dummy file of 1400 bytes is sent.
```
