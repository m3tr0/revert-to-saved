/* 
 * PiTiVi
 * Copyright (C) <2004>	 Guillaume Casanova <casano_g@epita.fr>
 *
 * This software has been written in EPITECH <http://www.epitech.net>
 * EPITECH is a computer science school in Paris - FRANCE -
 * under the direction of Flavien Astraud and Jerome Landrieu.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "pitivi.h"
#include "pitivi-timelinewindow.h"
#include "pitivi-timelinecellrenderer.h"
#include "pitivi-timelinemedia.h"
#include "pitivi-dragdrop.h"
#include "pitivi-toolboxwindow.h"
#include "pitivi-toolbox.h"
#include "pitivi-drawing.h"

// Parent Class
static GtkLayoutClass	    *parent_class = NULL;

// Caching Operation  
static GdkPixmap	    *pixmap = NULL;

struct _PitiviTimelineCellRendererPrivate
{
  /* instance private members */
  gboolean	       dispose_has_run;
  
  PitiviTimelineWindow *timewin;
  PitiviTimelineMedia  *draggedWidget;
  GtkSelectionData     *current_selection;
  gboolean	       selected;
    
  gint		       width;
  gint		       height;
  
  /* Slide */
  guint		       slide_width;
  gboolean	       slide_both;

  /* Backgrounds */
  
  GdkPixmap	       **bgs;
};

/*
 * forward definitions
 */
 
/*
 **********************************************************
 * Track informations  			                  *
 *							  *
 **********************************************************
*/

// Properties Enumaration

typedef enum {  
  PROP_LAYER_PROPERTY = 1,
  PROP_TYPE_LAYER_PROPERTY,
  PROP_TRACK_NB_PROPERTY,
  PROP_TIMELINEWINDOW
} PitiviLayerProperty;

static guint track_sizes[4][4] =
  {
    {PITIVI_VIDEO_TRACK,      7200, 50},
    {PITIVI_EFFECTS_TRACK,    7200, 25},
    {PITIVI_TRANSITION_TRACK, 7200, 25},
    {PITIVI_AUDIO_TRACK,      7200, 50},
  };

/*
 **********************************************************
 * Drag and drop  			                  *
 *							  *
 **********************************************************
*/

// Destination Acception mime type for drah 'n drop

static GtkTargetEntry TargetEntries[] =
  {
    { "pitivi/sourcefile", GTK_TARGET_SAME_APP, DND_TARGET_SOURCEFILEWIN },
    { "pitivi/sourceeffect", GTK_TARGET_SAME_APP, DND_TARGET_EFFECTSWIN },
    { "pitivi/sourcetimeline", GTK_TARGET_SAME_APP, DND_TARGET_TIMELINEWIN },
    { "STRING", GTK_TARGET_SAME_APP, DND_TARGET_STRING },
    { "text/plain", GTK_TARGET_SAME_APP, DND_TARGET_URI },
  };

static gint iNbTargetEntries = G_N_ELEMENTS (TargetEntries);

/*
 * Insert "added-value" functions here
 */

GtkWidget *
pitivi_timelinecellrenderer_new (guint track_nb, PitiviLayerType track_type, PitiviTimelineWindow *tw)
{
  PitiviTimelineCellRenderer	*timelinecellrenderer;
  
  timelinecellrenderer = (PitiviTimelineCellRenderer *) 
    g_object_new (PITIVI_TIMELINECELLRENDERER_TYPE, 
		  "track_nb", 
		  track_nb, 
		  "track_type", 
		  track_type,
		  "timelinewindow",
		  tw,
		  NULL);  
  g_assert(timelinecellrenderer != NULL);
  return GTK_WIDGET ( timelinecellrenderer );
}


void
set_tracksize ( PitiviTimelineCellRenderer *self )
{
  int count;

  for (count = 0; count < (sizeof (track_sizes)/sizeof(guint)); count ++)
    if (self->track_type == track_sizes[count][0])
      {
	gtk_widget_set_size_request(GTK_WIDGET(self), 
				    convert_time_pix(self, track_sizes[count][1]),
				    track_sizes[count][2]);
	break;
      }
}

static GObject *
pitivi_timelinecellrenderer_constructor (GType type,
					 guint n_construct_properties,
					 GObjectConstructParam * construct_properties)
{
  GObject *object;
  PitiviTimelineCellRenderer *self;
  
  /* Constructor  */
  
  object = (* G_OBJECT_CLASS (parent_class)->constructor) 
    (type, n_construct_properties, construct_properties);
  
  self = (PitiviTimelineCellRenderer *) object;
  
  /* Deactivation */
  pitivi_timelinecellrenderer_deactivate (self);
  
  /* Set Size Layer */
  set_tracksize (self);
  
  /* Gcs && Bgs */
  
  self->private->bgs = self->private->timewin->bgs;
  self->gcs = self->private->timewin->gcs;
  self->nb_added = self->private->timewin->nb_added;
  return object;
}

static gint
pitivi_timelinecellrenderer_expose (GtkWidget      *widget,
				    GdkEventExpose *event)
{
  PitiviTimelineCellRenderer	*self = PITIVI_TIMELINECELLRENDERER (widget);
  GtkLayout *layout;
  
  g_return_val_if_fail (GTK_IS_LAYOUT (widget), FALSE);
  layout = GTK_LAYOUT (widget);
  
  /* No track is activated */
  
  if (self->track_type != PITIVI_NO_TRACK)
    {
      gtk_paint_hline (widget->style,
		       layout->bin_window, 
		       GTK_STATE_NORMAL,
		       NULL, widget, "middle-line",
		       0, widget->allocation.width, widget->allocation.height/2);
    }
  if (event->window != layout->bin_window)
    return FALSE;
  return FALSE;
}

GtkWidget **layout_intersection_widget (GtkWidget *self, GtkWidget *widget, gint x)
{
  GList	*child;
  GtkRequisition req;
  GtkWidget **p;
  GtkWidget *matches[2];
  int xchild, widthchild = 0;
  
  matches[0] = 0;
  matches[1] = 0;
  p = matches;
  gtk_widget_size_request (widget, &req);
  for (child = gtk_container_get_children (GTK_CONTAINER (self)); 
       child; 
       child = child->next )
    {
      xchild = GTK_WIDGET(child->data)->allocation.x;
      widthchild = GTK_WIDGET(child->data)->allocation.width;
      if (xchild <= x && x <= xchild + widthchild)
	matches[0] = GTK_WIDGET (child->data);
      else if (xchild <= x + req.width && x + req.width <= xchild + widthchild)
	matches[1] = GTK_WIDGET (child->data);
    }
  g_list_free (child);
  return p;
}

void move_media (GtkWidget *cell, GtkWidget *widget, guint x)
{
  GtkWidget **intersec;
  GtkWidget *first;
  int       xbegin;

  intersec = layout_intersection_widget (cell, widget, x);
  first = intersec[1];
  if (first && GTK_IS_WIDGET (first) && first->allocation.x != x)
    {
      xbegin = x + first->allocation.width;
      gtk_layout_move (GTK_LAYOUT (cell), first, xbegin, 0);
      move_media (cell, first, xbegin);
    }
  return;
}

void
move_child_on_layout (GtkWidget *self, GtkWidget *widget, gint x)
{
  GtkWidget **intersec;
  GtkRequisition req;
  int xbegin = x;

  gtk_widget_size_request (widget, &req);
  intersec = layout_intersection_widget (self, widget, x);
  if (!intersec[1] && intersec[0])
    {
      xbegin = intersec[0]->allocation.x+intersec[0]->allocation.width;
      gtk_layout_move (GTK_LAYOUT (self), widget, xbegin, 0);
    }
  else if (!intersec[0] && intersec[1])
    {
      move_media (self, intersec[1], x);
      gtk_layout_move (GTK_LAYOUT (self), widget, x, 0);
    }
  else if (intersec[1] && intersec[0])
    {
      xbegin = intersec[0]->allocation.x+intersec[0]->allocation.width;
      gtk_layout_move (GTK_LAYOUT (self), widget, xbegin, 0);
      move_media (self, widget, xbegin);
    }
  else
    gtk_layout_move (GTK_LAYOUT (self), widget, xbegin, 0);
}

int add_to_layout (GtkWidget *self, GtkWidget *widget, gint x, gint y)
{
  PitiviTimelineCellRenderer *cell;
  GtkRequisition req;
  GtkWidget **intersec;
  int	    xbegin;
  
  cell = PITIVI_TIMELINECELLRENDERER (self);
  gtk_widget_size_request (widget, &req);
  intersec = layout_intersection_widget (self, widget, x);
  if (!intersec[0] && !intersec[1])
    gtk_layout_put (GTK_LAYOUT (self), widget, x, 0);
  else if (!intersec[1]) /* right */
    {
      move_media (self, widget, x);
      xbegin = intersec[0]->allocation.x+intersec[0]->allocation.width;
      gtk_layout_put (GTK_LAYOUT (self), widget, xbegin, y);
    }
  else if (!intersec[0]) /* left */
    {
      move_media (self, intersec[1], x);
      gtk_layout_put (GTK_LAYOUT (self), widget, x, 0);
    }
  else if (intersec[1] && intersec[0])
    { 
      xbegin = intersec[0]->allocation.x+intersec[0]->allocation.width;
      gtk_layout_put (GTK_LAYOUT (self), widget, xbegin, 0);
      move_media (self, intersec[1], xbegin);
    }
  return TRUE;
}


PitiviLayerType
check_media_type_str (gchar *media)
{
  if (!g_strcasecmp  (media, "video")) 
    return (PITIVI_VIDEO_TRACK);
  else if (!g_strcasecmp (media, "audio"))
    return (PITIVI_AUDIO_TRACK);
  else if (!g_strcasecmp (media, "video/audio") || !g_strcasecmp (media, "audio/video"))
    return (PITIVI_VIDEO_AUDIO_TRACK);
  return (PITIVI_NO_TRACK);
}

PitiviLayerType
check_media_type (PitiviSourceFile *sf)
{
  PitiviLayerType layer;
  gchar *media;

  if (sf)
    {
      if (sf->mediatype)
	{
	  media = g_strdup (sf->mediatype);
	  layer = check_media_type_str (media);
	  g_free (media);
	  return layer;
	}
    }
  return (PITIVI_NO_TRACK);
}

PitiviCursor *
pitivi_getcursor_id (GtkWidget *widget)
{
  PitiviCursor          *cursor;
  GtkWidget		*parent;
  PitiviToolbox		*toolbox;
  
  cursor = NULL;
  parent = gtk_widget_get_toplevel (GTK_WIDGET (widget));
  if ( GTK_IS_WINDOW (parent) )
    cursor = ((PitiviTimelineWindow *)parent)->toolbox->pitivi_cursor;
  return cursor;
}

static void
pitivi_timelinecellrenderer_callb_activate (PitiviTimelineCellRenderer *self)
{
  /* Activation of widget */
  gtk_widget_set_sensitive (GTK_WIDGET(self), TRUE);
  pitivi_setback_tracktype ( self );
}

static void
pitivi_timelinecellrenderer_callb_deactivate (PitiviTimelineCellRenderer *self)
{
  /* Desactivation of widget */
  gtk_widget_set_sensitive (GTK_WIDGET(self), FALSE);
}

void
pitivi_timelinecellrenderer_drag_leave (GtkWidget          *widget,
					GdkDragContext     *context,
					gpointer	    user_data)
{
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) widget;
  if (self->linked_track)
    {
      gdk_window_clear (GTK_LAYOUT (self->linked_track)->bin_window);
      pitivi_send_expose_event (self->linked_track);
    }
}

gboolean check_intersect_child (GtkWidget *widget)
{
  GList	*childlist;
  GtkWidget *child;
  GdkModifierType mods;
  int x, y;

  childlist = gtk_container_get_children (GTK_CONTAINER (widget));
  gdk_window_get_pointer (widget->window, &x, &y, &mods);
  for (; childlist; childlist = childlist->next)
    {
      child = childlist->data;
      if (x >= child->allocation.x && x <= child->allocation.x +  child->allocation.width)
	return TRUE;
    }
  g_list_free ( childlist );
  return FALSE;
}

void
check_intersect_layout (GtkWidget *widget, guint x)
{
  GList	*childlist;
  GtkWidget *media;
  guint x_rec_left = 0;
  guint x2_left = 0;

  childlist = gtk_container_get_children (GTK_CONTAINER (widget));
  childlist = g_list_sort ( childlist , compare_child);
   for (; childlist; childlist = childlist->next)
     {
       media = GTK_WIDGET (childlist->data);
       x2_left = media->allocation.x + media->allocation.width;
       if (x_rec_left < x2_left && x2_left < x)
	 x_rec_left = x2_left;
     }
  g_list_free ( childlist );
}

static gint
pitivi_timelinecellrenderer_button_release_event (GtkWidget      *widget,
						  GdkEventButton *event)
{
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) widget;
  PitiviCursor		*cursor;
  
  cursor = pitivi_getcursor_id (widget);
  if (cursor->type == PITIVI_CURSOR_SELECT && event->state != 0)
    {
      if (event->button == 1)
	{
	  if (!check_intersect_child (widget))
	    {
	      g_signal_emit_by_name (GTK_WIDGET (self->private->timewin), "deselect", NULL);
	      check_intersect_layout (widget, event->x);
	    }
	}
    }
  return FALSE;
}

static void
pitivi_timelinecellrenderer_dispose (GObject *object)
{
  PitiviTimelineCellRenderer	*self = PITIVI_TIMELINECELLRENDERER(object);

  /* If dispose did already run, return. */
  if (self->private->dispose_has_run)
    return;
  
  /* Make sure dispose does not run twice. */
  self->private->dispose_has_run = TRUE;	
}

static void
pitivi_timelinecellrenderer_finalize (GObject *object)
{
  PitiviTimelineCellRenderer	*self = PITIVI_TIMELINECELLRENDERER(object);
  g_free (self->private);
}

static void
pitivi_timelinecellrenderer_set_property (GObject * object,
					  guint property_id,
					  const GValue * value, GParamSpec * pspec)
{
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) object;

  switch (property_id)
    {
    case PROP_LAYER_PROPERTY:
      break;
    case PROP_TYPE_LAYER_PROPERTY:
      self->track_type = g_value_get_int (value);
      break;
    case PROP_TRACK_NB_PROPERTY:
      self->track_nb = g_value_get_int (value);
      break;
    case PROP_TIMELINEWINDOW:
      self->private->timewin = g_value_get_pointer (value);
      break;
    default:
      g_assert (FALSE);
      break;
    }

}

static void
pitivi_timelinecellrenderer_get_property (GObject * object,
					  guint property_id,
					  GValue * value, GParamSpec * pspec)
{
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) object;

  switch (property_id)
    {
    case PROP_LAYER_PROPERTY:
      break;
    case PROP_TYPE_LAYER_PROPERTY:
      break;
    case PROP_TRACK_NB_PROPERTY:
      break;
    default:
      g_assert (FALSE);
      break;
    }
}

/**************************************************************
 * Callbacks Signal Drag and Drop          		      *
 * This callbacks are used to motion get or delete  data from *
 * drag							      *
 **************************************************************/

/*
  convert_time_pix
  Returns the pixel size depending on the unit of the ruler, and the zoom level
*/

guint
convert_time_pix (PitiviTimelineCellRenderer *self, gint64 timelength)
{
  gint64 len = timelength;
  PitiviProject	*proj = PITIVI_WINDOWS(self->private->timewin)->mainapp->project;
  
  switch (self->private->timewin->unit)
    {
    case PITIVI_NANOSECONDS:
      len = timelength * self->private->timewin->zoom;
      break;
    case PITIVI_SECONDS:
      len = (timelength / GST_SECOND) * self->private->timewin->zoom;
      break;
    case PITIVI_FRAMES:
      len = (timelength / GST_SECOND) 
	* pitivi_projectsettings_get_videorate(proj->settings)
	* self->private->timewin->zoom;
      break;
    default:
      break;
    }
  return len;
}


void
create_media_video_audio_track (PitiviTimelineCellRenderer *cell, PitiviSourceFile *sf, int x)
{
  PitiviTimelineMedia *media[2];
  guint64 length = sf->length;
  
  if (!length)
    length = DEFAULT_MEDIA_SIZE;
  
  /* Creating widgets */
  
  media[0] = pitivi_timelinemedia_new (sf, cell);  
  gtk_widget_set_size_request (GTK_WIDGET (media[0]), convert_time_pix(cell, length), FIXED_HEIGHT);
  media[1] = pitivi_timelinemedia_new (sf, cell);
  gtk_widget_set_size_request (GTK_WIDGET (media[1]), convert_time_pix(cell, length), FIXED_HEIGHT);
  
  /* Putting on first Layout */
  
  add_to_layout ( GTK_WIDGET (cell), GTK_WIDGET (media[0]), x, 0);
  add_to_layout ( GTK_WIDGET (cell->linked_track), GTK_WIDGET (media[1]), x, 0);
  
  /* Linking widgets */
  
  media[1]->linked = GTK_WIDGET (media[0]);
  media[0]->linked = GTK_WIDGET (media[1]);
  gtk_widget_show (GTK_WIDGET (media[0]));
  gtk_widget_show (GTK_WIDGET (media[1]));
}

void
create_media_track (PitiviTimelineCellRenderer *self, 
		    PitiviSourceFile *sf, 
		    int x, 
		    gboolean invert)
{
  PitiviTimelineMedia *media;
  guint64 length = sf->length;
  
  if (!length)
    length = DEFAULT_MEDIA_SIZE;
  media = pitivi_timelinemedia_new (sf, self);
  gtk_widget_set_size_request (GTK_WIDGET (media), convert_time_pix(self, length), FIXED_HEIGHT);
  gtk_widget_show (GTK_WIDGET (media));
  if (invert)
    add_to_layout ( GTK_WIDGET (self->linked_track), GTK_WIDGET (media), x, 0);
  else
    add_to_layout ( GTK_WIDGET (self), GTK_WIDGET (media), x, 0);
}

void
create_effect_on_track (PitiviTimelineCellRenderer *self, PitiviSourceFile *sf, int x)
{
  PitiviTimelineMedia *media;
  
  media = pitivi_timelinemedia_new (sf, self);
  add_to_layout ( GTK_WIDGET (self), GTK_WIDGET (media), x, 0);
}

void
dispose_medias (PitiviTimelineCellRenderer *self, PitiviSourceFile *sf, int x)
{
  PitiviLayerType	type_track_cmp;

  if (self->track_type != PITIVI_EFFECTS_TRACK && self->track_type != PITIVI_TRANSITION_TRACK)
    {
      type_track_cmp = check_media_type (sf);
      if (type_track_cmp == PITIVI_VIDEO_AUDIO_TRACK)
	create_media_video_audio_track (self, sf, x);
      else
	{
	  if (self->track_type == type_track_cmp)
	    create_media_track (self, sf, x, FALSE);
	  else if (self->track_type != type_track_cmp)
	    create_media_track (self, sf, x, TRUE);
	}
    }
  
}

void
pitivi_timelinecellrenderer_drag_on_source_file (PitiviTimelineCellRenderer *self, 
						 GtkSelectionData *selection, 
						 int x, 
						 int y)
{
  PitiviSourceFile	*sf;
  PitiviLayerType	type_track_cmp;

  sf = (PitiviSourceFile *) selection->data;
  dispose_medias (self, sf, x);
}



static void
pitivi_timelinecellrenderer_drag_on_effects (PitiviTimelineCellRenderer *self,
					     GtkSelectionData *selection,
					     int x,
					     int y)
{
  PitiviSourceFile  *sf = NULL;
  
  sf = (PitiviSourceFile *) selection->data;
  if (sf)
    if (self->track_type == PITIVI_EFFECTS_TRACK && self->track_type == PITIVI_TRANSITION_TRACK)
      create_effect_on_track (self, sf, x);
}

static void 
pitivi_timelinecellrenderer_drag_on_track (PitiviTimelineCellRenderer *self, 
					   GtkWidget *source,
					   GtkSelectionData *selection,
					   int x,
					   int y)
{
  PitiviTimelineCellRenderer *parent;
  PitiviTimelineMedia	     *dragged;
  
  dragged = (PitiviTimelineMedia *) source;
  parent  = (PitiviTimelineCellRenderer *)gtk_widget_get_parent(GTK_WIDGET (dragged));
  
  /* Moving widget on same track */
  
  if (parent && self->track_type == parent->track_type)
    {
      if ( dragged->linked ) { /* Two widgets */
	if (parent == self)
	  {
	    move_child_on_layout (GTK_WIDGET (self), GTK_WIDGET (source), x);
	    move_child_on_layout (GTK_WIDGET (self->linked_track), dragged->linked, x);
	  }
	else
	  {
	    gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (source));
	    add_to_layout (GTK_WIDGET (self),  source, x, 0);
	    
	    /* linked widget */
	    
	    GtkWidget *linked_ref = gtk_widget_ref (dragged->linked);
	    gtk_container_remove (GTK_CONTAINER (parent->linked_track), dragged->linked);
	    add_to_layout (GTK_WIDGET (self->linked_track), linked_ref, x, 0);
	    gtk_widget_unref (linked_ref);
	  }
	pitivi_send_expose_event (self->linked_track);
      }
      else /* Single Widget */
	{
	  GTK_CONTAINER_CLASS (parent_class)->remove (GTK_CONTAINER (parent), GTK_WIDGET (dragged));
	  add_to_layout  (GTK_WIDGET (self),  GTK_WIDGET (dragged), x, 0);
	}
    }
}



static void
pitivi_timelinecellrenderer_drag_data_received (GObject *object,
						GdkDragContext *dc,
						int x,
						int y,
						GtkSelectionData *selection,
						guint info,
						guint time,
						gpointer data)
{
  PitiviCursor *cursor;
  GtkWidget    *source;
  PitiviTimelineCellRenderer *self;

  self = PITIVI_TIMELINECELLRENDERER (object);
  self->private->current_selection = selection;
  if (!selection->data) {
    gtk_drag_finish (dc, FALSE, FALSE, time);
    return;
  }
  
  cursor = pitivi_getcursor_id (GTK_WIDGET(self));
  self->private->current_selection = selection;
  source = gtk_drag_get_source_widget(dc);
  switch (info) 
    {
    case DND_TARGET_SOURCEFILEWIN:
      pitivi_timelinecellrenderer_drag_on_source_file (self, self->private->current_selection, x, y);
      gtk_drag_finish (dc, TRUE, TRUE, time);
      break;
    case DND_TARGET_TIMELINEWIN:
      if (cursor->type == PITIVI_CURSOR_SELECT || cursor->type == PITIVI_CURSOR_HAND)
	{  
	  pitivi_timelinecellrenderer_drag_on_track (self, source, self->private->current_selection, x, y);
	  gtk_drag_finish (dc, TRUE, TRUE, time);
	}
      break;
    case DND_TARGET_EFFECTSWIN:
      pitivi_timelinecellrenderer_drag_on_effects (self, self->private->current_selection, x, y);
      gtk_drag_finish (dc, TRUE, TRUE, time);
      break;
    default:
      break;
    }
  self->private->slide_width = 0;
  self->private->slide_both  = FALSE;
  PITIVI_TIMELINECELLRENDERER (self->linked_track)->private->slide_width = 0;
  PITIVI_TIMELINECELLRENDERER (self->linked_track)->private->slide_both  = FALSE;
}

guint 
slide_media_get_widget_size (PitiviTimelineMedia  *source)
{
  guint width = DEFAULT_MEDIA_SIZE;

  if (PITIVI_IS_TIMELINEMEDIA (source))
    width = GTK_WIDGET (source)->allocation.width;
  return width;
}

static void
pitivi_timelinecellrenderer_drag_motion (GtkWidget          *widget,
					 GdkDragContext     *dc,
					 gint                x,
					 gint                y,
					 guint               time)
{ 
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) widget;
  PitiviTimelineMedia  *source = NULL;
  PitiviCursor *cursor;
  guint width;

  cursor = pitivi_getcursor_id (widget);
  if (cursor->type == PITIVI_CURSOR_SELECT || cursor->type == PITIVI_CURSOR_HAND)
    {
      source = (PitiviTimelineMedia  *)gtk_drag_get_source_widget(dc);
      if (source && dc && PITIVI_IS_TIMELINEMEDIA (source))
      	width = slide_media_get_widget_size (source);
      else
	width = self->private->slide_width;
      gdk_window_clear (GTK_LAYOUT (widget)->bin_window);
      pitivi_draw_slide (widget, x, width);
      if ((self->linked_track && source && source->linked) || (self->linked_track && self->private->slide_both))
      	{
	  gdk_window_clear  (GTK_LAYOUT (self->linked_track)->bin_window);
	  pitivi_draw_slide (GTK_WIDGET (self->linked_track), x, width);
      	}
    }
}



/**************************************************************
 * Callbacks Signal Activate / Deactivate		      *
 * This callbacks are used to acitvate and deactivate Layout  *
 *							      *
 **************************************************************/

void
pitivi_timelinecellrenderer_activate (PitiviTimelineCellRenderer *self)
{
  g_signal_emit_by_name (GTK_OBJECT (self), "activate");
}

void
pitivi_timelinecellrenderer_deactivate (PitiviTimelineCellRenderer *self)
{
  
  g_signal_emit_by_name (GTK_OBJECT (self), "deactivate");
}

void
pitivi_timelinecellrenderer_zoom_changed (PitiviTimelineCellRenderer *self)
{
  // redraw all childs at the good position and at the good width
  g_printf("zoom-changed to %d for track %d\n", self->private->timewin->zoom, self->track_nb);
}

void
pitivi_setback_tracktype ( PitiviTimelineCellRenderer *self )
{
  GdkPixmap *pixmap = NULL;
  
  if (self->track_type != PITIVI_NO_TRACK)
    {
      pixmap = self->private->bgs[self->track_type];
      if ( pixmap )
	{
	  // Set background Color
	  pitivi_drawing_set_pixmap_bg (GTK_WIDGET(self), pixmap);
	}
    }
}

/*
** Scroll event
*/

static gboolean
pitivi_timelinecellrenderer_scroll_event (GtkWidget *widget, GdkEventScroll *event)
{
  PitiviTimelineCellRenderer	*self = PITIVI_TIMELINECELLRENDERER (widget);

  if (event->state & GDK_SHIFT_MASK) { // ZOOM
    
    if ((event->direction == GDK_SCROLL_UP) && (self->private->timewin->zoom < 16)) { // ZOOM IN
      self->private->timewin->zoom *= 2;
      pitivi_timelinewindow_zoom_changed (self->private->timewin);
    } else if ((event->direction == GDK_SCROLL_DOWN) && (self->private->timewin->zoom > 1)) { // ZOOM OUT
      self->private->timewin->zoom /= 2;
      pitivi_timelinewindow_zoom_changed (self->private->timewin);
    }
  } else { // MOVE
    gdouble	value, lower, upper, increment;
    
    g_object_get(G_OBJECT(self->private->timewin->hscrollbar),
		 "lower", &lower,
		 "upper", &upper,
		 "step-increment", &increment,
		 "value", &value,
		 NULL);
    if (event->direction == GDK_SCROLL_UP) { // MOVE LEFT
      value  = value - increment;
      if (value < lower)
	value = lower;
      gtk_adjustment_set_value (self->private->timewin->hscrollbar, value);
    } else if (event->direction == GDK_SCROLL_DOWN) { // MOVE RIGHT
      value = value + increment;
      if (value > upper)
	value = upper;
      gtk_adjustment_set_value (self->private->timewin->hscrollbar, value);
    }
  }
  return FALSE;
}


/*
 **********************************************************
 * Selection	  			                  *
 *							  *
 **********************************************************
*/

guint
pitivi_timecellrenderer_track_type ( PitiviTimelineCellRenderer *cell )
{
  return cell->track_type;
}

void
pitivi_timelinecellrenderer_callb_select (PitiviTimelineCellRenderer *self)
{
  
}

void
pitivi_timelinecellrenderer_callb_deselect (PitiviTimelineCellRenderer *self)
{
  self->private->selected = FALSE;
  send_signal_to_childs_direct (GTK_WIDGET (self), "deselect", NULL);
}


static void
pitivi_timelinecellrenderer_callb_dbk_source (PitiviTimelineCellRenderer *self, gpointer data)
{
  PitiviTimelineMedia *media;

  dispose_medias (self, data, 1);
}

static void
pitivi_timelinecellrenderer_callb_cut_source  (PitiviTimelineCellRenderer *self, guint x, gpointer data)
{
}

void
pitivi_timelinecellrenderer_destroy_child  (PitiviTimelineCellRenderer *self, GtkWidget *child)
{
  GtkWidget *elm;

  elm = gtk_widget_ref (child);
  gtk_widget_hide (child);
  gtk_container_remove (GTK_CONTAINER (self), child);
  gtk_widget_destroy (child);
  gtk_widget_unref (elm);
}

void
pitivi_timelinecellrenderer_callb_delete_sf (PitiviTimelineCellRenderer *self, gpointer data)
{
  PitiviTimelineMedia *media;
  PitiviSourceFile *sf;
  GList *child  = NULL;
  GList *delete = NULL;
  
  for (child = gtk_container_get_children (GTK_CONTAINER (self)); child; child = child->next )
    {
      sf = data;
      media = child->data;
      if (media->sourceitem->srcfile->filename == sf->filename)
	delete = g_list_append (delete, media);
    }
  while (delete)
    {
      if (GTK_IS_WIDGET (delete->data))
	{
	  media = delete->data;
	  if (media->linked)
	    pitivi_timelinecellrenderer_destroy_child ((PitiviTimelineCellRenderer *)self->linked_track, media->linked);
	  pitivi_timelinecellrenderer_destroy_child (self, GTK_WIDGET (media) );
	}
      delete = delete->next;
    }
  g_list_free (delete);
  g_list_free (child);
}


void
pitivi_timelinecellrenderer_callb_drag_source_begin (PitiviTimelineCellRenderer *self, 
						     gpointer data)
{
  struct _Pslide
  {
    gint64 length;
    gchar  *path;
  } *slide;
  gint64 len;
  PitiviLayerType type;
 
  slide = (struct _Pslide *) data;
  len = slide->length;
  if (len > 0)
    self->private->slide_width = convert_time_pix (self, len);
  type = check_media_type_str (slide->path);
  if (type == PITIVI_VIDEO_AUDIO_TRACK)
    self->private->slide_both = TRUE;
}

/*
 **********************************************************
 * Instance Init  			                  *
 *							  *
 **********************************************************
*/

static void
pitivi_timelinecellrenderer_key_delete (PitiviTimelineCellRenderer* self) 
{
  PitiviTimelineMedia *media;
  GList *child  = NULL;
  
  for (child = gtk_container_get_children (GTK_CONTAINER (self)); child; child = child->next )
    {
      media = child->data;
      if ( media->selected )
	gtk_widget_destroy (GTK_WIDGET (media));
    }
   g_list_free (child);
}

static void
pitivi_timelinecellrenderer_instance_init (GTypeInstance * instance, gpointer g_class)
{
  GdkPixmap *pixmap;
  
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) instance;

  self->private = g_new0(PitiviTimelineCellRendererPrivate, 1);
  
  /* Motion notify Event Button Press release for selection */

  gtk_widget_set_events (GTK_WIDGET (self),
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_POINTER_MOTION_MASK | 
			 gtk_widget_get_events (GTK_WIDGET (self)));
  
  /* initialize all public and private members to reasonable default values. */
  
  self->private->dispose_has_run = FALSE;
  
  /* Initializations */
    
  self->private->width  = FIXED_WIDTH;
  self->private->height = FIXED_HEIGHT;
  self->private->selected = FALSE;
  self->children = NULL;
  self->track_nb = 0;
  self->private->slide_width = DEFAULT_MEDIA_SIZE;
  self->private->slide_both  = FALSE;
  
  /* Set background Color Desactivation of default pixmap is possible */
  
  pixmap = pitivi_drawing_getpixmap (GTK_WIDGET(self), bg_xpm);
  pitivi_drawing_set_pixmap_bg (GTK_WIDGET(self), pixmap);
  
  /* Drag and drop signal connection */
  
  gtk_drag_dest_set  (GTK_WIDGET (self), GTK_DEST_DEFAULT_ALL, 
		      TargetEntries,
		      iNbTargetEntries,
		      GDK_ACTION_COPY|GDK_ACTION_MOVE);
  
  g_signal_connect (G_OBJECT (self), "drag_leave",\
		    G_CALLBACK ( pitivi_timelinecellrenderer_drag_leave ), NULL);
  g_signal_connect (G_OBJECT (self), "drag_data_received",\
		    G_CALLBACK ( pitivi_timelinecellrenderer_drag_data_received ), NULL);
  g_signal_connect (G_OBJECT (self), "drag_motion",
		    G_CALLBACK ( pitivi_timelinecellrenderer_drag_motion ), NULL);
  g_signal_connect (G_OBJECT (self), "button_release_event",
		    G_CALLBACK ( pitivi_timelinecellrenderer_button_release_event ), NULL);
}

static void
pitivi_timelinecellrenderer_class_init (gpointer g_class, gpointer g_class_data)
{
  GObjectClass *cellobj_class = G_OBJECT_CLASS (g_class);
  PitiviTimelineCellRendererClass *cell_class = PITIVI_TIMELINECELLRENDERER_CLASS (g_class);
 
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (g_class);
  GtkContainerClass *container_class = (GtkContainerClass*) (g_class);
  
  parent_class = gtk_type_class (GTK_TYPE_LAYOUT);
  cellobj_class->constructor = pitivi_timelinecellrenderer_constructor;
  cellobj_class->dispose = pitivi_timelinecellrenderer_dispose;
  cellobj_class->finalize = pitivi_timelinecellrenderer_finalize;
  cellobj_class->set_property = pitivi_timelinecellrenderer_set_property;
  cellobj_class->get_property = pitivi_timelinecellrenderer_get_property;
  
  /* Widget properties */
  
  widget_class->expose_event = pitivi_timelinecellrenderer_expose;
  widget_class->scroll_event = pitivi_timelinecellrenderer_scroll_event;
  
  /* Container Properties */

  /* Install the properties in the class here ! */
  
  g_object_class_install_property (G_OBJECT_CLASS (cellobj_class), PROP_TYPE_LAYER_PROPERTY,
				   g_param_spec_int ("track_type","track_type","track_type",
						     G_MININT, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  
  g_object_class_install_property (G_OBJECT_CLASS (cellobj_class), PROP_TIMELINEWINDOW,
				   g_param_spec_pointer ("timelinewindow","timelinewindow","timelinewindow",
							 G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  
  g_object_class_install_property (G_OBJECT_CLASS (cellobj_class), PROP_TRACK_NB_PROPERTY,
				   g_param_spec_int ("track_nb","track_nb","track_nb",
						     G_MININT, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /* Signals */
  
  g_signal_new ("activate",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (PitiviTimelineCellRendererClass, activate),
		NULL, 
		NULL,                
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
  
  g_signal_new ("deactivate",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (PitiviTimelineCellRendererClass, deactivate),
		NULL, 
		NULL,                
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

  g_signal_new ("select",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (PitiviTimelineCellRendererClass, select),
		NULL, 
		NULL,                
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
  
   g_signal_new ("deselect",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (PitiviTimelineCellRendererClass, deselect),
		NULL, 
		NULL,                
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
   
   g_signal_new ("key-delete-source",
		 G_TYPE_FROM_CLASS (g_class),
		 G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		 G_STRUCT_OFFSET (PitiviTimelineCellRendererClass, key_delete),
		 NULL,
		 NULL,       
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);

   g_signal_new ("delete-source",
		 G_TYPE_FROM_CLASS (g_class),
		 G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		 G_STRUCT_OFFSET (PitiviTimelineCellRendererClass, delete),
		 NULL,
		 NULL,       
		 g_cclosure_marshal_VOID__POINTER,
		 G_TYPE_NONE, 1, G_TYPE_POINTER);
   
   g_signal_new ("drag-source-begin",
		 G_TYPE_FROM_CLASS (g_class),
		 G_SIGNAL_RUN_FIRST,
		 G_STRUCT_OFFSET (PitiviTimelineCellRendererClass, drag_source_begin),
		 NULL,
		 NULL,       
		 g_cclosure_marshal_VOID__POINTER,
		 G_TYPE_NONE, 1, G_TYPE_POINTER);

   g_signal_new ("double-click-source",
		 G_TYPE_FROM_CLASS (g_class),
		 G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		 G_STRUCT_OFFSET (PitiviTimelineCellRendererClass, dbk_source),
		 NULL,
		 NULL,       
		 g_cclosure_marshal_VOID__POINTER,
		 G_TYPE_NONE, 1, G_TYPE_POINTER);

   g_signal_new ("cut-source",
		 G_TYPE_FROM_CLASS (g_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (PitiviTimelineCellRendererClass, cut_source),
		 NULL,
		 NULL,       
		 g_cclosure_marshal_VOID__UINT_POINTER,
		 G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
   
   g_signal_new ("zoom-changed",
		 G_TYPE_FROM_CLASS (g_class),
		 G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		 G_STRUCT_OFFSET (PitiviTimelineCellRendererClass, zoom_changed),
		 NULL, 
		 NULL,                
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);

   cell_class->activate = pitivi_timelinecellrenderer_callb_activate;
   cell_class->deactivate = pitivi_timelinecellrenderer_callb_deactivate;
   cell_class->select = pitivi_timelinecellrenderer_callb_select;
   cell_class->deselect = pitivi_timelinecellrenderer_callb_deselect;
   cell_class->drag_source_begin = pitivi_timelinecellrenderer_callb_drag_source_begin;
   cell_class->delete = pitivi_timelinecellrenderer_callb_delete_sf;
   cell_class->dbk_source = pitivi_timelinecellrenderer_callb_dbk_source;
   cell_class->cut_source = pitivi_timelinecellrenderer_callb_cut_source;
   cell_class->zoom_changed = pitivi_timelinecellrenderer_zoom_changed;
   cell_class->key_delete = pitivi_timelinecellrenderer_key_delete;
}

GType
pitivi_timelinecellrenderer_get_type (void)
{
  static GType type = 0;
 
  if (type == 0)
    {
      static const GTypeInfo info = {
	sizeof (PitiviTimelineCellRendererClass),
	NULL,			/* base_init */
	NULL,			/* base_finalize */
	pitivi_timelinecellrenderer_class_init,	/* class_init */
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	sizeof (PitiviTimelineCellRenderer),
	0,			/* n_preallocs */
	pitivi_timelinecellrenderer_instance_init	/* instance_init */
      };
      type = g_type_register_static (GTK_TYPE_LAYOUT,
				     "PitiviTimelineCellRendererType", &info, 0);
    }
  return type;
}
