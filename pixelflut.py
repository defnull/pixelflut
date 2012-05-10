#coding: utf8

import time
from gevent import spawn, sleep as gsleep
from gevent.server import StreamServer
from gevent.coros import Semaphore
from gevent.queue import Queue
from collections import deque


class Client(object):
    def __init__(self, canvas, socket, address):
        self.canvas = canvas
        self.socket = socket
        self.address = address
        self.connect_ts = time.time()
        # This buffer discards all but the newest 1024 messages
        self.sendbuffer = deque([], 1024)
        self.limit = Semaphore(0)
        self.counter = 0
        print 'CONNECT', address

    def send(self, line):
        self.sendbuffer.append(line.strip() + '\n')

    def disconnect(self):
        print 'DISCONNECT', self.address
        print self.counter / (time.time() - self.connect_ts)
        self.socket.close()
        del self.canvas.clients[self.address]

    def serve(self):
        sendall = self.socket.sendall
        readline = self.socket.makefile().readline
        try:
            while True:
                # Idea: Sand first, recieve later. If the client is to
                # slow to get the sendbuffer empty, he cannot send.
                while self.sendbuffer:
                    sendall(self.sendbuffer.popleft())
                line = readline()
                if not line:
                    break
                arguments = line.split()
                command = arguments.pop(0)
                if command == 'PX':
                    self.on_PX(arguments)
                elif command == 'SIZE':
                    self.on_SIZE(arguments)
        finally:
            self.disconnect()

    def on_SIZE(self, args):
        self.send('SIZE %d %d' % self.canvas.get_size())

    def on_PX(self, args):
        self.counter += 1
        self.limit.acquire()
        x,y,color = args
        x,y = int(x), int(y)
        c = int(color, 16)
        if c <= 16777215:
            r = (c & 0xff0000) >> 16
            g = (c & 0x00ff00) >> 8
            b =  c & 0x0000ff
            a =      0xff
        else:
            r = (c & 0xff000000) >> 24
            g = (c & 0x00ff0000) >> 16
            b = (c & 0x0000ff00) >> 8
            a =  c & 0x000000ff
        self.canvas.set_pixel(x, y, r, g, b, a)

    def tick(self):
        while self.limit.counter <= 10:
            self.limit.release()





import pygame
import cairo
import math
import random
import array

class Canvas(object):
    size  = 200,300
    flags = pygame.RESIZABLE#|pygame.FULLSCREEN

    def __init__(self):
        pygame.init()
        pygame.mixer.quit()
        pygame.display.set_caption('PIXELSTORM')
        self.screen = pygame.display.set_mode(self.size, self.flags)
        self.ticks = 0
        self.width  = self.screen.get_width()
        self.height = self.screen.get_height()
        self.clients = {}

    def serve(self, host, port):
        self.server = StreamServer((host, port), self.make_client)
        self.server.start()
        return spawn(self._loop)

    def make_client(self, socket, address):
        if address in self.clients:
            self.clients[address].disconnect()
        self.clients[address] = client = Client(self, socket, address)
        client.serve() # This blocks until ready

    def _loop(self):
        while True:
            gsleep(0) # Required to allow other tasks to run
            for e in pygame.event.get():
                if e.type == pygame.VIDEORESIZE:
                    self.on_resize(e.size)
                if e.type == pygame.KEYDOWN and e.unicode == 'q':
                    return
                if e.type == pygame.QUIT:
                    return
            self.ticks += 1
            if 0 and self.ticks % 1000 == 0:
                pygame.image.save(self.screen,
                'hist%000000d.png' % (self.ticks/1000))
            pygame.display.flip()
            for client in self.clients.itervalues():
                client.tick()

    def on_resize(self, size):
        old = self.screen.copy()
        self.screen = pygame.display.set_mode(size, self.flags)
        self.screen.blit(old, (0,0))
        self.width  = self.screen.get_width()
        self.height = self.screen.get_height()

    def get_size(self):
        return self.width, self.height

    def get_pixel(self, x, y):
        return self.screen.get_at((x,y))

    def set_pixel(self, x, y, r, g, b, a=255):
        if a == 0: return
        if a == 0xff:
            self.screen.set_at((x,y), (r,g,b))
        else:
            r2,g2,b2,a2 = self.screen.get_at((x, y))
            r = (r2*(0xff-a)+(r*a)) / 0xff
            g = (g2*(0xff-a)+(g*a)) / 0xff
            b = (b2*(0xff-a)+(b*a)) / 0xff
            self.screen.set_at((x, y), (r,g,b))

    



if __name__ == '__main__':
    canvas = Canvas()
    canvas.serve('', 2342).join()
