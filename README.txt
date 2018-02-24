Pixelflut & Pixelwar: Multiplayer canvas. 
=========================================

What happens if you give a bunch of hackers the ability to change pixel colors on a beamer screen? See yourself :)

p1xelflut uses a very simple (and inefficient) ASCII based network protocol. You can write a basic client in a single line of shell code if you want, but you only get to change a single pixel at a time. If you want to get rectangles, lines, text or images on the screen you have to implement that functionality yourself. That is part of the game.



Pixelflut (python)
------------------

Gevent and pygame based python implementation. easier to hack with, but a bit slow. Not recommended for more than 20 players.

    sudo aptitude install python-gevent python-pygame python-cairo
    cd pixelflut
    mkdir save
    python pixelflut.py brain.py

Pixelwar (java)
---------------

Netty based java7 implementation. Very fast but not scriptable (yet). 

    sudo aptitude install maven2 openjdk-7-jdk
    cd pixelwar
    mvn package
    java -jar target/pixelwar*-jar-with-dependencies.jar

Links
-----

Pixelflut at EasterHegg 2014 in Stuttgart, Germany: http://vimeo.com/92827556




