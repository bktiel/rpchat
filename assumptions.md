## Assumptions

This program implements a custom chat protocol, Basic Chat Protocol, but the standard does not specify 100% of
particulars useful for translating specifications
into a functioning program. This file includes all the assumptions made for the program.

### Default Port

The default port to host the server on is `9001`. This is a necessary assumption because in the absence
of user input a port should be selected to present least astonishment. This is a reasonable assumption
because there is no prescribed port for Basic Chat Protocol. Additionally, `9001` is in the unprivileged port range
and is not commonly associated with many other applications, so it is unlikely to present conflicts to the user.

### Maximum String Length

The maximum length for the contents of a string object in this program is `4095`. This is a necessary assumption
because a buffer must be created within each string struct definition in the declaration for the contents char array.
This is a reasonable assumption because the C99 standard requires string literals to be 4095 characters or less. While
no similar limit exists for the size of a character array, the C99 string literal standard is a reasonable baseline.
https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf

### Maximum Concurrent Connections

The maximum number of concurrent connections is based on the linux resource limit for file descriptors. This assumption
is necessary because supporting an infinite number of open connections is unrealistic as there is a defined limit for
open file descriptors by the process. Having a defined limit also enables creation of buffers of a specified, rather
than dynamic, size, minimizing unnecessary complexity. This assumption is reasonable because the resource limit for
file descriptors is pulled at runtime from the operating system using `getrlimit` with the `NOFILE` argument.
Subtracting the amount of descriptors that must be open at rest (epollfd, serverfd, signalfd) provides the amount of
usable file descriptors.

### Default Client Timeout

The default timeout for connected clients is 60 seconds. This assumption is necessary to enforce a timeout without
a command-line argument. This assumption is reasonable because 60 seconds allows demonstration of the timeout
functionality while still allowing for normal communication.

### Default Thread Count

The default thread count is 4. This assumption is necessary to set a number of threads for the program without
a command-line argument. This assumption is reasonable because the average consumer CPU in June 2023 had at least
6 cores, according to the Steam Hardware Survey:
https://store.steampowered.com/hwsurvey/Steam-Hardware-Software-Survey-Welcome-to-Steam

### Default Fail-Safe Behavior

Clients are disconnected as soon as their communications deviate from the BCP protocol. This assumption is necessary
in order to ensure only clients with safe behavior remain connected to the server. This assumption is reasonable
because all BCP functionality is available without violating the program, and noncompliant clients can simply reconnect
upon being disconnected.

### Maximum Message Length

Messages sent over BCP are assumed to be no greater than 8195 bytes (+ TCP, IP headers)
This assumption is necessary to allocate a buffer to hold every type of message. It is reasonable because
the largest message type in the BCP basic specification, SEND, is 2 strings (4097 each) and an opcode (1),
adding up to 8195 bytes.

### Restricted to Printable ASCII (sans spaces)

Messages sent over the BCP chat server are sanitized to only printable characters in the ASCII range.
This assumption is necessary because other characters could produce unexpected or erroneous results
when printing to screen or conducting tests with strcmp. This assumption is reasonable because the primary
purpose of a chat server is to relay and reproduce human-readable text.

### Server Platform

Project only builds on linux system owing to system-specific headers. This assumption
is reasonable because the JQR projects have the same limitation.

### Client Platform

Client script is in python and assumed to be cross-platform by design. This assumption is necessary
because some python functionality is limited by platform. This assumption is reasonable because a chat
client would need to be as portable as possible to maximize audience, and there are no compilation restrictions
like there would be for a C client. 