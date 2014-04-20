import pixelflut
import os
import time

pixelcount = 0

def guess_IP():
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("google.com", 80))
        return s.getsockname()[0]
    finally:
        s.close()

IP = guess_IP()

port = 1234
text  = 'P1XELFLUT! v%s (%d)\n' % (
    pixelflut.__version__,
    os.stat(__file__).st_mtime)
text += '$ echo "HELP" | netcat %s %d\n' % (IP, port)
text += 'https://github.com/defnull/pixelflut'

help = 'Commands:'
help += '>>> HELP\n'
help += '>>> SIZE\n'
help += '>>> TEXT x y text\n'
help += '>>> PX x y [RRGGBB (hex)]\n'

@on('LOAD')
def callback(c):
    c.load_font('./font.png')
    c.set_title('@  %s:%d' % (IP, port))

@on('UNLOAD')
def callback(canvas):
    canvas.text(5, 5, 'Reloading ...')

@on('RESIZE')
def on_resize(c):
    c.text(5, 5, 'Screen Size: %dx%d' % c.get_size())

@on('CONNECT')
def on_connect(c, client):
    pass
    #print client

@on('KEYDOWN-c')
def on_key_c(c):
    c.clear()
    pixelflut.async(c.text, 5, 5, text, delay=0.1)

@on('KEYDOWN-s')
def on_key_s(c):
    import os
    i, mask = 0, 'screen%05d.png'
    while os.path.exists(mask%i): i += 1 
    c.save_as(mask%i)
    c.text(5,5, 'Saved as %s' % mask % i)



@on('COMMAND-HELP')
def on_help(canvas, client):
    client.send(help)

@on('COMMAND-TEXT')
def on_text(canvas, client, x, y, *words):
    x, y = int(x), int(y)
    text = ' '.join(words)[:200]
    canvas.text(x, y, text, delay=0.5)

@on('COMMAND-SIZE')
def on_size(canvas, client):
    client.send('SIZE %d %d' % canvas.get_size())

@on('COMMAND-QUIT')
def on_quit(canvas, client):
    client.disconnect()

@on('COMMAND-PX')
def on_px(canvas, client, x, y, color=None):
    global pixelcount
    pixelcount += 1
    client.last_pixel = time.time()
    x, y = int(x), int(y)
    if color:
        c = int(color, 16)
        if len(color) == 6:
            r = (c & 0xff0000) >> 16
            g = (c & 0x00ff00) >> 8
            b =  c & 0x0000ff
            a =      0xff
        elif len(color) == 8:
            r = (c & 0xff000000) >> 24
            g = (c & 0x00ff0000) >> 16
            b = (c & 0x0000ff00) >> 8
            a =  c & 0x000000ff
        else:
            return
        canvas.set_pixel(x, y, r, g, b, a)
    else:
        r,g,b,a = canvas.get_pixel(x,y)
        client.send('PX %d %d %02x%02x%02x%02x' % (x,y,r,g,b,a))



last_save = 0

@on('TICK')
def on_tick(canvas, dt):
    global last_save, pixelcount
    canvas.text(5, 5, text, delay=0)

    if time.time() > last_save:
        last_save = time.time() + 5
        canvas.save_as('save/mov_%d.png' % last_save)
        print len(canvas.clients)
        canvas.text(5, 200, 'px/s %d' % (pixelcount/5), delay=0)
        canvas.text(5, 208, 'Connections %d' % len([c for c in canvas.clients.values() if c.socket]), delay=0)
        pixelcount = 0
