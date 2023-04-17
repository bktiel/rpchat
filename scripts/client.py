#!/usr/bin/python3
import errno
import argparse
import os
import socket
import struct
import re
import time
from enum import Enum
from select import select

NET_HDR_SZ = 48
NET_FNAME_MAX = 24
NET_NAME_FIELD_SZ = 32
PORT_MAX = 65535

TFTP_SOCKET_TIMEOUT = 1  # how long socket waits for packet before cycling
TFTP_MAX_PKT_SIZE = 516  # max tftp packet size, see assumptions.md
TFTP_MAX_DATA_SIZE = 512  # max data size IAW rfc 1350
TFTP_MAX_PATH_LEN = 504  # max path length, see assumptions.md
TFTP_MIN_PORT = 1024  # min ephemeral port
TFTP_MAX_PORT = 65535  # max ephemeral port


class TFTPClient:
    '''
    Basic TFTP client implementation in python IAW RFC 1350
    '''

    class TFTP_OPCODE(Enum):
        RRQ = 1
        WRQ = 2
        DATA = 3
        ACK = 4
        ERROR = 5

    class CLIENT_MODE(Enum):
        UPLOAD = 0
        DOWNLOAD = 1

    def __init__(self, remote_port, remote_ip, local_file, remote_file, timeout):
        # set fields
        self.client_mode = -1
        self.timeout = timeout
        self.complete = False
        self.bytes_read = 0
        self.sock: socket.socket
        self.remote_port = remote_port
        self.remote_ip = remote_ip
        self.local_file = local_file
        self.remote_file = remote_file
        self.block_number = 0
        self.last_transfer = TFTP_MAX_DATA_SIZE  # used to check when to end operations
        # set remaining fields
        if not self.initial_setup():
            return
        # handle connection
        self.handle_connection()

    def initial_setup(self):
        '''
        Sets up socket and client mode for client
        :return: True on success, False on problems
        '''
        # get mode
        self.client_mode = self.determine_client_mode()
        # create socket
        if not self.setup_socket():
            print("Unable to connect to server")
            return False
        return True

    def handle_connection(self):
        '''
        Given a set up client object, handle connection to server and upload/download operation
        :return: True on overall success, False on overall failure
        '''
        # sent initial request packet until response gotten or timeout..
        tm_begin_req = time.process_time()
        while True:
            (read, write, err) = select([self.sock], [], [], TFTP_SOCKET_TIMEOUT)
            delta = (time.process_time() - tm_begin_req) * 1000
            if (len(read) > 0):
                break
            if (delta > self.timeout):
                print("Unable to get response from server.")
                return False
            print("Sending transaction request..")
            self.send_req_pkt()
        # data pending on socket
        print("Server response")
        # invalidate port until proper can be assigned
        self.remote_port = None
        # handle appropriate operation
        if self.client_mode is self.CLIENT_MODE.UPLOAD:
            self.handle_upload()
        else:
            self.handle_download()

    def handle_download(self):
        '''
        Handle lifecycle of a download operation
        :return: True on success, False on problems
        '''
        # used for timeout
        last_success = time.process_time()
        while not self.complete:
            print(f"Packet {self.block_number} sent to remote..")
            # check timeout for kill
            delta = (time.process_time() - last_success) * 1000
            print(f"{delta}s since last activity..")
            if (delta > self.timeout):
                print("Timeout reached. Exiting.")
                break
            # attempt to receive data packet
            data = self.handle_incoming_pkt(TFTPClient.TFTP_OPCODE.DATA)
            if data == False:
                continue
            # send ack pkt
            self.send_ack_pkt()
            self.block_number += 1
            # write to file
            last_success = time.process_time()
            # append local file sets self.complete
            if not self.append_local_file(data):
                print("File error on operation.")
                break
            if self.last_transfer < TFTP_MAX_DATA_SIZE:
                self.complete = True
        # operation complete, check success
        if self.complete:
            print(f"Finished downloading {self.remote_file} as {self.local_file}.")
        else:
            print(f"Error occurred downloading {self.remote_file}")

    def handle_upload(self):
        '''
        Handle lifecycle of an upload operation
        :return: True on success, False on problems
        '''
        # used for timeout
        last_success = time.process_time()
        while not self.complete:
            print(f"Packet {self.block_number} sent to remote..")
            # check timeout for kill
            delta = (time.process_time() - last_success) * 1000
            print(f"{delta}s since last activity..")
            if (delta > self.timeout):
                print("Timeout reached. Exiting.")
                break
            # handle ack from server
            if self.handle_incoming_pkt(TFTPClient.TFTP_OPCODE.ACK) == False:
                # False here means it's either not ack or out of sequence
                # Resend and continue
                if self.remote_port is not None:
                    self.send_data_pkt()
                continue
            else:
                # if good ack, check the last transfer was good
                if self.last_transfer < TFTP_MAX_DATA_SIZE:
                    self.complete = True
                    break

            # send data packet
            self.send_data_pkt()
            self.block_number += 1
            last_success = time.process_time()
        # operation complete, check success
        if self.complete:
            print(f"Finished uploading {self.local_file} as {self.remote_file}.")
        else:
            print(f"Error occurred uploading {self.local_file}")

    def determine_client_mode(self):
        '''
        Determine what client mode to run in, based on presence of local file
        :return: Appropriate CLIENT_MODE
        '''
        if not os.path.isfile(self.local_file):
            print("Local file does not exist. Downloading from remote.")
            return self.CLIENT_MODE.DOWNLOAD
        else:
            print("Local file exists. Uploading to remote.")
            return self.CLIENT_MODE.UPLOAD

    def send_req_pkt(self):
        '''
        Intelligently send a request based on CLIENT_MODE on client
        :return: True on success, False on problems
        '''
        # sanity
        if not self.client_mode:
            return False
        # create correct type of packet
        req_pkt = None
        if self.client_mode is self.CLIENT_MODE.DOWNLOAD:
            req_pkt = self.create_rrq_pkt()
        else:
            req_pkt = self.create_wrq_pkt()
        # send packet to remote
        self.sock.sendto(req_pkt, (self.remote_ip, self.remote_port))
        return True

    def send_ack_pkt(self):
        '''
        Sends a TFTP ACK packet to the remote defined in fields
        :return: True on success, False on failure
        '''
        res = True
        ack_pkt = self.create_ack_pkt()
        try:
            self.sock.sendto(ack_pkt, (self.remote_ip, self.remote_port))
        except Exception as e:
            print(e)
            res = False
        return res

    def send_data_pkt(self):
        '''
        Reads a chunk from the local_file and sends to remote
        :return: True on success, False on problems
        '''
        buf = None
        # get contents
        try:
            with open(self.local_file, "rb") as reader:
                reader.seek(self.bytes_read)
                buf = reader.read(TFTP_MAX_DATA_SIZE)
        except Exception as e:
            print(e)
            return False
        self.bytes_read += len(buf)
        # update last transfer
        self.last_transfer = len(buf)
        # create data packet
        data_pkt = self.create_data_pkt(buf)
        if not data_pkt: return False
        # send upstream
        try:
            self.sock.sendto(data_pkt, (self.remote_ip, self.remote_port))
        except Exception as e:
            print(str(e))
            return False
        return True

    def setup_socket(self):
        '''
        Initializes a UDP socket for program and attempts to bind to a random ephemeral port
        :return: True on success, False on failure
        '''
        new_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        new_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 0)
        try:
            # attempt to bind to unique address (updates UDP source field)
            if not self.bind_eph_socket(new_sock):
                raise Exception
        except Exception as e:
            print("Error occurred connecting to server.")
            return False
        self.sock = new_sock
        return True

    def handle_incoming_pkt(self, exp: TFTP_OPCODE):
        '''
        Receives an object of TFTP maximum size from the client socket
        :param exp Expected opcode from the opcode enum
        :return: True on handled, False on error
        '''
        self.sock.settimeout(TFTP_SOCKET_TIMEOUT)
        try:
            pkt = self.sock.recvfrom(TFTP_MAX_PKT_SIZE)
        except socket.timeout:
            return False
        addr = pkt[1]
        pkt = pkt[0]
        if not pkt:
            return False
        # verify/update remote tid
        if not self.remote_port:
            self.remote_port = addr[1]
        else:
            if self.remote_port != addr[1]:
                print(f"bad tid (expected {self.remote_port}, got {addr[1]})")
                return False
        # get opcode
        opc = struct.unpack("!H", pkt[:2])[0]
        # check expected
        if exp.value != opc and opc != self.TFTP_OPCODE.ERROR.value:
            return False
        # get appropriate handler
        pkt_handler = self.get_pkt_handler(opc)
        if not pkt_handler:
            return False
        # call handler
        return pkt_handler(pkt)

    def get_pkt_handler(self, opc):
        '''
        Helper function to get approprate function pointer handler given an opcode
        :param opc: 2 byte opcode
        :return: Function pointer to handler
        '''
        if TFTPClient.TFTP_OPCODE.DATA.value == opc:
            return self.handle_data_pkt
        elif TFTPClient.TFTP_OPCODE.ACK.value == opc:
            return self.handle_ack_pkt
        elif TFTPClient.TFTP_OPCODE.ERROR.value == opc:
            return self.handle_err_pkt
        else:
            return None

    def handle_data_pkt(self, pkt):
        '''
        Handles a data packet, returns data contained
        :return: Data buffer
        '''
        hdr_len = TFTP_MAX_PKT_SIZE - TFTP_MAX_DATA_SIZE
        pkt_len = len(pkt)
        # sanity
        if pkt_len < hdr_len:
            return False

        # get header fields
        hdr = struct.unpack(f"!HH", pkt[:hdr_len])
        op = hdr[0]
        block_num = hdr[1]
        # validate
        if op is not self.TFTP_OPCODE.DATA.value:
            print("Unexpected non-data pkt")
            return False
        if block_num != self.block_number:
            print(f"Out of sequence packet received (expected {self.block_number}, got {block_num}) ")
            return False
        # check data length
        data_len = (pkt_len - hdr_len)
        self.last_transfer = data_len
        return pkt[hdr_len:]

    def handle_ack_pkt(self, pkt):
        '''
        Handle ACK packet
        :param pkt: Packet
        :return: True on success, False on failure
        '''
        # check sequence

        return True

    def handle_err_pkt(self, pkt):
        '''
        Handles an error packet, prints to screen and exits
        :param pkt:
        :return:
        '''
        # sanity
        err_code = 0
        err_msg = ""
        hdr = ()
        if len(pkt) >= 4:
            hdr = struct.unpack("!HH", pkt[:4])
            err_code = hdr[1]
            err_msg = pkt[4:].decode()
        print(f"Error code {err_code} reported with message: {err_msg}")
        self.send_ack_pkt()
        exit(1)

    # Create TFTP WRQ Packet given a remote path
    def create_wrq_pkt(self):
        opcode = self.TFTP_OPCODE.WRQ.value
        filename = self.remote_file.encode('utf-8')
        mode = "octet".encode('utf-8')
        fmt = "!H{}sB{}sB".format(len(filename), len(mode))
        pkt = struct.pack(fmt, opcode, filename, 0x00, mode, 0x00)
        return pkt

    # Create TFTP RRQ Packet given a remote path
    def create_rrq_pkt(self):
        opcode = self.TFTP_OPCODE.RRQ.value
        filename = self.remote_file.encode('utf-8')
        mode = "octet".encode('utf-8')
        fmt = "!H{}sB{}sB".format(len(filename), len(mode))
        pkt = struct.pack(fmt, opcode, filename, 0x00, mode, 0x00)
        return pkt

    # Create TFTP ACK Packet given a block number
    def create_ack_pkt(self):
        opcode = self.TFTP_OPCODE.ACK.value
        pkt = struct.pack("!HH", opcode, self.block_number)
        return pkt

    def create_data_pkt(self, data: bytes):
        '''
        Create data packet using passed data
        :param data: Data to send
        :return: True on success, False on failure
        '''
        opcode = self.TFTP_OPCODE.DATA.value
        if len(data) > TFTP_MAX_DATA_SIZE:
            raise OverflowError
        fmt = f"!HH{len(data)}s"
        pkt = struct.pack(fmt, opcode, self.block_number, data)
        return pkt

    def append_local_file(self, data):
        '''
        Append given data to the local_file on filesystem
        :param data: Data to write
        :return: True on success, False on failure
        '''
        try:
            with open(self.local_file, "ab+") as writer:
                writer.write(data)
        except PermissionError:
            print(f"Bad Permissions on {self.local_file}")
            return False
        return True

    def bind_eph_socket(self, sock):
        '''
        Bind a given socket to an ephemeral port
        :param sock: Socket to bind
        :return: True on success, False on failure
        '''
        success = False
        for new_port in range(TFTP_MIN_PORT, TFTP_MAX_PORT):
            try:
                sock.bind(('', new_port))
                success = True
                break
            except socket.error as e:
                if e.errno == errno.EADDRINUSE:
                    continue
                else:
                    print(e)
                    break
        # socket defined
        return success


def invalid_args(args):
    '''
    Check for invalid arguments
    :param args: Args from python argparse module
    :return: True on success, False on invalid args
    '''
    # validation
    error = False
    if not re.match("^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$", args.s):
        print("Invalid IP Address")
        error = True
    if args.p >= PORT_MAX:
        print("Invalid Port")
        error = True
    return error


def main():
    op_mode = None  # op_mode either "UL" or "DL"
    ret = 0  # used for ret
    # handle cli args
    parser = argparse.ArgumentParser(description="Submission Client for Network Calculator")
    parser.add_argument("-s", metavar="ip", default="127.0.0.1", help="Server IPv4 Address")
    parser.add_argument("-p", metavar="port", default="69", help="Server Port", type=int)
    parser.add_argument("-t", metavar="timeout", default="10",
                        help="Grace period to stay inactive before giving up, in seconds", type=int)
    parser.add_argument("-l", metavar="local", help="Local file", required=True)
    parser.add_argument("-r", metavar="remote", help="Remote file", required=True)

    args = parser.parse_args()

    if invalid_args(args):
        return 1

    # re-assign parameters
    server_ip = args.s
    server_port = args.p
    local_file = args.l
    remote_file = args.r
    timeout = args.t

    client = TFTPClient(server_port, server_ip, local_file, remote_file, timeout)
    exit(ret)


if __name__ == "__main__":
    main()
