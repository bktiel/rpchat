## RPTFTP

Implementation of TFTP server as defined by RFC 1350.

### Getting Started

1. Build server with `./build.sh`.
2. On successful build, binary appears at `/bin/rptftp`
3. Run server with `./rptftp`

RPTFTP server can be ran with arguments `-t` and `-l`

- `-t` sets the timeout for connections in seconds. Connections time out and are destroyed after this delay. The default
  is `10` seconds.
- `-d` sets the directory to serve by the server. You must have read and write permissions. The default is your home
  directory (e.g. `/home/supercooluser`).
- `-h` show help.

3. The RPTFTP client is at `/scripts/client.py`
4. Client can be ran with `python client.py`

RPTFTP client can be ran with `-l`,`-r`,`-p`, and `-s`

- `-l` Path to a valid local file to upload if in upload mode, or where to place the remote file described by `-r` if in
  download mode.
- `-r` Path to a remote file to download if in download mode, or where on the server to place the file described by `-l`
  if in upload mode.
- `-s` IP address of a remote TFTP server
- `-p` Port of a remote TFTP server. Defaults to 69, this probably shouldn't need to be adjusted.

### Limitations
- Both the server and client only implement 'octet' mode and will refuse connections on any other mode
- The client changes modes based on the local file. If the local file is available,
the client assumes client mode. If it is not, the client assumes download mode.