#!/usr/bin/lua

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
