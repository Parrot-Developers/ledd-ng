# ledd - led driving daemon

## Table of content

<div id="toc"></div>

## Overview

Ledd is a daemon responsible of controlling the leds of a product.
It is controlled by a [libpomp] socket on any transport method it supports.

The main parts of ledd have their own *README.md* file in their directory.
A full HTML documentation is generated from them in *doc/ledd.html*, by
executing **./doc/generate.sh** at the root of ledd's source directory.

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
*config/*  
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

## Usage / test

These instructions need that you are at the root of a workspace with ledd, in
which *pclinux* is a native variant, the example is ran in an evinrude
workspace.

1. build ledd and the needed bits:  
        ./build.sh -p pclinux -A ledd pwm_led_driver zzz_tricolor_led_driver socket_led_driver read_hsis flicker_transition ldc pomp-cli final -j
2. create a configuration:  
        sed "s#\\(workspace = \\).*#\\1\\"$PWD/\\"#g" packages/ledd/config/global.conf  > ledd.global.conf
3. launch ledd:  
        ULOG_STDERR=y ./out/evinrude-pclinux/final/native-wrapper.sh ledd ./ledd.global.conf  
If problems arise, increase the verbosity by setting the environment variable
*ULOG_LEVEL=D* and adapt the configuration.
4. in another window, launch the socket driver python example client:  
        PYTHONPATH=/home/ncarrier/drones_workspace/packages/libpomp/python ./packages/ledd/utils/sldUI.py unix:/tmp/socket_led_driver.sock
5. in a third window, ask ledd to play a pattern:  
        ./out/evinrude-pclinux/final/native-wrapper.sh ldc set_pattern color_rotation false  
and see the result in the client's window
6. then you can quit ledd with:  
        ./out/evinrude-pclinux/final/native-wrapper.sh ldc quit

[libpomp]: https://github.com/Parrot-Developers/libpomp

