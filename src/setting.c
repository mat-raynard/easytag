/* config.c - 2000/06/21 */
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

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "setting.h"
#include "prefs.h"
#include "bar.h"
#include "easytag.h"
#include "charset.h"
#include "scan.h"
#include "log.h"
#include "misc.h"
#include "cddb.h"
#include "browser.h"

#ifdef G_OS_WIN32
#include "win32/win32dep.h"
#endif /* G_OS_WIN32 */


/***************
 * Declaration *
 ***************/

/*
 * Nota :
 *  - no trailing slashes on directory name to avoid problem with
 *    NetBSD's mkdir(2).
 */

// File for configuration
static const gchar CONFIG_FILE[] = "easytagrc";
// File of masks for tag scanner
static const gchar SCAN_TAG_MASKS_FILE[] = "scan_tag.mask";
// File of masks for rename file scanner
static const gchar RENAME_FILE_MASKS_FILE[] = "rename_file.mask";
// File for history of RenameDirectoryMaskCombo combobox
static const gchar RENAME_DIRECTORY_MASKS_FILE[] = "rename_directory.mask";
// File for history of PlayListNameCombo combobox
static const gchar PLAY_LIST_NAME_MASKS_FILE[] = "play_list_name.mask";
// File for history of PlayListContentMaskEntry combobox
static const gchar PLAYLIST_CONTENT_MASKS_FILE[] = "playlist_content.mask";
// File for history of DefaultPathToMp3 combobox
static const gchar DEFAULT_PATH_TO_MP3_HISTORY_FILE[] = "default_path_to_mp3.history";
// File for history of DefaultComment combobox
static const gchar DEFAULT_TAG_COMMENT_HISTORY_FILE[] = "default_tag_comment.history";
// File for history of BrowserEntry combobox
static const gchar PATH_ENTRY_HISTORY_FILE[] = "browser_path.history";
// File for history of run program combobox for directories
static const gchar RUN_PROGRAM_WITH_DIRECTORY_HISTORY_FILE[] = "run_program_with_directory.history";
// File for history of run program combobox for files
static const gchar RUN_PROGRAM_WITH_FILE_HISTORY_FILE[] = "run_program_with_file.history";
// File for history of run player combobox
static const gchar AUDIO_FILE_PLAYER_HISTORY_FILE[] = "audio_file_player.history";
// File for history of search string combobox
static const gchar SEARCH_FILE_HISTORY_FILE[] = "search_file.history";
// File for history of FileToLoad combobox
static const gchar FILE_TO_LOAD_HISTORY_FILE[] = "file_to_load.history";
// File for history of CddbSearchStringEntry combobox
static const gchar CDDB_SEARCH_STRING_HISTORY_FILE[] = "cddb_search_string.history";
// File for history of CddbSearchStringInResultEntry combobox
static const gchar CDDB_SEARCH_STRING_IN_RESULT_HISTORY_FILE[] = "cddb_search_string_in_result.history";
// File for history of CddbLocalPath combobox
static const gchar CDDB_LOCAL_PATH_HISTORY_FILE[] = "cddb_local_path.history";



/**************
 * Prototypes *
 **************/

static void Save_Config_To_File (void);
static gboolean Create_Easytag_Directory (void);



/********************
 * Config Variables *
 ********************/
tConfigVariable Config_Variables[] =
{
    {"default_path_to_mp3",                 CV_TYPE_STRING,  &DEFAULT_PATH_TO_MP3               },

    {"sorting_file_mode",                    CV_TYPE_INT,     &SORTING_FILE_MODE                        },
    {"sorting_file_case_sensitive",          CV_TYPE_BOOL,    &SORTING_FILE_CASE_SENSITIVE              },

    {"filename_extension_lower_case",                  CV_TYPE_BOOL,    &FILENAME_EXTENSION_LOWER_CASE            },
    {"filename_extension_upper_case",                  CV_TYPE_BOOL,    &FILENAME_EXTENSION_UPPER_CASE            },
    {"filename_extension_no_change",                   CV_TYPE_BOOL,    &FILENAME_EXTENSION_NO_CHANGE             },
    {"filename_character_set_other",                   CV_TYPE_BOOL,    &FILENAME_CHARACTER_SET_OTHER             },
    {"filename_character_set_approximate",             CV_TYPE_BOOL,    &FILENAME_CHARACTER_SET_APPROXIMATE       },
    {"filename_character_set_discard",                 CV_TYPE_BOOL,    &FILENAME_CHARACTER_SET_DISCARD           },

    {"file_reading_id3v1v2_character_set",             CV_TYPE_STRING,&FILE_READING_ID3V1V2_CHARACTER_SET},
    {"file_writing_id3v2_version_4",                   CV_TYPE_BOOL,  &FILE_WRITING_ID3V2_VERSION_4   },
    {"file_writing_id3v2_unicode_character_set",       CV_TYPE_STRING,&FILE_WRITING_ID3V2_UNICODE_CHARACTER_SET},
    {"file_writing_id3v2_no_unicode_character_set",    CV_TYPE_STRING,&FILE_WRITING_ID3V2_NO_UNICODE_CHARACTER_SET},
    {"file_writing_id3v2_iconv_options_no",            CV_TYPE_BOOL,  &FILE_WRITING_ID3V2_ICONV_OPTIONS_NO},
    {"file_writing_id3v2_iconv_options_translit",      CV_TYPE_BOOL,  &FILE_WRITING_ID3V2_ICONV_OPTIONS_TRANSLIT},
    {"file_writing_id3v2_iconv_options_ignore",        CV_TYPE_BOOL,  &FILE_WRITING_ID3V2_ICONV_OPTIONS_IGNORE},
    {"file_writing_id3v1_character_set",               CV_TYPE_STRING,&FILE_WRITING_ID3V1_CHARACTER_SET},
    {"file_writing_id3v1_iconv_options_no",            CV_TYPE_BOOL,  &FILE_WRITING_ID3V1_ICONV_OPTIONS_NO},
    {"file_writing_id3v1_iconv_options_translit",      CV_TYPE_BOOL,  &FILE_WRITING_ID3V1_ICONV_OPTIONS_TRANSLIT},
    {"file_writing_id3v1_iconv_options_ignore",        CV_TYPE_BOOL,  &FILE_WRITING_ID3V1_ICONV_OPTIONS_IGNORE},

    {"audio_file_player",                       CV_TYPE_STRING,&AUDIO_FILE_PLAYER                        },

    {"scanner_type",                             CV_TYPE_INT, &SCANNER_TYPE                              },
    {"fts_convert_underscore_and_p20_into_space",CV_TYPE_BOOL,&FTS_CONVERT_UNDERSCORE_AND_P20_INTO_SPACE },
    {"fts_convert_space_into_underscore",        CV_TYPE_BOOL,&FTS_CONVERT_SPACE_INTO_UNDERSCORE         },
    {"rfs_convert_underscore_and_p20_into_space",CV_TYPE_BOOL,&RFS_CONVERT_UNDERSCORE_AND_P20_INTO_SPACE },
    {"rfs_convert_space_into_underscore",        CV_TYPE_BOOL,&RFS_CONVERT_SPACE_INTO_UNDERSCORE         },
    {"rfs_remove_spaces",                        CV_TYPE_BOOL,&RFS_REMOVE_SPACES                         },
    {"process_filename_field",                  CV_TYPE_BOOL,    &PROCESS_FILENAME_FIELD                 },
    {"process_title_field",                     CV_TYPE_BOOL,    &PROCESS_TITLE_FIELD                    },
    {"process_artist_field",                    CV_TYPE_BOOL,    &PROCESS_ARTIST_FIELD                   },
    {"process_album_artist_field",              CV_TYPE_BOOL,    &PROCESS_ALBUM_ARTIST_FIELD             },
    {"process_album_field",                     CV_TYPE_BOOL,    &PROCESS_ALBUM_FIELD                    },
    {"process_genre_field",                     CV_TYPE_BOOL,    &PROCESS_GENRE_FIELD                    },
    {"process_comment_field",                   CV_TYPE_BOOL,    &PROCESS_COMMENT_FIELD                  },
    {"process_composer_field",                  CV_TYPE_BOOL,    &PROCESS_COMPOSER_FIELD                 },
    {"process_orig_artist_field",               CV_TYPE_BOOL,    &PROCESS_ORIG_ARTIST_FIELD              },
    {"process_copyright_field",                 CV_TYPE_BOOL,    &PROCESS_COPYRIGHT_FIELD                },
    {"process_url_field",                       CV_TYPE_BOOL,    &PROCESS_URL_FIELD                      },
    {"process_encoded_by_field",                CV_TYPE_BOOL,    &PROCESS_ENCODED_BY_FIELD               },

    {"pf_convert_into_space",                   CV_TYPE_BOOL,    &PF_CONVERT_INTO_SPACE                  },
    {"pf_convert_space",                        CV_TYPE_BOOL,    &PF_CONVERT_SPACE                       },

    {"playlist_name",                           CV_TYPE_STRING,  &PLAYLIST_NAME                          },
    {"playlist_content_none",                   CV_TYPE_BOOL,    &PLAYLIST_CONTENT_NONE                  },
    {"playlist_content_filename",               CV_TYPE_BOOL,    &PLAYLIST_CONTENT_FILENAME              },
    {"playlist_content_mask",                   CV_TYPE_BOOL,    &PLAYLIST_CONTENT_MASK                  },
    {"playlist_content_mask_value",             CV_TYPE_STRING,  &PLAYLIST_CONTENT_MASK_VALUE            },

    {"cddb_local_path",                         CV_TYPE_STRING,  &CDDB_LOCAL_PATH                        },

    {"cddb_search_in_artist_field",             CV_TYPE_BOOL,    &CDDB_SEARCH_IN_ARTIST_FIELD            },
    {"cddb_search_in_title_field",              CV_TYPE_BOOL,    &CDDB_SEARCH_IN_TITLE_FIELD             },
    {"cddb_search_in_track_name_field",         CV_TYPE_BOOL,    &CDDB_SEARCH_IN_TRACK_NAME_FIELD        },
    {"cddb_search_in_other_field",              CV_TYPE_BOOL,    &CDDB_SEARCH_IN_OTHER_FIELD             },

    {"cddb_search_in_blues_categories",         CV_TYPE_BOOL,    &CDDB_SEARCH_IN_BLUES_CATEGORY          },
    {"cddb_search_in_classical_categories",     CV_TYPE_BOOL,    &CDDB_SEARCH_IN_CLASSICAL_CATEGORY      },
    {"cddb_search_in_country_categories",       CV_TYPE_BOOL,    &CDDB_SEARCH_IN_COUNTRY_CATEGORY        },
    {"cddb_search_in_folk_categories",          CV_TYPE_BOOL,    &CDDB_SEARCH_IN_FOLK_CATEGORY           },
    {"cddb_search_in_jazz_categories",          CV_TYPE_BOOL,    &CDDB_SEARCH_IN_JAZZ_CATEGORY           },
    {"cddb_search_in_misc_categories",          CV_TYPE_BOOL,    &CDDB_SEARCH_IN_MISC_CATEGORY           },
    {"cddb_search_in_newage_categories",        CV_TYPE_BOOL,    &CDDB_SEARCH_IN_NEWAGE_CATEGORY         },
    {"cddb_search_in_reggae_categories",        CV_TYPE_BOOL,    &CDDB_SEARCH_IN_REGGAE_CATEGORY         },
    {"cddb_search_in_rock_categories",          CV_TYPE_BOOL,    &CDDB_SEARCH_IN_ROCK_CATEGORY           },
    {"cddb_search_in_soundtrack_categories",    CV_TYPE_BOOL,    &CDDB_SEARCH_IN_SOUNDTRACK_CATEGORY     },

    {"cddb_set_to_all_fields",                  CV_TYPE_BOOL,    &CDDB_SET_TO_ALL_FIELDS                 },
    {"cddb_set_to_title",                       CV_TYPE_BOOL,    &CDDB_SET_TO_TITLE                      },
    {"cddb_set_to_artist",                      CV_TYPE_BOOL,    &CDDB_SET_TO_ARTIST                     },
    {"cddb_set_to_album",                       CV_TYPE_BOOL,    &CDDB_SET_TO_ALBUM                      },
    {"cddb_set_to_year",                        CV_TYPE_BOOL,    &CDDB_SET_TO_YEAR                       },
    {"cddb_set_to_track",                       CV_TYPE_BOOL,    &CDDB_SET_TO_TRACK                      },
    {"cddb_set_to_track_total",                 CV_TYPE_BOOL,    &CDDB_SET_TO_TRACK_TOTAL                },
    {"cddb_set_to_genre",                       CV_TYPE_BOOL,    &CDDB_SET_TO_GENRE                      },
    {"cddb_set_to_file_name",                   CV_TYPE_BOOL,    &CDDB_SET_TO_FILE_NAME                  },

    {"scan_tag_default_mask",                   CV_TYPE_STRING,  &SCAN_TAG_DEFAULT_MASK                  },
    {"rename_file_default_mask",                CV_TYPE_STRING,  &RENAME_FILE_DEFAULT_MASK               },
    {"rename_directory_default_mask",           CV_TYPE_STRING,  &RENAME_DIRECTORY_DEFAULT_MASK          },
};




/*************
 * Functions *
 *************/

/*
 * Define and Load default values into config variables
 */
void Init_Config_Variables (void)
{
    const gchar *music_dir;

    ETSettings = g_settings_new ("org.gnome.EasyTAG");
    /*
     * Common
     */

    music_dir = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
    DEFAULT_PATH_TO_MP3 = music_dir ? g_strdup (music_dir)
                                    : g_strdup (g_get_home_dir ());

    /*
     * Misc
     */
    SORTING_FILE_MODE                       = SORTING_BY_ASCENDING_FILENAME;
#ifdef G_OS_WIN32
    SORTING_FILE_CASE_SENSITIVE             = 1;
#else /* !G_OS_WIN32 */
    SORTING_FILE_CASE_SENSITIVE             = 0;
#endif /* !G_OS_WIN32 */

#ifdef G_OS_WIN32
    AUDIO_FILE_PLAYER                       = ET_Win32_Get_Audio_File_Player();
#else /* !G_OS_WIN32 */
    AUDIO_FILE_PLAYER                       = g_strdup("xdg-open");
#endif /* !G_OS_WIN32 */

    /*
     * File Settings
     */
    FILENAME_EXTENSION_LOWER_CASE               = 1;
    FILENAME_EXTENSION_UPPER_CASE               = 0;
    FILENAME_EXTENSION_NO_CHANGE                = 0;

    FILENAME_CHARACTER_SET_OTHER                = 1;
    FILENAME_CHARACTER_SET_APPROXIMATE          = 0;
    FILENAME_CHARACTER_SET_DISCARD              = 0;

    /*
     * Tag Settings
     */
    FILE_READING_ID3V1V2_CHARACTER_SET              = g_strdup("UTF-8");
#ifdef G_OS_WIN32
    FILE_WRITING_ID3V2_VERSION_4                    = 0;
#else /* !G_OS_WIN32 */
    FILE_WRITING_ID3V2_VERSION_4                    = 1;
#endif /* !G_OS_WIN32 */
#ifdef G_OS_WIN32
    FILE_WRITING_ID3V2_UNICODE_CHARACTER_SET        = g_strdup("UTF-16");
#else /* !G_OS_WIN32 */
    FILE_WRITING_ID3V2_UNICODE_CHARACTER_SET        = g_strdup("UTF-8");
#endif /* !G_OS_WIN32 */
    FILE_WRITING_ID3V2_NO_UNICODE_CHARACTER_SET     = g_strdup("ISO-8859-1");
    FILE_WRITING_ID3V2_ICONV_OPTIONS_NO             = 1;
    FILE_WRITING_ID3V2_ICONV_OPTIONS_TRANSLIT       = 0;
    FILE_WRITING_ID3V2_ICONV_OPTIONS_IGNORE         = 0;
    FILE_WRITING_ID3V1_CHARACTER_SET                = g_strdup("ISO-8859-1");
    FILE_WRITING_ID3V1_ICONV_OPTIONS_NO             = 0;
    FILE_WRITING_ID3V1_ICONV_OPTIONS_TRANSLIT       = 1;
    FILE_WRITING_ID3V1_ICONV_OPTIONS_IGNORE         = 0;

    /*
     * Scanner
     */
    SCANNER_TYPE                              = SCANNER_FILL_TAG;
    FTS_CONVERT_UNDERSCORE_AND_P20_INTO_SPACE = 1;
    FTS_CONVERT_SPACE_INTO_UNDERSCORE         = 0;
    RFS_CONVERT_UNDERSCORE_AND_P20_INTO_SPACE = 1;
    RFS_CONVERT_SPACE_INTO_UNDERSCORE         = 0;

    /*
     * Scanner window
     */
    PROCESS_FILENAME_FIELD             = 0;
    PROCESS_TITLE_FIELD                = 1;
    PROCESS_ARTIST_FIELD               = 1;
    PROCESS_ALBUM_ARTIST_FIELD         = 1;
    PROCESS_ALBUM_FIELD                = 1;
    PROCESS_GENRE_FIELD                = 1;
    PROCESS_COMMENT_FIELD              = 1;
    PROCESS_COMPOSER_FIELD             = 1;
    PROCESS_ORIG_ARTIST_FIELD          = 1;
    PROCESS_COPYRIGHT_FIELD            = 1;
    PROCESS_URL_FIELD                  = 1;
    PROCESS_ENCODED_BY_FIELD           = 1;

    PF_CONVERT_INTO_SPACE              = 1;
    PF_CONVERT_SPACE                   = 0;

    /*
     * Playlist window
     */
    PLAYLIST_NAME                   = g_strdup("playlist_%a_-_%b");
    PLAYLIST_CONTENT_NONE           = 0;
    PLAYLIST_CONTENT_FILENAME       = 1;
    PLAYLIST_CONTENT_MASK           = 0;
    PLAYLIST_CONTENT_MASK_VALUE     = g_strdup("%n/%l - %a - %b - %t");

    /*
     * CDDB window
     */
    CDDB_LOCAL_PATH                         = NULL;

    CDDB_SEARCH_IN_ARTIST_FIELD         = 1;
    CDDB_SEARCH_IN_TITLE_FIELD          = 1;
    CDDB_SEARCH_IN_TRACK_NAME_FIELD     = 0;
    CDDB_SEARCH_IN_OTHER_FIELD          = 0;

    CDDB_SEARCH_IN_BLUES_CATEGORY       = 0;
    CDDB_SEARCH_IN_CLASSICAL_CATEGORY   = 0;
    CDDB_SEARCH_IN_COUNTRY_CATEGORY     = 0;
    CDDB_SEARCH_IN_FOLK_CATEGORY        = 0;
    CDDB_SEARCH_IN_JAZZ_CATEGORY        = 0;
    CDDB_SEARCH_IN_MISC_CATEGORY        = 1;
    CDDB_SEARCH_IN_NEWAGE_CATEGORY      = 1;
    CDDB_SEARCH_IN_REGGAE_CATEGORY      = 0;
    CDDB_SEARCH_IN_ROCK_CATEGORY        = 1;
    CDDB_SEARCH_IN_SOUNDTRACK_CATEGORY  = 0;

    CDDB_SET_TO_ALL_FIELDS  = 1;
    CDDB_SET_TO_TITLE       = 1;
    CDDB_SET_TO_ARTIST      = 0;
    CDDB_SET_TO_ALBUM       = 0;
    CDDB_SET_TO_YEAR        = 0;
    CDDB_SET_TO_TRACK       = 1;
    CDDB_SET_TO_TRACK_TOTAL = 1;
    CDDB_SET_TO_GENRE       = 0;
    CDDB_SET_TO_FILE_NAME   = 1;

    /*
     * Masks
     */
    SCAN_TAG_DEFAULT_MASK           = NULL;
    RENAME_FILE_DEFAULT_MASK        = NULL;
    RENAME_DIRECTORY_DEFAULT_MASK   = NULL;
}



/*
 * Function called when pressing the "Save" button of the preferences window.
 * Save into the config variables the settings of each tab of the Preferences window...
 * If settings needs to be "shown/applied" to the corresponding window, we do it
 */
static void
Apply_Changes_Of_Preferences_Window (void)
{
    gchar *temp;
    int active;

    if (OptionsWindow)
    {
        /* Common */
        if (DEFAULT_PATH_TO_MP3) g_free(DEFAULT_PATH_TO_MP3);
        DEFAULT_PATH_TO_MP3           = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(DefaultPathToMp3))))); // Saved in UTF-8
#if 0
#ifdef G_OS_WIN32
        ET_Win32_Path_Replace_Backslashes(DEFAULT_PATH_TO_MP3);
#endif /* G_OS_WIN32 */
#endif

        /* Misc */
        SORTING_FILE_CASE_SENSITIVE            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(SortingFileCaseSensitive));

        SORTING_FILE_MODE = gtk_combo_box_get_active(GTK_COMBO_BOX(SortingFileCombo));
        Browser_List_Refresh_Sort ();

        if (AUDIO_FILE_PLAYER) g_free(AUDIO_FILE_PLAYER);
        AUDIO_FILE_PLAYER                       = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(FilePlayerCombo)))));

        /* File Settings */
        FILENAME_EXTENSION_LOWER_CASE             = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FilenameExtensionLowerCase));
        FILENAME_EXTENSION_UPPER_CASE             = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FilenameExtensionUpperCase));
        FILENAME_EXTENSION_NO_CHANGE              = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FilenameExtensionNoChange));

        FILENAME_CHARACTER_SET_OTHER              = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FilenameCharacterSetOther));
        FILENAME_CHARACTER_SET_APPROXIMATE        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FilenameCharacterSetApproximate));
        FILENAME_CHARACTER_SET_DISCARD            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FilenameCharacterSetDiscard));

        /* Tag Settings */
#ifdef ENABLE_ID3LIB
        active = gtk_combo_box_get_active(GTK_COMBO_BOX(FileWritingId3v2VersionCombo));
        FILE_WRITING_ID3V2_VERSION_4 = !active;
#else
        FILE_WRITING_ID3V2_VERSION_4 = 1;
#endif
        temp = Get_Active_Combo_Box_Item(GTK_COMBO_BOX(FileReadingId3v1v2CharacterSetCombo));
        FILE_READING_ID3V1V2_CHARACTER_SET = Charset_Get_Name_From_Title(temp);
        g_free(temp);

        active = gtk_combo_box_get_active(GTK_COMBO_BOX(FileWritingId3v2UnicodeCharacterSetCombo));
        FILE_WRITING_ID3V2_UNICODE_CHARACTER_SET     = (active == 1) ? "UTF-16" : "UTF-8";

        temp = Get_Active_Combo_Box_Item(GTK_COMBO_BOX(FileWritingId3v2NoUnicodeCharacterSetCombo));
        FILE_WRITING_ID3V2_NO_UNICODE_CHARACTER_SET  = Charset_Get_Name_From_Title(temp);
        g_free(temp);

        FILE_WRITING_ID3V2_ICONV_OPTIONS_NO          = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FileWritingId3v2IconvOptionsNo));
        FILE_WRITING_ID3V2_ICONV_OPTIONS_TRANSLIT    = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FileWritingId3v2IconvOptionsTranslit));
        FILE_WRITING_ID3V2_ICONV_OPTIONS_IGNORE      = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FileWritingId3v2IconvOptionsIgnore));

        temp = Get_Active_Combo_Box_Item(GTK_COMBO_BOX(FileWritingId3v1CharacterSetCombo));
        FILE_WRITING_ID3V1_CHARACTER_SET             = Charset_Get_Name_From_Title(temp);
        g_free(temp);

        FILE_WRITING_ID3V1_ICONV_OPTIONS_NO          = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FileWritingId3v1IconvOptionsNo));
        FILE_WRITING_ID3V1_ICONV_OPTIONS_TRANSLIT    = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FileWritingId3v1IconvOptionsTranslit));
        FILE_WRITING_ID3V1_ICONV_OPTIONS_IGNORE      = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FileWritingId3v1IconvOptionsIgnore));

        /* Scanner */
        // Fill Tag Scanner
        FTS_CONVERT_UNDERSCORE_AND_P20_INTO_SPACE = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FTSConvertUnderscoreAndP20IntoSpace));
        FTS_CONVERT_SPACE_INTO_UNDERSCORE         = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FTSConvertSpaceIntoUnderscore));
        // Rename File Scanner
        RFS_CONVERT_UNDERSCORE_AND_P20_INTO_SPACE = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RFSConvertUnderscoreAndP20IntoSpace));
        RFS_CONVERT_SPACE_INTO_UNDERSCORE         = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RFSConvertSpaceIntoUnderscore));
				RFS_REMOVE_SPACES                         = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RFSRemoveSpaces));

        /* CDDB */
        if (CDDB_LOCAL_PATH) g_free(CDDB_LOCAL_PATH);
        CDDB_LOCAL_PATH = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(CddbLocalPath)))));

        /* Parameters and variables of Scanner Window are in "scan.c" file */
        /* Parameters and variables of Cddb Window are in "cddb.c" file */
    }

    /*
     * Changes to apply to :
     */
    if (ScannerWindow)
    {
        gtk_window_set_transient_for (GTK_WINDOW (ScannerWindow),
                                      GTK_WINDOW (MainWindow));
    }
}

/*
 * Save into the config variables the settings of each window
 *  - Position/size of the window
 *  - Specific options in the window
 */
static void
Apply_Changes_Of_UI (void)
{
    /*
     * Changes in user interface
     */

    /* Configuration of the main window (see easytag.c) - Function also called
     * when destroying the window. */
    MainWindow_Apply_Changes();

    // Configuration of the preference window (see prefs.c) - Function also called when destroying the window
    OptionsWindow_Apply_Changes();

    // Configuration of the scanner window (see scan.c) - Function also called when destroying the window
    ScannerWindow_Apply_Changes();

    // Configuration of the cddb window (see cddb.c) - Function also called when destroying the window
    Cddb_Window_Apply_Changes();

    // Configuration of the playlist window (see misc.c) - Function also called when destroying the window
    Write_Playlist_Window_Apply_Changes();

    // Configuration of the search_file window (see misc.c) - Function also called when destroying the window
    Search_File_Window_Apply_Changes();

    // Configuration of the load_filename window (see misc.c) - Function also called when destroying the window
    Load_Filename_Window_Apply_Changes();

}

void Save_Changes_Of_UI (void)
{
    Apply_Changes_Of_UI();
    Save_Config_To_File();
}

void Save_Changes_Of_Preferences_Window (void)
{
    Apply_Changes_Of_Preferences_Window();
    Save_Config_To_File();

    Statusbar_Message(_("Configuration saved"),TRUE);
}



/*
 * Write the config file
 */
static void
Save_Config_To_File (void)
{
    gchar *file_path = NULL;
    FILE *file;

    file_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                  CONFIG_FILE, NULL);

    if (!Create_Easytag_Directory () || (file = fopen (file_path, "w+")) == 0)
    {
        Log_Print (LOG_ERROR,
                   _("Error: Cannot write configuration file: %s (%s)"),
                   file_path, g_strerror(errno));
    }
    else
    {
        gint ConfigVarListLen = sizeof(Config_Variables)/sizeof(tConfigVariable);
        gint i;
        gchar *data = NULL;

        for (i=0; i<ConfigVarListLen; i++)
        {
            switch (Config_Variables[i].type)
            {
                case CV_TYPE_INT:
                {
                    data = g_strdup_printf("%s=%i\n",Config_Variables[i].name,
                                                     *(int *)Config_Variables[i].pointer);
                    fwrite(data,strlen(data),1,file);
                    //g_print("# (type:%d) %s",Config_Variables[i].type,data);
                    g_free(data);
                    break;
                }
                case CV_TYPE_BOOL:
                {
                    data = g_strdup_printf("%s=%i\n",Config_Variables[i].name,
                                                     ( *(int *)Config_Variables[i].pointer ? 1 : 0 ));
                    fwrite(data,strlen(data),1,file);
                    //g_print("# (type:%d) %s",Config_Variables[i].type,data);
                    g_free(data);
                    break;
                }
                case CV_TYPE_STRING:
                {
                    /* Doesn't write datum if empty */
                    if ( (*(char **)Config_Variables[i].pointer)==NULL ) break;

                    data = g_strdup_printf("%s=%s\n",Config_Variables[i].name,
                                                     *(char **)Config_Variables[i].pointer);
                    fwrite(data,strlen(data),1,file);
                    //g_print("# (type:%d) %s",Config_Variables[i].type,data);
                    g_free(data);
                    break;
                }
                default:
                {
                    Log_Print(LOG_ERROR,"ERROR: Can't save: type of config variable not supported "
                              "for '%s'!",Config_Variables[i].name);
                    break;
                }
            }
        }
        fclose(file);
    }
    g_free(file_path);
}


/*
 * Parse lines read (line as <var_description>=<value>) and load the values
 * into the corresponding config variables.
 */
static void
Set_Config (gchar *line)
{
    gchar *var_descriptor;
    gchar *var_value;
    gint ConfigVarListLen;
    gint i;

    if (*line=='\n' || *line=='#') return;

    /* Cut string */
    var_descriptor = strtok(line,"=");
    var_value      = strtok(NULL,"=");
    //g_print("\nstr1:'%s',\t str2:'%s'",var_descriptor,var_value);

    ConfigVarListLen = sizeof(Config_Variables)/sizeof(tConfigVariable);
    for (i=0; i<ConfigVarListLen; i++)
    {
        if (Config_Variables[i].name!=NULL && var_descriptor
        && !strcmp(Config_Variables[i].name,var_descriptor))
        {
            switch (Config_Variables[i].type)
            {
                case CV_TYPE_INT:
                {
                    *(int *)Config_Variables[i].pointer = strtol(var_value, NULL, 10);
                    break;
                }

                case CV_TYPE_BOOL:
                {
                    if (strtol(var_value, NULL, 10))
                        *(int *)Config_Variables[i].pointer = 1;
                    else
                        *(int *)Config_Variables[i].pointer = 0;
                    break;
                }

                case CV_TYPE_STRING:
                {
                    if (!var_value)
                    {
                        *(char **)Config_Variables[i].pointer = NULL;
                        //g_print("\nConfig File Warning: Field of '%s' has no value!\n",var_descriptor);
                    } else
                    {
                        if ( *(char **)Config_Variables[i].pointer != NULL )
                            g_free(*(char **)Config_Variables[i].pointer);
                        *(char **)Config_Variables[i].pointer = g_malloc(strlen(var_value)+1);
                        strcpy( *(char **)Config_Variables[i].pointer,var_value );
                    }
                    break;
                }

                default:
                {
                    Log_Print(LOG_ERROR,"ERROR: Can't read: type of config variable not supported "
                              "for '%s'!",Config_Variables[i].name);
                    break;
                }
            }
        }
    }
}


/*
 * Read config from config file
 */
void Read_Config (void)
{
    gchar *file_path = NULL;
    FILE *file;
    gchar buffer[MAX_STRING_LEN];

    /* The file to read */
    file_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                  CONFIG_FILE, NULL);

    if ((file = fopen (file_path,"r")) == 0)
    {
        Log_Print (LOG_ERROR, _("Cannot open configuration file '%s' (%s)"),
                   file_path, g_strerror (errno));
        Log_Print (LOG_OK, _("Loading default configuration"));
    }else
    {
        while (fgets(buffer,sizeof(buffer),file))
        {
            if (buffer[strlen(buffer)-1]=='\n')
                buffer[strlen(buffer)-1]='\0';
            Set_Config(buffer);
        }
        fclose(file);

        // Force this configuration! - Disabled as it is boring for russian people
        //USE_ISO_8859_1_CHARACTER_SET_TRANSLATION = 1;
        //USE_CHARACTER_SET_TRANSLATION            = 0;
    }
    g_free(file_path);
}


/*
 * check_or_create_file:
 * @filename: (type filename): the filename to create
 *
 * Check that the provided @filename exists, and if not, create it.
 */
static void check_or_create_file (const gchar *filename)
{
    FILE  *file;
    gchar *file_path = NULL;

    g_return_if_fail (filename != NULL);

    file_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                  filename, NULL);

    if ((file = fopen (file_path, "a+")) != NULL )
    {
        fclose (file);
    }
    else
    {
        Log_Print (LOG_ERROR, _("Cannot create or open file '%s' (%s)"),
                   CONFIG_FILE, g_strerror (errno));
    }

    g_free (file_path);
}

/*
 * Create the main directory with empty history files
 */
gboolean Setting_Create_Files (void)
{
    /* The file to write */
    if (!Create_Easytag_Directory ())
    {
        return FALSE;
    }

    check_or_create_file (SCAN_TAG_MASKS_FILE);
    check_or_create_file (RENAME_FILE_MASKS_FILE);
    check_or_create_file (RENAME_DIRECTORY_MASKS_FILE);
    check_or_create_file (DEFAULT_PATH_TO_MP3_HISTORY_FILE);
    check_or_create_file (DEFAULT_TAG_COMMENT_HISTORY_FILE);
    check_or_create_file (PATH_ENTRY_HISTORY_FILE);
    check_or_create_file (PLAY_LIST_NAME_MASKS_FILE);
    check_or_create_file (RUN_PROGRAM_WITH_DIRECTORY_HISTORY_FILE);
    check_or_create_file (RUN_PROGRAM_WITH_FILE_HISTORY_FILE);
    check_or_create_file (AUDIO_FILE_PLAYER_HISTORY_FILE);
    check_or_create_file (SEARCH_FILE_HISTORY_FILE);
    check_or_create_file (FILE_TO_LOAD_HISTORY_FILE);
    check_or_create_file (PLAYLIST_CONTENT_MASKS_FILE);
    check_or_create_file (CDDB_SEARCH_STRING_HISTORY_FILE);
    check_or_create_file (CDDB_SEARCH_STRING_IN_RESULT_HISTORY_FILE);
    check_or_create_file (CDDB_LOCAL_PATH_HISTORY_FILE);

    return TRUE;
}



/*
 * Save the contents of a list store to a file
 */
static void
Save_List_Store_To_File (const gchar *filename, GtkListStore *liststore, gint colnum)
{
    gchar *file_path = NULL;
    FILE *file;
    gchar *data = NULL;
    gchar *text;
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(liststore), &iter))
        return;

    /* The file to write */
    file_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                  filename, NULL);

    if (!Create_Easytag_Directory () || (file = fopen (file_path, "w+")) == NULL)
    {
        Log_Print (LOG_ERROR, _("Error: Cannot write list to file: %s (%s)"),
                   file_path, g_strerror (errno));
    }else
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(liststore), &iter, colnum, &text, -1);
            data = g_strdup_printf("%s\n",text);
            g_free(text);

            if (data)
            {
                fwrite(data,strlen(data),1,file);
                g_free(data);
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(liststore), &iter));
        fclose(file);
    }
    g_free(file_path);
}

/*
 * Populate a list store with data from a file passed in as first parameter
 */
static gboolean
Populate_List_Store_From_File (const gchar *filename, GtkListStore *liststore, gint text_column)
{

    gchar *file_path = NULL;
    FILE *file;
    gchar buffer[MAX_STRING_LEN];
    GtkTreeIter iter;
    gboolean entries_set = FALSE;

    /* The file to write */
    g_return_val_if_fail (filename != NULL, FALSE);

    file_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                  filename, NULL);

    if ((file = fopen (file_path, "r")) == NULL)
    {
        Log_Print (LOG_ERROR, _("Cannot open file '%s' (%s)"), file_path,
                   g_strerror (errno));
    }else
    {
        gchar *data = NULL;

        while(fgets(buffer,sizeof(buffer),file))
        {
            if (buffer[strlen(buffer)-1]=='\n')
                buffer[strlen(buffer)-1]='\0';

            /*if (g_utf8_validate(buffer, -1, NULL))
                data = g_strdup(buffer);
            else
                data = convert_to_utf8(buffer);*/
            data = Try_To_Validate_Utf8_String(buffer);

            if (data && g_utf8_strlen(data, -1) > 0)
            {
                gtk_list_store_append(liststore, &iter);
                gtk_list_store_set(liststore, &iter, text_column, data, -1);
                entries_set = TRUE;
            }
            g_free(data);
        }
        fclose(file);
    }
    g_free(file_path);
    return entries_set;
}


/*
 * Functions for writing and reading list of 'Fill Tag' masks
 */
void Load_Scan_Tag_Masks_List (GtkListStore *liststore, gint colnum, gchar **fallback)
{
    gint i = 0;
    GtkTreeIter iter;

    if (!Populate_List_Store_From_File(SCAN_TAG_MASKS_FILE, liststore, colnum))
    {
        // Fall back to defaults
        Log_Print(LOG_OK,_("Loading default 'Fill Tag' masks…"));

        while(fallback[i])
        {
            gtk_list_store_append(liststore, &iter);
            gtk_list_store_set(liststore, &iter, colnum, fallback[i], -1);
            i++;
        }
    }
}

void Save_Scan_Tag_Masks_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(SCAN_TAG_MASKS_FILE, liststore, colnum);
}


/*
 * Functions for writing and reading list of 'Rename File' masks
 */
void Load_Rename_File_Masks_List (GtkListStore *liststore, gint colnum, gchar **fallback)
{
    gint i = 0;
    GtkTreeIter iter;

    if (!Populate_List_Store_From_File(RENAME_FILE_MASKS_FILE, liststore, colnum))
    {
        // Fall back to defaults
        Log_Print(LOG_OK,_("Loading default 'Rename File' masks…"));

        while(fallback[i])
        {
            gtk_list_store_append(liststore, &iter);
            gtk_list_store_set(liststore, &iter, colnum, fallback[i], -1);
            i++;
        }
    }
}

void Save_Rename_File_Masks_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(RENAME_FILE_MASKS_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of 'Rename Directory' masks
 */
void Load_Rename_Directory_Masks_List (GtkListStore *liststore, gint colnum, gchar **fallback)
{
    gint i = 0;
    GtkTreeIter iter;

    if (!Populate_List_Store_From_File(RENAME_DIRECTORY_MASKS_FILE, liststore, colnum))
    {
        // Fall back to defaults
        Log_Print(LOG_OK,_("Loading default 'Rename Directory' masks…"));

        while(fallback[i])
        {
            gtk_list_store_append(liststore, &iter);
            gtk_list_store_set(liststore, &iter, colnum, fallback[i], -1);
            i++;
        }
    }
}

void Save_Rename_Directory_Masks_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(RENAME_DIRECTORY_MASKS_FILE, liststore, colnum);
}




/*
 * Functions for writing and reading list of 'DefaultPathToMp3' combobox
 */
void Load_Default_Path_To_MP3_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(DEFAULT_PATH_TO_MP3_HISTORY_FILE, liststore, colnum);
}
void Save_Default_Path_To_MP3_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(DEFAULT_PATH_TO_MP3_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of 'DefaultComment' combobox
 */
void Load_Default_Tag_Comment_Text_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(DEFAULT_TAG_COMMENT_HISTORY_FILE, liststore, colnum);
}
void Save_Default_Tag_Comment_Text_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(DEFAULT_TAG_COMMENT_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of 'BrowserEntry' combobox
 */
void Load_Path_Entry_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(PATH_ENTRY_HISTORY_FILE, liststore, colnum);
}
void Save_Path_Entry_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(PATH_ENTRY_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of 'PlayListNameCombo' combobox
 */
void Load_Play_List_Name_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(PLAY_LIST_NAME_MASKS_FILE, liststore, colnum);
}
void Save_Play_List_Name_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(PLAY_LIST_NAME_MASKS_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox to run program (tree browser)
 */
void Load_Run_Program_With_Directory_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(RUN_PROGRAM_WITH_DIRECTORY_HISTORY_FILE, liststore, colnum);
}
void Save_Run_Program_With_Directory_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(RUN_PROGRAM_WITH_DIRECTORY_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox to run program (file browser)
 */
void Load_Run_Program_With_File_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(RUN_PROGRAM_WITH_FILE_HISTORY_FILE, liststore, colnum);
}
void Save_Run_Program_With_File_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(RUN_PROGRAM_WITH_FILE_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox to run file audio player
 */
void Load_Audio_File_Player_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(AUDIO_FILE_PLAYER_HISTORY_FILE, liststore, colnum);
}
void Save_Audio_File_Player_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(AUDIO_FILE_PLAYER_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox to search a string into file (tag or filename)
 */
void Load_Search_File_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(SEARCH_FILE_HISTORY_FILE, liststore, colnum);
}
void Save_Search_File_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(SEARCH_FILE_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox of path of file to load to rename files
 */
void Load_File_To_Load_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(FILE_TO_LOAD_HISTORY_FILE, liststore, colnum);
}
void Save_File_To_Load_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(FILE_TO_LOAD_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox of playlist content
 */
void Load_Playlist_Content_Mask_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(PLAYLIST_CONTENT_MASKS_FILE, liststore, colnum);
}
void Save_Playlist_Content_Mask_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(PLAYLIST_CONTENT_MASKS_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox of cddb search string
 */
void Load_Cddb_Search_String_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(CDDB_SEARCH_STRING_HISTORY_FILE, liststore, colnum);
}
void Save_Cddb_Search_String_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(CDDB_SEARCH_STRING_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox of cddb search string in result list
 */
void Load_Cddb_Search_String_In_Result_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(CDDB_SEARCH_STRING_IN_RESULT_HISTORY_FILE, liststore, colnum);
}
void Save_Cddb_Search_String_In_Result_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(CDDB_SEARCH_STRING_IN_RESULT_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of 'CddbLocalPath3' combobox
 */
void Load_Cddb_Local_Path_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(CDDB_LOCAL_PATH_HISTORY_FILE, liststore, colnum);
}
void Save_Cddb_Local_Path_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(CDDB_LOCAL_PATH_HISTORY_FILE, liststore, colnum);
}





/*
 * migrate_config_to_xdg_dir:
 * @old_path: (type filename): the path to migrate from
 * @new_path: (type filename): the path to migrate to
 *
 * Migrate the EasyTAG configuration files contained in the old path to the new
 * one.
 */
static void
migrate_config_file_dir (const gchar *old_path, const gchar *new_path)
{
    gsize i;
    static const gchar *filenames[] = { CONFIG_FILE,
                                        SCAN_TAG_MASKS_FILE,
                                        RENAME_FILE_MASKS_FILE,
                                        RENAME_DIRECTORY_MASKS_FILE,
                                        DEFAULT_PATH_TO_MP3_HISTORY_FILE,
                                        DEFAULT_TAG_COMMENT_HISTORY_FILE,
                                        PATH_ENTRY_HISTORY_FILE,
                                        PLAY_LIST_NAME_MASKS_FILE,
                                        RUN_PROGRAM_WITH_DIRECTORY_HISTORY_FILE,
                                        RUN_PROGRAM_WITH_FILE_HISTORY_FILE,
                                        AUDIO_FILE_PLAYER_HISTORY_FILE,
                                        SEARCH_FILE_HISTORY_FILE,
                                        FILE_TO_LOAD_HISTORY_FILE,
                                        PLAYLIST_CONTENT_MASKS_FILE,
                                        CDDB_SEARCH_STRING_HISTORY_FILE,
                                        CDDB_SEARCH_STRING_IN_RESULT_HISTORY_FILE,
                                        CDDB_LOCAL_PATH_HISTORY_FILE,
                                        NULL
    };

    Log_Print (LOG_OK, _("Migrating configuration from directory '%s' to '%s'"),
               old_path, new_path);

    for (i = 0; filenames[i]; i++)
    {
        gchar *old_filename, *new_filename;
        GFile *old_file, *new_file;

        old_filename = g_build_filename (old_path, filenames[i], NULL);

        if (!g_file_test (old_filename, G_FILE_TEST_EXISTS))
        {
            g_free (old_filename);
            continue;
        }

        new_filename = g_build_filename (new_path, filenames[i], NULL);
        old_file = g_file_new_for_path (old_filename);
        new_file = g_file_new_for_path (new_filename);

        if (!g_file_move (old_file, new_file, G_FILE_COPY_NONE, NULL, NULL,
                          NULL, NULL))
        {
            Log_Print (LOG_ERROR,
                       _("Failed to migrate configuration file '%s'"),
                       filenames[i]);
        }

        g_free (old_filename);
        g_free (new_filename);
        g_object_unref (old_file);
        g_object_unref (new_file);
    }
}

/**
 * Create the directory used by EasyTAG to store user configuration files.
 *
 * Returns: %TRUE if the directory was created, or already exists. %FALSE if
 * the directory could not be created.
 */
static gboolean
Create_Easytag_Directory (void)
{
    gchar *easytag_path = NULL;
    gint result;

    /* Directory to create (if it does not exist) with absolute path. */
    easytag_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                     NULL);

    if (g_file_test (easytag_path, G_FILE_TEST_IS_DIR))
    {
        g_free (easytag_path);
        return TRUE;
    }

    result = g_mkdir_with_parents (easytag_path, S_IRWXU);

    if (result == -1)
    {
        Log_Print (LOG_ERROR,_("Error: Cannot create directory '%s' (%s)"),
                  easytag_path, g_strerror (errno));
        g_free (easytag_path);
        return FALSE;
    }
    else
    {
        gchar *old_path = g_build_filename (g_get_home_dir (),
                                            "." PACKAGE_TARNAME, NULL);

        if (g_file_test (old_path, G_FILE_TEST_IS_DIR))
        {
            migrate_config_file_dir (old_path, easytag_path);
        }

        g_free (old_path);
        g_free (easytag_path);

        return TRUE;
    }
}
