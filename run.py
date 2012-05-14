import pixelflut


def guess_IP():
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("google.com", 80))
        return s.getsockname()[0]
    finally:
        s.close()


port = 2342
text  = 'P1XELFLUT! v%s\n' % pixelflut.__version__
text += 'Connect to %s:%d\n\n' % (guess_IP(), port)
text += '>>> SIZE\n'
text += '>>> PX x y hex-color\n'
text += '... and more ...\n\n'
text += 'H A C K  O N\n'

canvas = pixelflut.Canvas()

@canvas.on('START')
def callback(c):
    c.load_font('./font.png')
    pixelflut.async(c.text, 5, 5, text, delay=0.1)

@canvas.on('RESIZE')
def callback(c):
    c.text(5, 5, 'Screen Size: %dx%d' % c.get_size())

@canvas.on('CONNECT')
def callback(c, client):
    c.text(5, 5, 'Connect: %s' % client.address[0])

@canvas.on('KEYDOWN-c')
def callback(c):
    c.clear()

@canvas.on('KEYDOWN-s')
def callback(c):
    import os
    i, mask = 0, 'screen%05d.png'
    while os.path.exists(mask%i): i += 1 
    c.save_as(mask%i)
    c.text(5,5, 'Saved as %s' % mask % i)

@canvas.on('COMMAND-CLEAR')
def callback(canvas, client, *args):
    canvas.clear()

task = canvas.serve('0.0.0.0', port)
task.join()
