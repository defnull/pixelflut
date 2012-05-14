#coding: utf8

__version__ = '0.4'

import time
from gevent import spawn, sleep as gsleep
from gevent.server import StreamServer
from gevent.coros import Semaphore
from gevent.queue import Queue
from collections import deque

async = spawn

class Client(object):
    px_per_tick = 10
    
    def __init__(self, canvas, socket, address):
        self.canvas = canvas
        self.socket = socket
        self.address = address
        self.connect_ts = time.time()
        # This buffer discards all but the newest 1024 messages
        self.sendbuffer = deque([], 1024)
        # And this is used to limit clients to X messages per tick
        # We start at 0 (instead of x) to add a reconnect-penalty.
        self.limit = Semaphore(0)
        print 'CONNECT', address

    def send(self, line):
        self.sendbuffer.append(line.strip() + '\n')

    def disconnect(self):
        print 'DISCONNECT', self.address
        self.socket.close()
        del self.canvas.clients[self.address]

    def serve(self):
        sendall = self.socket.sendall
        readline = self.socket.makefile().readline
        try:
            while True:
                # Idea: Send first, receive later. If the client is to
                # slow to get the send-buffer empty, he cannot send.
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
                else:
                    self.canvas.fire('COMMAND-%s' % command.upper, self, *arguments)
        finally:
            self.disconnect()

    def on_SIZE(self, args):
        self.send('SIZE %d %d' % self.canvas.get_size())

    def on_PX(self, args):
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
        while self.limit.counter <= self.px_per_tick:
            self.limit.release()




import pygame
import cairo
import math
import random
import array

class Canvas(object):
    size  = 640,480
    flags = pygame.RESIZABLE#|pygame.FULLSCREEN

    def __init__(self):
        pygame.init()
        pygame.mixer.quit()
        pygame.display.set_caption('P1XELFLUT')
        self.screen = pygame.display.set_mode(self.size, self.flags)
        self.ticks = 0
        self.width  = self.screen.get_width()
        self.height = self.screen.get_height()
        self.clients = {}
        self.events = {}
    
    def serve(self, host, port):
        self.server = StreamServer((host, port), self.make_client)
        self.server.start()
        return spawn(self._loop)

    def make_client(self, socket, address):
        if address in self.clients:
            self.clients[address].disconnect()
        self.clients[address] = client = Client(self, socket, address)
        self.fire('CONNECT', client)
        client.serve() # This blocks until ready
        self.fire('DISCONNECT', client)

    def _loop(self):
        self.fire('START')
        while True:
            gsleep(0.01) # Required to allow other tasks to run
            if not self.ticks % 10:
                for e in pygame.event.get():
                    if e.type == pygame.VIDEORESIZE:
                        old = self.screen.copy()
                        self.screen = pygame.display.set_mode(e.size, self.flags)
                        self.screen.blit(old, (0,0))
                        self.width  = self.screen.get_width()
                        self.height = self.screen.get_height()
                        self.fire('RESIZE')
                    elif e.type == pygame.QUIT:
                        self.fire('QUIT')
                        return
                    elif e.type == pygame.KEYDOWN:
                        self.fire('KEYDOWN-' + e.unicode)
            self.ticks += 1
            self.fire('TICK', self.ticks)
            pygame.display.flip()

    def on(self, name):
        ''' If used as a decorator, binds a function to an event. '''
        def decorator(func):
            self.events[name] = func
            return func
        return decorator

    def fire(self, name, *a, **ka):
        ''' Fire an event. '''
        if name in self.events:
            self.events[name](self, *a, **ka)

    def get_size(self):
        ''' Get the current screen dimension as a (width, height) tuple.'''
        return self.width, self.height

    def get_pixel(self, x, y):
        ''' Get colour of a pixel as an (r,g,b) tuple. '''
        return self.screen.get_at((x,y))

    def set_pixel(self, x, y, r, g, b, a=255):
        ''' Change the colour of a pixel. If an alpha value is given, the new
            colour is mixed with the old colour accordingly. '''
        if a == 0: return
        if a == 0xff:
            self.screen.set_at((x,y), (r,g,b))
        else:
            r2,g2,b2,a2 = self.screen.get_at((x, y))
            r = (r2*(0xff-a)+(r*a)) / 0xff
            g = (g2*(0xff-a)+(g*a)) / 0xff
            b = (b2*(0xff-a)+(b*a)) / 0xff
            self.screen.set_at((x, y), (r,g,b))

    def clear(self, r=0, g=0, b=0):
        ''' Fill the entire screen with a solid colour (default: black)'''
        self.screen.fill((r,g,b))

    def save_as(self, filename):
        ''' Save screen to disk. '''
        pygame.image.save(self.screen, filename)

    def load_font(self, fname):
        ''' Load a font image with 16x16 sprites. '''
        self.font_img = pygame.image.load(fname).convert()
        self.font_res = int(self.font_img.get_width())/16        

    def putc(self, x, y, c):
        if not self.font_img:
            self.load_font('font.png')
        fx = (c%16) * self.font_res
        fy = (c/16) * self.font_res
        self.screen.blit(self.font_img, (x,y),
                         (fx,fy,self.font_res,self.font_res))

    def text(self, x, y, text, delay=0, linespace=1):
        for i, line in enumerate(text.splitlines()):
            line += '   '
            for j, c in enumerate(line):
                self.putc(x+j*self.font_res, y+i*self.font_res, ord(c))
                gsleep(delay)
            y += linespace



