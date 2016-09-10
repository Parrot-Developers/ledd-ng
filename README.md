# ledd - led driving daemon

## Overview

Ledd is a daemon responsible of controlling the leds of a product.
It is controlled by a [libpomp] socket on any transport method it supports.
Several led controlling methods, or userland drivers are provided, such as
gpios and pwms.
"Virtual" leds can be controled too, writing data on a file or on a libpomp
socket.

The main parts of ledd have their own *README.md* file in their directory.
A full HTML documentation is generated from them in *doc/generated/ledd.html*,
by executing **./doc/generate.sh** at the root of ledd's source directory.

## Quick start

The following instructions will help you build and configure a ledd daemon, to
run on a pc, simulating a tri-color led with a gtk window.

1. install the needed tools, as root

        apt-get update
        apt-get install parallel git python3 make gcc pkg-config g++ \
            libcunit1-dev liblua5.2-dev linux-headers-amd64 python-gtk2 \
            python-gobject lua5.2

2. prepare a workspace with the source and all the needed dependencies

        mkdir ledd
        cd ledd
        wget -O - https://raw.githubusercontent.com/ncarrier/ledd/master/doc/sources | parallel git clone

3. setup convenient environment variables for the build

        . ledd-ng/doc/setenv

4. build:

        alchemake all final -j 5

5. create a configuration

        sed "s#\(workspace = \).*#\1\"$PWD/\"#g" ledd-ng/config/global.conf > global.conf

6. setup convenient environment variables for runtime

        . Alchemy-out/linux-native-x64/final/native-wrapper.sh

7. launch ledd

        ledd-ng ./global.conf &

If problems arise, increase the verbosity by setting the environment variable
*ULOG_LEVEL=D* and adapt the configuration.

8. in another window, launch the socket driver python example client in
background

        PYTHONPATH=./libpomp/python ./ledd-ng/utils/sldUI.py unix:/tmp/socket_led_driver.sock &

9. then ask ledd to play a pattern

        ldc set_pattern color_rotation false  

and see the result in the client's window.

10. then you can quit ledd with

        ldc quit

<h2 id="toc_title"></h2>

<div id="toc"></div>

## Features

### Lua configuration files

No need to know the lua syntax, example configuration files are provided in
*config/* and should be straight forward to understand.

Three configuration files (which can be merged in one big file) are used :

* *global.conf* : handles ledd's global configuration.
* *platform.conf* : describes the leds present on the product, with their
   channels and the userland driver they use
* *patterns.conf* : contains the ledd patterns descriptions, independently of
   the hardware it is used on

Full access to lua's standard library and features is available, although not
mandatory to use.

### Plugin system

The plug-in system allows for now to register three types of things :

 * a *userland led driver*, basically by implementing a *set\_value* callback
 * a new *led state transition*, more info in the *ledd\_plugins/transitions*
folder's readme
 * *lua global values or functions*, accessible in *platform.conf* or
*patterns.conf*

Examples of drivers, transitions and lua globals plug-ins are provided in the
*ledd\_plugins/* directory.

### Different integration methods available

Depending on the situation you're in, you have the following solution in order
to integrate ledd :

* as a *standalone system daemon*, a boxinit service file is provided in
*config/*, but any service monitoring facily can be used, such as systemd or
busybox' init.  
    this is the nominal case
* as a *shared library*, to be integrated on an event loop via it's exposed fd  
    if you want, for example, to limit the processes in resources constrained
    environments (e.g. P6 products)
* as a *static library*, in an event loop and with all the plugins built-in  
    the typical use case is for an installer UI driving the leds

### Other features

* *simulator friendly*  
With the *socket* plugin, a libpomp client can receive the values sent by ledd.
An example client is provided in utils/sldUI.py.
* *logs via ulog*
