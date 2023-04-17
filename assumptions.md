## Assumptions

This program implements RFC 1350, but the standard does not specify some particulars useful for translating specifications
to a functioning program. This file includes all of the assumptions made for the program.

### Default Directory

The default directory is assumed to be the user's home directory. This assumption is necessary because a default
directory must be
set in the program. It is reasonable because users should have r/w access to their home directoryThis assumption also
depends on the environment variable $HOME being present, which is reasonable on most shell implementations.

### Default Timeout

The default socket timeout is 10 seconds. Because poll() runs every other second,
10 seconds allows 10 attempts at resending before closing the connection. This number is ultimately arbitrary
and picked for a balance between convenience and utility.

### Filename Length

The filename length is assumed to be no more than 256. This assumption is necessary because unconstrained path length
could lead to unexpected behavior like buffer overflows, and setting a predefined limit allows more predictable packet
length when parsing. This assumption is reasonable because 1) the linux maximum filename is 255 bytes and 2) RFC 1122 on
UDP states "In general, no host is required to accept an IP datagram larger than 576 bytes" as larger sizes result in 
fragmentation. Up to 12 bytes must be used for the RRQ/WRQ header.  A data packet header is 4 bytes + a 512 byte buffer.
516-12=504 bytes available for a filename in the initial RRQ/WRQ.

### Error Message Length

No error messages are present in RFC 1350 longer than 128. 128 bytes
is sufficient for even verbose messages and fits in the error packet easily.

### Packet Size

Packets are assumed to be no larger than 516 bytes. The largest header
in the TFTP specification (RFC 1350) is the data packet, whichrequirements
is 512 bytes and a 4 bytes header. Additionally, RFC 1122 on UDP states requirements
"In general, no host is required to accept an IP datagram larger than 576 bytes."
There is not enough of a measurable benefit to justify exceeding the largest TFTP packet of 516 bytes.

### Only real files in the served directory are accessible

I interpreted guidance given to only serve files in the specified directory to
mean to serve files nad directories within that directory. This seemed like the most
reasonable approach provided vulnerabilities could be mitigated (e.g. path traversal exploit)

### New files are written with only user R/W/X

In the absence of a specific environment the most safe option is to set the most
restrictive permissions on files newly created.

### Platform

Project only builds on linux system owing to system-specific headers. This assumption
is reasonable because the JQR projects have the same limitation.