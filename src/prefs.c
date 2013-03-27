/* prefs.c - 2000/05/06 */
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
#include <errno.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gtk2_compat.h"
#include "prefs.h"
#include "setting.h"
#include "bar.h"
#include "misc.h"
#include "scan.h"
#include "easytag.h"
#include "enums.h"
#include "browser.h"
#include "cddb.h"
#include "charset.h"
#ifdef G_OS_WIN32
#include "win32/win32dep.h"
#endif /* G_OS_WIN32 */


static GtkWidget *FilenameExtensionNoChange;
static GtkWidget *FilenameExtensionLowerCase;
static GtkWidget *FilenameExtensionUpperCase;

/**************
 * Prototypes *
 **************/
/* Options window */
static void OptionsWindow_Quit (void);
static void OptionsWindow_Save_Button (void);
static void OptionsWindow_Cancel_Button (void);
static gboolean OptionsWindow_Key_Press (GtkWidget *window, GdkEvent *event);
static gboolean Check_Config (void);

static void Number_Track_Formatted_Toggled (void);
static void Number_Track_Formatted_Spin_Button_Changed (GtkWidget *Label,
                                                        GtkWidget *SpinButton);
static void Change_Id3_Settings_Toggled (void);
static void Use_Non_Standard_Id3_Reading_Character_Set_Toggled (void);
static void Scanner_Convert_Check_Button_Toggled_1 (GtkWidget *object_rec,
                                                    GtkWidget *object_emi);
static void Cddb_Use_Proxy_Toggled (void);

static void DefaultPathToMp3_Combo_Add_String (void);
static void CddbLocalPath_Combo_Add_String (void);


/*************
 * Functions *
 *************/
void Init_OptionsWindow (void)
{
    OptionsWindow = NULL;
}

/*
 * The window for options
 */
void Open_OptionsWindow (void)
{
    GtkWidget *OptionsVBox;
    GtkWidget *ButtonBox;
    GtkWidget *Button;
    GtkWidget *Label;
    GtkWidget *Frame;
    GtkWidget *Table;
    GtkWidget *VBox, *vbox;
    GtkWidget *HBox, *hbox, *id3v1v2hbox;
    GtkWidget *Separator;
    gchar *path_utf8;
    gchar *program_path;
    gchar *default_comment;

    /* Check if already opened */
    if (OptionsWindow)
    {
        gtk_window_present(GTK_WINDOW(OptionsWindow));
        return;
    }

    /* The window */
    OptionsWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    /* Config */
    gtk_window_set_position(GTK_WINDOW(OptionsWindow),GTK_WIN_POS_CENTER);
    gtk_window_set_transient_for(GTK_WINDOW(OptionsWindow),GTK_WINDOW(MainWindow));
    gtk_container_set_border_width(GTK_CONTAINER(OptionsWindow), 5);
    {
        gint width, height;

        g_settings_get (ETSettings, "preferences-location", "(iiii)", NULL,
                        NULL, &width, &height);
        gtk_window_set_default_size (GTK_WINDOW (OptionsWindow), width,
                                     height);
    }
    /* Title */
    gtk_window_set_title (GTK_WINDOW (OptionsWindow), _("Preferences"));

    /* Signals connection */
    g_signal_connect(G_OBJECT(OptionsWindow),"destroy", G_CALLBACK(OptionsWindow_Quit),NULL);
    g_signal_connect(G_OBJECT(OptionsWindow),"delete_event",G_CALLBACK(OptionsWindow_Quit),NULL);
    g_signal_connect(G_OBJECT(OptionsWindow),"key_press_event",
                     G_CALLBACK(OptionsWindow_Key_Press),NULL);

     /* Options */
     /* The vbox */
    OptionsVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_box_set_spacing (GTK_BOX(OptionsVBox),5);
    gtk_container_add(GTK_CONTAINER(OptionsWindow),OptionsVBox);

     /* Options NoteBook */
    OptionsNoteBook = gtk_notebook_new();
    gtk_notebook_popup_enable(GTK_NOTEBOOK(OptionsNoteBook));
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(OptionsNoteBook),TRUE);
    gtk_box_pack_start(GTK_BOX(OptionsVBox),OptionsNoteBook,TRUE,TRUE,0);



    /*
     * Browser
     */
    Label = gtk_label_new(_("Browser"));
    Frame = gtk_frame_new(_("Browser"));
    gtk_notebook_append_page(GTK_NOTEBOOK(OptionsNoteBook),Frame,Label);
    gtk_container_set_border_width(GTK_CONTAINER(Frame), 5);

    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    gtk_container_add(GTK_CONTAINER(Frame),VBox);
    gtk_container_set_border_width(GTK_CONTAINER(VBox), 4);


    /* File Browser frame */
    Frame = gtk_frame_new(_("File Browser"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    /* Default directory */
    HBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_box_pack_start(GTK_BOX(vbox),HBox,FALSE,FALSE,0);

    // Label
    Label = gtk_label_new(_("Default directory:"));
    gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);

    // Combo
    if (DefaultPathModel != NULL)
        gtk_list_store_clear(DefaultPathModel);
    else
        DefaultPathModel = gtk_list_store_new(MISC_COMBO_COUNT, G_TYPE_STRING);

    DefaultPathToMp3 = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(DefaultPathModel));
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(DefaultPathToMp3), MISC_COMBO_TEXT);
    gtk_box_pack_start(GTK_BOX(HBox),DefaultPathToMp3,TRUE,TRUE,0);
    gtk_widget_set_size_request(DefaultPathToMp3, 400, -1);
    gtk_widget_set_tooltip_text(gtk_bin_get_child(GTK_BIN(DefaultPathToMp3)),_("Specify the directory where "
        "your files are located. This path will be loaded when EasyTAG starts without parameter."));
    g_signal_connect(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(DefaultPathToMp3)))),"activate",G_CALLBACK(DefaultPathToMp3_Combo_Add_String),NULL);
    //g_signal_connect(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(DefaultPathToMp3)))),"focus_out_event",G_CALLBACK(DefaultPathToMp3_Combo_Add_String),NULL);

    // History list
    Load_Default_Path_To_MP3_List(DefaultPathModel, MISC_COMBO_TEXT);
    // If default path hasn't been added already, add it now..
    path_utf8 = filename_to_display(DEFAULT_PATH_TO_MP3);
    Add_String_To_Combo_List(DefaultPathModel, path_utf8);
    if (path_utf8)
        gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(DefaultPathToMp3))), path_utf8);
    g_free(path_utf8);

    // Button browse
    Button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    gtk_box_pack_start(GTK_BOX(HBox),Button,FALSE,FALSE,0);
    g_signal_connect_swapped(G_OBJECT(Button),"clicked",
                             G_CALLBACK(File_Selection_Window_For_Directory),G_OBJECT(gtk_bin_get_child(GTK_BIN(DefaultPathToMp3))));

    /* Load directory on startup */
    LoadOnStartup = gtk_check_button_new_with_label(_("Load on startup the default directory or the directory passed as argument"));
    gtk_box_pack_start(GTK_BOX(vbox),LoadOnStartup,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "load-on-startup", LoadOnStartup, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(LoadOnStartup,_("Automatically search files, when EasyTAG starts, "
        "into the default directory. Note that this path may be overridden by the parameter "
        "passed to easytag (easytag /path_to/mp3_files)."));

    /* Browse subdirectories */
    BrowseSubdir = gtk_check_button_new_with_label(_("Search subdirectories"));
    gtk_box_pack_start(GTK_BOX(vbox),BrowseSubdir,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "browse-subdir", BrowseSubdir, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(BrowseSubdir,_("Search subdirectories for files when reading "
        "a directory into the tree."));

    /* Open the node to show subdirectories */
    OpenSelectedBrowserNode = gtk_check_button_new_with_label(_("Show subdirectories when selecting "
        "a directory"));
    gtk_box_pack_start(GTK_BOX(vbox),OpenSelectedBrowserNode,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "browse-expand-children",
                     OpenSelectedBrowserNode, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(OpenSelectedBrowserNode,_("This expands the selected node into the file "
        "browser to display the sub-directories."));

    /* Browse hidden directories */
    BrowseHiddendir = gtk_check_button_new_with_label(_("Search hidden directories"));
#ifndef G_OS_WIN32 /* Always true and not user modifiable on win32 */
    gtk_box_pack_start(GTK_BOX(vbox),BrowseHiddendir,FALSE,FALSE,0);
#endif /* !G_OS_WIN32 */
    g_settings_bind (ETSettings, "browse-show-hidden", BrowseHiddendir,
                     "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(BrowseHiddendir,_("Search hidden directories for files "
        "(directories starting by a '.')."));



    /*
     * Misc
     */
    Label = gtk_label_new (_("Misc"));
    Frame = gtk_frame_new (_("Misc"));
    gtk_notebook_append_page (GTK_NOTEBOOK(OptionsNoteBook),Frame,Label);
    gtk_container_set_border_width(GTK_CONTAINER(Frame), 5);

    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    gtk_container_add(GTK_CONTAINER(Frame),VBox);
    gtk_container_set_border_width(GTK_CONTAINER(VBox), 4);


    /* User interface */
    Frame = gtk_frame_new (_("User Interface"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    /* Show header information. */
    ShowHeaderInfos = gtk_check_button_new_with_label(_("Show header information of file"));
    gtk_box_pack_start(GTK_BOX(vbox),ShowHeaderInfos,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "file-show-header", ShowHeaderInfos, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(ShowHeaderInfos,_("If activated, information about the file as "
        "the bitrate, the time, the size, will be displayed under the filename entry."));

    // Display color mode for changed files in list
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
    Label = gtk_label_new(_("Display changed files in list using:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,0);

    ChangedFilesDisplayedToRed = gtk_radio_button_new_with_label(NULL,_("Red color"));
    gtk_box_pack_start(GTK_BOX(hbox),ChangedFilesDisplayedToRed,FALSE,FALSE,4);

    // Set "new" Gtk+-2.0ish black/bold style for changed items
    ChangedFilesDisplayedToBold = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(ChangedFilesDisplayedToRed)),_("Bold style"));
    gtk_box_pack_start(GTK_BOX(hbox),ChangedFilesDisplayedToBold,FALSE,FALSE,2);
    g_settings_bind (ETSettings, "file-changed-bold",
                     ChangedFilesDisplayedToBold, "active",
                     G_SETTINGS_BIND_DEFAULT);
    g_signal_connect (ChangedFilesDisplayedToBold, "notify::active",
                      Browser_List_Refresh_Whole_List, NULL);

    /* Sorting List Options */
    Frame = gtk_frame_new (_("Sorting List Options"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
    /* Sorting method */
    Label = gtk_label_new(_("Sort the file list by:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,0);

    SortingFileCombo = gtk_combo_box_text_new();
    gtk_box_pack_start (GTK_BOX (hbox), SortingFileCombo, FALSE, FALSE, 2);
    gtk_widget_set_size_request(GTK_WIDGET(SortingFileCombo), 260, -1);
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(SortingFileCombo),2); // Two columns

    // Items of option menu
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Ascending filename"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Descending filename"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Ascending title"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Descending title"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Ascending artist"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Descending artist"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Ascending album artist"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Descending album artist"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Ascending album"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Descending album"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Ascending year"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Descending year"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Ascending disc number"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Descending disc number"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Ascending track number"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Descending track number"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Ascending genre"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Descending genre"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Ascending comment"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SortingFileCombo), _("Descending comment"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Ascending composer"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Descending composer"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Ascending original artist"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Descending original artist"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Ascending copyright"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Descending copyright"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Ascending URL"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Descending URL"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Ascending encoded by"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Descending encoded by"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Ascending creation date"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (SortingFileCombo),
                                    _("Descending creation date"));

    g_settings_bind_with_mapping (ETSettings, "sort-mode", SortingFileCombo,
                                  "active", G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_get, et_settings_enum_set,
                                  GSIZE_TO_POINTER (ET_TYPE_SORT_MODE), NULL);
    gtk_widget_set_tooltip_text(SortingFileCombo,
                                _("Select the type of file sorting when "
                                "loading a directory."));

    SortingFileCaseSensitive = gtk_check_button_new_with_label(_("Case sensitive"));
#ifndef G_OS_WIN32
    /* Always true and not user modifiable on win32, as strncasecmp() does not
     * work correctly with g_utf8_collate_key().
     */
    gtk_box_pack_start(GTK_BOX(hbox),SortingFileCaseSensitive,FALSE,FALSE,0);
#endif /* !G_OS_WIN32 */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(SortingFileCaseSensitive),
        SORTING_FILE_CASE_SENSITIVE);
    gtk_widget_set_tooltip_text(SortingFileCaseSensitive,_("If activated, the "
        "sorting of the list will be dependent on the case."));

    /* File Player */
    Frame = gtk_frame_new (_("File Audio Player"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);

    // Player name with params
    if (FilePlayerModel == NULL)
        FilePlayerModel = gtk_list_store_new(MISC_COMBO_COUNT, G_TYPE_STRING);
    else
        gtk_list_store_clear(FilePlayerModel);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),hbox);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    Label = gtk_label_new (_("Player to run:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,0);
    FilePlayerCombo = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(FilePlayerModel));
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(FilePlayerCombo),MISC_COMBO_TEXT);
    gtk_widget_set_size_request(GTK_WIDGET(FilePlayerCombo), 300, -1);
    gtk_box_pack_start(GTK_BOX(hbox),FilePlayerCombo,FALSE,FALSE,0);
    gtk_widget_set_tooltip_text(gtk_bin_get_child(GTK_BIN(FilePlayerCombo)),_("Enter the program used to "
        "play the files. Some arguments can be passed for the program (as 'xmms -p') before "
        "to receive files as other arguments."));
    // History List
    Load_Audio_File_Player_List(FilePlayerModel, MISC_COMBO_TEXT);
    Add_String_To_Combo_List(FilePlayerModel, AUDIO_FILE_PLAYER);
    // Don't load the parameter if XMMS not found, else user can't save the preference
    if ( (program_path=Check_If_Executable_Exists(AUDIO_FILE_PLAYER)))
        gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(FilePlayerCombo))), AUDIO_FILE_PLAYER);
    g_free(program_path);

    // Button browse
    Button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    gtk_box_pack_start(GTK_BOX(hbox),Button,FALSE,FALSE,0);
    g_signal_connect_swapped(G_OBJECT(Button),"clicked",
        G_CALLBACK(File_Selection_Window_For_File), G_OBJECT(gtk_bin_get_child(GTK_BIN(FilePlayerCombo))));

    /* Log options */
    Frame = gtk_frame_new (_("Log Options"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    // Show / hide log view
    ShowLogView = gtk_check_button_new_with_label(_("Show log view in main window"));
    gtk_box_pack_start(GTK_BOX(vbox),ShowLogView,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "log-show", ShowLogView, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(ShowLogView,_("If activated, the log view would be "
                                            "visible in the main window."));
   
    // Max number of lines
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    Label = gtk_label_new (_("Max number of lines:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,0);
    
    LogMaxLinesSpinButton = gtk_spin_button_new_with_range(10.0,1500.0,10.0);
    gtk_box_pack_start(GTK_BOX(hbox),LogMaxLinesSpinButton,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "log-lines", LogMaxLinesSpinButton,
                     "value", G_SETTINGS_BIND_DEFAULT);
    /* g_signal_connect(G_OBJECT(NumberTrackFormated),"toggled",G_CALLBACK(Number_Track_Formatted_Toggled),NULL);
     * g_signal_emit_by_name(G_OBJECT(NumberTrackFormated),"toggled");
       gtk_tooltips_set_tip(Tips,GTK_BIN(FilePlayerCombo)->child,_("Enter the program used to "
        "play the files. Some arguments can be passed for the program (as 'xmms -p') before "
        "to receive files as other arguments."),NULL);
*/



    /*
     * File Settings
     */
    Label = gtk_label_new (_("File Settings"));
    Frame = gtk_frame_new (_("File Settings"));
    gtk_notebook_append_page (GTK_NOTEBOOK(OptionsNoteBook),Frame,Label);
    gtk_container_set_border_width(GTK_CONTAINER(Frame),5);

    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),VBox);
    gtk_container_set_border_width(GTK_CONTAINER(VBox),4);


    /* File (name) Options */
    Frame = gtk_frame_new (_("File Options"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    ReplaceIllegalCharactersInFilename = gtk_check_button_new_with_label(_("Replace illegal characters in filename (for Windows and CD-Rom)"));
    gtk_box_pack_start(GTK_BOX(vbox),ReplaceIllegalCharactersInFilename,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "rename-replace-illegal-chars",
                     ReplaceIllegalCharactersInFilename, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(ReplaceIllegalCharactersInFilename,_("Convert illegal characters for "
        "FAT32/16 and ISO9660 + Joliet filesystems ('\\', ':', ';', '*', '?', '\"', '<', '>', '|') "
        "of the filename to avoid problem when renaming the file. This is useful when renaming the "
        "file from the tag with the scanner."));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
    /* Extension case (lower/upper?) */
    Label = gtk_label_new(_("Convert filename extension to:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,0);

    FilenameExtensionLowerCase = gtk_radio_button_new_with_label(NULL,_("Lower Case"));
    gtk_widget_set_name (FilenameExtensionLowerCase, "lower-case");
    gtk_box_pack_start(GTK_BOX(hbox),FilenameExtensionLowerCase,FALSE,FALSE,2);
    gtk_widget_set_tooltip_text(FilenameExtensionLowerCase,_("For example, the extension will be converted to '.mp3'"));

    FilenameExtensionUpperCase = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(FilenameExtensionLowerCase)),_("Upper Case"));
    gtk_widget_set_name (FilenameExtensionUpperCase, "upper-case");
    gtk_box_pack_start(GTK_BOX(hbox),FilenameExtensionUpperCase,FALSE,FALSE,2);
    gtk_widget_set_tooltip_text(FilenameExtensionUpperCase,_("For example, the extension will be converted to '.MP3'"));

    FilenameExtensionNoChange = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(FilenameExtensionLowerCase)),_("No Change"));
    gtk_widget_set_name (FilenameExtensionNoChange, "no-change");
    gtk_box_pack_start(GTK_BOX(hbox),FilenameExtensionNoChange,FALSE,FALSE,2);
    gtk_widget_set_tooltip_text(FilenameExtensionNoChange,_("The extension will not be converted"));

    g_settings_bind_with_mapping (ETSettings, "rename-extension-mode",
                                  FilenameExtensionLowerCase, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  FilenameExtensionLowerCase, NULL);
    g_settings_bind_with_mapping (ETSettings, "rename-extension-mode",
                                  FilenameExtensionUpperCase, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  FilenameExtensionUpperCase, NULL);
    g_settings_bind_with_mapping (ETSettings, "rename-extension-mode",
                                  FilenameExtensionNoChange, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  FilenameExtensionNoChange, NULL);

    /* Preserve modification time */
    PreserveModificationTime = gtk_check_button_new_with_label(_("Preserve modification time of the file"));
    gtk_box_pack_start(GTK_BOX(vbox),PreserveModificationTime,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "file-preserve-modification-time",
                     PreserveModificationTime, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(PreserveModificationTime,_("Preserve the modification time "
        "(in file properties) when saving the file."));

    /* Change directory modification time */
    UpdateParentDirectoryModificationTime = gtk_check_button_new_with_label(_("Update modification time "
        "of the parent directory of the file (recommended when using Amarok)"));
    gtk_box_pack_start(GTK_BOX(vbox),UpdateParentDirectoryModificationTime,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "file-update-parent-modification-time",
                     UpdateParentDirectoryModificationTime, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(UpdateParentDirectoryModificationTime,_("The modification time "
        "of the parent directory of the file will be updated when saving tag the file. At the "
        "present time it is automatically done only when renaming a file.\nThis feature is "
        "interesting when using applications like Amarok. For performance reasons, they refresh "
        "file information by detecting changes of the parent directory."));


    /* Character Set for Filename */
    Frame = gtk_frame_new (_("Character Set for Filename"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    /****hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);***/

    Table = et_grid_new (4, 2);
    gtk_box_pack_start(GTK_BOX(vbox),Table,FALSE,FALSE,0);
    /*gtk_grid_set_row_spacing (GTK_GRID (Table), 2);*/
    gtk_grid_set_column_spacing (GTK_GRID (Table), 2);

    /* Rules for character set */
    Label = gtk_label_new(_("Rules to apply if some characters can't be converted to "
        "the system character encoding when writing filename:"));
    gtk_grid_attach (GTK_GRID (Table), Label, 0, 0, 2, 1);
    gtk_misc_set_alignment(GTK_MISC(Label),0,0.5);

    Label = gtk_label_new("    ");
    gtk_grid_attach (GTK_GRID (Table), Label, 0, 1, 1, 1);

    FilenameCharacterSetOther = gtk_radio_button_new_with_label(NULL,_("Try another "
        "character encoding"));
    gtk_grid_attach (GTK_GRID (Table), FilenameCharacterSetOther, 1, 1, 1, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FilenameCharacterSetOther),FILENAME_CHARACTER_SET_OTHER);
    gtk_widget_set_tooltip_text(FilenameCharacterSetOther,_("With this option, it will "
        "try the conversion to the encoding associated to your locale (for example: "
        "ISO-8859-1 for 'fr', KOI8-R for 'ru', ISO-8859-2 for 'ro'). If it fails, it "
        "will try the character encoding ISO-8859-1."));

    FilenameCharacterSetApproximate = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(FilenameCharacterSetOther)),
        _("Force using the system character encoding and activate the transliteration"));
    gtk_grid_attach (GTK_GRID (Table), FilenameCharacterSetApproximate, 1, 2,
                     1, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FilenameCharacterSetApproximate),FILENAME_CHARACTER_SET_APPROXIMATE);
    gtk_widget_set_tooltip_text(FilenameCharacterSetApproximate,_("With this option, when "
        "a character cannot be represented in the target character set, it can be "
        "approximated through one or several similarly looking characters."));

    FilenameCharacterSetDiscard = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(FilenameCharacterSetOther)),
        _("Force using the system character encoding and silently discard some characters"));
    gtk_grid_attach (GTK_GRID (Table), FilenameCharacterSetDiscard, 1, 3, 1,
                     1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FilenameCharacterSetDiscard),FILENAME_CHARACTER_SET_DISCARD);
    gtk_widget_set_tooltip_text(FilenameCharacterSetDiscard,_("With this option, when "
        "a character cannot be represented in the target character set, it will "
        "be silently discarded."));



    /*
     * Tag Settings
     */
    Label = gtk_label_new (_("Tag Settings"));
    Frame = gtk_frame_new (_("Tag Settings"));
    gtk_notebook_append_page (GTK_NOTEBOOK(OptionsNoteBook),Frame,Label);
    gtk_container_set_border_width(GTK_CONTAINER(Frame),5);

    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),VBox);
    gtk_container_set_border_width(GTK_CONTAINER(VBox),4);

    /* Tag Options */
    Frame = gtk_frame_new (_("Tag Options"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    DateAutoCompletion = gtk_check_button_new_with_label(_("Auto completion of date if not complete"));
    gtk_box_pack_start(GTK_BOX(vbox),DateAutoCompletion,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "tag-date-autocomplete", DateAutoCompletion,
                     "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(DateAutoCompletion,_("Try to complete the year field if you enter "
        "only the last numerals of the date (for instance, if the current year is 2005: "
        "5 => 2005, 4 => 2004, 6 => 1996, 95 => 1995…)."));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);

    NumberTrackFormated = gtk_check_button_new_with_label(_("Write the track field with the following number of digits:"));
    gtk_box_pack_start(GTK_BOX(hbox),NumberTrackFormated,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "tag-number-padded", NumberTrackFormated,
                     "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(NumberTrackFormated,_("If activated, the track field is written using "
        "the number '0' as padding to obtain a number with 'n' digits (for example, with two digits: '05', "
        "'09', '10'…). Else it keeps the 'raw' track value."));

    NumberTrackFormatedSpinButton = gtk_spin_button_new_with_range(2.0,6.0,1.0);
    gtk_box_pack_start(GTK_BOX(hbox),NumberTrackFormatedSpinButton,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "tag-number-length",
                     NumberTrackFormatedSpinButton, "value",
                     G_SETTINGS_BIND_DEFAULT);
    g_signal_connect(G_OBJECT(NumberTrackFormated),"toggled",G_CALLBACK(Number_Track_Formatted_Toggled),NULL);
    g_signal_emit_by_name(G_OBJECT(NumberTrackFormated),"toggled");

    Label = gtk_label_new(""); // Label to show the example
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,4);
    g_signal_connect_swapped(G_OBJECT(NumberTrackFormatedSpinButton),"changed",G_CALLBACK(Number_Track_Formatted_Spin_Button_Changed),G_OBJECT(Label));
    g_signal_emit_by_name(G_OBJECT(NumberTrackFormatedSpinButton),"changed",NULL);

    // Separator line
    Separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox),Separator,FALSE,FALSE,0);

    /* Tag field focus */
    SetFocusToSameTagField = gtk_check_button_new_with_label (_("Keep focus on the same tag field when switching files"));
    gtk_box_pack_start (GTK_BOX (vbox), SetFocusToSameTagField, FALSE, FALSE,
                        0);
    g_settings_bind (ETSettings, "tag-preserve-focus", SetFocusToSameTagField,
                     "active", G_SETTINGS_BIND_DEFAULT);

    /* Tag Splitting */
    Frame = gtk_frame_new (_("Tag Splitting"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);

    Table = et_grid_new (5, 2);
    gtk_container_add(GTK_CONTAINER(Frame),Table);
    gtk_grid_set_column_spacing (GTK_GRID (Table), 2);
    
    Label = gtk_label_new(_("For Vorbis tags, selected fields will be split at dashes and saved as separate tags"));
    gtk_grid_attach (GTK_GRID (Table), Label, 0, 0, 2, 1);
    gtk_misc_set_alignment(GTK_MISC(Label),0,0.5);

    VorbisSplitFieldTitle = gtk_check_button_new_with_label(_("Title"));
    VorbisSplitFieldArtist = gtk_check_button_new_with_label(_("Artist"));
    VorbisSplitFieldAlbum = gtk_check_button_new_with_label(_("Album"));
    VorbisSplitFieldGenre = gtk_check_button_new_with_label(_("Genre"));
    VorbisSplitFieldComment = gtk_check_button_new_with_label(_("Comment"));
    VorbisSplitFieldComposer = gtk_check_button_new_with_label(_("Composer"));
    VorbisSplitFieldOrigArtist = gtk_check_button_new_with_label(_("Original artist"));

    gtk_grid_attach (GTK_GRID (Table), VorbisSplitFieldTitle, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (Table), VorbisSplitFieldArtist, 0, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (Table), VorbisSplitFieldAlbum, 0, 3, 1, 1);
    gtk_grid_attach (GTK_GRID (Table), VorbisSplitFieldGenre, 0, 4, 1, 1);
    gtk_grid_attach (GTK_GRID (Table), VorbisSplitFieldComment, 1, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (Table), VorbisSplitFieldComposer, 1, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (Table), VorbisSplitFieldOrigArtist, 1, 3, 1, 1);

    g_settings_bind (ETSettings, "ogg-split-title", VorbisSplitFieldTitle,
                     "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (ETSettings, "ogg-split-artist", VorbisSplitFieldArtist,
                     "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (ETSettings, "ogg-split-album", VorbisSplitFieldAlbum,
                     "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (ETSettings, "ogg-split-genre", VorbisSplitFieldGenre,
                     "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (ETSettings, "ogg-split-comment", VorbisSplitFieldComment,
                     "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (ETSettings, "ogg-split-composer",
                     VorbisSplitFieldComposer, "active",
                     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (ETSettings, "ogg-split-original-artist",
                     VorbisSplitFieldOrigArtist, "active",
                     G_SETTINGS_BIND_DEFAULT);

    /*
     * ID3 Tag Settings
     */
    Label = gtk_label_new (_("ID3 Tag Settings"));
    Frame = gtk_frame_new (_("ID3 Tag Settings"));
#ifdef ENABLE_MP3
    gtk_notebook_append_page (GTK_NOTEBOOK(OptionsNoteBook),Frame,Label);
#endif
    gtk_container_set_border_width(GTK_CONTAINER(Frame),5);

    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),VBox);
    gtk_container_set_border_width(GTK_CONTAINER(VBox),4);


    /* Tag Rules frame */
    Frame = gtk_frame_new (_("ID3 Tag Rules"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox),2);

    Table = et_grid_new (3, 2);
    gtk_box_pack_start(GTK_BOX(vbox),Table,FALSE,FALSE,0);
    gtk_grid_set_row_spacing (GTK_GRID (Table), 2);
    gtk_grid_set_column_spacing (GTK_GRID (Table), 2);

    /* Write ID3 tags in FLAC files */
    WriteId3TagsInFlacFiles = gtk_check_button_new_with_label(_("Write ID3 tags in FLAC files (in addition to FLAC tag)"));
    gtk_grid_attach (GTK_GRID (Table), WriteId3TagsInFlacFiles, 0, 0, 1, 1);
    g_settings_bind (ETSettings, "flac-write-id3", WriteId3TagsInFlacFiles,
                     "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(WriteId3TagsInFlacFiles,_("If activated, ID3 tags will be "
        "also added in the FLAC file (according the two rules above, plus the FLAC tag). "
        "Else ID3 tags will be stripped."));

    /* Strip tag when fields (managed by EasyTAG) are empty */
    StripTagWhenEmptyFields = gtk_check_button_new_with_label(_("Strip tags if all fields are set to blank"));
    gtk_grid_attach (GTK_GRID (Table), StripTagWhenEmptyFields, 0, 1, 1, 1);
    g_settings_bind (ETSettings, "id3-strip-empty", StripTagWhenEmptyFields,
                     "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(StripTagWhenEmptyFields,_("As ID3v2 tags may contain other data than "
        "Title, Artist, Album, Year, Track, Genre or Comment (as an attached image, lyrics…), "
        "this option allows you to strip the whole tag when these seven standard data fields have "
        "been set to blank."));

    /* Convert old ID3v2 tag version */
    ConvertOldId3v2TagVersion = gtk_check_button_new_with_label(_("Automatically convert old ID3v2 tag versions"));
    gtk_grid_attach (GTK_GRID (Table), ConvertOldId3v2TagVersion, 0, 2, 1, 1);
    g_settings_bind (ETSettings, "id3v2-convert-old",
                     ConvertOldId3v2TagVersion, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(ConvertOldId3v2TagVersion,_("If activated, an old ID3v2 tag version (as "
        "ID3v2.2) will be updated to the ID3v2.3 version."));

    /* Use CRC32 */
    FileWritingId3v2UseCrc32 = gtk_check_button_new_with_label(_("Use CRC32"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v2UseCrc32, 1, 0, 1, 1);
    g_settings_bind (ETSettings, "id3v2-crc32", FileWritingId3v2UseCrc32,
                     "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(FileWritingId3v2UseCrc32,_("Set CRC32 in the ID3v2 tags"));

    /* Use Compression */
    FileWritingId3v2UseCompression = gtk_check_button_new_with_label(_("Use Compression"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v2UseCompression, 1, 1, 1,
                     1);
    g_settings_bind (ETSettings, "id3v2-compression",
                     FileWritingId3v2UseCompression, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(FileWritingId3v2UseCompression,_("Set Compression in the ID3v2 tags"));
	
    /* Write Genre in text */
    FileWritingId3v2TextOnlyGenre = gtk_check_button_new_with_label(_("Write Genre in text only"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v2TextOnlyGenre, 1, 2, 1,
                     1);
    g_settings_bind (ETSettings, "id3v2-text-only-genre",
                     FileWritingId3v2TextOnlyGenre, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(FileWritingId3v2TextOnlyGenre,_("Don't use ID3v1 number references in genre tag. Enable this if you see numbers as genre in your music player."));	

    /* Character Set for writing ID3 tag */
    Frame = gtk_frame_new (_("Character Set for writing ID3 tags"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    id3v1v2hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,3);
    gtk_container_add(GTK_CONTAINER(Frame),id3v1v2hbox);
    gtk_container_set_border_width(GTK_CONTAINER(id3v1v2hbox), 2);

    // ID3v2 tags
    Frame = gtk_frame_new (_("ID3v2 tags"));
    gtk_box_pack_start(GTK_BOX(id3v1v2hbox),Frame,FALSE,FALSE,2);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox),2);

    Table = et_grid_new (8, 6);
    gtk_box_pack_start(GTK_BOX(vbox),Table,FALSE,FALSE,0);
    gtk_grid_set_row_spacing (GTK_GRID (Table), 2);
    gtk_grid_set_column_spacing (GTK_GRID (Table), 2);

    /* Write ID3v2 tag */
    FileWritingId3v2WriteTag = gtk_check_button_new_with_label(_("Write ID3v2 tag"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v2WriteTag, 0, 0, 5, 1);
    g_settings_bind (ETSettings, "id3v2-enabled", FileWritingId3v2WriteTag,
                     "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(FileWritingId3v2WriteTag,_("If activated, an ID3v2.4 tag will be added or "
        "updated at the beginning of the MP3 files. Else it will be stripped."));
    g_signal_connect_after(G_OBJECT(FileWritingId3v2WriteTag),"toggled",
        G_CALLBACK(Change_Id3_Settings_Toggled),NULL);

#ifdef ENABLE_ID3LIB
    /* ID3v2 tag version */
    LabelId3v2Version = gtk_label_new(_("Version:"));
    gtk_grid_attach (GTK_GRID (Table), LabelId3v2Version, 0, 1, 2, 1);
    gtk_misc_set_alignment(GTK_MISC(LabelId3v2Version),0,0.5);

    FileWritingId3v2VersionCombo = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text (FileWritingId3v2VersionCombo,
                                 _("Select the ID3v2 tag version to write:\n"
                                   " - ID3v2.3 is written using id3lib,\n"
                                   " - ID3v2.4 is written using libid3tag (recommended)."));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(FileWritingId3v2VersionCombo), "ID3v2.4");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(FileWritingId3v2VersionCombo), "ID3v2.3");
    gtk_combo_box_set_active(GTK_COMBO_BOX(FileWritingId3v2VersionCombo),
        FILE_WRITING_ID3V2_VERSION_4 ? 0 : 1);
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v2VersionCombo, 2, 1, 2,
                     1);
    g_signal_connect_after(G_OBJECT(FileWritingId3v2VersionCombo),"changed",
        G_CALLBACK(Change_Id3_Settings_Toggled),NULL);
#endif


    /* Charset */
    LabelId3v2Charset = gtk_label_new(_("Charset:"));
    gtk_grid_attach (GTK_GRID (Table), LabelId3v2Charset, 0, 2, 5, 1);
    gtk_misc_set_alignment(GTK_MISC(LabelId3v2Charset),0,0.5);

    Label = gtk_label_new("        ");
    gtk_grid_attach (GTK_GRID (Table), Label, 0, 3, 1, 1);

    // Unicode
    FileWritingId3v2UseUnicodeCharacterSet = gtk_radio_button_new_with_label(NULL, _("Unicode "));
    g_settings_bind (ETSettings, "id3v2-enable-unicode",
                     FileWritingId3v2UseUnicodeCharacterSet, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v2UseUnicodeCharacterSet,
                     1, 3, 1, 1);

    FileWritingId3v2UnicodeCharacterSetCombo = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(FileWritingId3v2UnicodeCharacterSetCombo,
                                _("Unicode type to use"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(FileWritingId3v2UnicodeCharacterSetCombo), "UTF-8");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(FileWritingId3v2UnicodeCharacterSetCombo), "UTF-16");
    if ( FILE_WRITING_ID3V2_UNICODE_CHARACTER_SET == NULL )
        gtk_combo_box_set_active(GTK_COMBO_BOX(FileWritingId3v2UnicodeCharacterSetCombo), 0); // Set UTF-8 by default
    else
        gtk_combo_box_set_active(GTK_COMBO_BOX(FileWritingId3v2UnicodeCharacterSetCombo),
            strcmp(FILE_WRITING_ID3V2_UNICODE_CHARACTER_SET, "UTF-8") ? 1 : 0);
    gtk_grid_attach (GTK_GRID (Table),
                     FileWritingId3v2UnicodeCharacterSetCombo, 2, 3, 2, 1);
    g_signal_connect_after(G_OBJECT(FileWritingId3v2UseUnicodeCharacterSet),"toggled",
        G_CALLBACK(Change_Id3_Settings_Toggled),NULL);

    // Non-unicode
    FileWritingId3v2UseNoUnicodeCharacterSet = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(FileWritingId3v2UseUnicodeCharacterSet)),
        _("Other"));
    gtk_grid_attach (GTK_GRID (Table),
                     FileWritingId3v2UseNoUnicodeCharacterSet, 1, 4, 1, 1);

    FileWritingId3v2NoUnicodeCharacterSetCombo = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text (FileWritingId3v2NoUnicodeCharacterSetCombo,
                                 _("Character set used to write the tag data in the file."));

    Charset_Populate_Combobox(GTK_COMBO_BOX(FileWritingId3v2NoUnicodeCharacterSetCombo), 
        FILE_WRITING_ID3V2_NO_UNICODE_CHARACTER_SET);
    gtk_grid_attach (GTK_GRID (Table),
                     FileWritingId3v2NoUnicodeCharacterSetCombo, 2, 4, 3, 1);
    g_signal_connect_after(G_OBJECT(FileWritingId3v2UseNoUnicodeCharacterSet),"toggled",
        G_CALLBACK(Change_Id3_Settings_Toggled),NULL);
    
    // ID3v2 Additional iconv() options
    LabelAdditionalId3v2IconvOptions = gtk_label_new(_("Additional settings for iconv():"));
    gtk_grid_attach (GTK_GRID (Table), LabelAdditionalId3v2IconvOptions, 2, 5,
                     3, 1);
    gtk_misc_set_alignment(GTK_MISC(LabelAdditionalId3v2IconvOptions),0,0.5);

    FileWritingId3v2IconvOptionsNo = gtk_radio_button_new_with_label(NULL,
        _("No"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v2IconvOptionsNo, 2, 6, 1,
                     1);
    gtk_widget_set_name (FileWritingId3v2IconvOptionsNo, "none");
    gtk_widget_set_tooltip_text(FileWritingId3v2IconvOptionsNo,_("With this option, when "
        "a character cannot be represented in the target character set, it isn't changed. "
        "But note that an error message will be displayed for information."));
    FileWritingId3v2IconvOptionsTranslit = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(FileWritingId3v2IconvOptionsNo)),
        _("//TRANSLIT"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v2IconvOptionsTranslit, 3,
                     6, 1, 1);
    gtk_widget_set_name (FileWritingId3v2IconvOptionsTranslit,
                         "transliterate");
    gtk_widget_set_tooltip_text(FileWritingId3v2IconvOptionsTranslit,_("With this option, when "
        "a character cannot be represented in the target character set, it can be "
        "approximated through one or several similarly looking characters."));

    FileWritingId3v2IconvOptionsIgnore = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(FileWritingId3v2IconvOptionsNo)),
        _("//IGNORE"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v2IconvOptionsIgnore, 4,
                     6, 1, 1);
    gtk_widget_set_name (FileWritingId3v2IconvOptionsIgnore, "ignore");
    gtk_widget_set_tooltip_text(FileWritingId3v2IconvOptionsIgnore,_("With this option, when "
        "a character cannot be represented in the target character set, it will "
        "be silently discarded."));

    g_settings_bind_with_mapping (ETSettings, "id3v2-encoding-option",
                                  FileWritingId3v2IconvOptionsNo, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  FileWritingId3v2IconvOptionsNo, NULL);
    g_settings_bind_with_mapping (ETSettings, "id3v2-encoding-option",
                                  FileWritingId3v2IconvOptionsTranslit,
                                  "active", G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  FileWritingId3v2IconvOptionsTranslit, NULL);
    g_settings_bind_with_mapping (ETSettings, "id3v2-encoding-option",
                                  FileWritingId3v2IconvOptionsIgnore, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  FileWritingId3v2IconvOptionsIgnore, NULL);

    // ID3v1 tags
    Frame = gtk_frame_new (_("ID3v1 tags"));
    gtk_box_pack_start(GTK_BOX(id3v1v2hbox),Frame,FALSE,FALSE,2);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox),2);

    Table = et_grid_new (6, 5);
    gtk_box_pack_start(GTK_BOX(vbox),Table,FALSE,FALSE,0);
    gtk_grid_set_row_spacing (GTK_GRID (Table), 2);
    gtk_grid_set_column_spacing (GTK_GRID (Table), 2);


    /* Write ID3v1 tag */
    FileWritingId3v1WriteTag = gtk_check_button_new_with_label(_("Write ID3v1.x tag"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v1WriteTag, 0, 0, 4, 1);
    g_settings_bind (ETSettings, "id3v1-enabled", FileWritingId3v1WriteTag,
                     "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(FileWritingId3v1WriteTag,_("If activated, an ID3v1 tag will be added or "
        "updated at the end of the MP3 files. Else it will be stripped."));
    g_signal_connect_after(G_OBJECT(FileWritingId3v1WriteTag),"toggled",
        G_CALLBACK(Change_Id3_Settings_Toggled),NULL);

    /* Id3V1 writing character set */
    LabelId3v1Charset = gtk_label_new(_("Charset:"));
    gtk_grid_attach (GTK_GRID (Table), LabelId3v1Charset, 0, 1, 4, 1);
    gtk_misc_set_alignment(GTK_MISC(LabelId3v1Charset),0,0.5);

    Label = gtk_label_new("        ");
    gtk_grid_attach (GTK_GRID (Table), Label, 0, 2, 1, 1);

    FileWritingId3v1CharacterSetCombo = gtk_combo_box_text_new();
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v1CharacterSetCombo, 1, 2,
                     3, 1);
    gtk_widget_set_tooltip_text (FileWritingId3v1CharacterSetCombo,
                                 _("Character set used to write ID3v1 tag data in the file."));
    Charset_Populate_Combobox(GTK_COMBO_BOX(FileWritingId3v1CharacterSetCombo), FILE_WRITING_ID3V1_CHARACTER_SET);

    /* ID3V1 Additional iconv() options*/
    LabelAdditionalId3v1IconvOptions = gtk_label_new(_("Additional settings for iconv():"));
    gtk_grid_attach (GTK_GRID (Table), LabelAdditionalId3v1IconvOptions, 1, 3,
                     3, 1);
    gtk_misc_set_alignment(GTK_MISC(LabelAdditionalId3v1IconvOptions),0,0.5);

    FileWritingId3v1IconvOptionsNo = gtk_radio_button_new_with_label(NULL,
        _("No"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v1IconvOptionsNo, 1, 4, 1,
                     1);
    gtk_widget_set_name (FileWritingId3v1IconvOptionsNo, "none");
    gtk_widget_set_tooltip_text(FileWritingId3v1IconvOptionsNo,_("With this option, when "
        "a character cannot be represented in the target character set, it isn't changed. "
        "But note that an error message will be displayed for information."));
    FileWritingId3v1IconvOptionsTranslit = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(FileWritingId3v1IconvOptionsNo)),
        _("//TRANSLIT"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v1IconvOptionsTranslit, 2,
                     4, 1, 1);
    gtk_widget_set_name (FileWritingId3v1IconvOptionsTranslit,
                         "transliterate");
    gtk_widget_set_tooltip_text(FileWritingId3v1IconvOptionsTranslit,_("With this option, when "
        "a character cannot be represented in the target character set, it can be "
        "approximated through one or several similarly looking characters."));

    FileWritingId3v1IconvOptionsIgnore = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(FileWritingId3v1IconvOptionsNo)),
        _("//IGNORE"));
    gtk_grid_attach (GTK_GRID (Table), FileWritingId3v1IconvOptionsIgnore, 3,
                     4, 1, 1);
    gtk_widget_set_name (FileWritingId3v1IconvOptionsIgnore, "ignore");
    gtk_widget_set_tooltip_text(FileWritingId3v1IconvOptionsIgnore,_("With this option, when "
        "a character cannot be represented in the target character set, it will "
        "be silently discarded."));

    g_settings_bind_with_mapping (ETSettings, "id3v1-encoding-option",
                                  FileWritingId3v1IconvOptionsNo, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  FileWritingId3v1IconvOptionsNo, NULL);
    g_settings_bind_with_mapping (ETSettings, "id3v1-encoding-option",
                                  FileWritingId3v1IconvOptionsTranslit,
                                  "active", G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  FileWritingId3v1IconvOptionsTranslit, NULL);
    g_settings_bind_with_mapping (ETSettings, "id3v1-encoding-option",
                                  FileWritingId3v1IconvOptionsIgnore, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  FileWritingId3v1IconvOptionsIgnore, NULL);

    /* Character Set for reading tag */
    Frame = gtk_frame_new (_("Character Set for reading ID3 tags"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    Table = et_grid_new(4,2);
    gtk_box_pack_start(GTK_BOX(vbox),Table,FALSE,FALSE,0);
    //gtk_container_set_border_width(GTK_CONTAINER(Table), 2);
    /*gtk_grid_set_row_spacing (GTK_GRID (Table), 2);*/
    gtk_grid_set_column_spacing (GTK_GRID (Table), 2);

    // "File Reading Charset" Check Button + Combo
    UseNonStandardId3ReadingCharacterSet = gtk_check_button_new_with_label(_(
        "Non-standard:"));
    g_settings_bind (ETSettings, "id3-override-read-encoding",
                     UseNonStandardId3ReadingCharacterSet, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_grid_attach (GTK_GRID (Table), UseNonStandardId3ReadingCharacterSet, 0,
                     0, 1, 1);
    gtk_widget_set_tooltip_text(UseNonStandardId3ReadingCharacterSet,
        _("This character set will be used when reading the tag data, to convert "
        "each string found in an ISO-8859-1 field in the tag (for ID3v2 or/and ID3v1 tag).\n"
        "\n"
        "For example:\n"
        "  - In previous versions of EasyTAG, you could save UTF-8 strings in an ISO-8859-1 "
        "field. This is not correct. To convert these tags to Unicode: activate this option "
        "and select UTF-8. You must also activate above the option 'Try to save tags to "
        "ISO-8859-1. If it isn't possible then use UNICODE (recommended)' or 'Always save "
        "tags to UNICODE character set'.\n"
        "  - If Unicode was not used, Russian people can select the character set "
        "'Windows-1251' to load tags written under Windows. And 'KOI8-R' to load tags "
        "written under Unix systems."));

    FileReadingId3v1v2CharacterSetCombo = gtk_combo_box_text_new();
    gtk_grid_attach (GTK_GRID (Table), FileReadingId3v1v2CharacterSetCombo, 2,
                     0, 1, 1);

    gtk_widget_set_tooltip_text (FileReadingId3v1v2CharacterSetCombo,
                                 _("Character set used to read tag data in the file."));

    Charset_Populate_Combobox(GTK_COMBO_BOX(FileReadingId3v1v2CharacterSetCombo), 
        FILE_READING_ID3V1V2_CHARACTER_SET);
    g_signal_connect_after(G_OBJECT(UseNonStandardId3ReadingCharacterSet),"toggled",
                           G_CALLBACK(Use_Non_Standard_Id3_Reading_Character_Set_Toggled),NULL);

    Use_Non_Standard_Id3_Reading_Character_Set_Toggled();
    Change_Id3_Settings_Toggled();


    /*
     * Scanner
     */
    Label = gtk_label_new (_("Scanner"));
    Frame = gtk_frame_new (_("Scanner"));
    gtk_notebook_append_page (GTK_NOTEBOOK(OptionsNoteBook),Frame,Label);
    gtk_container_set_border_width(GTK_CONTAINER(Frame), 5);

    /* Save the number of the page. Asked in Scanner window */
    OptionsNoteBook_Scanner_Page_Num = gtk_notebook_page_num(GTK_NOTEBOOK(OptionsNoteBook),Frame);

    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),VBox);
    gtk_container_set_border_width(GTK_CONTAINER(VBox), 4);

    /* Character conversion for the 'Fill Tag' scanner (=> FTS...) */
    Frame = gtk_frame_new (_("Fill Tag Scanner - Character Conversion"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    FTSConvertUnderscoreAndP20IntoSpace = gtk_check_button_new_with_label(_("Convert underscore "
        "character '_' and string '%20' to space ' '"));
    FTSConvertSpaceIntoUnderscore = gtk_check_button_new_with_label(_("Convert space ' ' to underscore '_'"));
    gtk_box_pack_start(GTK_BOX(vbox),FTSConvertUnderscoreAndP20IntoSpace,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),FTSConvertSpaceIntoUnderscore,      FALSE,FALSE,0);
    g_signal_connect_swapped(G_OBJECT(FTSConvertUnderscoreAndP20IntoSpace),"toggled",
        G_CALLBACK(Scanner_Convert_Check_Button_Toggled_1),G_OBJECT(FTSConvertSpaceIntoUnderscore));
    g_signal_connect_swapped(G_OBJECT(FTSConvertSpaceIntoUnderscore),"toggled",
        G_CALLBACK(Scanner_Convert_Check_Button_Toggled_1),G_OBJECT(FTSConvertUnderscoreAndP20IntoSpace));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FTSConvertUnderscoreAndP20IntoSpace),
        FTS_CONVERT_UNDERSCORE_AND_P20_INTO_SPACE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FTSConvertSpaceIntoUnderscore),
        FTS_CONVERT_SPACE_INTO_UNDERSCORE);
    gtk_widget_set_tooltip_text(FTSConvertUnderscoreAndP20IntoSpace,_("If activated, this conversion "
        "will be used when applying a mask from the scanner for tags."));
    gtk_widget_set_tooltip_text(FTSConvertSpaceIntoUnderscore,_("If activated, this conversion "
        "will be used when applying a mask from the scanner for tags."));

    /* Character conversion for the 'Rename File' scanner (=> RFS...) */
    Frame = gtk_frame_new (_("Rename File Scanner - Character Conversion"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox),2);
    RFSConvertUnderscoreAndP20IntoSpace = gtk_radio_button_new_with_label(NULL, _("Convert underscore " "character '_' and string '%20' to space ' '"));
    RFSConvertSpaceIntoUnderscore = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(RFSConvertUnderscoreAndP20IntoSpace), _("Convert space ' ' to underscore '_'"));
		RFSRemoveSpaces = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(RFSConvertUnderscoreAndP20IntoSpace), _("Remove spaces"));
    gtk_box_pack_start(GTK_BOX(vbox),RFSConvertUnderscoreAndP20IntoSpace,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),RFSConvertSpaceIntoUnderscore,      FALSE,FALSE,0);
		 gtk_box_pack_start(GTK_BOX(vbox),RFSRemoveSpaces,                    FALSE,FALSE,0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RFSConvertUnderscoreAndP20IntoSpace),
        RFS_CONVERT_UNDERSCORE_AND_P20_INTO_SPACE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RFSConvertSpaceIntoUnderscore),
        RFS_CONVERT_SPACE_INTO_UNDERSCORE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RFSRemoveSpaces),
        RFS_REMOVE_SPACES);
    gtk_widget_set_tooltip_text(RFSConvertUnderscoreAndP20IntoSpace,_("If activated, this conversion "
        "will be used when applying a mask from the scanner for filenames."));
    gtk_widget_set_tooltip_text(RFSConvertSpaceIntoUnderscore,_("If activated, this conversion "
        "will be used when applying a mask from the scanner for filenames."));
    gtk_widget_set_tooltip_text(RFSRemoveSpaces,_("If activated, this conversion "        "will be used when applying a mask from the scanner for filenames."));

    /* Character conversion for the 'Process Fields' scanner (=> PFS...) */
    Frame = gtk_frame_new (_("Process Fields Scanner - Character Conversion"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox),2);

    // Don't convert some words like to, feat. first letter uppercase.
    PFSDontUpperSomeWords = gtk_check_button_new_with_label(_("Don't uppercase "
        "first letter of words for some prepositions and articles."));
    gtk_box_pack_start(GTK_BOX(vbox),PFSDontUpperSomeWords, FALSE, FALSE, 0);
    g_settings_bind (ETSettings, "process-uppercase-prepositions",
                     PFSDontUpperSomeWords, "active",
                     G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
    gtk_widget_set_tooltip_text(PFSDontUpperSomeWords, _("Don't convert first "
        "letter of words like prepositions, articles and words like feat., "
        "when using the scanner 'First letter uppercase of each word' (for "
        "example, you will obtain 'Text in an Entry' instead of 'Text In An Entry')."));

    /* Properties of the scanner window */
    Frame = gtk_frame_new (_("Scanner Window"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    OpenScannerWindowOnStartup = gtk_check_button_new_with_label(_("Open the Scanner Window on startup"));
    gtk_box_pack_start(GTK_BOX(vbox),OpenScannerWindowOnStartup,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "scan-startup", OpenScannerWindowOnStartup,
                     "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(OpenScannerWindowOnStartup,_("Activate this option to open automatically "
        "the scanner window when EasyTAG starts."));


    /* Other options */
    Frame = gtk_frame_new (_("Fields"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    // Overwrite text into tag fields
    OverwriteTagField = gtk_check_button_new_with_label(_("Overwrite fields when scanning tags"));
    gtk_box_pack_start(GTK_BOX(vbox),OverwriteTagField,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "fill-overwrite-tag-fields",
                     OverwriteTagField, "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(OverwriteTagField,_("If activated, the scanner will replace existing text "
        "in fields by the new one. If deactivated, only blank fields of the tag will be filled."));

    // Set a default comment text or CRC-32 checksum
    if (!DefaultCommentModel)
        DefaultCommentModel = gtk_list_store_new(MISC_COMBO_COUNT, G_TYPE_STRING);
    else
        gtk_list_store_clear(DefaultCommentModel);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    SetDefaultComment = gtk_check_button_new_with_label(_("Set this text as default comment:"));
    gtk_box_pack_start(GTK_BOX(hbox),SetDefaultComment,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "fill-set-default-comment", SetDefaultComment,
                     "active", G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(SetDefaultComment,_("Activate this option if you want to put the "
        "following string into the comment field when using the 'Fill Tag' scanner."));
    DefaultComment = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(DefaultCommentModel));
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(DefaultComment),MISC_COMBO_TEXT);
    gtk_box_pack_start(GTK_BOX(hbox),DefaultComment,FALSE,FALSE,0);
    gtk_widget_set_size_request(GTK_WIDGET(DefaultComment), 250, -1);
    g_settings_bind (ETSettings, "fill-set-default-comment", DefaultComment,
                     "sensitive", G_SETTINGS_BIND_GET);

    /* History list */
    Load_Default_Tag_Comment_Text_List(DefaultCommentModel, MISC_COMBO_TEXT);
    default_comment = g_settings_get_string (ETSettings,
                                             "fill-default-comment");
    Add_String_To_Combo_List (DefaultCommentModel, default_comment);
    g_free (default_comment);
    g_settings_bind (ETSettings, "fill-default-comment",
                     gtk_bin_get_child (GTK_BIN (DefaultComment)), "text",
                     G_SETTINGS_BIND_DEFAULT);

    // CRC32 comment
    Crc32Comment = gtk_check_button_new_with_label(_("Use CRC32 as the default "
        "comment (for files with ID3 tags only)."));
    gtk_box_pack_start(GTK_BOX(vbox),Crc32Comment,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "fill-crc32-comment", Crc32Comment, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(Crc32Comment,_("Calculates the CRC-32 value of the file "
        "and writes it into the comment field when using the 'Fill Tag' scanner."));
    g_signal_connect_swapped(G_OBJECT(SetDefaultComment), "toggled",
        G_CALLBACK(Scanner_Convert_Check_Button_Toggled_1),G_OBJECT(Crc32Comment));
    g_signal_connect_swapped(G_OBJECT(Crc32Comment), "toggled",
        G_CALLBACK(Scanner_Convert_Check_Button_Toggled_1),G_OBJECT(SetDefaultComment));


    /*
     * CDDB
     */
    Label = gtk_label_new (_("CD Data Base"));
    Frame = gtk_frame_new (_("CD Data Base"));
    gtk_notebook_append_page (GTK_NOTEBOOK(OptionsNoteBook),Frame,Label);
    gtk_container_set_border_width(GTK_CONTAINER(Frame), 5);

    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),VBox);
    gtk_container_set_border_width(GTK_CONTAINER(VBox), 4);

    // CDDB Server Settings (Automatic Search)
    Frame = gtk_frame_new (_("Server Settings for Automatic Search"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE, 0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

    // 1rst automatic search server
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_container_add(GTK_CONTAINER(vbox),hbox);
    Label = gtk_label_new(_("Name:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,2);
    CddbServerNameAutomaticSearch = gtk_combo_box_text_new_with_entry();
    gtk_box_pack_start(GTK_BOX(hbox),CddbServerNameAutomaticSearch,FALSE,FALSE,0);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch), "freedb.freedb.org");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch), "www.gnudb.org");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch), "at.freedb.org");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch), "au.freedb.org");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch), "ca.freedb.org");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch), "es.freedb.org");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch), "fi.freedb.org");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch), "ru.freedb.org");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch), "uk.freedb.org");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch), "us.freedb.org");
    g_settings_bind (ETSettings, "cddb-automatic-search-hostname",
                     gtk_bin_get_child (GTK_BIN (CddbServerNameAutomaticSearch)),
                     "text", G_SETTINGS_BIND_DEFAULT);

    Label = gtk_label_new (_("Port:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,2);
    CddbServerPortAutomaticSearch = gtk_spin_button_new_with_range (0.0,
                                                                    65535.0,
                                                                    1.0);
    g_settings_bind (ETSettings, "cddb-automatic-search-port",
                     CddbServerPortAutomaticSearch, "value",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_size_request(GTK_WIDGET(CddbServerPortAutomaticSearch), 45, -1);
    gtk_box_pack_start(GTK_BOX(hbox),CddbServerPortAutomaticSearch,FALSE,FALSE,0);

    Label = gtk_label_new (_("CGI Path:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,2);
    CddbServerCgiPathAutomaticSearch = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox),CddbServerCgiPathAutomaticSearch,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "cddb-automatic-search-path",
                     CddbServerCgiPathAutomaticSearch, "text",
                     G_SETTINGS_BIND_DEFAULT);

    // 2sd automatic search server
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_container_add(GTK_CONTAINER(vbox),hbox);
    Label = gtk_label_new(_("Name:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,2);
    CddbServerNameAutomaticSearch2 = gtk_combo_box_text_new_with_entry();
    gtk_box_pack_start(GTK_BOX(hbox),CddbServerNameAutomaticSearch2,FALSE,FALSE,0);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameAutomaticSearch2), "freedb.musicbrainz.org");
    g_settings_bind (ETSettings, "cddb-automatic-search-hostname2",
                     gtk_bin_get_child (GTK_BIN (CddbServerNameAutomaticSearch2)),
                     "text", G_SETTINGS_BIND_DEFAULT);

    Label = gtk_label_new (_("Port:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,2);
    CddbServerPortAutomaticSearch2 = gtk_spin_button_new_with_range (0.0,
                                                                     65535.0,
                                                                     1.0);
    g_settings_bind (ETSettings, "cddb-automatic-search-port2",
                     CddbServerPortAutomaticSearch2, "value",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_size_request(GTK_WIDGET(CddbServerPortAutomaticSearch2), 45, -1);
    gtk_box_pack_start(GTK_BOX(hbox),CddbServerPortAutomaticSearch2,FALSE,FALSE,0);

    Label = gtk_label_new (_("CGI Path:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,2);
    CddbServerCgiPathAutomaticSearch2 = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox),CddbServerCgiPathAutomaticSearch2,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "cddb-automatic-search-path2",
                     CddbServerCgiPathAutomaticSearch2, "text",
                     G_SETTINGS_BIND_DEFAULT);

    // CDDB Server Settings (Manual Search)
    Frame = gtk_frame_new (_("Server Settings for Manual Search"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_container_add(GTK_CONTAINER(vbox),hbox);
    Label = gtk_label_new(_("Name:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,2);
    CddbServerNameManualSearch = gtk_combo_box_text_new_with_entry();
    gtk_box_pack_start(GTK_BOX(hbox),CddbServerNameManualSearch,FALSE,FALSE,0);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameManualSearch), "www.freedb.org");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(CddbServerNameManualSearch), "www.gnudb.org");
    g_settings_bind (ETSettings, "cddb-manual-search-hostname",
                     gtk_bin_get_child (GTK_BIN (CddbServerNameManualSearch)),
                     "text", G_SETTINGS_BIND_DEFAULT);

    Label = gtk_label_new (_("Port:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,2);
    CddbServerPortManualSearch = gtk_spin_button_new_with_range (0.0, 65535.0,
                                                                 1.0);
    g_settings_bind (ETSettings, "cddb-manual-search-port",
                     CddbServerPortManualSearch, "value",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_size_request(GTK_WIDGET(CddbServerPortManualSearch), 45, -1);
    gtk_entry_set_max_length(GTK_ENTRY(CddbServerPortManualSearch),5);
    gtk_box_pack_start(GTK_BOX(hbox),CddbServerPortManualSearch,FALSE,FALSE,0);

    Label = gtk_label_new (_("CGI Path:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,2);
    CddbServerCgiPathManualSearch = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox),CddbServerCgiPathManualSearch,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "cddb-manual-search-path",
                     CddbServerCgiPathManualSearch, "text",
                     G_SETTINGS_BIND_DEFAULT);

    // Local access for CDDB (Automatic Search)
    Frame = gtk_frame_new (_("Local CD Data Base"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_container_add(GTK_CONTAINER(vbox),hbox);
    Label = gtk_label_new(_("Path:"));
    gtk_box_pack_start(GTK_BOX(hbox),Label,FALSE,FALSE,2);

    if (CddbLocalPathModel != NULL)
        gtk_list_store_clear(CddbLocalPathModel);
    else
        CddbLocalPathModel = gtk_list_store_new(MISC_COMBO_COUNT, G_TYPE_STRING);

    CddbLocalPath = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(CddbLocalPathModel));
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(CddbLocalPath),MISC_COMBO_TEXT);
    gtk_box_pack_start(GTK_BOX(hbox),CddbLocalPath,FALSE,FALSE,0);
    gtk_widget_set_size_request(GTK_WIDGET(CddbLocalPath), 450, -1);
    gtk_widget_set_tooltip_text(gtk_bin_get_child(GTK_BIN(CddbLocalPath)),_("Specify the directory "
        "where the local CD database is located. The local CD data base contains the eleven following "
        "directories 'blues', 'classical', 'country', 'data', 'folk', 'jazz', 'newage', 'reggae', "
        "'rock', 'soundtrack' and 'misc'."));
    g_signal_connect(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(CddbLocalPath)))),"activate",G_CALLBACK(CddbLocalPath_Combo_Add_String),NULL);
    //g_signal_connect(G_OBJECT(GTK_ENTRY(GTK_BIN(CddbLocalPath)->child)),"focus_out_event",G_CALLBACK(CddbLocalPath_Combo_Add_String),NULL);

    // History list
    Load_Cddb_Local_Path_List(CddbLocalPathModel, MISC_COMBO_TEXT);

    // If default path hasn't been added already, add it now..
    if (CDDB_LOCAL_PATH)
    {
        path_utf8 = filename_to_display(CDDB_LOCAL_PATH);
        Add_String_To_Combo_List(CddbLocalPathModel, path_utf8);
        if (path_utf8)
            gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(CddbLocalPath))), path_utf8);
        g_free(path_utf8);
    }

    Button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    gtk_box_pack_start(GTK_BOX(hbox),Button,FALSE,FALSE,0);
    g_signal_connect_swapped(G_OBJECT(Button),"clicked",
                             G_CALLBACK(File_Selection_Window_For_Directory),G_OBJECT(gtk_bin_get_child(GTK_BIN(CddbLocalPath))));

    // CDDB Proxy Settings
    Frame = gtk_frame_new (_("Proxy Settings"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);

    Table = et_grid_new (3, 5);
    gtk_container_add(GTK_CONTAINER(Frame),Table);
    gtk_grid_set_row_spacing (GTK_GRID (Table), 2);
    gtk_grid_set_column_spacing (GTK_GRID (Table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(Table), 2);

    CddbUseProxy = gtk_check_button_new_with_label(_("Use a proxy"));
    gtk_grid_attach (GTK_GRID (Table), CddbUseProxy, 0, 0, 5, 1);
    g_settings_bind (ETSettings, "cddb-proxy-enabled", CddbUseProxy, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(CddbUseProxy,_("Set active the settings of the proxy server."));

    Label = gtk_label_new("     ");
    gtk_grid_attach (GTK_GRID (Table), Label, 0, 1, 1, 1);

    Label = gtk_label_new(_("Host Name:"));
    gtk_grid_attach (GTK_GRID (Table), Label, 1, 1, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(Label),1,0.5);
    CddbProxyName = gtk_entry_new();
    gtk_grid_attach (GTK_GRID (Table), CddbProxyName, 2, 1, 1, 1);
    g_settings_bind (ETSettings, "cddb-proxy-hostname", CddbProxyName, "text",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(CddbProxyName,_("Name of the proxy server."));
    Label = gtk_label_new (_("Port:"));
    gtk_grid_attach (GTK_GRID (Table), Label, 3, 1, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(Label),1,0.5);
    CddbProxyPort = gtk_spin_button_new_with_range (0.0, 65535.0, 1.0);
    g_settings_bind (ETSettings, "cddb-proxy-port", CddbProxyPort, "value",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_size_request(GTK_WIDGET(CddbProxyPort), 45, -1);
    gtk_grid_attach (GTK_GRID (Table), CddbProxyPort, 4, 1, 1, 1);
    gtk_widget_set_tooltip_text(CddbProxyPort,_("Port of the proxy server."));
    g_signal_connect(G_OBJECT(CddbUseProxy),"toggled",G_CALLBACK(Cddb_Use_Proxy_Toggled),NULL);
    Label = gtk_label_new(_("User Name:"));
    gtk_grid_attach (GTK_GRID (Table), Label, 1, 2, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(Label),1,0.5);
    CddbProxyUserName = gtk_entry_new();
    g_settings_bind (ETSettings, "cddb-proxy-username", CddbProxyUserName,
                     "text", G_SETTINGS_BIND_DEFAULT);
    gtk_grid_attach (GTK_GRID (Table), CddbProxyUserName, 2, 2, 1, 1);
    gtk_widget_set_tooltip_text(CddbProxyUserName,_("Name of user for the the proxy server."));
    Label = gtk_label_new(_("User Password:"));
    gtk_grid_attach (GTK_GRID (Table), Label, 3, 2, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(Label),1,0.5);
    CddbProxyUserPassword = gtk_entry_new();
    gtk_entry_set_visibility (GTK_ENTRY (CddbProxyUserPassword), FALSE);
    g_settings_bind (ETSettings, "cddb-proxy-password", CddbProxyUserPassword,
                     "text", G_SETTINGS_BIND_DEFAULT);
    gtk_grid_attach (GTK_GRID (Table), CddbProxyUserPassword, 4, 2, 1, 1);
    gtk_widget_set_tooltip_text(CddbProxyUserPassword,_("Password of user for the the proxy server."));
    Cddb_Use_Proxy_Toggled();


    // Track Name list (CDDB results)
    Frame = gtk_frame_new (_("Track Name List"));
    gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    CddbFollowFile = gtk_check_button_new_with_label(_("Select corresponding audio "
        "file (according position or DLM if activated below)"));
    gtk_box_pack_start(GTK_BOX(vbox),CddbFollowFile,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "cddb-follow-file", CddbFollowFile, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(CddbFollowFile,_("If activated, when selecting a "
        "line in the list of tracks name, the corresponding audio file in the "
        "main list will be also selected."));

    // Check box to use DLM (also used in the cddb window)
    CddbUseDLM = gtk_check_button_new_with_label(_("Use the Levenshtein algorithm "
        "(DLM) to match lines (using title) with audio files (using filename)"));
    gtk_box_pack_start(GTK_BOX(vbox),CddbUseDLM,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "cddb-dlm-enabled", CddbUseDLM, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(CddbUseDLM,_("When activating this option, the "
        "Levenshtein algorithm (DLM: Damerau-Levenshtein Metric) will be used "
        "to match the CDDB title against every filename in the current folder, "
        "and to select the best match. This will be used when selecting the "
        "corresponding audio file, or applying CDDB results, instead of using "
        "directly the position order."));


    /*
     * Confirmation
     */
    Label = gtk_label_new (_("Confirmation"));
    Frame = gtk_frame_new (_("Confirmation"));
    gtk_notebook_append_page (GTK_NOTEBOOK(OptionsNoteBook),Frame,Label);
    gtk_container_set_border_width(GTK_CONTAINER(Frame), 5);

    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(Frame),VBox);
    gtk_container_set_border_width(GTK_CONTAINER(VBox), 2);

    ConfirmBeforeExit = gtk_check_button_new_with_label(_("Confirm exit from program"));
    gtk_box_pack_start(GTK_BOX(VBox),ConfirmBeforeExit,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "confirm-quit", ConfirmBeforeExit, "active",
                     G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_tooltip_text(ConfirmBeforeExit,_("If activated, opens a dialog box to ask "
        "confirmation before exiting the program."));

    ConfirmWriteTag = gtk_check_button_new_with_label(_("Confirm writing of file tag"));
    gtk_box_pack_start(GTK_BOX(VBox),ConfirmWriteTag,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "confirm-write-tags", ConfirmWriteTag,
                     "active", G_SETTINGS_BIND_DEFAULT);

    ConfirmRenameFile = gtk_check_button_new_with_label(_("Confirm renaming of file"));
    gtk_box_pack_start(GTK_BOX(VBox),ConfirmRenameFile,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "confirm-rename-file", ConfirmRenameFile,
                     "active", G_SETTINGS_BIND_DEFAULT);

    ConfirmDeleteFile = gtk_check_button_new_with_label(_("Confirm deleting of file"));
    gtk_box_pack_start(GTK_BOX(VBox),ConfirmDeleteFile,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "confirm-delete-file", ConfirmDeleteFile,
                     "active", G_SETTINGS_BIND_DEFAULT);

    ConfirmWritePlayList = gtk_check_button_new_with_label(_("Confirm writing of playlist"));
    gtk_box_pack_start(GTK_BOX(VBox),ConfirmWritePlayList,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "confirm-write-playlist",
                     ConfirmWritePlayList, "active", G_SETTINGS_BIND_DEFAULT);

    ConfirmWhenUnsavedFiles = gtk_check_button_new_with_label(_("Confirm changing directory when there are unsaved changes"));
    gtk_box_pack_start(GTK_BOX(VBox),ConfirmWhenUnsavedFiles,FALSE,FALSE,0);
    g_settings_bind (ETSettings, "confirm-when-unsaved-files",
                     ConfirmWhenUnsavedFiles, "active",
                     G_SETTINGS_BIND_DEFAULT);

    /*
     * Buttons box of Option Window
     */
    ButtonBox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(OptionsVBox), ButtonBox, FALSE, FALSE, 4);

    gtk_button_box_set_layout (GTK_BUTTON_BOX (ButtonBox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing (GTK_BOX(ButtonBox), 15);


    /* Apply Button */
    Button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    gtk_widget_set_tooltip_text(Button,_("Apply changes (but don't save) and close this window"));


    /* Cancel Button */
    Button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_container_add(GTK_CONTAINER(ButtonBox), Button);
    g_signal_connect(G_OBJECT(Button),"clicked", G_CALLBACK(OptionsWindow_Cancel_Button),NULL);
    gtk_widget_set_can_default(Button,TRUE);
    gtk_widget_grab_default(Button);
    gtk_widget_set_tooltip_text(Button,_("Close this window without saving"));


    /* Save Button */
    Button = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_container_add(GTK_CONTAINER(ButtonBox), Button);
    g_signal_connect(G_OBJECT(Button),"clicked", G_CALLBACK(OptionsWindow_Save_Button),NULL);
    gtk_widget_set_can_default(Button,TRUE);
    gtk_widget_set_tooltip_text(Button,_("Save changes and close this window"));

    /* Show all in the options window */
    gtk_widget_show_all(OptionsWindow);

    /* Load the default page */
    g_settings_bind (ETSettings, "preferences-page", OptionsNoteBook, "page",
                     G_SETTINGS_BIND_DEFAULT);
}

void Number_Track_Formatted_Toggled (void)
{
    gtk_widget_set_sensitive(NumberTrackFormatedSpinButton,gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(NumberTrackFormated)));
    // To update the example...
    g_signal_emit_by_name(G_OBJECT(NumberTrackFormatedSpinButton),"changed",NULL);
}

static void
Number_Track_Formatted_Spin_Button_Changed (GtkWidget *Label,
                                            GtkWidget *SpinButton)
{
    gchar *tmp;
    gint val;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(NumberTrackFormated)))
        val = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(SpinButton));
    else
        val = 1;

    // For translators : be aware to NOT translate '%.*d' in this string
    tmp = g_strdup_printf(_("(Example: %.*d_-_Track_name_1.mp3)"),val,1);

    gtk_label_set_text(GTK_LABEL(Label),tmp);
    g_free(tmp);
}

static void
Use_Non_Standard_Id3_Reading_Character_Set_Toggled (void)
{
    gtk_widget_set_sensitive(FileReadingId3v1v2CharacterSetCombo,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(UseNonStandardId3ReadingCharacterSet)));
}

static void
Change_Id3_Settings_Toggled (void)
{
    int active;

    if ( !FileWritingId3v2UseUnicodeCharacterSet 
    ||   !FileWritingId3v2UseNoUnicodeCharacterSet
    ||   !FileWritingId3v2WriteTag
#ifdef ENABLE_ID3LIB
    ||   !FileWritingId3v2VersionCombo
    ||   !LabelId3v2Version
#endif
    ||   !FileWritingId3v1WriteTag
    ||   !LabelId3v2Charset
    ||   !FileWritingId3v2UnicodeCharacterSetCombo
    ||   !FileWritingId3v2NoUnicodeCharacterSetCombo
    ||   !LabelAdditionalId3v2IconvOptions
    ||   !FileWritingId3v2IconvOptionsNo
    ||   !FileWritingId3v2IconvOptionsTranslit
    ||   !FileWritingId3v2IconvOptionsIgnore
    ||   !FileWritingId3v2UseCrc32
    ||   !FileWritingId3v2UseCompression
    ||   !FileWritingId3v2TextOnlyGenre
    ||   !ConvertOldId3v2TagVersion
    ||   !LabelId3v1Charset
    ||   !FileWritingId3v1CharacterSetCombo
    ||   !LabelAdditionalId3v1IconvOptions
    ||   !FileWritingId3v1IconvOptionsNo
    ||   !FileWritingId3v1IconvOptionsTranslit
    ||   !FileWritingId3v1IconvOptionsIgnore
       )
        return;

    active = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FileWritingId3v2UseUnicodeCharacterSet)) != 0);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FileWritingId3v2WriteTag)))
    {
        gtk_widget_set_sensitive(LabelId3v2Charset, TRUE);

#ifdef ENABLE_ID3LIB
        gtk_widget_set_sensitive(LabelId3v2Version, TRUE);
        gtk_widget_set_sensitive(FileWritingId3v2VersionCombo, TRUE);
        if (gtk_combo_box_get_active(GTK_COMBO_BOX(FileWritingId3v2VersionCombo)) == 1)
        {
            // When "ID3v2.3" is selected
            gtk_combo_box_set_active(GTK_COMBO_BOX(FileWritingId3v2UnicodeCharacterSetCombo), 1);
            gtk_widget_set_sensitive(FileWritingId3v2UnicodeCharacterSetCombo, FALSE);
        }else
        {
            // When "ID3v2.4" is selected
            gtk_combo_box_set_active(GTK_COMBO_BOX(FileWritingId3v2UnicodeCharacterSetCombo), 0); // Set "UTF-8" as default value for this version of tag
            gtk_widget_set_sensitive(FileWritingId3v2UnicodeCharacterSetCombo, active);
        }
#else 
        gtk_widget_set_sensitive(FileWritingId3v2UnicodeCharacterSetCombo, active);
#endif
        gtk_widget_set_sensitive(FileWritingId3v2UseUnicodeCharacterSet, TRUE);
        gtk_widget_set_sensitive(FileWritingId3v2UseNoUnicodeCharacterSet, TRUE);
        gtk_widget_set_sensitive(FileWritingId3v2NoUnicodeCharacterSetCombo, !active);
        gtk_widget_set_sensitive(LabelAdditionalId3v2IconvOptions, !active);
        gtk_widget_set_sensitive(FileWritingId3v2IconvOptionsNo, !active);
        gtk_widget_set_sensitive(FileWritingId3v2IconvOptionsTranslit, !active);
        gtk_widget_set_sensitive(FileWritingId3v2IconvOptionsIgnore, !active);
        gtk_widget_set_sensitive(FileWritingId3v2UseCrc32, TRUE);
        gtk_widget_set_sensitive(FileWritingId3v2UseCompression, TRUE);
        gtk_widget_set_sensitive(FileWritingId3v2TextOnlyGenre, TRUE);
        gtk_widget_set_sensitive(ConvertOldId3v2TagVersion, TRUE);

    }else
    {
        gtk_widget_set_sensitive(LabelId3v2Charset, FALSE);
#ifdef ENABLE_ID3LIB
        gtk_widget_set_sensitive(LabelId3v2Version, FALSE);
        gtk_widget_set_sensitive(FileWritingId3v2VersionCombo, FALSE);
#endif
        gtk_widget_set_sensitive(FileWritingId3v2UseUnicodeCharacterSet, FALSE);
        gtk_widget_set_sensitive(FileWritingId3v2UseNoUnicodeCharacterSet, FALSE);
        gtk_widget_set_sensitive(FileWritingId3v2UnicodeCharacterSetCombo, FALSE);
        gtk_widget_set_sensitive(FileWritingId3v2NoUnicodeCharacterSetCombo, FALSE);
        gtk_widget_set_sensitive(LabelAdditionalId3v2IconvOptions, FALSE);
        gtk_widget_set_sensitive(FileWritingId3v2IconvOptionsNo, FALSE);
        gtk_widget_set_sensitive(FileWritingId3v2IconvOptionsTranslit, FALSE);
        gtk_widget_set_sensitive(FileWritingId3v2IconvOptionsIgnore, FALSE);
        gtk_widget_set_sensitive(FileWritingId3v2UseCrc32, FALSE);
        gtk_widget_set_sensitive(FileWritingId3v2UseCompression, FALSE);
        gtk_widget_set_sensitive(FileWritingId3v2TextOnlyGenre, FALSE);
        gtk_widget_set_sensitive(ConvertOldId3v2TagVersion, 0);
    }

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FileWritingId3v1WriteTag));

    gtk_widget_set_sensitive(LabelId3v1Charset, active);
    gtk_widget_set_sensitive(FileWritingId3v1CharacterSetCombo, active);
    gtk_widget_set_sensitive(LabelAdditionalId3v1IconvOptions, active);
    gtk_widget_set_sensitive(FileWritingId3v1IconvOptionsNo, active);
    gtk_widget_set_sensitive(FileWritingId3v1IconvOptionsTranslit, active);
    gtk_widget_set_sensitive(FileWritingId3v1IconvOptionsIgnore, active);
}

static void
Cddb_Use_Proxy_Toggled (void)
{
    gboolean active;

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(CddbUseProxy));
    gtk_widget_set_sensitive(CddbProxyName,active);
    gtk_widget_set_sensitive(CddbProxyPort,active);
    gtk_widget_set_sensitive(CddbProxyUserName,active);
    gtk_widget_set_sensitive(CddbProxyUserPassword,active);
}

/* Callback from Open_OptionsWindow */
static gboolean
OptionsWindow_Key_Press (GtkWidget *window, GdkEvent *event)
{
    GdkEventKey *kevent;

    if (event && event->type == GDK_KEY_PRESS)
    {
        kevent = (GdkEventKey *)event;
        switch(kevent->keyval)
        {
            case GDK_KEY_Escape:
            {
                OptionsWindow_Quit();
                break;
            }
        }
    }
    return FALSE;
}

/* Callback from Open_OptionsWindow */
static void
OptionsWindow_Save_Button (void)
{
    if (!Check_Config()) return;

#ifndef G_OS_WIN32
    /* FIXME : make gtk crash on win32 */
    Add_String_To_Combo_List(DefaultPathModel,      gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(DefaultPathToMp3)))));
    Add_String_To_Combo_List(FilePlayerModel,       gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(FilePlayerCombo)))));
    Add_String_To_Combo_List(DefaultCommentModel,   gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(DefaultComment)))));
    Add_String_To_Combo_List(CddbLocalPathModel,    gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(CddbLocalPath)))));
#endif /* !G_OS_WIN32 */

    Save_Changes_Of_Preferences_Window();

    OptionsWindow_Quit();
    Statusbar_Message(_("Configuration saved"),TRUE);
}

/* Callback from Open_OptionsWindow */
static void
OptionsWindow_Cancel_Button (void)
{
    OptionsWindow_Quit();
    Statusbar_Message(_("Configuration unchanged"),TRUE);
}

/* Callback from Open_OptionsWindow */
static void
OptionsWindow_Quit (void)
{
    if (OptionsWindow)
    {
        OptionsWindow_Apply_Changes();

        /* Now quit */
        gtk_widget_destroy(OptionsWindow);
        OptionsWindow = NULL;
        gtk_widget_set_sensitive(MainWindow, TRUE);
    }
}

/*
 * For the configuration file...
 */
void OptionsWindow_Apply_Changes (void)
{
    if (OptionsWindow)
    {
        GdkWindow *window;

        window = gtk_widget_get_window(OptionsWindow);

        if ( window && gdk_window_is_visible(window) && gdk_window_get_state(window)!=GDK_WINDOW_STATE_MAXIMIZED )
        {
            gint x, y, width, height;

            /* Position and Origin of the preferences window. */
            gdk_window_get_root_origin(window, &x, &y);
            width = gdk_window_get_width (window);
            height = gdk_window_get_height (window);
            g_settings_set (ETSettings, "preferences-location", "(iiii)", x, y,
                            width, height);
        }

        /* Save combobox history lists before exit */
        Save_Default_Path_To_MP3_List     (DefaultPathModel, MISC_COMBO_TEXT);
        Save_Default_Tag_Comment_Text_List(DefaultCommentModel, MISC_COMBO_TEXT);
        Save_Audio_File_Player_List       (FilePlayerModel, MISC_COMBO_TEXT);
        Save_Cddb_Local_Path_List         (CddbLocalPathModel, MISC_COMBO_TEXT);
    }
}



/*
 * Check_Config: Check if config information are correct
 * dsd: Check this... going from utf8 to raw is dodgy stuff
 *
 * Problem noted : if a character is escaped (like : 'C\351line DION') in
 *                 gtk_file_chooser it will converted to UTF-8. So after, there
 *                 is a problem to convert it in the right system encoding to be
 *                 passed to stat(), and it can find the directory.
 * exemple :
 *  - initial file on system                        : C\351line DION - D'eux (1995)
 *  - converted to UTF-8 (path_utf8)                : Céline DION - D'eux (1995)
 *  - try to convert to system encoding (path_real) : ?????
 */
static gboolean
Check_DefaultPathToMp3 (void)
{
    gchar *path_utf8;
    gchar *path_real;
    struct stat stbuf;

    path_utf8 = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(DefaultPathToMp3)))));
    if (!path_utf8 || g_utf8_strlen(path_utf8, -1) < 1)
    {
        g_free(path_utf8);
        return TRUE;
    }

    path_real = filename_from_display(path_utf8);

#ifdef G_OS_WIN32
    /* On win32 : stat("c:\path\to\dir") succeed, while stat("c:\path\to\dir\")
     * fails.
     */
    ET_Win32_Path_Remove_Trailing_Backslash(path_real);
#endif /* G_OS_WIN32 */

    if ( stat(path_real,&stbuf)==0 && S_ISDIR(stbuf.st_mode) )
    {
        g_free(path_real);
        g_free(path_utf8);
        return TRUE; /* Path is good */
    }else
    {
        GtkWidget *msgdialog = gtk_message_dialog_new(GTK_WINDOW(OptionsWindow),
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      GTK_MESSAGE_ERROR,
                                                      GTK_BUTTONS_CLOSE,
                                                      "%s",
                                                      _("The selected path for 'Default path to files' is invalid"));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),_("Path: '%s'\nError: %s"),path_utf8,g_strerror(errno));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Invalid Path Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        g_free(path_real);
        g_free(path_utf8);
        return FALSE;
    }
}

/*
 * The character set conversion is used for ID3 tag. UTF-8 is used to display.
 *  - reading_character is converted to UTF-8
 *  - writing_character is converted from UTF-8
 */
/*****************
gint Check_CharacterSetTranslation (void)
{
    gchar *temp;
    gchar *reading_character;
    gchar *writing_character;

    temp = Get_Active_Combo_Box_Item(GTK_COMBO_BOX(FileReadingCharacterSetCombo));
    reading_character = Charset_Get_Name_From_Title(temp);
    g_free(temp);

    temp = Get_Active_Combo_Box_Item(GTK_COMBO_BOX(FileWritingCharacterSetCombo));
    writing_character = Charset_Get_Name_From_Title(temp);
    g_free(temp);

    // Check conversion when reading file
    if ( GTK_TOGGLE_BUTTON(UseNonStandardId3ReadingCharacterSet)->active
    && (test_conversion_charset(reading_character,"UTF-8")!=TRUE) )
    {
        gchar *msg = g_strdup_printf(_("The character set translation from '%s'\n"
                                       "to '%s' is not supported"),reading_character,"UTF-8");
        GtkWidget *msgbox = msg_box_new(_("Error"),
                                        GTK_WINDOW(OptionsWindow),
                                        NULL,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        msg,
                                        GTK_STOCK_DIALOG_ERROR,
                                        GTK_STOCK_OK, GTK_RESPONSE_OK,
                                        NULL);
        gtk_dialog_run(GTK_DIALOG(msgbox));
        gtk_widget_destroy(msgbox);
        g_free(msg);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(UseNonStandardId3ReadingCharacterSet),FALSE);
        return 0;
    }
    // Check conversion when writing file
    if ( GTK_TOGGLE_BUTTON(UseNonStandardId3WritingCharacterSet)->active
    && (test_conversion_charset("UTF-8",writing_character)!=TRUE) )
    {
        gchar *msg = g_strdup_printf(_("The character set translation from '%s'\n"
                                       "to '%s' is not supported"),"UTF-8",writing_character);
        GtkWidget *msgbox = msg_box_new(_("Error"),
                                        GTK_WINDOW(OptionsWindow),
                                        NULL,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        msg,
                                        GTK_STOCK_DIALOG_ERROR,
                                        GTK_STOCK_OK, GTK_RESPONSE_OK,
                                        NULL);
        gtk_dialog_run(GTK_DIALOG(msgbox));
        gtk_widget_destroy(msgbox);
        g_free(msg);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(UseNonStandardId3WritingCharacterSet),FALSE);
        return 0;
    }

    return 1;
}
*************/

static gboolean
Check_DefaultComment (void)
{
    const gchar *file;

    file = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(DefaultComment))));
    if (!file || g_utf8_strlen(file, -1) < 1)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(SetDefaultComment),FALSE);

    return TRUE;    /* A blank entry is ignored */
}

/*
 * Check if player binary is found
 */
static gboolean
Check_FilePlayerCombo (void)
{
    gchar *program_path = NULL;
    gchar *program_path_validated = NULL;

#ifdef G_OS_WIN32
    return TRUE; /* FIXME see Check_If_Executable_Exists */
    /* Note : Check_If_Executable_Exists crashes when player is 'winamp.exe' with g_find_program_in_path */
#endif /* G_OS_WIN32 */

    // The program typed
    program_path = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(FilePlayerCombo)))));
    g_strstrip(program_path);
    // The program file validated
    if (program_path && strlen(program_path)>0)
        program_path_validated = Check_If_Executable_Exists(program_path);

    if ( program_path && strlen(program_path)>0 && !program_path_validated ) // A file is typed but it is invalid!
    {
        GtkWidget *msgdialog = gtk_message_dialog_new(GTK_WINDOW(OptionsWindow),
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      GTK_MESSAGE_ERROR,
                                                      GTK_BUTTONS_CLOSE,
                                                      _("The audio file player '%s' cannot be found"),
                                                      program_path);
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Audio Player Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);

        g_free(program_path);
        return FALSE;
    } else
    {
        g_free(program_path);
        g_free(program_path_validated);
        return TRUE;
    }
}

static gboolean
Check_Config (void)
{
    if (Check_DefaultPathToMp3 ()
        && Check_DefaultComment ()
        && Check_FilePlayerCombo ())
        return TRUE; /* No problem detected */
    else
        return FALSE; /* Oops! */
}



/*
 * Manage Check buttons into Scanner tab: conversion group
 * This reproduces "something" like the behaviour of radio buttons with check buttons
 */
static void
Scanner_Convert_Check_Button_Toggled_1 (GtkWidget *object_rec,
                                        GtkWidget *object_emi)
{
    g_return_if_fail (object_rec != NULL || object_emi != NULL);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(object_emi)) == TRUE)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(object_rec),!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(object_emi)));
}

static void
DefaultPathToMp3_Combo_Add_String (void)
{
    const gchar *path;

    path = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(DefaultPathToMp3))));
    Add_String_To_Combo_List(GTK_LIST_STORE(DefaultPathModel), path);
}

static void
CddbLocalPath_Combo_Add_String (void)
{
    const gchar *path;

    path = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(CddbLocalPath))));
    Add_String_To_Combo_List(GTK_LIST_STORE(CddbLocalPath), path);
}
