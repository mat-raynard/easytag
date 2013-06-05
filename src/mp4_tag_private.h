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

typedef enum
{
    MP4_TAG_ARTWORK_TYPE_UNDEFINED,
    MP4_TAG_ARTWORK_TYPE_BMP,
    MP4_TAG_ARTWORK_TYPE_GIF,
    MP4_TAG_ARTWORK_TYPE_JPEG,
    MP4_TAG_ARTWORK_TYPE_PNG
} MP4TagArtworkType;

typedef enum
{
    MP4_MPEG4_INVALID_AUDIO_TYPE,
    MP4_MPEG4_AAC_MAIN_AUDIO_TYPE,
    MP4_MPEG4_AAC_LC_AUDIO_TYPE,
    MP4_MPEG4_AAC_SSR_AUDIO_TYPE,
    MP4_MPEG4_AAC_LTP_AUDIO_TYPE,
    MP4_MPEG4_AAC_HE_AUDIO_TYPE,
    MP4_MPEG4_AAC_SCALABLE_AUDIO_TYPE,
    MP4_MPEG4_TWINVQ_AUDIO_TYPE, /* Not in mp4v2. */
    MP4_MPEG4_CELP_AUDIO_TYPE,
    MP4_MPEG4_HVXC_AUDIO_TYPE,
    /* 10, 11 unused. */
    MP4_MPEG4_TTSI_AUDIO_TYPE = 12,
    MP4_MPEG4_MAIN_SYNTHETIC_AUDIO_TYPE,
    MP4_MPEG4_WAVETABLE_AUDIO_TYPE,
    MP4_MPEG4_MIDI_AUDIO_TYPE,
    MP4_MPEG4_ALGORITHMIC_FX_AUDIO_TYPE,
    MP4_MPEG4_AUDIO_TYPE = 64
} MP4AudioType;

typedef enum
{
    MP4_MPEG2_AAC_MAIN_AUDIO_TYPE = 102,
    MP4_MPEG2_AAC_LC_AUDIO_TYPE = 103,
    MP4_MPEG2_AAC_SSR_AUDIO_TYPE = 104,
    MP4_MPEG2_AUDIO_TYPE = 105,
    MP4_MPEG1_AUDIO_TYPE = 107,
    // mpeg4ip's private definitions
    MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE = 224,
    MP4_VORBIS_AUDIO_TYPE = 225,
    MP4_ALAW_AUDIO_TYPE = 227,
    MP4_ULAW_AUDIO_TYPE = 228,
    MP4_G723_AUDIO_TYPE = 229,
    MP4_PCM16_BIG_ENDIAN_AUDIO_TYPE = 230
} MP4ProfileType;

/* Typedefs for calling into libmp4v2. */
typedef gpointer MP4FileHandle;
typedef guint32 MP4TrackId;
typedef guint64 MP4Duration;

#define MP4_INVALID_FILE_HANDLE ((MP4FileHandle)NULL)
#define MP4_AUDIO_TRACK_TYPE "soun"

typedef struct
{
    gpointer data;
    guint32 size;
    MP4TagArtworkType type;
} MP4TagArtwork;

typedef struct
{
    guint16 index;
    guint16 total;
} MP4TagDisk;

typedef struct
{
    guint16 index;
    guint16 total;
} MP4TagTrack;

typedef struct
{
    void *handle;
    const gchar *name;
    const gchar *artist;
    const gchar *albumArtist;
    const gchar *album;
    const gchar *grouping;
    const gchar *composer;
    const gchar *comments;
    const gchar *genre;
    const guint16 *genreType;
    const char *releaseDate;
    const MP4TagTrack *track;
    const MP4TagDisk *disk;
    const guint16 *tempo;
    const guint8 *compilation;
    const gchar *tvShow;
    const gchar *tvNetwork;
    const gchar *tvEpisodeID;
    const guint32 *tvSeason;
    const guint32 *tvEpisode;
    const gchar *description;
    const gchar *longDescription;
    const gchar *lyrics;
    const gchar *sortName;
    const gchar *sortArtist;
    const gchar *sortAlbumArtist;
    const gchar *sortAlbum;
    const gchar *sortComposer;
    const gchar *sortTVShow;
    const MP4TagArtwork *artwork;
    guint32 artworkCount;
    const gchar *copyright;
    const gchar *encodingTool;
    const gchar *encodedBy;
    const gchar *purchaseDate;
    const guint8 *podcast;
    const gchar *keywords;
    const gchar *category;
    const guint8 *hdVideo;
    const guint8 *mediaType;
    const guint8 *contentRating;
    const guint8 *gapless;
    const gchar *iTunesAccount;
    const guint8 *iTunesAccountType;
    const guint32 *iTunesCountry;
    const guint32 *contentID;
    const guint32 *artistID;
    const guint64 *playlistID;
    const guint32 *genreID;
    const guint32 *composerID;
    const gchar *xid;
} MP4Tags;

typedef MP4FileHandle (*_mp4v2_read) (const gchar *filename);
typedef MP4FileHandle (*_mp4v2_modify) (const gchar *filename, guint32 flags);
typedef void (*_mp4v2_close) (MP4FileHandle handle, guint32 flags);
typedef const MP4Tags * (*_mp4v2_tags_alloc) (void);
typedef gboolean (*_mp4v2_tags_fetch) (const MP4Tags *tags,
                                       MP4FileHandle handle);
typedef void (*_mp4v2_tags_free) (const MP4Tags *tags);
typedef gboolean (*_mp4v2_tags_store) (const MP4Tags *tags,
                                       MP4FileHandle handle);
typedef gboolean (*_mp4v2_tags_set_title) (const MP4Tags *tags,
                                           const gchar *title);
typedef gboolean (*_mp4v2_tags_set_artist) (const MP4Tags *tags,
                                            const gchar *artist);
typedef gboolean (*_mp4v2_tags_set_album) (const MP4Tags *tags,
                                           const gchar *album);
typedef gboolean (*_mp4v2_tags_set_album_artist) (const MP4Tags *tags,
                                                  const gchar *album_artist);
typedef gboolean (*_mp4v2_tags_set_disk) (const MP4Tags *tags,
                                          const MP4TagDisk *disk);
typedef gboolean (*_mp4v2_tags_set_release_date) (const MP4Tags *tags,
                                                  const gchar *date);
typedef gboolean (*_mp4v2_tags_set_track) (const MP4Tags *tags,
                                           const MP4TagTrack *tag);
typedef gboolean (*_mp4v2_tags_set_genre) (const MP4Tags *tags,
                                           const gchar *genre);
typedef gboolean (*_mp4v2_tags_set_comments) (const MP4Tags *tags,
                                              const gchar *comments);
typedef gboolean (*_mp4v2_tags_set_composer) (const MP4Tags *tags,
                                              const gchar *composer);
typedef gboolean (*_mp4v2_tags_set_encoded_by) (const MP4Tags *tags,
                                                const gchar *encoded_by);
typedef gboolean (*_mp4v2_tags_add_artwork) (const MP4Tags *tags,
                                             const MP4TagArtwork *disk);
typedef gboolean (*_mp4v2_tags_remove_artwork) (const MP4Tags *tags,
                                                guint32 id);
typedef gboolean (*_mp4v2_tags_set_artwork) (const MP4Tags *tags, guint32 id,
                                             const MP4TagArtwork *disk);
typedef const gchar * (*_mp4v2_get_track_media_data_name) (MP4FileHandle handle,
                                                           MP4TrackId id);
typedef guint8 (*_mp4v2_get_track_esds_object_type_id) (MP4FileHandle handle,
                                                        MP4TrackId id);
typedef gboolean (*_mp4v2_get_track_es_configuration) (MP4FileHandle handle,
                                                       MP4TrackId id,
                                                       guint8 **configs,
                                                       guint32 *config_size);
typedef guint32 (*_mp4v2_get_n_tracks) (MP4FileHandle handle,
                                        const gchar *type,
                                        guint8 sub_type);
typedef MP4TrackId (*_mp4v2_find_track_id) (MP4FileHandle handle,
                                            guint16 index,
                                            const gchar *type,
                                            const gchar *sub_type);
typedef guint32 (*_mp4v2_get_track_bitrate) (MP4FileHandle handle,
                                             MP4TrackId id);
typedef gint (*_mp4v2_get_track_audio_channels) (MP4FileHandle handle,
                                                 MP4TrackId id);
typedef guint32 (*_mp4v2_get_track_timescale) (MP4FileHandle handle,
                                               MP4TrackId id);
typedef MP4Duration (*_mp4v2_get_track_duration) (MP4FileHandle handle,
                                                  MP4TrackId id);
typedef guint64 (*_mp4v2_convert_from_track_duration) (MP4FileHandle handle,
                                                       MP4TrackId id,
                                                       MP4Duration duration,
                                                       guint32 timescale);

struct _EtMP4TagPrivate
{
    /*< private >*/
    GModule *module;

    MP4FileHandle (*mp4v2_read) (const gchar *filename);
    MP4FileHandle (*mp4v2_modify) (const gchar *filename, guint32 flags);
    void (*mp4v2_close) (MP4FileHandle handle, guint32 flags);

    _mp4v2_tags_alloc mp4v2_tags_alloc;
    _mp4v2_tags_fetch mp4v2_tags_fetch;
    _mp4v2_tags_free mp4v2_tags_free;
    _mp4v2_tags_store mp4v2_tags_store;
    _mp4v2_tags_set_title mp4v2_tags_set_title;
    _mp4v2_tags_set_artist mp4v2_tags_set_artist;
    _mp4v2_tags_set_album mp4v2_tags_set_album;
    _mp4v2_tags_set_album_artist mp4v2_tags_set_album_artist;
    _mp4v2_tags_set_disk mp4v2_tags_set_disk;
    _mp4v2_tags_set_release_date mp4v2_tags_set_release_date;
    _mp4v2_tags_set_track mp4v2_tags_set_track;
    _mp4v2_tags_set_genre mp4v2_tags_set_genre;
    _mp4v2_tags_set_comments mp4v2_tags_set_comments;
    _mp4v2_tags_set_composer mp4v2_tags_set_composer;
    _mp4v2_tags_set_encoded_by mp4v2_tags_set_encoded_by;
    _mp4v2_tags_add_artwork mp4v2_tags_add_artwork;
    _mp4v2_tags_remove_artwork mp4v2_tags_remove_artwork;
    _mp4v2_tags_set_artwork mp4v2_tags_set_artwork;
    _mp4v2_get_track_media_data_name mp4v2_get_track_media_data_name;
    _mp4v2_get_track_esds_object_type_id mp4v2_get_track_esds_object_type_id;
    _mp4v2_get_track_es_configuration mp4v2_get_track_es_configuration;
    _mp4v2_get_n_tracks mp4v2_get_n_tracks;
    _mp4v2_find_track_id mp4v2_find_track_id;
    _mp4v2_get_track_bitrate mp4v2_get_track_bitrate;
    _mp4v2_get_track_duration mp4v2_get_track_duration;
    _mp4v2_get_track_timescale mp4v2_get_track_timescale;
    _mp4v2_get_track_audio_channels mp4v2_get_track_audio_channels;
    _mp4v2_convert_from_track_duration mp4v2_convert_from_track_duration;
};
