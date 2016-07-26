# Configuration files

## Overview

Ledd uses lua script files for it's configuration. Examples are provided in the
*config/* directory. The full lua standard library is available.

3 different configuration files are used, global.conf, platform.conf and patterns.conf.

Plug-ins can register functions or values to be accessible in one or more of the
config files. For example the *tricolor* led driver registers *red*, *yellow* and so on, as values for a *hue* channel.

## Syntax

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

## global.conf

First and only command line argument of ledd, not mandatory.
If not provided, ledd will load **/etc/ledd/global.conf** by default.

Please refer to the *config/global.conf* file for a completely documented
example.

### granularity

Number of milliseconds between led channels' value update. 
Updates occur only when the value for the channel has changed.
Pattern frames' duration must be a multiple of the granularity.  
Defaults to **10**ms.

### platform\_config

Path to the platform.conf configuration file.  
Defaults to **/etc/ledd/platform.conf**.

### patterns\_config

Path to the patterns.conf configuration file.  
Defaults to **/etc/ledd/patterns.conf**.

### startup\_pattern

If not nil, ledd will play this pattern at startup.  
Defaults to **nil**.

### address

Libpomp address ledd will listen to, in the [libpomp address format].  
Defaults to **unix:@ledd.socket**.

### plugins\_dir

Directory which will be scanned to look for plug-ins, normally useful only for
development.  
Defaults to **/usr/lib/ledd-plugins**.

## platform.conf

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

## patterns.conf

Configuration file describing which led patterns ledd will be able to play.

Please refer to the *config/patterns.conf.tricolor* file for a completely
documented example.

### The patterns table

Mandatory, it's elements are the patterns descriptions indexed by their name.

### The pattern description tables

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

### The pattern channel tables

A pattern channel description table *must* contain the following fields:

* **led_id**: name of the led to control
* **channel_id**: name of the led's channel this pattern controls
* a list of **pattern frames** tables

### The pattern frame tables

They *must* contain two integer numbers:

* **value** or **transition**: if the first number is in [0, 255], then it's a
**value** the led channel must take, if it is above 256, then it's a
**transition**, which must have been registered by ledd or one of it's
plugins.  
Transitions built-in ledd are *cosine* and *ramp*.
* **duration** in milliseconds, must be a multiple of the granularity defined in
the *global.conf* file.

### Fully commented example

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

