#!/usr/bin/lua

--------------------------------------------------------------------------------
-- @file intro_outro.lua
--
-- Copyright (c) 2016 Parrot S.A.
-- All rights reserved.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are met:
--   * Redistributions of source code must retain the above copyright
--     notice, this list of conditions and the following disclaimer.
--   * Redistributions in binary form must reproduce the above copyright
--     notice, this list of conditions and the following disclaimer in the
--     documentation and/or other materials provided with the distribution.
--   * Neither the name of the Parrot Company nor the
--     names of its contributors may be used to endorse or promote products
--     derived from this software without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
-- DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
-- (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
-- LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
-- ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
-- (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
-- THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------------------------------------------

-- generate shell commands to be executed for the generation of an animated gif
-- simulating the intro_outro pattern

local i = 0
local j = 0
local frame = 0

local format = "convert -size 100x100  xc:#0000%02x canvas_%04d.jpg"
local format2 = "%s -delay 4 canvas_%04d.jpg"
local step = 4

local gif_creation_command = "convert -loop 1"

for i=0,49,step do
	frame = frame + step
	print(string.format(format, i * 255 / 49, frame)) 
	gif_creation_command = string.format(format2, gif_creation_command, frame)
end

for j=0,2 do
	for i=0,249,(step * 10) do
		print(string.format(format, 255, frame)) 
		gif_creation_command = string.format(format2, gif_creation_command, frame)
		frame = frame + step
	end
	for i=0,490,(step * 10) do
		print(string.format(format, 0, frame)) 
		gif_creation_command = string.format(format2, gif_creation_command, frame)
		frame = frame + step
	end
	for i=0,240,(step * 10) do
		print(string.format(format, 255, frame)) 
		gif_creation_command = string.format(format2, gif_creation_command, frame)
		frame = frame + step
	end
end

for i=0,49,step do
	print(string.format(format, (49 - i) * 255 / 49, frame)) 
	gif_creation_command = string.format(format2, gif_creation_command, frame)
	frame = frame + step
end

gif_creation_command = gif_creation_command .. " intro_outro.gif" 
print(gif_creation_command)
