#!/usr/bin/python
# PiTiVi , Non-linear video editor
#
#       ui/viewer.py
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

import gobject
import gtk
import gst
import pango
import gst.interfaces
from pitivi.bin import SmartTimelineBin
from pitivi.objectfactory import FileSourceFactory
import pitivi.dnd as dnd

def time_to_string(value):
    ms = value / gst.MSECOND
    sec = ms / 1000
    ms = ms % 1000
    min = sec / 60
    sec = sec % 60
    return "%02dm%02ds%03d" % (min, sec, ms)

class PitiviViewer(gtk.VBox):
    """ Pitivi's graphical viewer """

    def __init__(self, pitivi):
        self.pitivi = pitivi
        gtk.VBox.__init__(self)
        self.current_time = long(0)
        self._create_gui()

        # connect to the sourcelist for temp factories
        # TODO remove/replace the signal when closing/opening projects
        self.pitivi.current.sources.connect("tmp_is_ready", self._tmp_is_ready)

        gobject.timeout_add(100, self._check_time)
        self.pitivi.connect("new-project", self._new_project_cb)
        self.pitivi.playground.connect("current-state", self._current_state_cb)
        self.pitivi.playground.connect("bin-added", self._bin_added_cb)
        self.pitivi.playground.connect("bin-removed", self._bin_removed_cb)

        self._add_timeline_to_playground()

    def _create_gui(self):
        """ Creates the Viewer GUI """
        self.set_spacing(5)
        
        # drawing area
        self.aframe = gtk.AspectFrame(xalign=0.5, yalign=0.0, ratio=4.0/3.0, obey_child=False)
        self.aframe.connect("expose-event", self._frame_expose_event)
        self.pack_start(self.aframe, expand=True)
        self.drawingarea = ViewerWidget()
##         self.drawingarea = gtk.DrawingArea()
##         self.drawingarea.connect("expose-event", self._drawingarea_expose_event)
        self.drawingarea.connect("realize", self._drawingarea_realize_cb)
##         self.drawingarea.connect("configure-event", self._drawingarea_configure_event)
        self.aframe.add(self.drawingarea)

        # horizontal line
        self.pack_start(gtk.HSeparator(), expand=False)

        # Slider
        self.posadjust = gtk.Adjustment()
        self.slider = gtk.HScale(self.posadjust)
        self.slider.set_draw_value(False)
        self.slider.connect("button-press-event", self._slider_button_press_cb)
        self.slider.connect("button-release-event", self._slider_button_release_cb)
        self.pack_start(self.slider, expand=False)
        self.moving_slider = False
        
        # Buttons/Controls
        bbox = gtk.HBox()
        boxalign = gtk.Alignment(xalign=0.5, yalign=0.5)
        boxalign.add(bbox)
        self.pack_start(boxalign, expand=False)

        self.record_button = gtk.ToolButton(gtk.STOCK_MEDIA_RECORD)
        self.record_button.connect("clicked", self.record_cb)
        self.record_button.set_sensitive(False)
        bbox.pack_start(self.record_button, expand=False)
        
        self.rewind_button = gtk.ToolButton(gtk.STOCK_MEDIA_REWIND)
        self.rewind_button.connect("clicked", self.rewind_cb)
        self.rewind_button.set_sensitive(False)
        bbox.pack_start(self.rewind_button, expand=False)
        
        self.back_button = gtk.ToolButton(gtk.STOCK_MEDIA_PREVIOUS)
        self.back_button.connect("clicked", self.back_cb)
        self.back_button.set_sensitive(False)
        bbox.pack_start(self.back_button, expand=False)
        
        self.pause_button = gtk.ToggleToolButton(gtk.STOCK_MEDIA_PAUSE)
        self.pause_button.connect("clicked", self.pause_cb)
        bbox.pack_start(self.pause_button, expand=False)
        
        self.play_button = gtk.ToggleToolButton(gtk.STOCK_MEDIA_PLAY)
        self.play_button.connect("clicked", self.play_cb)
        bbox.pack_start(self.play_button, expand=False)
        
        self.next_button = gtk.ToolButton(gtk.STOCK_MEDIA_NEXT)
        self.next_button.connect("clicked", self.next_cb)
        self.next_button.set_sensitive(False)
        bbox.pack_start(self.next_button, expand=False)
        
        self.forward_button = gtk.ToolButton(gtk.STOCK_MEDIA_FORWARD)
        self.forward_button.connect("clicked", self.forward_cb)
        self.forward_button.set_sensitive(False)
        bbox.pack_start(self.forward_button, expand=False)
        
        infohbox = gtk.HBox()
        self.pack_start(infohbox, expand=False)

        # available sources combobox
        infoframe = gtk.Frame()
        self.sourcelist = gtk.ListStore(gobject.TYPE_STRING, gobject.TYPE_PYOBJECT)
        self.sourcecombobox = gtk.ComboBox(self.sourcelist)
        cell = gtk.CellRendererText()
        self.sourcecombobox.pack_start(cell, True)
        self.sourcecombobox.add_attribute(cell, "text", 0)
        self.sourcecombobox.set_sensitive(False)
        self.sourcecombobox.connect("changed", self._combobox_changed_cb)
        infoframe.add(self.sourcecombobox)
        
        # current time
        timeframe = gtk.Frame()
        self.timelabel = gtk.Label("00m00s000 / --m--s---")
        self.timelabel.set_alignment(1.0, 0.5)
        self.timelabel.set_padding(5, 5)
        timeframe.add(self.timelabel)
        infohbox.pack_start(infoframe, expand=True)
        infohbox.pack_end(timeframe, expand=False)

        # drag and drop
        self.drag_dest_set(gtk.DEST_DEFAULT_DROP | gtk.DEST_DEFAULT_MOTION,
                           [dnd.DND_FILESOURCE_TUPLE, dnd.DND_URI_TUPLE],
                           gtk.gdk.ACTION_COPY)
        self.connect("drag_data_received", self._dnd_data_received)

    def _create_sinkthreads(self):
        """ Creates the sink threads for the playground """
        print "_create_sinkthread"
        self.videosink = gst.element_factory_make("xvimagesink", "vsink")
        if not self.videosink:
            self.videosink = gst.element_factory_make("ximagesink", "vsink")
            csp = gst.element_factory_make("ffmpegcolorspace", "csp")
        else:
            csp = None

        print "before set_xwindow_id"
        self.drawingarea.videosink = self.videosink
        self.videosink.set_xwindow_id(self.drawingarea.window.xid)
        print "after set_xwindow_id"
        
        self.audiosink = gst.element_factory_make("alsasink", "asink")
        if not self.audiosink:
            self.audiosink = gst.element_factory_make("osssink", "asink")

        self.vqueue = gst.element_factory_make("queue", "vqueue")
        self.aqueue = gst.element_factory_make("queue", "aqueue")
        self.vsinkthread = gst.Thread("vsinkthread")
        self.asinkthread = gst.Thread("asinkthread")
        self.vsinkthread.add_many(self.videosink, self.vqueue)
        if csp:
            self.vsinkthread.add(csp)
            self.vqueue.link(csp)
            csp.link(self.videosink)
        else:
            self.vqueue.link(self.videosink)
        
        self.vsinkthread.add_ghost_pad(self.vqueue.get_pad("sink"), "sink")
        self.asinkthread.add_many(self.audiosink, self.aqueue)
        self.aqueue.link(self.audiosink)
        self.asinkthread.add_ghost_pad(self.aqueue.get_pad("sink"), "sink")

        # setting sinkthreads on playground
        self.pitivi.playground.set_video_sink_thread(self.vsinkthread)
        self.pitivi.playground.set_audio_sink_thread(self.asinkthread)
        self.pitivi.playground.connect("current-changed", self._current_playground_changed)

##     def _drawingarea_expose_event(self, drawingarea, event):
##         print "drawingarea expose_event"
##         self.videosink.set_xwindow_id(drawingarea.window.xid)
##         drawingarea.window.draw_rectangle(drawingarea.style.black_gc,
##                                           True,
##                                           0, 0,
##                                           drawingarea.allocation.width,
##                                           drawingarea.allocation.height)
##         self.videosink.expose()
##         return False

##     def _drawingarea_configure_event(self, drawingarea, event):
##         print "drawingarea configure_event"
##         self._drawingarea_expose_event(drawingarea, event)

    def _frame_expose_event(self, frame, event):
        print "frame expose_event"
        return False

    def _drawingarea_realize_cb(self, drawingarea):
        print "drawingarea realize_cb"
        drawingarea.modify_bg(gtk.STATE_NORMAL, drawingarea.style.black)
        self._create_sinkthreads()
        self.pitivi.playground.play()
        print drawingarea.flags()
        print drawingarea.window.get_events()

    def _slider_button_press_cb(self, slider, event):
        print "slider button_press"
        self.moving_slider = True
        return False

    def _slider_button_release_cb(self, slider, event):
        print "slider button release at", time_to_string(long(slider.get_value()))
        self.moving_slider = False
        self.pitivi.playground.seek_in_current(long(slider.get_value()))
        return False

    def _check_time(self):
        # check time callback
        if self.pitivi.playground.current == self.pitivi.playground.default:
            return True
        # don't check time if the timeline is paused !
        value = self.current_time
        if not self.pitivi.playground.state == gst.STATE_PAUSED:
            value = self.videosink.query(gst.QUERY_POSITION, gst.FORMAT_TIME)
        # if the current_time or the length has changed, update time
        if not float(self.pitivi.playground.current.length) == self.posadjust.upper or not value == self.current_time:
            self.posadjust.upper = float(self.pitivi.playground.current.length)
            self._new_time(value)
        return True

    def _new_time(self, value):
        self.current_time = value
        self.timelabel.set_text(time_to_string(value) + " / " + time_to_string(self.pitivi.playground.current.length))
        if not self.moving_slider:
            self.posadjust.set_value(float(value))
        return False

    def _combobox_changed_cb(self, cbox):
        # selected another source
        idx = cbox.get_active()
        # get the corresponding smartbin
        smartbin = self.sourcelist[idx][1]
        if not self.pitivi.playground.current == smartbin:
            self.pitivi.playground.switch_to_pipeline(smartbin)

    def _get_smartbin_index(self, smartbin):
        # find the index of a smartbin
        # return -1 if it's not in there
        for pos in range(len(self.sourcelist)):
            if self.sourcelist[pos][1] == smartbin:
                return pos
        return -1

    def _bin_added_cb(self, playground, smartbin):
        # a smartbin was added
        self.sourcelist.append([smartbin.displayname, smartbin])
        self.sourcecombobox.set_sensitive(True)

    def _bin_removed_cb(self, playground, smartbin):
        # a smartbin was removed
        idx = self._get_smartbin_index(smartbin)
        if idx < 0:
            return
        del self.sourcelist[idx]
        if len(self.sourcelist) == 0:
            self.sourcecombobox.set_sensitive(False)

    def _current_playground_changed(self, playground, smartbin):
        if smartbin.width and smartbin.height:
            self.aframe.set_property("ratio", float(smartbin.width) / float(smartbin.height))
        else:
            self.aframe.set_property("ratio", 4.0/3.0)
        if not smartbin == playground.default:
            if isinstance(smartbin, SmartTimelineBin):
                start = smartbin.project.timeline.videocomp.start
                stop = smartbin.project.timeline.videocomp.stop
                self.posadjust.upper = float(stop - start)
            else:
                self.posadjust.upper = float(smartbin.factory.length)
        self.sourcecombobox.set_active(self._get_smartbin_index(smartbin))

    def _dnd_data_received(self, widget, context, x, y, selection, targetType, time):
        print "data received in viewer, type:", targetType
        if targetType == dnd.DND_TYPE_URI_LIST:
            uri = selection.data.strip().split("\n")[0].strip()
        elif targetType == dnd.DND_TYPE_PITIVI_FILESOURCE:
            uri = selection.data
        else:
            return
        print "got file:", uri
        if uri in self.pitivi.current.sources:
            self.pitivi.playground.play_temporary_filesourcefactory(self.pitivi.current.sources[uri])
        else:
            self.pitivi.current.sources.add_tmp_uri(uri)

    def _tmp_is_ready(self, sourcelist, factory):
        """ the temporary factory is ready, we can know set it to play """
        print "tmp_is_ready", factory
        self.pitivi.playground.play_temporary_filesourcefactory(factory)

    def _new_project_cb(self, pitivi, project):
        """ the current project has changed """
        self.pitivi.current.sources.connect("tmp_is_ready", self._tmp_is_ready)
        
    def _add_timeline_to_playground(self):
        self.pitivi.playground.add_pipeline(SmartTimelineBin(self.pitivi.current))

    def record_cb(self, button):
        pass

    def rewind_cb(self, button):
        pass

    def back_cb(self, button):
        pass


    def pause_cb(self, button):
        print "pause_cb"
        if self.pause_button.get_active():
            #self.pause_button.set_active(False)
            self.pitivi.playground.pause()

    def play_cb(self, button):
        print "play_cb"
        if self.play_button.get_active():
            #self.play_button.set_active(False)
            self.pitivi.playground.play()

    def next_cb(self, button):
        pass

    def forward_cb(self, button):
        pass

    def _current_state_cb(self, playground, state):
        print "current state changed", state
        if state == int(gst.STATE_PLAYING):
            self.play_button.set_active(True)
            self.pause_button.set_active(False)
        elif state == int(gst.STATE_PAUSED):
            self.pause_button.set_active(True)
            self.play_button.set_active(False)
        elif state == int(gst.STATE_READY):
            self.play_button.set_active(False)
            self.pause_button.set_active(False)

gobject.type_register(PitiviViewer)

class ViewerWidget(gtk.DrawingArea):

    __gsignals__ = {
        "expose-event" : "override"
        }

    def __init__(self):
        self.videosink = None
        gobject.GObject.__init__(self)

    def do_expose_event(self, event):
        print "viewer widget expose"
        if self.videosink:
            self.window.draw_rectangle(self.style.white_gc,
                                       True, 0, 0,
                                       self.allocation.width,
                                       self.allocation.height)
            self.videosink.set_xwindow_id(self.window.xid)
            self.videosink.expose()
        return True

gobject.type_register(ViewerWidget)