#!/usr/bin/python
# PiTiVi , Non-linear video editor
#
#       pitivi/ui/complexlayer.py
#
# Copyright (c) 2006, Edward Hervey <bilboed@bilboed.com>
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
Interfaces for complex view elements
"""

import gst

#
# Complex Timeline interfaces v2 (01 Jul 2008)
#
# Zoomable
# -----------------------
# Interface for the Complex Timeline widgets for setting, getting,
# distributing and modifying the zoom ratio and the size of the widget.
#
# A zoomratio is the number of pixels per second
# ex : 10.0 = 10 pixels for a second
# ex : 0.1 = 1 pixel for 10 seconds
# ex : 1.0 = 1 pixel for a second
#
# Methods:
# . setZoomAdjustment(adj)
# . getZoomAdjustment()
# . setChildZoomAdjustment()
# . zoomChanged()
# . setZoomRatio(ratio)
# . getZoomRatio(ratio)
# . pixelToNs(pixels)
# . nsToPixels(time)

# FIXME: this might be poor design. while sharing the adjustment 
# does provide an easy way of ensuring that all the UI elements are
# updated, it's a little bit kludgey when it comes to sharing the
# adjustment among elements which have children. In general, it migh
# be better to factor this interface out into a separate Transformation
# class which can handle the conversion between coordinate systems, for
# both horizontal and vertical coordinates. This interface only handles
# the horizontal.

class Zoomable:

    zoomratio = 0
    zoom_adj = None

    def setZoomAdjustment(self, adjustment):
        if self.zoom_adj:
            self.zoom_adj.disconnect(self._zoom_changed_sigid)
        self.zoom_adj = adjustment
        if adjustment:
            self._zoom_changed_sigid = adjustment.connect("value-changed",
                self._zoom_changed_cb)
            self.zoomratio = adjustment.get_value()
            self.setChildZoomAdjustment(adjustment)
            self.zoomChanged()

    def getZoomAdjustment(self):
        return self.zoom_adj

    def _zoom_changed_cb(self, adjustment):
        self.zoomratio = adjustment.get_value()
        self.zoomChanged()

    def getZoomRatio(self):
        return self.zoomratio

    def setZoomRatio(self, ratio):
        self.zoom_adj.set_value(ratio)

    def pixelToNs(self, pixel):
        """
        Returns the pixel equivalent in nanoseconds according to the zoomratio
        """
        return long(pixel * gst.SECOND / self.zoomratio)

    def nsToPixel(self, duration):
        """
        Returns the pixel equivalent of the given duration, according to the
        set zoom ratio
        """
        if duration == gst.CLOCK_TIME_NONE:
            return 0
        return int((float(duration) / gst.SECOND) * self.zoomratio)

    # Override in subclasses

    def zoomChanged(self):
        pass

    def setChildZoomAdjustment(self, adj):
        pass