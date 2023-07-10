# rpchat

Implementation of Basic Chat Protocol using C and Python.

## Usage

### Server Execution

Build the project, then run `rpchat` from the `/bin` directory.

`./rpchat -l[log_location] -p[port number]`

#### Arguments

* `-l` Where output from the server should be stored. Defaults to `stdout`, but can be a file of your choice.
* `-p` What port to run the server on. Defaults to 9001, but any unprivileged port will work as long as selection
  reflected on client.

### Client Execution

Enter the `/scripts` folder from the base directory and run `client.py` with python.

`python3 client.py -u [username] -s[server ip] -p[port number]`

#### Arguments

* `-u` Username to use on the server. This can be up to 4095 characters long, with no spaces or nonprintable characters.
* `-s` IP Address of the server to connect to. Defaults to `127.0.0.1`, localhost, so must be specified if attempting to
  reach a remote.
* `-p` Port Number of the server to connect to. Defaults to `9001`, if server is running on a different port be sure to
  update this.

Once in the client, sending messages in the prompt will automatically send them to the server.

**Note:** In order to fully exit the client, CTRL+C out or send a SIGINT signal to the process.

## Building

### Requirements

* **CMake** >= v3.22.1
* **GNU Make** >= 4.3
* **GCC** >= 11.3.0
* **Python** >= 3.7
* [optional] **Clang-Format**
* [optional] **CodeChecker**

### Instructions

To compile the project, run `./build.sh` in the base directory. This shell script will automatically configure
the project for compilation and run `make` on the results. `clang-format` and `codechecker` are included
in configuration and will allow greater code insights if installed, although are not required.

## Theory of Operation

### Overview

This is a multithreaded, task-oriented implementation of Basic Chat Protocol that leverages a state machine to manage
connections. All network activity occurs over TCP on the port specified by the command line argument on
address `0.0.0.0`.

Sockets are created on demand by the server for incoming clients. An epoll instance watches all socket descriptors
for activity and dispatches events to the program's threadpool for handling. All connections are statefully opened and
statefully closed according to deterministic transitions.

### Major End Items

#### Connection Queue

A connection queue defined by `rpchat_conn_queue` contains objects of type `rpchat_conn_info_t`. Each of these objects
corresponds to a specific client, and contains the fields necessary to manage the lifecycle of each connection.

#### State Machine

`rpchat_process_event.c` contains `rpchat_task_conn_proc_event`, a task responsible for all state transitions in the
program and actions to take depending on the state. `rpchat_task_conn_proc_event` can be queued up by other functions,
but is primarily enqueued by itself in order to properly manage multi-state operations for each connection.

#### Threadpool

A threadpool is created on setup that processes all tasks for the program.

### Lifecycle

1. _Setup_. The program's entry-point is in `main.c`, which then calls `begin_networking` in `rpchat_basic_chat.c`.
2. _Listening_. `monitor_connections` is called by `begin_networking`, which uses epoll to listen to a set of socket
   descriptors.
3. _Actions_ `handle_events` is called by `monitor_connections`, which assesses the type of event and creates an event
   in the program threadpool, `p_tpool`.
4. _Handling_ `rpchat_process_event.c` contains `rpchat_task_conn_proc_event`, which manages the lifecycle of each
   connection by conducting state changes and coordinating `send` and `receive` actions.
5. _Exiting_ If a `SIGINT` is raised, it will be caught by `monitor_connections` and dispatched to `handle_events`,
   which will then begin the process of gracefully closing the program.

### Connection States

#### RPCHAT_CONN_PRE_REGISTER:

The initial state of a client once it's connected to the server and been assigned a socket.
The server expects the client to immediately send a REGISTER message to exit this state, otherwise, the client will be
disconnected.

##### _Transitions_

* Receiving a `REGISTER` message will transition the client's status to `SEND_STAT`.
* Transitions to `RPCHAT_CONN_ERR` if a message was received that was not `REGISTER`.

#### RPCHAT_CONN_AVAILABLE:

The default state of a client. If a client has not sent messages, or is not awaiting any actions, it will be in this
state. Crucially, this is the only state a client is able to action incoming messages other than `REGISTER` in.
If `POLLHUP` or `POLLERR` are received in the event, the server will transition the client to `RPCHAT_CONN_ERR`.

##### _Transitions_

* Receiving a `SEND` message will transition the state to `RPCHAT_CONN_SEND_STAT`
* Being assigned a `DELIVER` message from `rpchat_broadcast_message` will transition the state to `RPCHAT_CONN_SEND_MSG`
* Encountering an epoll event other than `POLLIN` will transition the state to `RPCHAT_CONN_ERR`.

#### RPCHAT_CONN_SEND_STAT:

The state of a client once it has received a `SEND` or `REGISTER` message. In this state, the server will send
a `STATUS` message to the associated client.

##### _Transitions_

* Automatically transitions to `RPCHAT_CONN_AVAILABLE` once the status message is successfully sent.
* Transitions to `RPCHAT_CONN_ERR` if the message could not be sent.

#### RPCHAT_CONN_SEND_MSG:

The state of a client that must be sent a message. This is usually the result of `rpchat_broadcast_message`
enqueueing `DELIVER` messages for every client containing a message sent by one client to the rest. The message will be
contained within a passed buffer that will be freed once the message is sent.

##### _Transitions_

* Automatically transitions to `RPCHAT_CONN_PENDING_STATUS` once the message is successfully sent.
* Transitions to `RPCHAT_CONN_ERR` if the message could not be sent.

#### RPCHAT_CONN_PENDING_STATUS:

The state of a client that has been sent a message. The server is now awaiting a message of type `STATUS` from the
client before actioning anything else for the client (other events will be requeued in the background).

##### _Transitions_

* Transitions to `RPCHAT_CONN_AVAILABLE` once a `STATUS` message with code `0` has been received.
* Transitions to `RPCHAT_CONN_ERR` if a message that is _not_ `STATUS` was sent by the client (noncompliant).

#### RPCHAT_CONN_ERR:

The state of a client that has become noncompliant with protocol by sending messages out of sequence, or a client that
has encountered an internal error. Additionally, if the client has timed out, it will fall into this state. In this
state, the server begins releasing the client's resources. The server will not transition this client until all pending
jobs for the client
are complete.

##### _Transitions_

* Transitions to `RPCHAT_CONN_CLOSING` once all preliminary cleanup actions are complete.

#### RPCHAT_CONN_CLOSING

The state of a client that is actively closing. The server is releasing all resources related to the client and removing
it from the connection queue.

##### _Transitions_

* None, the connection will be destroyed once in this state.

### Timeout

Clients will timeout if they are inactive for a time defined in `RPCHAT_CONNECTION_TIMEOUT` in `rpchat_networking.h`.
This timeout is enforced by a timer which raises a `SIGALRM` signal listened to by a `signalfd` in the main event loop.
Once caught, `rpchat_task_conn_proc_event` is called for all clients with a special `HEARTBEAT` argument.
The connections are checked for `current_time - last_active_time` and whether it exceeds `RPCHAT_CONNECTION_TIMEOUT`. If
they do, they are disconnected.