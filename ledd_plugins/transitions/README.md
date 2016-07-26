# Plug-ins of type Transition

This directory contains the code for ledd plug-ins to register transitions types
in pattern definitions.
Built-in transition types are *ramp* and *cosine*.

## flicker transition

Registers the transition **flicker\_transition**, to be available in *patterns*
configuration files.  
This transition is a ramp between the initial and target value with some added
noise.
It is more an example on how to implement a custom transition than a real life
useful example though.
