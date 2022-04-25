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
* `SIZE`: Returns the size of the visible canvas in pixel as `SIZE <w> <h>`.
* `PX <x> <y>` Return the current color of a pixel as `PX <x> <y> <rrggbb>`.
* `PX <x> <y> <rrggbb(aa)>`: Draw a single pixel at position (x, y) with the specified hex color code.
  If the color code contains an alpha channel value, it is blended with the current color of the pixel.

You can send multiple commands over the same connection by terminating each command with a single newline character (`\n`).

Example:

    $ echo "SIZE" | netcat pixelflut.example.com 1337
    SIZE 800 600
    $ echo "PX 23 42 ff8000" | netcat pixelflut.example.com 1337
    $ echo "PX 32 42" | netcat pixelflut.example.com 1337
    PX 23 42 ff8000

Implementations MAY support additional commands or have less strict parsing rules (e.g. allow `\r\n` or any whitespace between parameters) but they MUST support the commands above. 

Server Implementations
----------------------

This repository contains multiple implementations of the pixelflut protocol. Pull requests for additional implementations or improvements are always welcomed.

### `/pixelflut` (python server)

Server written in Python, based on gevent and pygame. Easy to hack with, but a bit slow. In fact, it was slowed down on purpose to be more fair and encourage smart drawing techniques instead of image spamming. Perfect for small groups.

    cd pixelflut
    sudo apt-get install python-gevent python-pygame python-cairo
    mkdir save
    python pixelflut.py brain.py

#### `/pixelwar` (java server)

Server written in Java8, based on netty and awt. Optimized for speed and large player groups, fast networks or high-resolution projectors. This is probably the most portable version and runs on windows, too.

    cd pixelwar
    sudo apt-get install maven openjdk-8-jdk
    mvn package
    java -jar target/pixelwar*-jar-with-dependencies.jar
    
or, if you have docker installed but don't want to install maven:

    docker run -it --rm --user "`id -u`:`id -g`" --volume "`pwd`:/build" --workdir /build maven:3-jdk-8-alpine mvn -Duser.home=/build clean package
    java -jar target/pixelwar*-jar-with-dependencies.jar

#### `/pixelnuke` (C server)

Server written in C, based on libevent2, OpenGL, GLFW and pthreads. It won't get any faster than this. Perfect for fast networks and large groups.

    cd pixelnuke
    sudo apt-get install build-essential libevent-dev libglew-dev libglfw3-dev
    make
    ./pixelnuke

Commandline Arguments:

* `H`: Show help for commandline arguments
* `F[screen_id]`: Start fullscreened. You can additionally specify the fullscreen monitor by adding a screen id from 0 to 9 after the F

Keyboard controls:

* `F11`: Toggle between fullscreen and windowed mode
* `F12`: Switch between multiple monitors in fullscreen mode
* `c`: Clear the screen (50% black, hit multiple times)
* `q` or `ESC`: Quit

Additional Commands:

* `STATS` Return statistics as `STATS <name>:<value> ...`
  * `px:<uint>` Number of pixels drawn so far. Will overflow eventually.
  * `conn:<uint>` Number of currently connected clients.

Planned Features:
- [x] Toggle between windowed/fullscreen mode and switch monitors in fullscreen mode.
- [ ] Persist pixel buffer between restarts. Use an mmap-ed file for pixel data?
- [ ] Save to PPM (via key, timer or admin command) and add docs/tools to convert these into a video.
- [ ] Support to draw directly to a framebuffer (no OpenGL or X Server dependency -> Raspberry-PI compatible)
- [ ] Showcase-Mode: Players won't draw at the same time, but take turns. Each player gets N seconds of exclusive draw time)
- [ ] Limit concurrent connections on a per IP basis.
- [ ] Admin commands: Unlock additional commands with a password (e.g. `PX2 <x> <y> <rrggbbaa>` to draw to the overlay layer)


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

Rüspeler Tüfteltage (Kliemannsland, 2018)
https://youtu.be/TijSQYZoRUU?t=6m

