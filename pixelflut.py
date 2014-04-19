#coding: utf8

__version__ = '0.5'

import time
from gevent import spawn, sleep as gsleep
from gevent.server import StreamServer
from gevent.coros import Semaphore
from gevent.queue import Queue
from collections import deque
import pygame
import cairo
import math
import random
import array
import os
import os.path

import logging
log = logging.getLogger('pixelflut')

async = spawn

class Client(object):
    px_per_tick = 100
    
    def __init__(self, canvas):
        self.canvas = canvas
        self.socket = None
        self.connect_ts = time.time()
        # This buffer discards all but the newest 1024 messages
        self.sendbuffer = deque([], 1024)
        # And this is used to limit clients to X messages per tick
        # We start at 0 (instead of x) to add a reconnect-penalty.
        self.limit = Semaphore(0)

    def send(self, line):
        self.sendbuffer.append(line.strip() + '\n')

    def nospam(self, line):
        if not self.sendbuffer:
            self.sendbuffer.append(line.strip() + '\n')

    def disconnect(self):
        if self.socket:
            self.socket.close()
            self.socket = None

    def serve(self, socket):
        self.socket = socket
        sendall = self.socket.sendall
        readline = self.socket.makefile().readline
        try:
            while True:
                self.limit.acquire()
                # Idea: Send first, receive later. If the client is to
                # slow to get the send-buffer empty, he cannot send.
                while self.sendbuffer:
                    sendall(self.sendbuffer.popleft())
                line = readline().strip()
                if not line:
                    break
                arguments = line.split()
                command = arguments.pop(0)
                try:
                    self.canvas.fire('COMMAND-%s' % command.upper(), self, *arguments)
                except TypeError, e:
                    self.nospam('ERROR %r :(' % e)
        finally:
            self.disconnect()

    def tick(self):
        while self.limit.counter <= self.px_per_tick:
            self.limit.release()






class Canvas(object):
    size  = 640,480
    flags = pygame.RESIZABLE#|pygame.FULLSCREEN

    def __init__(self):
        pygame.init()
        pygame.mixer.quit()
        self.set_title()
        self.screen = pygame.display.set_mode(self.size, self.flags)
        self.ticks = 0
        self.width  = self.screen.get_width()
        self.height = self.screen.get_height()
        self.clients = {}
        self.events = {}
        self.font = pygame.font.Font(None, 17)

    def serve(self, host, port):
        self.server = StreamServer((host, port), self.make_client)
        self.server.start()
        return spawn(self._loop)

    def make_client(self, socket, address):
        ip = address[0]
        client = self.clients.get(ip)
        if client:
            client.disconnect()
        else:
            self.clients[ip] = client = Client(self)

        try:
            self.fire('CONNECT', client)
            client.serve(socket) # This blocks until ready
            self.fire('DISCONNECT', client)
        finally:
            client.disconnect()

    def _loop(self):
        while True:
            gsleep(0.01) # Required to allow other tasks to run
            for e in pygame.event.get():
                if e.type == pygame.VIDEORESIZE:
                    old = self.screen.copy()
                    self.screen = pygame.display.set_mode(e.size, self.flags)
                    self.screen.blit(old, (0,0))
                    self.width, self.height = e.size
                    self.fire('RESIZE')
                elif e.type == pygame.QUIT:
                    self.fire('QUIT')
                    return
                elif e.type == pygame.KEYDOWN:
                    self.fire('KEYDOWN-' + e.unicode)
            self.ticks += 1
            self.fire('TICK', self.ticks)
            for c in self.clients.values():
                c.tick()
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
            try:
                self.events[name](self, *a, **ka)
            except:
                log.exception('Error in callback for %r', name)

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
        screen = self.screen
        if a == 0xff:
            screen.set_at((x, y), (r,g,b))
        elif 0 <= x < self.width and 0 <= y < self.height:
            r2, g2, b2, a2 = screen.get_at((x, y))
            r = (r2*(0xff-a)+(r*a)) / 0xff
            g = (g2*(0xff-a)+(g*a)) / 0xff
            b = (b2*(0xff-a)+(b*a)) / 0xff
            screen.set_at((x, y), (r,g,b))

    def clear(self, r=0, g=0, b=0, a=255):
        ''' Fill the entire screen with a solid colour (default: black)'''
        self.screen.fill((r, g, b))

    def save_as(self, filename):
        ''' Save screen to disk. '''
        pygame.image.save(self.screen, filename)

    def load_from(self, filename):
        img = pygame.image.load(filename).convert()
        self.screen.blit(img, (0,0))

    def load_font(self, fname):
        ''' Load a font image with 16x16 sprites. '''
        self.font_img = pygame.image.load(fname).convert()
        self.font_res = int(self.font_img.get_width())/16        

    def set_title(self, text=None):
        title = 'P1XELFLUT'
        if text:
            title += ' ' + text
        pygame.display.set_caption(title)

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



if __name__ == '__main__':
    logging.basicConfig()

    import optparse
    parser = optparse.OptionParser("usage: %prog [options] brain_script")
    parser.add_option("-H", "--host", dest="hostname",
                      default="0.0.0.0", type="string",
                      help="specify hostname to run on")
    parser.add_option("-p", "--port", dest="portnum", default=1234,
                      type="int", help="port number to run on")

    (options, args) = parser.parse_args()
    if len(args) != 1:
        parser.error("incorrect number of arguments")

    canvas = Canvas()
    task = canvas.serve(options.hostname, options.portnum)
    
    brainfile = args[0]
    mtime = 0
    
    while True:
        gsleep(1)
        if mtime < os.stat(brainfile).st_mtime:
            canvas.fire('UNLOAD')
            canvas.events.clear()
            try:
                execfile(brainfile, {'on':canvas.on, '__file__': brainfile})
            except:
                log.exception('Brain failed')
                continue
            canvas.fire('LOAD')
            mtime = os.stat(brainfile).st_mtime
    
    task.join()

