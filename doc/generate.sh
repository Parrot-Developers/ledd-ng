#!/bin/bash

###############################################################################
# @file generate.sh
#
# Copyright (c) 2016 Parrot S.A.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#   * Neither the name of the Parrot Company nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
################################################################################

set -eu

# Generates the documentation for the ledd project from all the README.md's and
# the API's doxyfiles

# Must be executed at the ledd project's root
if [ ! -d doc -o ! -d ledd_plugins ]; then
	echo "must be executed at the ledd project's root"
	exit 1
fi

rm -rf doc/generated/
mkdir -p doc/generated/
for f in intro_outro.gif intro_outro_repetitions.png markdown.css toc.js; do
	cp doc/${f} doc/generated/${f}
done

# we have to enable the nullglob option to avoid */*/README.md to expand as
# */*/README.md if no match is found
shopt -s nullglob

echo "concatenate makdown files with a header for css"
(echo -e "<title>Ledd</title>
<link href=\"markdown.css\" rel=\"stylesheet\"></link>
<script src=\"toc.js\" type=\"text/javascript\"></script>
<body onload=\"generateTOC(document.getElementById('toc'));\">\n"; \
	sed -e '$a\' README.md;
	echo;) \
	> doc/generated/ledd.md

for d in ledd config ledd_client utils ledd_plugins; do sed -e '$a\' ${d}/README.md \
	| sed 's/^\(#\+\) /\1# /g' ; echo; done \
	>> doc/generated/ledd.md

# we want the list to end with ledd_plugins so that plugins doc follow
for f in */*/README.md; do sed -e '$a\' ${f} \
	| sed 's/^\(#\+\) /\1## /g' ; echo; done \
	>> doc/generated/ledd.md

echo "convert to html"
pandoc -s doc/generated/ledd.md > doc/generated/ledd.html

common_doxygen_options="
GENERATE_LATEX         = NO
OPTIMIZE_OUTPUT_FOR_C  = YES
EXTRACT_ALL            = YES
HTML_OUTPUT            = .
"

echo "generate doxygen documentation for ledd_client"
name=ledd_client
name_md5=$(echo ${name} | md5sum -)
color=$(( 0x${name_md5::3} % 360 ))
project_doxygen_options="
INPUT                  = ledd_client/include/
OUTPUT_DIRECTORY       = doc/generated/${name}/
PROJECT_NAME           = ${name}
PROJECT_BRIEF          = \"Client library for the ledd daemon\"
HTML_COLORSTYLE_HUE    = ${color}
"

echo "$(doxygen -g -)" \
	"${common_doxygen_options}" \
	"${project_doxygen_options}" | \
	doxygen - > /dev/null

echo "generate doxygen documentation for libledd"
name=libledd
name_md5=$(echo ${name} | md5sum -)
color=$(( 0x${name_md5::3} % 360 ))
project_doxygen_options="
INPUT                  = ledd/include/
OUTPUT_DIRECTORY       = doc/generated/${name}/
PROJECT_NAME           = ${name}
PROJECT_BRIEF          = \"Library implementing a ledd daemon\"
HTML_COLORSTYLE_HUE    = ${color}
"

echo "$(doxygen -g -)" \
	"${common_doxygen_options}" \
	"${project_doxygen_options}" | \
	doxygen - > /dev/null

echo "generate doxygen documentation for ledd_plugin"
name=ledd_plugins
name_md5=$(echo ${name} | md5sum -)
color=$(( 0x${name_md5::3} % 360 ))
project_doxygen_options="
INPUT                  = ledd_plugins/include/
OUTPUT_DIRECTORY       = doc/generated/${name}/
PROJECT_NAME           = ${name}
PROJECT_BRIEF          = \"Library for implementing ledd plug-ins\"
HTML_COLORSTYLE_HUE    = ${color}
"

echo "$(doxygen -g -)" \
	"${common_doxygen_options}" \
	"${project_doxygen_options}" | \
	doxygen - > /dev/null

