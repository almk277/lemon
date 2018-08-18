#!/usr/bin/env python3

import argparse
import socket
import selectors
import time
from threading import Thread

listen_port = 8088
remote_host = 'localhost'
remote_port = 8080
buffer_size = 10
crash = False
delay = 500
debug = lambda *_: None

def parse_command_line():
    global listen_port, remote_host, remote_port, buffer_size, crash, delay, debug
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', '--listen', type=int, help='listen port')
    parser.add_argument('-a', '--host', help='server host string')
    parser.add_argument('-p', '--port', type=int, help='server port')
    parser.add_argument('-b', '--buffer', type=int, help='buffer size in bytes')
    parser.add_argument('-c', '--crash', action='store_true', help='break connection')
    parser.add_argument('-d', '--delay', type=int, help='buffer send delay in milliseconds')
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args()
    if args.listen:
        listen_port = args.listen
    if args.host:
        remote_host = args.host
    if args.port:
        remote_port = args.port
    if args.buffer:
        buffer_size = args.buffer
    crash = args.crash
    if args.delay:
        delay = args.delay
    if args.verbose:
        debug = print

def send(from_sock, to_sock):
    data = from_sock.recv(buffer_size)
    if data: to_sock.sendall(data)
    debug('sent', from_sock.getpeername(), '->', to_sock.getpeername(), data)
    return data

def client_to_server(client, server):
    return send(client, server)

def server_to_client(client, server):
    return send(server, client)

def run_connection(client):
    with client:
        print('connected:', client.getpeername())
        with socket.create_connection((remote_host, remote_port)) as server, \
                selectors.DefaultSelector() as sel:
            sel.register(client, selectors.EVENT_READ, client_to_server)
            sel.register(server, selectors.EVENT_READ, server_to_client)
            running = True
            while running:
                for key, _ in sel.select():
                    handler = key.data
                    sent = handler(client, server)
                    if not sent: running = False
                if crash: running = False
                else: time.sleep(delay / 1000)
            sel.unregister(server)
            sel.unregister(client)
        print('disconnected:', client.getpeername())


def main():
    parse_command_line()
    with socket.socket() as s:
        s.bind(('', listen_port))
        s.listen()
        while True:
            conn, _ = s.accept()
            t = Thread(target=run_connection, args=(conn,), daemon=True)
            t.start()


if __name__ == "__main__":
    main()
