#!/usr/bin/python
import gobject
import pygtk
import gtk
import pomp
import logging
import sys


class SocketLedDriverClient(object):
    def __init__(self, address):
        super(SocketLedDriverClient, self).__init__()
        window = gtk.Window()
        window.set_title("Socket led driver client - evinrude")
        window.connect("destroy", self.on_window_destroy)

        pomp.looper.prepareLoop(self)
        self.ctx = pomp.Context(self)
        (self.sockFamily, self.sockAddr) = pomp.parseAddr(address)
        self.ctx.connect(self.sockFamily, self.sockAddr)

        self.label = gtk.Label()
        self.label.set_text("DISCONNECTED")
        self.event_box = gtk.EventBox()
        self.event_box.add(self.label)
        self.color = gtk.gdk.Color()
        self.event_box.modify_bg(gtk.STATE_NORMAL, self.color)

        window.add(self.event_box)
        window.resize(400, 400)
        window.show_all()

    def onConnected(self, ctx, conn):
        self.label.set_text("CONNECTED")

    def onDisconnected(self, ctx, conn):
        self.label.set_text("DISCONNECTED")
        self.color.red = 0;
        self.color.green = 0;
        self.color.blue = 0;
        self.event_box.modify_bg(gtk.STATE_NORMAL, self.color)

    def handle(self, handler, req):
        handler.cb(req)

    def post(self, handler, req):
        gobject.idle_add(self.handle, handler, req)

    def recvMessage(self, ctx, conn, msg):
        res = msg.read("%s%s%u")
        if res[1] == "red":
            self.color.red = res[2] * 256
        if res[1] == "green":
            self.color.green = res[2] * 256
        if res[1] == "blue":
            self.color.blue = res[2] * 256
        self.event_box.modify_bg(gtk.STATE_NORMAL, self.color)

    def on_window_destroy(self, widget):
        self.ctx.stop()
        gtk.main_quit()


def usage():
    print("usage: sldUI.py libpomp_address")
    exit(1)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        usage()
    address = sys.argv[1]

    logging.basicConfig(stream=sys.stderr)

    gobject.threads_init()
    SocketLedDriverClient(address)
    gtk.main()
