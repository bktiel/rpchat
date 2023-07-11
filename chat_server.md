# Basic Chat Server

## Basic Protocol

```
string = len:u16 | contents:char[]

register || 1:u8 | username:string
send     || 2:u8 | message:string
deliver  || 3:u8 | from:string | message:string
status   || 4:u8 | code:u8 | message:string
```

Upon connecting to a server, the client *must* send a `register` message.  The server must either respond with "ok" (status code of 0) or a server defined error message, followed by disconnecting.  When accepting a `register` the server `deliver`s a message to all clients already registered saying that user has connected (message is from "server").  The server should also send a message to the client who just registered listing all other users currently registered.

Once registered, the client can `send` a message that the server will `deliver` to every _other_ client currently `register`ed.  Clients will acknowledge `deliver` messages with a `status` message of "ok" (status code of 0).  Servers will acknowledge `send` messages with either an "ok" or an appropriate server defined message.  Clients and servers _must_ wait for a `status` message after each `send`/`deliver` before sending the next message.

### Clarifications

1. A registered `username` must be unique among all currently active connections.
2. The server must _not_ deliver a message to the original sender.
3. Upon disconnect, the server must "unregister" the client and notify other clients that this user has left the chat.
4. The server _should_ identify unresponsive clients and forcefully disconnect them.  The criteria for "unresponsive" is up to the implementation, but _must_ be documented.

## Server

`bcs_server -p <port> [-l <log_file>]`

The server listens and serves the protocol on the port given.  It logs successful registrations, messages, and disconnects to `log_file` (or `stdout`, if not given).  It _may_ use stderr for server-defined error messages, but it must _not_ log those messages to `log_file`.

## Client

`bcs_client.py -h <host> -p <port> -u <username>`

The client attempts to connect to and register with the BCS server at `host:port` using username.  If it receives a non-OK status message from the server, it must exit with that status code.  Once registered, the client should print any `deliver` messages it receives to `stdout` as well as any `send` messages it sends.  It is up to the client implementation to decide the specific interface.

## Extended Protocol

The following four messages are added:

```
sendfile | 5:u8 | filename:string | flen:u32 | contents:u8[]
fnotify  | 6:u8 | user:string     | file:string
getfile  | 7:u8 | filename:string
recvfile | 8:u8 | flen:u32 | contents:u8[]
```

Clients may now use `sendfile` to upload a file to the server and the server replies with an appropriate `status` message.  On success, the server sends an `fnotify` message to all other clients currently registered.

Clients can download the message by sending a `getfile` request to the server which replies with either the file contents (in a `recvfile` message) or an appropriate status message.

### Server

The server should take an additional `-f <file_dir>` argument and _must_ ensure all uploaded files end up underneath this directory (i.e., no directory traversal vulnerabilities).

### Client

The client implementation determines the interface for sending and recieving files, but must print `fnotify` messages as if they had been `deliver` messages from the server with the contents "<user> has uploaded <filename> to ther server".