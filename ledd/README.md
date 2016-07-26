# libledd

## Overview

*libledd* is the library used to implement a ledd daemon.

## Usage

The API is designed to work asynchronously, with no thread and to integrate in
a file descriptor event loop, such as select, poll or epoll.

For more details, please refer to the
[doxygen documentation provided](libledd/ledd_8h.html)

The file **ledd/src/ledd/main.c** provides an example implementation of a ledd
daemon using libledd.

