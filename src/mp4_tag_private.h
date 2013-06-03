/* EasyTAG - tag editor for audio files
 * Copyright (C) 2013  David King <amigadave@amigadave.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include <mp4v2/mp4v2.h>

struct _EtMP4TagPrivate
{
    /*< private >*/
    GModule *module;

    MP4FileHandle (*mp4v2_read) (const gchar *filename);
    MP4FileHandle (*mp4v2_modify) (const gchar *filename, guint32 flags);
    void (*mp4v2_close) (MP4FileHandle handle, guint32 flags);

    const MP4Tags* (*mp4v2_tagsalloc) (void);
    gboolean (*mp4v2_tagsfetch) (const MP4Tags *tags, MP4FileHandle handle);
    void (*mp4v2_tagsfree) (const MP4Tags *tags);
    gboolean (*mp4v2_tagsstore) (const MP4Tags *tags, MP4FileHandle handle);

    gboolean (*mp4v2_tagssetname) (const MP4Tags *tags, const gchar *str);
    gboolean (*mp4v2_tagssetartist) (const MP4Tags *tags, const gchar *str);
    gboolean (*mp4v2_tagssetalbum) (const MP4Tags *tags, const gchar *str);
    gboolean (*mp4v2_tagssetalbumartist) (const MP4Tags *tags,
                                          const gchar *str);
    gboolean (*mp4v2_tagssetdisk) (const MP4Tags *tags,
                                   const MP4TagDisk *disk);
    gboolean (*mp4v2_tagssetreleasedate) (const MP4Tags *tags,
                                          const gchar *str);
    gboolean (*mp4v2_tagssettrack) (const MP4Tags *tags,
                                    const MP4TagTrack *track);
    gboolean (*mp4v2_tagssetgenre) (const MP4Tags *tags, const gchar *str);
    gboolean (*mp4v2_tagssetcomments) (const MP4Tags *tags, const gchar *str);
    gboolean (*mp4v2_tagssetcomposer) (const MP4Tags *tags, const gchar *str);
    gboolean (*mp4v2_tagssetencodedby) (const MP4Tags *tags, const gchar *str);

    gboolean (*mp4v2_addartwork) (const MP4Tags *tags,
                                  const MP4TagArtwork *art);
    gboolean (*mp4v2_setartwork) (const MP4Tags *tags, guint32 artwork,
                                  const MP4TagArtwork *art);
    gboolean (*mp4v2_removeartwork) (const MP4Tags *tags, guint32 artwork);

    const gchar *(*mp4v2_gettrackmediadataname) (MP4FileHandle handle,
                                                 MP4TrackId id);
    guint8 (*mp4v2_gettrackesdsobjecttypeid) (MP4FileHandle handle,
                                              MP4TrackId trackId);
    gboolean (*mp4v2_gettrackesconfiguration) (MP4FileHandle handle,
                                               MP4TrackId id,
                                               guint8 **ppconfig,
                                               guint32 *pconfigsize);
    guint32 (*mp4v2_getnumberoftracks) (MP4FileHandle handle,
                                        const gchar *type, guint8 subtype);
    MP4TrackId (*mp4v2_findtrackid) (MP4FileHandle handle, guint16 index,
                                     const gchar *type, guint8 subtype);
    guint32 (*mp4v2_gettrackbitrate) (MP4FileHandle handle, MP4TrackId id);
    guint32 (*mp4v2_gettracktimescale) (MP4FileHandle handle, MP4TrackId id);
    int (*mp4v2_gettrackaudiochannels) (MP4FileHandle handle, MP4TrackId id);
    MP4Duration (*mp4v2_gettrackduration) (MP4FileHandle handle,
                                           MP4TrackId id);
    guint64 (*mp4v2_convertfromtrackduration) (MP4FileHandle handle,
                                               MP4TrackId id,
                                               MP4Duration duration,
                                               guint32 timescale);
};
