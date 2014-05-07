Pixelflut & Pixelwar
====================

Multiplayer canvas. 

Pixelflut (python)
------------------

Gevent and pygame based python implementation. easier to hack with, but a bit slow. Not recommended for more than 20 players.

    sudo aptitude install python-gevent python-pygame
    cd pixelflut
    python pixelflut.py brain.py

Pixelwar (java)
---------------

Netty based java7 implementation. Very fast but not scriptable (yet). 

    sudo aptitude install maven2 openjdk-7-jdk
    cd pixelwar
    mvn package
    java -jar package/pixelwar*-jar-with-dependencies.jar




