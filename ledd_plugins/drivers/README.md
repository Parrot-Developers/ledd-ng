# Plug-ins of type Driver

## Overview

Set of provided drivers for driving leds, which can be referenced by a
*platform.conf* file.

## file\_led\_driver

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

## gpio\_led\_driver

Allows to drive leds controlled by a gpio, by using the standard linux gpio
sysfs API or similar.

### Basic operation

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

### Software dimming

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


## pwm\_led\_driver

Allows to drive leds controlled by a pwm, by using the standard linux pwm sysfs
API.

The *parameters* of the led channels controlled by this driver must contain a
space-separated list of *key=value* configuration pairs.
The supported configuration keys are :

### pwm\_index

Used to export the pwm, mandatory.
The pwm control directory used will be */sys/class/pwm/pwm_N*.

### period\_ns

Period of the pwm, written in the corresponding file, defaults to 1000000 ns.

### max\_duty\_ns

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

## socket\_led\_driver

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

## tricolor\_led\_driver

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
