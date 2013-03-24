/* config.h - 2000/06/21 */
/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
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


#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <gtk/gtk.h>

/***************
 * Declaration *
 ***************/

typedef enum
{
    CV_TYPE_STRING=0,
    CV_TYPE_INT,
    CV_TYPE_BOOL
} Config_Variable_Type;


typedef struct _tConfigVariable tConfigVariable;
struct _tConfigVariable
{
    char *name;                 /* Variable name written in config file */
    Config_Variable_Type type;  /* Variable type: Integer, Alphabetic, ... */
    void *pointer;              /* Pointer to our variable */
};

typedef enum
{
    ET_FILENAME_EXTENSION_LOWER_CASE,
    ET_FILENAME_EXTENSION_UPPER_CASE,
    ET_FILENAME_EXTENSION_NO_CHANGE
} EtFilenameExtensionMode;

/*
 * The mode for the scanner window. See the GSettings key "scanner-type".
 */
typedef enum
{
    ET_SCAN_TYPE_FILL_TAG,
    ET_SCAN_TYPE_RENAME_FILE,
    ET_SCAN_TYPE_PROCESS_FIELDS
} EtScanType;

/*
 * Types of sorting. See the GSettings key "sort-mode".
 */
typedef enum
{
    ET_SORT_MODE_ASCENDING_FILENAME,
    ET_SORT_MODE_DESCENDING_FILENAME,
    ET_SORT_MODE_ASCENDING_TITLE,
    ET_SORT_MODE_DESCENDING_TITLE,
    ET_SORT_MODE_ASCENDING_ARTIST,
    ET_SORT_MODE_DESCENDING_ARTIST,
    ET_SORT_MODE_ASCENDING_ALBUM_ARTIST,
    ET_SORT_MODE_DESCENDING_ALBUM_ARTIST,
    ET_SORT_MODE_ASCENDING_ALBUM,
    ET_SORT_MODE_DESCENDING_ALBUM,
    ET_SORT_MODE_ASCENDING_YEAR,
    ET_SORT_MODE_DESCENDING_YEAR,
    ET_SORT_MODE_ASCENDING_DISC_NUMBER,
    ET_SORT_MODE_DESCENDING_DISC_NUMBER,
    ET_SORT_MODE_ASCENDING_TRACK_NUMBER,
    ET_SORT_MODE_DESCENDING_TRACK_NUMBER,
    ET_SORT_MODE_ASCENDING_GENRE,
    ET_SORT_MODE_DESCENDING_GENRE,
    ET_SORT_MODE_ASCENDING_COMMENT,
    ET_SORT_MODE_DESCENDING_COMMENT,
    ET_SORT_MODE_ASCENDING_COMPOSER,
    ET_SORT_MODE_DESCENDING_COMPOSER,
    ET_SORT_MODE_ASCENDING_ORIG_ARTIST,
    ET_SORT_MODE_DESCENDING_ORIG_ARTIST,
    ET_SORT_MODE_ASCENDING_COPYRIGHT,
    ET_SORT_MODE_DESCENDING_COPYRIGHT,
    ET_SORT_MODE_ASCENDING_URL,
    ET_SORT_MODE_DESCENDING_URL,
    ET_SORT_MODE_ASCENDING_ENCODED_BY,
    ET_SORT_MODE_DESCENDING_ENCODED_BY,
    ET_SORT_MODE_ASCENDING_CREATION_DATE,
    ET_SORT_MODE_DESCENDING_CREATION_DATE,
    ET_SORT_MODE_ASCENDING_FILE_TYPE,
    ET_SORT_MODE_DESCENDING_FILE_TYPE,
    ET_SORT_MODE_ASCENDING_FILE_SIZE,
    ET_SORT_MODE_DESCENDING_FILE_SIZE,
    ET_SORT_MODE_ASCENDING_FILE_DURATION,
    ET_SORT_MODE_DESCENDING_FILE_DURATION,
    ET_SORT_MODE_ASCENDING_FILE_BITRATE,
    ET_SORT_MODE_DESCENDING_FILE_BITRATE,
    ET_SORT_MODE_ASCENDING_FILE_SAMPLERATE,
    ET_SORT_MODE_DESCENDING_FILE_SAMPLERATE
} EtSortMode;


/*
 * Config variables
 */
GSettings *ETSettings;

/* Common */
gchar  *DEFAULT_PATH_TO_MP3;

/* Misc */
/* User Interface. */
gint    SORTING_FILE_CASE_SENSITIVE;

gchar  *AUDIO_FILE_PLAYER;

/* File Settings */
gint    FILENAME_CHARACTER_SET_OTHER;
gint    FILENAME_CHARACTER_SET_APPROXIMATE;
gint    FILENAME_CHARACTER_SET_DISCARD;

/* Tag Settings */
gint    FILE_WRITING_ID3V2_VERSION_4;
gchar  *FILE_READING_ID3V1V2_CHARACTER_SET;

gchar  *FILE_WRITING_ID3V2_UNICODE_CHARACTER_SET;
gchar  *FILE_WRITING_ID3V2_NO_UNICODE_CHARACTER_SET;
gint    FILE_WRITING_ID3V2_ICONV_OPTIONS_NO;
gint    FILE_WRITING_ID3V2_ICONV_OPTIONS_TRANSLIT;
gint    FILE_WRITING_ID3V2_ICONV_OPTIONS_IGNORE;

gchar  *FILE_WRITING_ID3V1_CHARACTER_SET;
gint    FILE_WRITING_ID3V1_ICONV_OPTIONS_NO;
gint    FILE_WRITING_ID3V1_ICONV_OPTIONS_TRANSLIT;
gint    FILE_WRITING_ID3V1_ICONV_OPTIONS_IGNORE;

/* Scanner */
gint    FTS_CONVERT_UNDERSCORE_AND_P20_INTO_SPACE;
gint    FTS_CONVERT_SPACE_INTO_UNDERSCORE;
gint    RFS_CONVERT_UNDERSCORE_AND_P20_INTO_SPACE;
gint    RFS_CONVERT_SPACE_INTO_UNDERSCORE;
gint    RFS_REMOVE_SPACES;

/* Scanner window */
gint    PROCESS_FILENAME_FIELD;
gint    PROCESS_TITLE_FIELD;
gint    PROCESS_ARTIST_FIELD;
gint    PROCESS_ALBUM_ARTIST_FIELD;
gint    PROCESS_ALBUM_FIELD;
gint    PROCESS_GENRE_FIELD;
gint    PROCESS_COMMENT_FIELD;
gint    PROCESS_COMPOSER_FIELD;
gint    PROCESS_ORIG_ARTIST_FIELD;
gint    PROCESS_COPYRIGHT_FIELD;
gint    PROCESS_URL_FIELD;
gint    PROCESS_ENCODED_BY_FIELD;
gint    PF_CONVERT_INTO_SPACE;
gint    PF_CONVERT_SPACE;

/* Playlist window */
gchar  *PLAYLIST_NAME;
gint    PLAYLIST_CONTENT_NONE;
gint    PLAYLIST_CONTENT_FILENAME;
gint    PLAYLIST_CONTENT_MASK;
gchar  *PLAYLIST_CONTENT_MASK_VALUE;

/* CDDB in preferences window */
gchar  *CDDB_LOCAL_PATH;

/* CDDB window */
gint    CDDB_SEARCH_IN_ARTIST_FIELD;
gint    CDDB_SEARCH_IN_TITLE_FIELD;
gint    CDDB_SEARCH_IN_TRACK_NAME_FIELD;
gint    CDDB_SEARCH_IN_OTHER_FIELD;

gint    CDDB_SEARCH_IN_BLUES_CATEGORY;
gint    CDDB_SEARCH_IN_CLASSICAL_CATEGORY;
gint    CDDB_SEARCH_IN_COUNTRY_CATEGORY;
gint    CDDB_SEARCH_IN_FOLK_CATEGORY;
gint    CDDB_SEARCH_IN_JAZZ_CATEGORY;
gint    CDDB_SEARCH_IN_MISC_CATEGORY;
gint    CDDB_SEARCH_IN_NEWAGE_CATEGORY;
gint    CDDB_SEARCH_IN_REGGAE_CATEGORY;
gint    CDDB_SEARCH_IN_ROCK_CATEGORY;
gint    CDDB_SEARCH_IN_SOUNDTRACK_CATEGORY;

gint    CDDB_SET_TO_ALL_FIELDS;
gint    CDDB_SET_TO_TITLE;
gint    CDDB_SET_TO_ARTIST;
gint    CDDB_SET_TO_ALBUM;
gint    CDDB_SET_TO_YEAR;
gint    CDDB_SET_TO_TRACK;
gint    CDDB_SET_TO_TRACK_TOTAL;
gint    CDDB_SET_TO_GENRE;
gint    CDDB_SET_TO_FILE_NAME;

/* Default mask */
gchar  *SCAN_TAG_DEFAULT_MASK;
gchar  *RENAME_FILE_DEFAULT_MASK;
gchar  *RENAME_DIRECTORY_DEFAULT_MASK;


/**************
 * Prototypes *
 **************/

void Init_Config_Variables (void);
void Read_Config           (void);

void Save_Changes_Of_Preferences_Window  (void);
void Save_Changes_Of_UI                  (void);

gboolean Setting_Create_Files     (void);


/* MasksList */
void Load_Scan_Tag_Masks_List (GtkListStore *liststore, gint colnum, gchar **fallback);
void Save_Scan_Tag_Masks_List (GtkListStore *liststore, gint colnum);

/* RenameFileMasksList */
void Load_Rename_File_Masks_List (GtkListStore *liststore, gint colnum, gchar **fallback);
void Save_Rename_File_Masks_List (GtkListStore *liststore, gint colnum);

/* RenameDirectoryMasksList 'RenameDirectoryMaskCombo' combobox */
void Load_Rename_Directory_Masks_List (GtkListStore *liststore, gint colnum, gchar **fallback);
void Save_Rename_Directory_Masks_List (GtkListStore *liststore, gint colnum);

/* 'DefaultPathToMp3' combobox */
void Load_Default_Path_To_MP3_List (GtkListStore *liststore, gint colnum);
void Save_Default_Path_To_MP3_List (GtkListStore *liststore, gint colnum);

/* 'DefaultComment' combobox */
void Load_Default_Tag_Comment_Text_List (GtkListStore *liststore, gint colnum);
void Save_Default_Tag_Comment_Text_List (GtkListStore *liststore, gint colnum);

/* 'BrowserEntry' combobox */
void Load_Path_Entry_List (GtkListStore *liststore, gint colnum);
void Save_Path_Entry_List (GtkListStore *liststore, gint colnum);

/* 'PlayListNameEntry' combobox */
void Load_Play_List_Name_List (GtkListStore *liststore, gint colnum);
void Save_Play_List_Name_List (GtkListStore *liststore, gint colnum);

/* Run Program combobox (tree browser) */
void Load_Run_Program_With_Directory_List (GtkListStore *liststore, gint colnum);
void Save_Run_Program_With_Directory_List (GtkListStore *liststore, gint colnum);

/* Run Program combobox (file browser) */
void Load_Run_Program_With_File_List (GtkListStore *liststore, gint colnum);
void Save_Run_Program_With_File_List (GtkListStore *liststore, gint colnum);

/* 'FilePlayerEntry' combobox */
void Load_Audio_File_Player_List (GtkListStore *liststore, gint colnum);
void Save_Audio_File_Player_List (GtkListStore *liststore, gint colnum);

/* 'SearchStringEntry' combobox */
void Load_Search_File_List (GtkListStore *liststore, gint colnum);
void Save_Search_File_List (GtkListStore *liststore, gint colnum);

/* 'FileToLoad' combobox */
void Load_File_To_Load_List (GtkListStore *liststore, gint colnum);
void Save_File_To_Load_List (GtkListStore *liststore, gint colnum);

/* 'PlayListContentMaskEntry' combobox */
void Load_Playlist_Content_Mask_List (GtkListStore *liststore, gint colnum);
void Save_Playlist_Content_Mask_List (GtkListStore *liststore, gint colnum);

/* 'CddbSearchStringEntry' combobox */
void Load_Cddb_Search_String_List (GtkListStore *liststore, gint colnum);
void Save_Cddb_Search_String_List (GtkListStore *liststore, gint colnum);

/* 'CddbSearchStringInResultEntry' combobox */
void Load_Cddb_Search_String_In_Result_List (GtkListStore *liststore, gint colnum);
void Save_Cddb_Search_String_In_Result_List (GtkListStore *liststore, gint colnum);

/* 'CddbLocalPath' combobox */
void Load_Cddb_Local_Path_List (GtkListStore *liststore, gint colnum);
void Save_Cddb_Local_Path_List (GtkListStore *liststore, gint colnum);

gboolean et_settings_enum_get (GValue *value, GVariant *variant,
                               gpointer user_data);
GVariant *et_settings_enum_set (const GValue *value,
                                const GVariantType *expected_type,
                                gpointer user_data);
gboolean et_settings_enum_radio_get (GValue *value, GVariant *variant,
                                     gpointer user_data);
GVariant *et_settings_enum_radio_set (const GValue *value,
                                      const GVariantType *expected_type,
                                      gpointer user_data);


#endif /* __CONFIG_H__ */
