#!/usr/bin/python3
import errno
import argparse
import os
import socket
import struct
import re
import sys
import threading
import time
from enum import Enum
from select import select

NET_HDR_SZ = 48
NET_FNAME_MAX = 24
NET_NAME_FIELD_SZ = 32
PORT_MAX = 65535

BCP_SOCKET_TIMEOUT = 1  # how long socket waits for packet before cycling
TFTP_MAX_PKT_SIZE = 516  # max tftp packet size, see assumptions.md
BCP_MAX_MSG_SIZE = 2048
BCP_MAX_STR_LEN = 4095
TFTP_MAX_DATA_SIZE = 512  # max data size IAW rfc 1350
TFTP_MAX_PATH_LEN = 504  # max path length, see assumptions.md
TFTP_MIN_PORT = 1024  # min ephemeral port
TFTP_MAX_PORT = 65535  # max ephemeral port

BCP_DEFAULT_TIMEOUT = 6000


class BCPClient:
    '''
    Basic BCP client implementation in python
    '''

    class BCP_OPCODE(Enum):
        REGISTER = 1
        SEND = 2
        DELVR = 3
        STAT = 4

    class BCP_CONN_STATUS(Enum):
        SETUP = 0
        READY = 1
        PENDING_STATUS = 2
        SHUTDOWN = 3

    def __init__(self, remote_port, remote_ip, username):
        # set fields
        self.inbound_thread = None  # holds thread for incoming operations
        self.outbound_thread = None  # holds thread for outgoing operations
        self.sock = None  # holds ref to connected TCP socket
        self.sock: socket.socket
        self.username = username  # BCP username
        self.last_msg = None
        self.remote_port = remote_port  # Remote BCP Server Port
        self.remote_ip = remote_ip  # Remote BCP Server Addr
        self.client_state = self.BCP_CONN_STATUS.SETUP.value
        self.conn_lock: threading.Lock
        self.timeout = BCP_DEFAULT_TIMEOUT
        # set remaining fields

        print(f"Welcome {self.username}. Attempting to connect to server..")
        if not self.initial_setup():
            return

    def initial_setup(self):
        '''
        Sets up socket for client
        :return: True on success, False on problems
        '''
        # create socket
        if not self.setup_socket():
            print("Unable to connect to server")
            return False
        if not self.register_client():
            print("Registration failed")
            return False

        # notify success
        print("Registration successful.")
        print(f"Logged in as {self.username}")
        # update state
        self.client_state = self.BCP_CONN_STATUS.READY.value
        # create lock
        self.conn_lock = threading.Lock()

        # define threads
        self.inbound_thread = threading.Thread(target=self.handle_inbound, daemon=True)
        # self.outbound_thread = threading.Thread(target=self.handle_outbound)
        # start threads
        self.inbound_thread.start()
        # self.outbound_thread.start()
        # join threads (wait for completion)
        self.handle_outbound()
        exit(1)


    def handle_outbound(self):
        '''
        Handle messages from user and pushes them to server as SEND messages
        '''
        success = False
        lding_step = 0
        lding_seq = ["⢿", "⣻", "⣽", "⣾", "⣷", "⣯", "⣟", "⡿"]
        while True:
            # if shutdown, get out
            self.conn_lock.acquire()
            if self.client_state is self.BCP_CONN_STATUS.SHUTDOWN.value:
                return
            # if client ready to send, allow submission message
            if self.client_state is self.BCP_CONN_STATUS.READY.value:
                # print last message as applicable
                if self.last_msg is not None:
                    print(f"{self.username}: {self.last_msg}")
                # attempt to get input for msg
                self.conn_lock.release()
                self.last_msg = input()
                # send completed input
                self.send_send(self.last_msg)
                self.conn_lock.acquire()
                self.client_state = self.BCP_CONN_STATUS.PENDING_STATUS.value
            self.conn_lock.release()

            # show loading
            # lding_step = (lding_step + 1) % (len(lding_seq) - 1)
            # sys.stdout.write('\r' + 'LOADING ' + lding_seq[lding_step]+'\n')
            # time.sleep(0.1)
            # sys.stdout.flush()

    def handle_inbound(self):
        '''
        Handle messages coming from server
        '''
        res=None
        received = None
        while True:
            # attempt to get incoming
            try:
                received = self.sock.recv(BCP_MAX_MSG_SIZE)
            except ConnectionError as e:
                break
            if len(received) > 0:
                with self.conn_lock:
                    res=self.handle_incoming_msg(received)
            # sock.recv should return b'' on close, inconsistent
            else:
                break
        # signal shutdown
        print("Server disconnected.\n")
        self.client_state = self.BCP_CONN_STATUS.SHUTDOWN.value
        exit(1)

    def handle_deliver(self, msg):
        '''
        Handle an inbound deliver msg
        '''
        sender_len, sender, content_len, content = None, None, None, None
        index = 1  # skip opcode
        try:
            sender_len = struct.unpack("!h", msg[index:index + 2])[0]
            index += 2
            sender = struct.unpack(f"!{sender_len}s", msg[index:(index + sender_len)])[0]
            index += sender_len
            content_len = struct.unpack("!h", msg[index:index + 2])[0]
            index += 2
            content = struct.unpack(f"!{content_len}s", msg[index:])[0]
        except:
            print("Error: Invalid message received from server.")
            return False

        sender=sender.decode().replace('\x00','')
        content=content.decode().replace('\x00','')
        print(f"{sender}: {content}")
        return True

    def handle_stat(self, msg):
        '''
        Handle an inbound status msg
        '''
        stat, stat_msg_len, stat_msg = None, None, None
        try:
            stat, stat_msg_len = struct.unpack("!Bh", msg[1:4])
            if stat_msg_len > 0:
                stat_msg = struct.unpack(f"{stat_msg_len}s", msg[4:])[0]
        except:
            print("Error: Invalid message received from server.")
            return False
        # notify client
        if stat_msg:
            print(f"Server: {stat_msg}")
        # if stat bad..
        if stat != 0:
            return False
        # reset
        if self.client_state is not self.BCP_CONN_STATUS.READY.value:
            self.client_state = self.BCP_CONN_STATUS.READY.value
        return True

    def register_client(self):
        '''
        Handle registration transaction with the server
        '''
        ret_stat=None
        fmt = f"!Bh{len(self.username)}s"
        msg = struct.pack(fmt, BCPClient.BCP_OPCODE.REGISTER.value, len(self.username), self.username.encode('utf-8'))
        # send
        self.sock.sendall(msg)
        # await response
        return self.handle_stat(self.sock.recv(BCP_MAX_MSG_SIZE))

    def send_send(self, content):
        '''
        Send a send msg to the server
        '''
        # trim to fit
        content = content[:min(len(content), BCP_MAX_STR_LEN)]
        # encode
        fmt = f"!Bh{len(content)}s"
        msg = struct.pack(fmt, BCPClient.BCP_OPCODE.SEND.value, len(content), content.encode('utf-8'))
        return None is self.sock.sendall(msg)

    def send_stat(self, stat_code):
        '''
        Send a status msg to the server
        '''
        # trim
        stat_code = int(str(stat_code)[:1])
        # encode
        fmt = f"!BB"
        msg = struct.pack(fmt, BCPClient.BCP_OPCODE.STAT.value, stat_code)
        return None is self.sock.sendall(msg)

    def setup_socket(self):
        '''
        Initializes a TCP socket for program and connect to server
        '''
        new_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        new_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 0)
        try:
            new_sock.connect((self.remote_ip, self.remote_port))
        except Exception as e:
            print("Error occurred connecting to server.")
            return False
        self.sock = new_sock
        return True

    def handle_incoming_msg(self, msg):
        res = False
        # get opcode
        opc = struct.unpack("!B", msg[:1])[0]
        if opc is self.BCP_OPCODE.STAT.value:
            res = self.handle_stat(msg)
        elif opc is self.BCP_OPCODE.DELVR.value:
            res = self.handle_deliver(msg)
            self.send_stat(int(not res))
        return res

    def handle_incoming_pkt(self, exp: BCP_OPCODE):
        '''
        Receives an object of TFTP maximum size from the client socket
        :param exp Expected opcode from the opcode enum
        :return: True on handled, False on error
        '''
        self.sock.settimeout(BCP_SOCKET_TIMEOUT)
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
    parser = argparse.ArgumentParser(description="Chat Client for Basic Chat Protocol")
    parser.add_argument("-s", metavar="ip", default="127.0.0.1", help="Server IPv4 Address")
    parser.add_argument("-p", metavar="port", default="9001", help="Server Port", type=int)
    parser.add_argument("-u", metavar="username", help="Username", required=True)

    args = parser.parse_args()

    if invalid_args(args):
        return 1

    # re-assign parameters
    server_ip = args.s
    server_port = args.p
    username = args.u

    client = BCPClient(server_port, server_ip, username)
    exit(ret)


if __name__ == "__main__":
    main()
