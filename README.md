Pixelflut: Multiplayer canvas
=============================

What happens if you give a bunch of hackers the ability to change pixel colors on a projector screen? See yourself :)

Pixelflut is a very simple (and inefficient) ASCII based network protocol to draw pixels on a screen.
You can write a basic client in a single line of shell code if you want, but you only get to change a single pixel at a time.
If you want to get rectangles, lines, text or images on the screen you have to implement that functionality yourself. That is part of the game.

Pixelflut Protocol
------------------

Pixelflut defines four main commands that are always supported to get you started:

* `HELP`: Returns a short introductional help text.
* `SIZE`: Returns the size of the canvas in pixel.
* `PX <x> <y>` Return the current color of a pixel at position (x, y).
* `PX <x> <y> <rrggbb(aa)>`: Draw a single pixel at position (x, y) with the specified hex color code.
  If the color code contains an alpha channel value, it is blended with the current color of the pixel.

You can send multiple commands over the same TCP socket by terminating each command with a single newline character (`\n`).

Example: `echo "PX 23 42 ff8000" | netcat pixelflut.example.com 1337`

Server Implementations
----------------------

This repository contains multiple implementations of the pixelflut protocol. Pull requests for additional implementations or improvements are always welcomed.

#### `/pixelflut` (python server)

Server written in Python, based on gevent and pygame. Easy to hack with, but a bit slow.

    cd pixelflut
    sudo apt-get install python-gevent python-pygame python-cairo
    mkdir save
    python pixelflut.py brain.py

#### `/pixelwar` (java server)

Server written in Java8, based on netty and awt. Optimized for speed and large player groups, fast networks or high resolution projectors.

    cd pixelwar
    sudo apt-get install maven openjdk-8-jdk
    mvn package
    java -jar target/pixelwar*-jar-with-dependencies.jar

#### `/pixelnuke` (C server)

Server written in C, based on libevent2, OpenGL, GLFW and pthreads. It won't get any faster than this.

    cd pixelnuke
    sudo apt-get install build-essential libevent-dev libglew-dev libglfw3-dev
    make
    ./pixelnuke

Pull requests that improve performance or portability (e.g. Windows or RasPI) are always welcomed.

Even more implementations
-------------------------

A fast GPU accelerated pixelflut server in Rust.  
https://github.com/timvisee/pixelpwnr-server

Links and Videos
----------------

Pixelflut at EasterHegg 2014 in Stuttgart, Germany  
http://vimeo.com/92827556

Pixelflut Bar (SHA2017)  
https://wiki.sha2017.org/w/Pixelflut_bar  
https://www.youtube.com/watch?v=1Jt-X437MKM

Pixelflut GPN17 Badge  
https://www.youtube.com/watch?v=JGg4zqqumvs
