# ledd\_client - ledd client library

## Overview

*ledd\_client* is a simple library to connect to a ledd daemon via it's libpomp
socket, locally or remotely, allowing to ask it to play led patterns.

## Usage

The API is designed to work asynchronously, with no thread and to integrate in
a file descriptor event loop, such as select, poll or epoll.

For more details, please refer to the [doxygen documentation provided](ledd_client/ledd__client_8h.html).
A complete, functional example, is provided in **ledd\_client/example/**.

