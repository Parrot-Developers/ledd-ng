<title>Ledd</title>
<link href="markdown.css" rel="stylesheet"></link>
<script src="toc.js" type="text/javascript"></script>
<body onload="generateTOC(document.getElementById('toc'));">

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


## libledd

### Overview

*libledd* is the library used to implement a ledd daemon.

### Usage

The API is designed to work asynchronously, with no thread and to integrate in
a file descriptor event loop, such as select, poll or epoll.

For more details, please refer to the
[doxygen documentation provided](libledd/ledd_8h.html)

The file **ledd/src/ledd/main.c** provides an example implementation of a ledd
daemon using libledd.


## Configuration files

### Overview

Ledd uses lua script files for it's configuration. Examples are provided in the
*config/* directory. The full lua standard library is available.

3 different configuration files are used, global.conf, platform.conf and patterns.conf.

Plug-ins can register functions or values to be accessible in one or more of the
config files. For example the *tricolor* led driver registers *red*, *yellow* and so on, as values for a *hue* channel.

### Syntax

This is not a comprehensive guide on how to write lua programs, just the bits to
get started with ledd.
For more in depth information, please refer to [the lua website] or in the
online book, [Programming in lua].

3 types of variables are used, *numbers*, *tables* and *strings*.
All variables can be referenced without declaration, they will have the value
*nil* until they are given another one (hence the errors of the type "expected
number, got nil").

*Strings* can be built by concatenation, using the *..* operator.
Concatenation of a number with a string converts it automatically to a string.

*Tables* are associative array, with any type except nil, being accepted for the
keys.
Elements inserted in a table with no explicit key are associated a number key,
incremented automatically, starting at index 1.

### global.conf

First and only command line argument of ledd, not mandatory.
If not provided, ledd will load **/etc/ledd/global.conf** by default.

Please refer to the *config/global.conf* file for a completely documented
example.

#### granularity

Number of milliseconds between led channels' value update. 
Updates occur only when the value for the channel has changed.
Pattern frames' duration must be a multiple of the granularity.  
Defaults to **10**ms.

#### platform\_config

Path to the platform.conf configuration file.  
Defaults to **/etc/ledd/platform.conf**.

#### patterns\_config

Path to the patterns.conf configuration file.  
Defaults to **/etc/ledd/patterns.conf**.

#### startup\_pattern

If not nil, ledd will play this pattern at startup.  
Defaults to **nil**.

#### address

Libpomp address ledd will listen to, in the [libpomp address format].  
Defaults to **unix:@ledd.socket**.

#### plugins\_dir

Directory which will be scanned to look for plug-ins, normally useful only for
development.  
Defaults to **/usr/lib/ledd-plugins**.

### platform.conf

Configuration file describing which leds are present on the product, with how
many channels and controlled by which userland drivers.

Please refer to the *config/platform.conf.tricolor* file for a completely
documented example.

It must contain a global table *leds*, whose elements are the led descriptions.

Each led description is indexed by the led's name and must contain the *driver*
element, with the name of the driver driving the channels for this led.
Admissible names depend on the drivers enabled in alchemy's config and drivers
do register themselves with a call to *led\_driver\_register()*, which you can
use to retrieve the name they are registered under.

Then comes the channels list in which each *channel* is indexed by it's name.
Each channel can contain a *parameters* element which is a string whose content
depends on the driver in use.

### patterns.conf

Configuration file describing which led patterns ledd will be able to play.

Please refer to the *config/patterns.conf.tricolor* file for a completely
documented example.

#### The patterns table

Mandatory, it's elements are the patterns descriptions indexed by their name.

#### The pattern description tables

They describe how the led channels will behave when the pattern is playing.
If a led channel is controlled by a pattern, then all it's channels will be.
A pattern can control multiple leds.

They *may* contain these optional fields:

 * **repetitions**: number of times the pattern will be played, with 0
meaning infinite.  
Defaults to **1**.
 * **intro**: when repeating a pattern, this number gives the offset in
milliseconds at which the loop will restart.  
Defaults to **0**.
 * **outro**: when repeating a pattern, this number gives the offset in
milliseconds from which the loop will go back to the intro offset, 0 means no
outro.  
Defaults to **0**.
 * **default\_value**: when a led is controlled by a pattern, all it's channels
are controlled, but not all must be explicitly designated by the pattern. All
channels not explicitly controlled are put to a fixed value, the
**default\_value**.  
Defaults to **0**.

![Intro, outro and repetitions](intro_outro_repetitions.png "See how
I'm mastering gimp ?")

It *must* contain as many pattern description tables, the **pattern channels**,
as the number of led channels the pattern must control.

#### The pattern channel tables

A pattern channel description table *must* contain the following fields:

* **led_id**: name of the led to control
* **channel_id**: name of the led's channel this pattern controls
* a list of **pattern frames** tables

#### The pattern frame tables

They *must* contain two integer numbers:

* **value** or **transition**: if the first number is in [0, 255], then it's a
**value** the led channel must take, if it is above 256, then it's a
**transition**, which must have been registered by ledd or one of it's
plugins.  
Transitions built-in ledd are *cosine* and *ramp*.
* **duration** in milliseconds, must be a multiple of the granularity defined in
the *global.conf* file.

#### Fully commented example

        patterns = {
          intro_outro = {
            {
              led_id = "pitot",
              channel_id = "hue",
              {blue, 2000},
            },
            {
              led_id = "pitot",
              channel_id = "value",
              {0x00, 10},
              {ramp, 490},
              {0xff, 250},
              {0x00, 500},
              {0xff, 250},
              {ramp, 500},
            },
            default_value = 0xFF,
            repetitions = 3,
            intro = 500,
            outro = 500,
          },
        }

This patterns table contains one pattern, **intro\_outro**.
It controls explicitly the **hue** and **value** channels of the **pitot** led,
which is controlled by the **tricolor** driver (this must have been declared
previously in a **platform.conf** file).
The **saturation** channel is controlled implicitly and it's value will be 0xFF,
or 255, during all the duration of the pattern.
The **hue** channel will stay at the color blue, or 171 as defined by the
tricolor driver, during 2000 milliseconds, which is the base duration of the
pattern.
The base behavior of the **value** channel will be: ramp from 0 (switched off),
to 0xff (maximum luminosity) in 500 ms (10 + 490), then stay on for 250 ms,
switch off during 500 ms, switch on again for 250 ms and ramp down to off in 500
ms.

Because **repetitions**, **intro** and **outro** are set, the final behavior
will change a little:

1. the value will ramp from 0 to 0xff in 500 ms (intro)
2. the led will switch off for 250 ms
3. the led will switch on for 500 ms
4. the led will switch off for 250 ms
5. jump twice to 2.
6. the value will ramp down to switched off state in 500ms

So the led will blink 3 times in the body of the pattern and the final total
pattern duration will be of 4 seconds, 500 ms (intro) + 3 * 1000 ms (main body
of the pattern) + 500 ms (outro).  
The resulting animation is shown by next image, refresh the page to replay the
animation.

![intro_outro pattern, refresh the page to replay the animation](
intro_outro.gif "You can't imagine how much time such a simple animated
gif can take to make...")

[Programming in lua]: https://www.lua.org/pil/contents.html
[the lua website]: https://www.lua.org/
[libpomp address format]: https://github.com/Parrot-Developers/libpomp/blob/master/include/libpomp.h#L859


## ledd\_client - ledd client library

### Overview

*ledd\_client* is a simple library to connect to a ledd daemon via it's libpomp
socket, locally or remotely, allowing to ask it to play led patterns.

### Usage

The API is designed to work asynchronously, with no thread and to integrate in
a file descriptor event loop, such as select, poll or epoll.

For more details, please refer to the [doxygen documentation provided](ledd_client/ledd__client_8h.html)


## utils

### ldc

Thin shell script wrapper around pomp-cli.
Allows to interact with ledd in command-line.
Please execute the **ldc help** command for a comprehensive documentation of the
ldc's command-line options.

### sldUI.py

Gtk based GUI client for a ledd socket platform.
Allows to visualize patterns in a window, thus making ledd compatible with
simulators.
Intended to serve as a documentation on how to write a ledd socket driver
libpomp client.

Please the **Usage / test** section in the main documentation for an usage
example.

## ledd\_plugin - ledd plugins library

### Overview

*ledd\_plugin* is a library to implement ledd plugins.
It provides registration functions for all the types of plug-ins available plus
some helper functions.

Some plug-ins are provided with ledd and are documented later in this section.

### Usage

For more details, please refer to the [doxygen documentation provided](ledd_plugins/ledd__plugin_8h.html)

### Plug-ins of type Driver

#### Overview

Set of provided drivers for driving leds, which can be referenced by a
*platform.conf* file.

#### file\_led\_driver

Allows to log to a file the values which would be set to the leds' channels,
mainly for debugging purpose.

The environment variable **FILE_LED_DRIVER_BACKING_FILE_PATH** will be used as
the path of the output file which will be truncated at ledd's startup.
The value defaults to **/tmp/file_led_driver**.

For example, if the platform config file has the following content :

<pre>
leds = {
	pitot = {
		driver = "file",
		channels = {
			red = { parameters = "red", },
			green = { parameters = "green", },
			blue = { parameters = "blue", },
		},
	},
}
</pre>

After a ledd run, the following script :

<pre>
#!/usr/bin/gnuplot

print "Hit Ctrl+C to quit"
plot "/tmp/file_led_driver" \
	using 2:(stringcolumn(1) eq "R" ? $3 : 1/0) title 'red channel' lc rgb "red", \
	"" using 2:(stringcolumn(1) eq "G" ? $3 : 1/0) title 'green channel' lc rgb "green", \
	"" using 2:(stringcolumn(1) eq "B" ? $3 : 1/0) title 'blue channel' lc rgb "blue"
pause -1
</pre>

will allow to visualize the curves of the channels.

#### gpio\_led\_driver

Allows to drive leds controlled by a gpio, by using the standard linux gpio
sysfs API or similar.

##### Basic operation

The *parameters* of the led channels controlled by this driver can contain
either a number (say *N*) or direcly the path to the gpio's directory.
In the former case, it will be echoed to the */sys/class/gpio/export* file and
the gpio directory used will be */sys/class/gpio/gpioN*.
The later case is mainly useful for non standard location for gpio control
files.
In both situations, the *direction* file inside the directory will be set to
*out* and the *value* file will be used to pilot the state of the led.

For exemple, the following *platform.conf* file :

<pre>
leds = {
	main = {
		driver = "gpio",
		channels = {
			blue = { parameters = "408", },
		},
	},
}
</pre>

declares one blue led wired on the gpio 408.
It's control directory will be */sys/class/gpio/gpio408*.

##### Software dimming

If the **GPIO\_LED\_DRIVER\_BIT\_BANGING\_PRECISION** environment variable is
defined to a non-zero value, the gpio will be bit-banged to obtain different
levels of brightness in a PWM fashion.
The higher the precision, the better the granularity, but if it is too high, the
flickering can become visible.
A typical good value is *50*, which will give you 50 different levels of
brightness (mapped to \[0,255\]) and will be updated at a period of 20Hz.

**Note** that enabling bit banging will created an update loop which will run at
1kHz which isn't negligible on low resource platforms.
Not defining the environment variable will make the driver consume resources
only on value update.


#### pwm\_led\_driver

Allows to drive leds controlled by a pwm, by using the standard linux pwm sysfs
API.

The *parameters* of the led channels controlled by this driver must contain a
space-separated list of *key=value* configuration pairs.
The supported configuration keys are :

##### pwm\_index

Used to export the pwm, mandatory.
The pwm control directory used will be */sys/class/pwm/pwm_N*.

##### period\_ns

Period of the pwm, written in the corresponding file, defaults to 1000000 ns.

##### max\_duty\_ns

Maximum value to be written to the *duty\_ns* control file.
Defaults to the value of *period\_ns*.

For exemple, the following *platform.conf* file :

<pre>
parameters = "pwm_index=12 max_duty_ns=900000 period_ns=1000000"
leds = {
	main = {
		driver = "pwm",
		channels = {
			white = { parameters = parameters, },
		},
	},
}
</pre>

declares one led wired on the pwm 12.
The period of the pwm will be set to 1000000ns, note that this parameter could
have been omitted, because it's the default value.
The value in \[0,255\] will be mapped linearly into the \[0,900000\] interval.

#### socket\_led\_driver

The socket led driver creates a libpomp socket to which a client can connect and
receive the led channels' values sent by ledd.
This driver can be used for example, to tweak led patterns on a PC, prior to
uploading them to an embedded target.

The *utils/sldUI.py* client demonstrates as simplistic gtk gui to visualize a
three color led.
It's usage is described in the *Usage / test* section of the top level README.md
file.

The default address for the socket created will be
*unix:/tmp/socket\_led\_driver.sock*, that is a named unix socket backed by the
*/tmp/socket\_led\_driver.sock* file.

If the **SOCKET\_LED\_DRIVER\_ADDRESS** environment variable is passed to ledd,
it's content will be used as a libpomp address, whose format is described in the
documentation of the libpomp project.
Possible value are, e.g. : unix:@my.socket (abstract unix address),
inet:IPV4\_ADDRESS:PORT or inet6:IPV6\_ADDRESS:PORT.

#### tricolor\_led\_driver

This driver allows to control an RGB tricolor led with hue, saturation and value
channels.
It must be used above any of the other led drivers available, for exemple, the
file, socket, gpio or pwm drivers provided.

The *parameters* led channel configuration key must contain the string
*"driver\_name\[|red\_chan\_prm|green\_chan\_prm|blue\_chan\_prm\]"*.
Where *driver\_name* is the name of the underlying driver used to pilot the
led's channels, where *red\_chan\_prm* is the parameters string used to
configure the red channel with the *driver\_name* driver and so on.

A limitation is that both three channels can't be driven by different drivers.

For exemple, the following *platform.conf* file :

<pre>
leds = {
	pitot = {
		driver = "tricolor",
		channels = {
			hue = { parameters = "socket", },
			saturation = { parameters = "socket", },
			value = { parameters = "socket", },
		},
	},
}
</pre>

describes a platform with a tricolor led whose values will be sent to a libpomp
socket.

The following *platform.conf* file :
<pre>
leds = {
	pitot = {
		driver = "tricolor",
		channels = {
			hue = { parameters = "gpio|408|410|412", },
			saturation = { parameters = "gpio|408|410|412", },
			value = { parameters = "gpio|408|410|412", },
		},
	},
}
</pre>

describes a platform with a tricolor led whose red, green and blue channels are
plugged respectively on the gpios 408, 410 and 412.

### Plug-ins of type Lua Globals

This directory contains the code for ledd plug-ins registering lua globals
available in ledd configuration files.

#### read_hsis Lua global function

Registers the lua function **read\_hsis\_int**, to be available in *platform*
configuration files.  
This function allows to read a file in */sys/kernel/hsis/* by giving it's name
and returns it's content.


### Plug-ins of type Transition

This directory contains the code for ledd plug-ins to register transitions types
in pattern definitions.
Built-in transition types are *ramp* and *cosine*.

#### flicker transition

Registers the transition **flicker\_transition**, to be available in *patterns*
configuration files.  
This transition is a ramp between the initial and target value with some added
noise.
It is more an example on how to implement a custom transition than a real life
useful example though.

