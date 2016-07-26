# Plug-ins of type Lua Globals

This directory contains the code for ledd plug-ins registering lua globals
available in ledd configuration files.

## read_hsis Lua global function

Registers the lua function **read\_hsis\_int**, to be available in *platform*
configuration files.  
This function allows to read a file in */sys/kernel/hsis/* by giving it's name
and returns it's content.

