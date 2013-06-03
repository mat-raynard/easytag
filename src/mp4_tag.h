/* mp4_tag.h - 2005/08/06 */
/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2001-2005  Jerome Couderc <easytag@gmail.com>
 *  Copyright (C) 2005  Michael Ihde <mike.ihde@randomwalking.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef ET_MP4_TAG_H_
#define ET_MP4_TAG_H_

#include "et_core.h"

G_BEGIN_DECLS

#define ET_TYPE_MP4_TAG (et_mp4_tag_get_type ())
#define ET_MP4_TAG(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), ET_TYPE_MP4_TAG, EtMP4Tag))
#define ET_IS_MP4_TAG(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), ET_TYPE_MP4_TAG))

typedef struct _EtMP4Tag EtMP4Tag;
typedef struct _EtMP4TagClass EtMP4TagClass;
typedef struct _EtMP4TagPrivate EtMP4TagPrivate;

struct _EtMP4Tag
{
    /*< private >*/
    GObject parent_instance;
    EtMP4TagPrivate *priv;
};

struct _EtMP4TagClass
{
    /*< private >*/
    GObjectClass parent_class;
};

GType et_mp4_tag_get_type (void);
gboolean et_mp4_tag_load (EtMP4Tag *tag);

gboolean Mp4tag_Read_File_Tag  (EtMP4Tag *tag, gchar *filename,
                                File_Tag *FileTag);
gboolean Mp4tag_Write_File_Tag (EtMP4Tag *tag, ET_File *ETFile);

G_END_DECLS

#endif /* !ET_MP4_TAG_H_ */
