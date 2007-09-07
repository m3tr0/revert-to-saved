# PiTiVi , Non-linear video editor
#
#       threads.py
#
# Copyright (c) 2005, Edward Hervey <bilboed@bilboed.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
"""
Threading support
"""

import threading
import gobject
import gst

#
# Following code was freely adapted by code from:
#   John Stowers <john.stowers@gmail.com>
#

class Thread(threading.Thread, gobject.GObject):
    """
    GObject-powered thread
    """

    __gsignals__ = {
        "done" : ( gobject.SIGNAL_RUN_LAST,
                   gobject.TYPE_NONE,
                   ( ))
        }

    def __init__(self):
        threading.Thread.__init__(self)
        gobject.GObject.__init__(self)

    def stop(self):
        """ stop the thread, do not override """
        self.abort()
        self.emit("done")

    def run(self):
        """ thread processing """
        self.process()
        gobject.idle_add(self.emit, "done")


    def abort(self):
        """ Abort the thread. Subclass have to implement this method ! """
        pass

gobject.type_register(Thread)

class ThreadMaster(gobject.GObject):
    """
    Controls all thread existing in pitivi
    """

    def __init__(self):
        gobject.GObject.__init__(self)
        self.threads = []

    def addThread(self, threadClass, *args):
        # IDEA : We might need a limit of concurrent threads ?
        # ... or some priorities ?
        # FIXME : we should only accept subclasses of our Thread class
        gst.log("Adding thread of type %r" % threadClass)
        thread = threadClass(*args)
        thread.connect("done", self._threadDoneCb)
        self.threads.append(thread)
        gst.log("starting it...")
        thread.start()
        gst.log("started !")

    def _threadDoneCb(self, thread):
        gst.log("thread %r is done" % thread)
        self.threads.remove(thread)

    def stopAllThreads(self):
        gst.log("stopping all threads")
        joinedthreads = 0
        while(joinedthreads < len(self.threads)):
            for thread in self.threads:
                gst.log("Trying to stop thread %r" % thread)
                try:
                    thread.join()
                    joinedthreads += 1
                except:
                    gst.warning("what happened ??")

