/* 
 * PiTiVi
 * Copyright (C) <2004> Edward G. Hervey <hervey_e@epita.fr>
 *                      Guillaume Casanova <casano_g@epita.fr>
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

char *
pitivi_file (const char *filename)
{
  char *fullname;
  int i;

  static char *paths[] = {
    DATADIR "/",
    DATADIR "/pixmaps/",
    DATADIR "/pixmaps/pitivi/",
    DATADIR "/pitivi/ui/",
    DATADIR "/ui/pitivi",
    DATADIR "/ui/",
    /* Developpement */
    "../pixmaps/",
    "../ui/"
  };

  for (i = 0; i < (int) G_N_ELEMENTS (paths); i++) {
    fullname = g_strconcat (paths[i], filename, NULL);
    if (g_file_test (fullname, G_FILE_TEST_EXISTS) == TRUE) {
      return fullname;
    }

    g_free (fullname);
  }

  g_warning ("Failed to find %s", filename);
  return NULL;
}