/* mp4_tag.c - 2005/08/06 */
/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2001-2005  Jerome Couderc <easytag@gmail.com>
 *  Copyright (C) 2005  Michael Ihde <mike.ihde@randomwalking.com>
 *  Copyright (C) 2005  Stewart Whitman <swhitman@cox.net>
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Portions of this code was borrowed from the MPEG4IP tools project */
#include "config.h" /* For definition of ENABLE_MP4. */

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "mp4_tag.h"
#include "picture.h"
#include "easytag.h"
#include "setting.h"
#include "log.h"
#include "misc.h"
#include "et_core.h"
#include "charset.h"
#include "mp4_tag_private.h"

G_DEFINE_TYPE (EtMP4Tag, et_mp4_tag, G_TYPE_OBJECT);

/**************
 * Prototypes *
 **************/
static void et_mp4_tag_unload (EtMP4Tag *tag);

/*************
 * Functions *
 *************/

static void
et_mp4_tag_finalize (GObject *object)
{
    et_mp4_tag_unload (ET_MP4_TAG (object));
    G_OBJECT_CLASS (et_mp4_tag_parent_class)->finalize (object);
}

static void
et_mp4_tag_init (EtMP4Tag *tag)
{
    tag->priv = G_TYPE_INSTANCE_GET_PRIVATE (tag, ET_TYPE_MP4_TAG,
                                             EtMP4TagPrivate);
}

static void
et_mp4_tag_class_init (EtMP4TagClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = et_mp4_tag_finalize;
    g_type_class_add_private (klass, sizeof (EtMP4TagPrivate));
}

static gboolean
et_mp4_tag_load_symbol (EtMP4Tag *tag, const gchar *name, gpointer *func_ptr)
{
    EtMP4TagPrivate *priv = tag->priv;

    if (!g_module_symbol (priv->module, name, func_ptr))
    {
        g_warning ("Failed to lookup symbol '%s'", name);
        return FALSE;
    }

    if (func_ptr == NULL)
    {
        g_warning ("Attempt to lookup symbol '%s' was NULL", name);
        return FALSE;
    }

    return TRUE;
}

static gboolean
et_mp4_tag_load_symbols (EtMP4Tag *tag)
{
    EtMP4TagPrivate *priv = tag->priv;
    gsize i;
    struct
    {
        const gchar *name;
        gpointer *func_ptr;
    } symbols[] =
    {
        { "MP4Read", (gpointer *)&priv->mp4v2_read },
        { "MP4Modify", (gpointer *)&priv->mp4v2_modify },
        { "MP4Close", (gpointer *)&priv->mp4v2_close },
        { "MP4TagsAlloc", (gpointer *)&priv->mp4v2_tags_alloc },
        { "MP4TagsFetch", (gpointer *)&priv->mp4v2_tags_fetch },
        { "MP4TagsFree", (gpointer *)&priv->mp4v2_tags_free },
        { "MP4TagsStore", (gpointer *)&priv->mp4v2_tags_store },
        { "MP4TagsSetName", (gpointer *)&priv->mp4v2_tags_set_title },
        { "MP4TagsSetArtist", (gpointer *)&priv->mp4v2_tags_set_artist },
        { "MP4TagsSetAlbum", (gpointer *)&priv->mp4v2_tags_set_album },
        { "MP4TagsSetAlbumArtist", (gpointer *)&priv->mp4v2_tags_set_album_artist },
        { "MP4TagsSetDisk", (gpointer *)&priv->mp4v2_tags_set_disk },
        { "MP4TagsSetReleaseDate", (gpointer *)&priv->mp4v2_tags_set_release_date },
        { "MP4TagsSetTrack", (gpointer *)&priv->mp4v2_tags_set_track },
        { "MP4TagsSetGenre", (gpointer *)&priv->mp4v2_tags_set_genre },
        { "MP4TagsSetComments", (gpointer *)&priv->mp4v2_tags_set_comments },
        { "MP4TagsSetComposer", (gpointer *)&priv->mp4v2_tags_set_composer },
        { "MP4TagsSetCopyright", (gpointer *)&priv->mp4v2_tags_set_copyright },
        { "MP4TagsSetEncodedBy", (gpointer *)&priv->mp4v2_tags_set_encoded_by },
        { "MP4TagsAddArtwork", (gpointer *)&priv->mp4v2_tags_add_artwork },
        { "MP4TagsSetArtwork", (gpointer *)&priv->mp4v2_tags_set_artwork },
        { "MP4TagsRemoveArtwork", (gpointer *)&priv->mp4v2_tags_remove_artwork },
        { "MP4GetTrackMediaDataName", (gpointer *)&priv->mp4v2_get_track_media_data_name },
        { "MP4GetTrackEsdsObjectTypeId", (gpointer *)&priv->mp4v2_get_track_esds_object_type_id },
        { "MP4GetTrackESConfiguration", (gpointer *)&priv->mp4v2_get_track_es_configuration },
        { "MP4GetNumberOfTracks", (gpointer *)&priv->mp4v2_get_n_tracks },
        { "MP4FindTrackId", (gpointer *)&priv->mp4v2_find_track_id },
        { "MP4GetTrackBitRate", (gpointer *)&priv->mp4v2_get_track_bitrate },
        { "MP4GetTrackAudioChannels", (gpointer *)&priv->mp4v2_get_track_audio_channels },
        { "MP4GetTrackTimeScale", (gpointer *)&priv->mp4v2_get_track_timescale },
        { "MP4GetTrackDuration", (gpointer *)&priv->mp4v2_get_track_duration },
        { "MP4ConvertFromTrackDuration", (gpointer *)&priv->mp4v2_convert_from_track_duration },
    };

    for (i = 0; i < G_N_ELEMENTS (symbols); i++)
    {
        if (!et_mp4_tag_load_symbol (tag, symbols[i].name,
                                     symbols[i].func_ptr))
        {
            return FALSE;
        }
    }

    return TRUE;
}

gboolean
et_mp4_tag_load (EtMP4Tag *tag)
{
    gchar *path;
    EtMP4TagPrivate *priv;

    g_return_val_if_fail (ET_IS_MP4_TAG (tag), FALSE);

    if (!g_module_supported ())
    {
        return FALSE;
    }

    path = g_module_build_path (LIBDIR, "mp4v2");

    priv = tag->priv;
    priv->module = g_module_open (path, G_MODULE_BIND_LAZY);

    if (!priv->module)
    {
        gchar *utf8_path = g_filename_display_name (path);
        Log_Print (LOG_WARNING, _("Unable to open mp4v2 library: '%s' (%s)"),
                   utf8_path, g_module_error ());
        g_free (utf8_path);
        g_free (path);

        return FALSE;
    }

    if (!et_mp4_tag_load_symbols (tag))
    {
        Log_Print (LOG_WARNING,
                   _("Unable to load symbols from mp4v2 library"));
        g_free (path);

        return FALSE;
    }

    return TRUE;
}

static void
et_mp4_tag_unload (EtMP4Tag *tag)
{
    g_return_if_fail (ET_IS_MP4_TAG (tag));

    if (!g_module_close (tag->priv->module))
    {
        Log_Print (LOG_WARNING, _("Unable to close mp4v2 library (%s)"),
                   g_module_error ());
    }
}

/*
 * Mp4_Tag_Read_File_Tag:
 *
 * Read tag data into an Mp4 file.
 *
 * cf. http://mp4v2.googlecode.com/svn/doc/1.9.0/api/example_2itmf_2tags_8c-example.html
 *
 * Note:
 *  - for string fields, //if field is found but contains no info (strlen(str)==0), we don't read it
 *  - for track numbers, if they are zero, then we don't read it
 */
gboolean
Mp4tag_Read_File_Tag (EtMP4Tag *tag, gchar *filename, File_Tag *FileTag)
{
    EtMP4TagPrivate *priv;
    MP4FileHandle mp4file = NULL;
    const MP4Tags *mp4tags = NULL;
    guint16 track, track_total;
    guint16 disk, disktotal;
    Picture *prev_pic = NULL;
    gint pic_num;
    const MP4TagArtwork *mp4artwork = NULL;

    g_return_val_if_fail (ET_IS_MP4_TAG (tag), FALSE);
    g_return_val_if_fail (filename != NULL && FileTag != NULL, FALSE);

    priv = tag->priv;

    /* Get data from tag */
    mp4file = priv->mp4v2_read (filename);
    if (mp4file == MP4_INVALID_FILE_HANDLE)
    {
        gchar *filename_utf8 = filename_to_display(filename);
        Log_Print(LOG_ERROR,_("ERROR while opening file: '%s' (%s)."),filename_utf8,_("MP4 format invalid"));
        g_free(filename_utf8);
        return FALSE;
    }

    mp4tags = priv->mp4v2_tags_alloc ();
    if (!priv->mp4v2_tags_fetch (mp4tags, mp4file))
    {
        gchar *filename_utf8 = filename_to_display(filename);
        Log_Print(LOG_ERROR,_("ERROR reading tags from file: '%s' (%s)."),filename_utf8,_("MP4 format invalid"));
        g_free(filename_utf8);
        return FALSE;
    }

    /* TODO Add error detection */

    /*********
     * Title *
     *********/
    if (mp4tags->name)
        FileTag->title = g_strdup(mp4tags->name);

    /**********
     * Artist *
     **********/
    if (mp4tags->artist)
        FileTag->artist = g_strdup(mp4tags->artist);

    /*********
     * Album *
     *********/
    if (mp4tags->album)
        FileTag->album = g_strdup(mp4tags->album);

    /****************
     * Album Artist *
     ****************/
    if (mp4tags->albumArtist)
        FileTag->album_artist = g_strdup(mp4tags->albumArtist);

    /**********************
     * Disk / Total Disks *
     **********************/
    if (mp4tags->disk)
    {
	disk = mp4tags->disk->index, disktotal = mp4tags->disk->total;
        if (disk != 0 && disktotal != 0)
            FileTag->disc_number = g_strdup_printf("%d/%d",(gint)disk,(gint)disktotal);
        else if (disk != 0)
            FileTag->disc_number = g_strdup_printf("%d",(gint)disk);
        else if (disktotal != 0)
            FileTag->disc_number = g_strdup_printf("/%d",(gint)disktotal);
        //if (disktotal != 0)
        //    FileTag->disk_number_total = g_strdup_printf("%d",(gint)disktotal);
    }

    /********
     * Year *
     ********/
    if (mp4tags->releaseDate)
        FileTag->year = g_strdup(mp4tags->releaseDate);

    /*************************
     * Track and Total Track *
     *************************/
    if (mp4tags->track)
    {

	track = mp4tags->track->index, track_total = mp4tags->track->total;
        if (track != 0)
            FileTag->track = NUMBER_TRACK_FORMATED ? g_strdup_printf("%.*d",NUMBER_TRACK_FORMATED_SPIN_BUTTON,(gint)track) : g_strdup_printf("%d",(gint)track);
        if (track_total != 0)
            FileTag->track_total = NUMBER_TRACK_FORMATED ? g_strdup_printf("%.*d",NUMBER_TRACK_FORMATED_SPIN_BUTTON,(gint)track_total) : g_strdup_printf("%d",(gint)track_total);
    }

    /*********
     * Genre *
     *********/
    if (mp4tags->genre)
        FileTag->genre = g_strdup(mp4tags->genre);

    /***********
     * Comment *
     ***********/
    if (mp4tags->comments)
        FileTag->comment = g_strdup(mp4tags->comments);

    /**********************
     * Composer or Writer *
     **********************/
    if (mp4tags->composer)
        FileTag->composer = g_strdup(mp4tags->composer);

    /* Copyright. */
    if (mp4tags->copyright)
    {
        FileTag->copyright = g_strdup (mp4tags->copyright);
    }

    /*****************
     * Encoding Tool *
     *****************/
    if (mp4tags->encodedBy)
        FileTag->encoded_by = g_strdup(mp4tags->encodedBy);

    /* Unimplemented
    Tempo / BPM
    MP4GetMetadataTempo(file, &string)
    */

    /***********
     * Picture *
     ***********/
    // Version 1.9.1 of mp4v2 and up handle multiple cover art
    mp4artwork = mp4tags->artwork;
    for (pic_num = 0; pic_num < mp4tags->artworkCount; ++pic_num, ++mp4artwork)
    {
        Picture *pic;

        pic = Picture_Allocate();
        if (!prev_pic)
            FileTag->picture = pic;
        else
            prev_pic->next = pic;
        prev_pic = pic;

        pic->size = mp4artwork->size;
        pic->data = g_memdup(mp4artwork->data, pic->size);
	/* mp4artwork->type gives image type. */
        pic->type = ET_PICTURE_TYPE_FRONT_COVER;
        pic->description = NULL;
    }


    /* Free allocated data */
    priv->mp4v2_tags_free (mp4tags);
    priv->mp4v2_close (mp4file, 0);

    return TRUE;
}


/*
 * Mp4_Tag_Write_File_Tag:
 *
 * Write tag data into an Mp4 file.
 *
 * Note:
 *  - for track numbers, we write 0's if one or the other is blank
 */
gboolean
Mp4tag_Write_File_Tag (EtMP4Tag *tag, ET_File *ETFile)
{
    EtMP4TagPrivate *priv;
    File_Tag *FileTag;
    gchar    *filename;
    gchar    *filename_utf8;
    MP4FileHandle mp4file = NULL;
    const MP4Tags *mp4tags = NULL;
    MP4TagDisk mp4disk;
    MP4TagTrack mp4track;
    MP4TagArtwork mp4artwork;
    gint error = 0;

    g_return_val_if_fail (ET_IS_MP4_TAG (tag), FALSE);
    g_return_val_if_fail (ETFile != NULL && ETFile->FileTag != NULL, FALSE);

    /* extra initializers */
    mp4disk.index  = 0;
    mp4disk.total  = 0;
    mp4track.index = 0;
    mp4track.total = 0;

    priv = tag->priv;

    FileTag = (File_Tag *)ETFile->FileTag->data;
    filename      = ((File_Name *)ETFile->FileNameCur->data)->value;
    filename_utf8 = ((File_Name *)ETFile->FileNameCur->data)->value_utf8;

    /* Open file for writing */
    mp4file = priv->mp4v2_modify (filename, 0);
    if (mp4file == MP4_INVALID_FILE_HANDLE)
    {
        Log_Print(LOG_ERROR,_("ERROR while opening file: '%s' (%s)."),filename_utf8,_("MP4 format invalid"));
        return FALSE;
    }

    mp4tags = priv->mp4v2_tags_alloc ();
    if (!priv->mp4v2_tags_fetch (mp4tags, mp4file))
    {
        Log_Print(LOG_ERROR,_("ERROR reading tags from file: '%s' (%s)."),filename_utf8,_("MP4 format invalid"));
        return FALSE;
    }

    /*********
     * Title *
     *********/
    if (FileTag->title && g_utf8_strlen(FileTag->title, -1) > 0)
    {
        priv->mp4v2_tags_set_title (mp4tags, FileTag->title);
    }else
    {
        priv->mp4v2_tags_set_title (mp4tags, "");
    }

    /**********
     * Artist *
     **********/
    if (FileTag->artist && g_utf8_strlen(FileTag->artist, -1) > 0)
    {
        priv->mp4v2_tags_set_artist (mp4tags, FileTag->artist);
    }else
    {
        priv->mp4v2_tags_set_artist (mp4tags, "");
    }

    /*********
     * Album *
     *********/
    if (FileTag->album && g_utf8_strlen(FileTag->album, -1) > 0)
    {
        priv->mp4v2_tags_set_album (mp4tags, FileTag->album);
    }else
    {
        priv->mp4v2_tags_set_album (mp4tags, "");
    }

    /****************
     * Album Artist *
     ****************/
    if (FileTag->album_artist && g_utf8_strlen(FileTag->album_artist, -1) > 0)
    {
        priv->mp4v2_tags_set_album_artist (mp4tags, FileTag->album_artist);
    }else
    {
        priv->mp4v2_tags_set_album_artist (mp4tags, "");
    }

    /**********************
     * Disk / Total Disks *
     **********************/
    if (FileTag->disc_number && g_utf8_strlen(FileTag->disc_number, -1) > 0)
    //|| FileTag->disc_number_total && g_utf8_strlen(FileTag->disc_number_total, -1) > 0)
    {
        /* At the present time, we manage only disk number like '1' or '1/2', we
         * don't use disk number total... so here we try to decompose */
        if (FileTag->disc_number)
        {
            gchar *dn_tmp = g_strdup(FileTag->disc_number);
            gchar *tmp    = strchr(dn_tmp,'/');
            if (tmp)
            {
                // A disc_number_total was entered
                if ( (tmp+1) && atoi(tmp+1) )
                    mp4disk.total = atoi(tmp+1);

                // Fill disc_number
                *tmp = '\0';
                mp4disk.index = atoi(dn_tmp);
            }else
            {
                mp4disk.index = atoi(FileTag->disc_number);
            }
            g_free(dn_tmp);
        }
        /*if (FileTag->disc_number)
            mp4disk.index = atoi(FileTag->disc_number);
        if (FileTag->disc_number_total)
            mp4disk.total = atoi(FileTag->disc_number_total);
        */
    }
    priv->mp4v2_tags_set_disk (mp4tags, &mp4disk);

    /********
     * Year *
     ********/
    if (FileTag->year && g_utf8_strlen(FileTag->year, -1) > 0)
    {
        priv->mp4v2_tags_set_release_date (mp4tags, FileTag->year);
    }else
    {
        priv->mp4v2_tags_set_release_date (mp4tags, "");
    }

    /*************************
     * Track and Total Track *
     *************************/
    if ( (FileTag->track       && g_utf8_strlen(FileTag->track, -1) > 0)
    ||   (FileTag->track_total && g_utf8_strlen(FileTag->track_total, -1) > 0) )
    {
        if (FileTag->track)
            mp4track.index = atoi(FileTag->track);
        if (FileTag->track_total)
            mp4track.total = atoi(FileTag->track_total);
    }
    priv->mp4v2_tags_set_track (mp4tags, &mp4track);

    /*********
     * Genre *
     *********/
    if (FileTag->genre && g_utf8_strlen(FileTag->genre, -1) > 0 )
    {
        priv->mp4v2_tags_set_genre (mp4tags, FileTag->genre);
    }else
    {
        //MP4DeleteMetadataGenre(mp4tags);
        priv->mp4v2_tags_set_genre (mp4tags, "");
    }

    /***********
     * Comment *
     ***********/
    if (FileTag->comment && g_utf8_strlen(FileTag->comment, -1) > 0)
    {
        priv->mp4v2_tags_set_comments (mp4tags, FileTag->comment);
    }else
    {
        priv->mp4v2_tags_set_comments (mp4tags, "");
    }

    /**********************
     * Composer or Writer *
     **********************/
    if (FileTag->composer && g_utf8_strlen(FileTag->composer, -1) > 0)
    {
        priv->mp4v2_tags_set_composer (mp4tags, FileTag->composer);
    }else
    {
        priv->mp4v2_tags_set_composer (mp4tags, "");
    }

    /* Copyright. */
    if (FileTag->copyright && g_utf8_strlen (FileTag->copyright, -1) > 0)
    {
        priv->mp4v2_tags_set_copyright (mp4tags, FileTag->copyright);
    }
    else
    {
        priv->mp4v2_tags_set_copyright (mp4tags, "");
    }

    /*****************
     * Encoding Tool *
     *****************/
    if (FileTag->encoded_by && g_utf8_strlen(FileTag->encoded_by, -1) > 0)
    {
        priv->mp4v2_tags_set_encoded_by (mp4tags, FileTag->encoded_by);
    }else
    {
        priv->mp4v2_tags_set_encoded_by (mp4tags, "");
    }

    /***********
     * Picture *
     ***********/
    {
        // Can handle only one picture...
        Picture *pic;
        if (mp4tags->artworkCount && mp4tags->artwork)
            priv->mp4v2_tags_remove_artwork (mp4tags, 0);
        priv->mp4v2_tags_set_artwork (mp4tags, 0, NULL);
        for (pic = FileTag->picture; pic; pic = pic->next)
        {
            if (pic->type == ET_PICTURE_TYPE_FRONT_COVER)
            {
                 mp4artwork.data = pic->data;
                 mp4artwork.size = pic->size;
                 switch (pic->type) {
                  case PICTURE_FORMAT_JPEG:
                     mp4artwork.type = MP4_TAG_ARTWORK_TYPE_JPEG;
                     break;
                  case PICTURE_FORMAT_PNG:
                     mp4artwork.type = MP4_TAG_ARTWORK_TYPE_PNG;
                     break;
                  default:
                     mp4artwork.type = MP4_TAG_ARTWORK_TYPE_UNDEFINED;
                 }
                 if (mp4tags->artworkCount)
                     priv->mp4v2_tags_set_artwork (mp4tags, 0, &mp4artwork);
                 else
                     priv->mp4v2_tags_add_artwork (mp4tags, &mp4artwork);
            }
        }
    }

    priv->mp4v2_tags_store (mp4tags, mp4file);
    priv->mp4v2_tags_free (mp4tags);
    priv->mp4v2_close (mp4file, 0);

    if (error) return FALSE;
    else       return TRUE;
}
