#!/usr/bin/python3
import argparse
import os
import socket
import struct
import re
import threading
from enum import Enum

PORT_MAX = 65535

BCP_SOCKET_TIMEOUT = 1  # how long socket waits for packet before cycling
BCP_MAX_MSG_SIZE = 8195
BCP_MAX_STR_LEN = 4095

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
        self.return_code = 0
        self.remote_port = remote_port  # Remote BCP Server Port
        self.remote_ip = remote_ip  # Remote BCP Server Addr
        self.client_state = self.BCP_CONN_STATUS.SETUP.value  # current state of client
        self.conn_lock = threading.Lock()  # lock for connection operations
        self.screen_lock = threading.Lock()  # lock for screen operations
        self.timeout = BCP_DEFAULT_TIMEOUT  # how long to wait before operation failure
        self.screen = ""
        self.prompt = ""
        # set remaining fields

        # create locks

        print(f"Welcome {self.username}. Attempting to connect to server..")
        if not self.initial_setup():
            return

    def initial_setup(self):
        '''
        Sets up socket for client
        :return: False on problems, otherwise, exits caller (thread/app)
        '''
        # create socket
        if not self.setup_socket():
            print("Unable to connect to server")
            return False
        self.register_client()
        # define threads
        self.inbound_thread = threading.Thread(target=self.handle_inbound, daemon=True)
        # start threads
        self.inbound_thread.start()
        # main (input) thread
        self.handle_outbound()
        exit(self.return_code)

    def clear_screen(self):
        '''
        Clear console of all input
        :return: None
        '''
        # WINDOWS
        if os.name == 'nt':
            _ = os.system('cls')

        # LINUX
        else:
            _ = os.system('clear')

    def input_screen(self, prompt):
        '''
        Handle input to screen
        :param prompt: Prompt to use before input()
        :return: Input that was sent
        '''
        user_input = None
        with self.screen_lock:
            self.clear_screen()
            print(self.screen)
        user_input = input(prompt)
        with self.screen_lock:
            self.prompt = prompt
        return user_input

    def print_screen(self, msg):
        '''
        Print contents of buffer and a new message on to screen
        :param msg: New message to print
        :return: None
        '''
        with self.screen_lock:
            self.clear_screen()
            self.screen += (msg + '\n')
            print(self.screen.rstrip())  # print contents of screen
            print(self.prompt)  # always print prompt beneath

    def handle_outbound(self):
        '''
        Handle messages from user and pushes them to server as SEND messages
        :return: Runs continuously until exited by user
        '''
        while True:
            self.conn_lock.acquire()
            if self.client_state is self.BCP_CONN_STATUS.SHUTDOWN.value:
                return
            # if client ready to send, allow submission message
            if self.client_state is self.BCP_CONN_STATUS.READY.value:
                # print last message as applicable
                if self.last_msg is not None:
                    self.print_screen(f"{self.username}: {self.last_msg}")
                # attempt to get input for msg
                self.conn_lock.release()
                self.last_msg = self.input_screen("")
                if (len(self.last_msg) == 0):
                    self.last_msg = ' '
                # send completed input
                self.send_send(self.last_msg)
                self.conn_lock.acquire()
                self.client_state = self.BCP_CONN_STATUS.PENDING_STATUS.value
            self.conn_lock.release()

    def handle_inbound(self):
        '''
        Handle messages coming from server
        :return: Exits caller (thread/app) on error or receipt of negative status message
        '''
        received = None
        msg_buf = None
        while True:
            # attempt to get incoming
            try:
                received = self.sock.recv(1)
            except ConnectionError as e:
                break
            # sock.recv should return b'' on close
            if len(received) == 0:
                break
            with self.conn_lock:
                self.handle_incoming_msg(received)
        # signal shutdown
        print("Server disconnected.\n")
        self.client_state = self.BCP_CONN_STATUS.SHUTDOWN.value
        exit(1)

    def handle_deliver(self):
        '''
        Handle an inbound deliver msg
        :return: True if Deliver handled successfully; False if not
        '''
        sender_len, sender, content_len, content = None, None, None, None
        msg = ""
        index = 1  # skip opcode
        sender_len_len = 2
        content_len_len = 2
        try:
            # sender field
            msg = self.sock.recv(sender_len_len)
            sender_len = struct.unpack("!h", msg)[0]
            index += sender_len_len
            msg = self.sock.recv(sender_len)
            sender = struct.unpack(f"!{sender_len}s", msg)[0]
            index += sender_len
            # message field
            msg = self.sock.recv(content_len_len)
            content_len = struct.unpack("!h", msg)[0]
            index += content_len_len
            msg = self.sock.recv(content_len)
            content = struct.unpack(f"!{content_len}s", msg)[0]
            index += content_len
        except:
            print("Error: Invalid message received from server.")
            return False

        sender = sender.decode().replace('\x00', '')
        content = content.decode().replace('\x00', '')
        self.print_screen(f"{sender}: {content}")
        return True

    def handle_stat(self):
        '''
        Handle an inbound status msg
        :return: True if status handled successfully; False if not
        '''
        stat, stat_msg_len, stat_msg = None, None, None
        msg = ""
        len_statcode = 1
        len_msg_len = 2
        offset = len_statcode + len_msg_len
        try:
            msg = self.sock.recv(offset)
            stat, stat_msg_len = struct.unpack("!Bh", msg)
            if stat_msg_len > 0:
                msg = self.sock.recv(stat_msg_len)
                offset += stat_msg_len
                stat_msg = struct.unpack(f"{stat_msg_len}s", msg)[0].decode()
        except:
            print("Error: Invalid message received from server.")
            return False
        self.return_code = stat
        # if stat bad..
        if stat != 0:
            self.print_screen(f"Server: Negative status received")
        # notify client
        if stat_msg:
            self.print_screen(f"Server: {stat_msg}")
        # set state to READY
        if self.client_state is not self.BCP_CONN_STATUS.READY.value:
            self.client_state = self.BCP_CONN_STATUS.READY.value
        return True

    def parse_commands(self):
        '''
        Handle client-side commands
        :return: True
        '''

    def register_client(self):
        '''
        Handle registration transaction with the server
        :return: True
        '''
        ret_stat = None
        fmt = f"!Bh{len(self.username)}s"
        msg = struct.pack(fmt, BCPClient.BCP_OPCODE.REGISTER.value, len(self.username), self.username.encode('utf-8'))
        # send
        self.sock.sendall(msg)
        # await response
        return True

    def send_send(self, content):
        '''
        Send a SEND msg to the server
        :param content: Content to send
        :return: True if send operation successful; False if not
        '''
        # trim to fit
        content = content[:min(len(content), BCP_MAX_STR_LEN)]
        # encode and pack
        fmt = f"!Bh{len(content)}s"
        msg = struct.pack(fmt, BCPClient.BCP_OPCODE.SEND.value, len(content), content.encode('utf-8'))
        return None is self.sock.sendall(msg)

    def send_stat(self, stat_code):
        '''
        Send a STATUS msg to the server
        :param stat_code: status code to send
        :return: True if send operation successful; False if not
        '''
        # trim
        stat_code = int(str(stat_code)[:1])
        # encode and pack
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
        updated_msg = False
        # get opcode
        opc = struct.unpack("!B", msg[:1])[0]
        if opc is self.BCP_OPCODE.STAT.value:
            updated_msg = self.handle_stat()
        elif opc is self.BCP_OPCODE.DELVR.value:
            updated_msg = self.handle_deliver()
            # NOTE: assumes status 0 is good and status 1 is error
            self.send_stat(int(updated_msg == False))
        return updated_msg


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


if __name__ == "__main__":
    main()
