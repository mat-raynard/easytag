/* easytag.c - 2000/04/28 */
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


#include "config.h" // For definition of ENABLE_OGG
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#ifdef ENABLE_MP3
#include <id3tag.h>
#endif
#if defined ENABLE_MP3 && defined ENABLE_ID3LIB
#include <id3.h>
#endif
#include <sys/types.h>
#include <utime.h>

#include "gtk2_compat.h"
#include "easytag.h"
#include "application.h"
#include "browser.h"
#include "log.h"
#include "misc.h"
#include "bar.h"
#include "prefs.h"
#include "setting.h"
#include "scan.h"
#include "mpeg_header.h"
#include "id3_tag.h"
#include "ogg_tag.h"
#include "et_core.h"
#include "cddb.h"
#include "picture.h"
#include "charset.h"

#ifdef G_OS_WIN32
#include "win32/win32dep.h"
#else /* !G_OS_WIN32 */
#include <sys/wait.h>
#endif /* !G_OS_WIN32 */


/****************
 * Declarations *
 ****************/
static guint idle_handler_id;

static GtkWidget *QuitRecursionWindow = NULL;

/* Used to force to hide the msgbox when saving tag */
static gboolean SF_HideMsgbox_Write_Tag;
/* To remember which button was pressed when saving tag */
static gint SF_ButtonPressed_Write_Tag;
/* Used to force to hide the msgbox when renaming file */
static gboolean SF_HideMsgbox_Rename_File;
/* To remember which button was pressed when renaming file */
static gint SF_ButtonPressed_Rename_File;
/* Used to force to hide the msgbox when deleting file */
static gboolean SF_HideMsgbox_Delete_File;
/* To remember which button was pressed when deleting file */
static gint SF_ButtonPressed_Delete_File;

#ifdef ENABLE_FLAC
    #include <FLAC/metadata.h>
#endif


/**************
 * Prototypes *
 **************/
#ifdef G_OS_WIN32
int easytag_main (struct HINSTANCE__ *hInstance, int argc, char *argv[]);
#endif /* G_OS_WIN32 */
#ifndef G_OS_WIN32
static void Handle_Crash (gint signal_id);
static const gchar *signal_to_string (gint signal);
#endif /* !G_OS_WIN32 */

static GtkWidget *Create_Browser_Area (void);
static GtkWidget *Create_File_Area    (void);
static GtkWidget *Create_Tag_Area     (void);

static void Mini_Button_Clicked (GObject *object);
static void Disable_Command_Buttons (void);

static gboolean Make_Dir (const gchar *dirname_old, const gchar *dirname_new);
static gboolean Remove_Dir (const gchar *dirname_old,
                            const gchar *dirname_new);
static gboolean Write_File_Tag (ET_File *ETFile, gboolean hide_msgbox);
static gboolean Rename_File (ET_File *ETFile, gboolean hide_msgbox);
static gint Save_File (ET_File *ETFile, gboolean multiple_files,
                       gboolean force_saving_files);
static gint Delete_File (ET_File *ETFile, gboolean multiple_files);
static gint Save_Selected_Files_With_Answer (gboolean force_saving_files);
static gint Save_List_Of_Files (GList *etfilelist,
                                gboolean force_saving_files);
static gint Delete_Selected_Files_With_Answer (void);
static gboolean Copy_File (const gchar *fileold, const gchar *filenew);

static void Init_Load_Default_Dir (void);
static void EasyTAG_Exit (void);

static GList *Read_Directory_Recursively (GList *file_list, const gchar *path,
                                          gboolean recurse);
static void Open_Quit_Recursion_Function_Window (void);
static void Destroy_Quit_Recursion_Function_Window (void);
static void Quit_Recursion_Function_Button_Pressed (void);
static void Quit_Recursion_Window_Key_Press (GtkWidget *window,
                                             GdkEvent *event);
static void File_Area_Set_Sensitive (gboolean activate);
static void Tag_Area_Set_Sensitive  (gboolean activate);

#ifndef G_OS_WIN32
static void
setup_sigbus_fpe_segv (void)
{
    struct sigaction sa;
    memset (&sa, 0, sizeof (struct sigaction));
    sa.sa_handler = Handle_Crash;
    sigaction (SIGBUS, &sa, NULL);
    sigaction (SIGFPE, &sa, NULL);
    sigaction (SIGSEGV, &sa, NULL);
}

static void
sigchld_handler (int signum)
{
    wait (NULL);
}

static void
setup_sigchld (void)
{
    struct sigaction sa;
    memset (&sa, 0, sizeof (struct sigaction));
    sa.sa_handler = sigchld_handler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction (SIGCHLD, &sa, NULL);
}
#endif /* !G_OS_WIN32 */

/*
 * command_line:
 * @application: the application
 * @command_line: the command line to process
 * @user_data: user data set when the signal handler was connected
 *
 * Handle the command-line arguments passed to the primary instance. The local
 * instance arguments are handled in EtApplication.
 *
 * Returns: the exit status to be passed to the calling process
 */
static gint
command_line (GApplication *application,
              GApplicationCommandLine *command_line, gpointer user_data)
{
    gchar **argv;
    gint argc;

    argv = g_application_command_line_get_arguments (command_line, &argc);

    /* Check given arguments */
    if (argc > 1)
    {
        /* TODO: Replace this mess with GFile. */
        struct stat statbuf;
        gchar *path2check = NULL, *path2check_tmp = NULL;
        gint resultstat;
        gchar **pathsplit;
        gint ps_index = 0;

        /* Check if relative or absolute path */
        if (g_path_is_absolute(argv[1]))
        {
            path2check = g_strdup(argv[1]);
        }
        else
        {
            gchar *curdir = g_get_current_dir();
            path2check = g_strconcat(g_get_current_dir(),G_DIR_SEPARATOR_S,argv[1],NULL);
            g_free(curdir);
        }

#ifdef G_OS_WIN32
        ET_Win32_Path_Replace_Slashes(path2check);
#endif /* G_OS_WIN32 */

        /* Check if contains hidden directories. */
        pathsplit = g_strsplit(path2check,G_DIR_SEPARATOR_S,0);
        g_free(path2check);
        path2check = NULL;

        /* Browse the list to build again the path. */
        /* FIXME: Should manage directory ".." in path. */
        while (pathsplit[ps_index])
        {
            /* Activate hidden directories in browser if path contains a
             * dir like ".hidden_dir". */
            if ((g_ascii_strcasecmp (pathsplit[ps_index], "..") != 0)
                && (g_ascii_strncasecmp (pathsplit[ps_index], ".", 1) == 0)
                && (strlen (pathsplit[ps_index]) > 1))
            {
                g_settings_set_boolean (ETSettings, "browse-show-hidden",
                                        TRUE);
            }

            if (pathsplit[ps_index]
            && g_ascii_strcasecmp(pathsplit[ps_index],".") != 0
            && g_ascii_strcasecmp(pathsplit[ps_index],"")  != 0)
            {
                if (path2check)
                {
                    path2check_tmp = g_strconcat(path2check,G_DIR_SEPARATOR_S,pathsplit[ps_index],NULL);
                }else
                {
#ifdef G_OS_WIN32
                    /* Build a path starting with the drive letter. */
                    path2check_tmp = g_strdup(pathsplit[ps_index]);
#else /* !G_OS_WIN32 */
                    path2check_tmp = g_strconcat(G_DIR_SEPARATOR_S,pathsplit[ps_index],NULL);
#endif /* !G_OS_WIN32 */

                }

                path2check = g_strdup(path2check_tmp);
                g_free(path2check_tmp);
            }
            ps_index++;
        }

        g_strfreev (pathsplit);

        /* Check if file or directory. */
        resultstat = stat(path2check,&statbuf);
        if (resultstat==0 && S_ISDIR(statbuf.st_mode))
        {
            INIT_DIRECTORY = g_strdup(path2check);
        }else if (resultstat==0 && S_ISREG(statbuf.st_mode))
        {
            /* When passing a file, we load only the directory. */
            INIT_DIRECTORY = g_path_get_dirname(path2check);
        }else
        {
            g_application_command_line_printerr (command_line,
                                                 _("Unknown parameter or path '%s'\n"),
                                                 argv[1]);
            g_free (path2check);
            g_strfreev (argv);
            return 1;
        }
        g_free(path2check);
    }

    /* Initialize GTK. */
    if (et_application_get_window (ET_APPLICATION (application)) == NULL)
    {
        gtk_init (&argc, &argv);
    }

    g_strfreev (argv);

    g_application_activate (application);

    return 0;
}

static void
activate (GApplication *application, gpointer user_data)
{
    GtkWindow *main_window;
    GtkWidget *MainVBox;
    GtkWidget *HBox, *VBox;

    main_window = et_application_get_window (ET_APPLICATION (application));
    if (main_window != NULL)
    {
        gtk_window_present (main_window);
        return;
    }

    Charset_Insert_Locales_Init();

    /* Starting messages */
    Log_Print(LOG_OK,_("Starting EasyTAG version %s (PID: %d)…"),PACKAGE_VERSION,getpid());
#ifdef ENABLE_MP3
    Log_Print(LOG_OK,_("Using libid3tag version %s"), ID3_VERSION);
#endif
#if defined ENABLE_MP3 && defined ENABLE_ID3LIB
    Log_Print (LOG_OK, _("Using id3lib version %d.%d.%d"), ID3LIB_MAJOR_VERSION,
               ID3LIB_MINOR_VERSION, ID3LIB_PATCH_VERSION);
#endif

#ifdef G_OS_WIN32
    if (g_getenv("EASYTAGLANG"))
        Log_Print(LOG_OK,_("Variable EASYTAGLANG defined. Setting locale: '%s'"),g_getenv("EASYTAGLANG"));
    else
        Log_Print(LOG_OK,_("Setting locale: '%s'"),g_getenv("LANG"));
#endif /* G_OS_WIN32 */

    if (get_locale())
        Log_Print (LOG_OK,
                   _("Currently using locale '%s' (and eventually '%s')"),
                   get_locale (), get_encoding_from_locale (get_locale ()));


    /* Create all config files. */
    if (!Setting_Create_Files())
    {
        Log_Print (LOG_WARNING, _("Unable to create setting directories"));
    }

    /* Load Config */
    Init_Config_Variables();
    Read_Config();
    /* Display_Config(); // <- for debugging */


    /* Initialization */
    ET_Core_Create();
    Main_Stop_Button_Pressed = FALSE;
    Init_Custom_Icons();
    Init_Mouse_Cursor();
    Init_OptionsWindow();
    Init_ScannerWindow();
    Init_CddbWindow();
    BrowserEntryModel    = NULL;
    TrackEntryComboModel = NULL;
    GenreComboModel      = NULL;

    /* The main window */
    MainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    et_application_set_window (ET_APPLICATION (application),
                               GTK_WINDOW (MainWindow));
    gtk_window_set_title (GTK_WINDOW (MainWindow),
                          PACKAGE_NAME " " PACKAGE_VERSION);
    // This part is needed to set correctly the position of handle panes
    gtk_window_set_default_size(GTK_WINDOW(MainWindow),MAIN_WINDOW_WIDTH,MAIN_WINDOW_HEIGHT);

    g_signal_connect(G_OBJECT(MainWindow),"delete_event",G_CALLBACK(Quit_MainWindow),NULL);
    g_signal_connect(G_OBJECT(MainWindow),"destroy",G_CALLBACK(Quit_MainWindow),NULL);

    /* Minimised window icon */
    gtk_widget_realize(MainWindow);

    gtk_window_set_icon_name (GTK_WINDOW (MainWindow), PACKAGE_TARNAME);

    /* MainVBox for Menu bar + Tool bar + "Browser Area & FileArea & TagArea" + Log Area + "Status bar & Progress bar" */
    MainVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_container_add (GTK_CONTAINER(MainWindow),MainVBox);
    gtk_widget_show(MainVBox);

    /* Menu bar and tool bar */
    Create_UI(&MenuArea, &ToolArea);
    gtk_box_pack_start(GTK_BOX(MainVBox),MenuArea,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(MainVBox),ToolArea,FALSE,FALSE,0);


    /* The two panes: BrowserArea on the left, FileArea+TagArea on the right */
    MainWindowHPaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    //gtk_box_pack_start(GTK_BOX(MainVBox),MainWindowHPaned,TRUE,TRUE,0);
    gtk_paned_set_position(GTK_PANED(MainWindowHPaned),PANE_HANDLE_POSITION1);
    gtk_widget_show(MainWindowHPaned);

    /* Browser (Tree + File list + Entry) */
    BrowseArea = Create_Browser_Area();
    gtk_paned_pack1(GTK_PANED(MainWindowHPaned),BrowseArea,TRUE,TRUE);

    /* Vertical box for FileArea + TagArea */
    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_paned_pack2(GTK_PANED(MainWindowHPaned),VBox,FALSE,FALSE);
    gtk_widget_show(VBox);

    /* File */
    FileArea = Create_File_Area();
    gtk_box_pack_start(GTK_BOX(VBox),FileArea,FALSE,FALSE,0);

    /* Tag */
    TagArea = Create_Tag_Area();
    gtk_box_pack_start(GTK_BOX(VBox),TagArea,FALSE,FALSE,0);

    /* Vertical pane for Browser Area + FileArea + TagArea */
    MainWindowVPaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(MainVBox),MainWindowVPaned,TRUE,TRUE,0);
    gtk_paned_pack1(GTK_PANED(MainWindowVPaned),MainWindowHPaned,TRUE,FALSE);
    gtk_paned_set_position(GTK_PANED(MainWindowVPaned),PANE_HANDLE_POSITION4);
    gtk_widget_show(MainWindowVPaned);


    /* Log */
    LogArea = Create_Log_Area();
    gtk_paned_pack2(GTK_PANED(MainWindowVPaned),LogArea,FALSE,TRUE);

    /* Horizontal box for Status bar + Progress bar */
    HBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_box_pack_start(GTK_BOX(MainVBox),HBox,FALSE,FALSE,0);
    gtk_widget_show(HBox);

    /* Status bar */
    StatusArea = Create_Status_Bar();
    gtk_box_pack_start(GTK_BOX(HBox),StatusArea,TRUE,TRUE,0);

    /* Progress bar */
    ProgressArea = Create_Progress_Bar();
    gtk_box_pack_end(GTK_BOX(HBox),ProgressArea,FALSE,FALSE,0);

    gtk_widget_show(MainWindow);

    if (g_settings_get_boolean (ETSettings, "remember-location"))
        gtk_window_move(GTK_WINDOW(MainWindow), MAIN_WINDOW_X, MAIN_WINDOW_Y);

    /* Load the default dir when the UI is created and displayed
     * to the screen and open also the scanner window */
    idle_handler_id = g_idle_add((GSourceFunc)Init_Load_Default_Dir,NULL);

    /* Enter the event loop */
    gtk_main ();
}

/********
 * Main *
 ********/
int main (int argc, char *argv[])
{
    EtApplication *application;
    gint status;

    /* FIXME: Move remaining initialisation code into EtApplication. */
#ifdef G_OS_WIN32
    weasytag_init();
    /* ET_Win32_Init(hInstance); */
#else /* !G_OS_WIN32 */
    /* Signal handling to display a message(SIGSEGV, ...) */
    setup_sigbus_fpe_segv ();
    /* Must handle this signal to avoid zombies of child processes (e.g. xmms)
     */
    setup_sigchld ();
#endif /* !G_OS_WIN32 */

    INIT_DIRECTORY = NULL;

    application = et_application_new ();
    g_signal_connect (application, "command-line", G_CALLBACK (command_line),
                      NULL);
    g_signal_connect (application, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (application), argc, argv);
    g_object_unref (application);

    return status;
}


static GtkWidget *
Create_Browser_Area (void)
{
    GtkWidget *Frame;
    GtkWidget *Tree;

    Frame = gtk_frame_new(_("Browser"));
    gtk_container_set_border_width(GTK_CONTAINER(Frame), 2);

    Tree = Create_Browser_Items(MainWindow);
    gtk_container_add(GTK_CONTAINER(Frame),Tree);

    /* Don't load init dir here because Tag area hasn't been yet created!.
     * It will be load at the end of the main function */
    //Browser_Tree_Select_Dir(DEFAULT_PATH_TO_MP3);

    gtk_widget_show(Frame);
    return Frame;
}


static GtkWidget *
Create_File_Area (void)
{
    GtkWidget *VBox, *HBox;
    GtkWidget *Separator;


    FileFrame = gtk_frame_new(_("File"));
    gtk_container_set_border_width(GTK_CONTAINER(FileFrame),2);

    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_container_add(GTK_CONTAINER(FileFrame),VBox);
    gtk_container_set_border_width(GTK_CONTAINER(VBox),2);

    /* HBox for FileEntry and IconBox */
    HBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);

    /* File index (position in list + list length) */
    FileIndex = gtk_label_new("0/0:");
    gtk_box_pack_start(GTK_BOX(HBox),FileIndex,FALSE,FALSE,0);

    /* Filename. */
    FileEntry = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(FileEntry), TRUE);
    gtk_box_pack_start(GTK_BOX(HBox),FileEntry,TRUE,TRUE,2);

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(FileEntry));


    /*
     *  File Infos
     */
    HeaderInfosTable = et_grid_new(3,5);
    gtk_container_add(GTK_CONTAINER(VBox),HeaderInfosTable);
    gtk_container_set_border_width(GTK_CONTAINER(HeaderInfosTable),2);
    gtk_grid_set_row_spacing (GTK_GRID (HeaderInfosTable), 1);
    gtk_grid_set_column_spacing (GTK_GRID (HeaderInfosTable), 2);

    VersionLabel = gtk_label_new(_("Encoder:"));
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), VersionLabel, 0, 0, 1, 1);
    VersionValueLabel = gtk_label_new("");
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), VersionValueLabel, 1, 0, 1,
                     1);
    gtk_misc_set_alignment(GTK_MISC(VersionLabel),1,0.5);
    gtk_misc_set_alignment(GTK_MISC(VersionValueLabel),0,0.5);

    BitrateLabel = gtk_label_new(_("Bitrate:"));
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), BitrateLabel, 0, 1, 1, 1);
    BitrateValueLabel = gtk_label_new("");
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), BitrateValueLabel, 1, 1, 1,
                     1);
    gtk_misc_set_alignment(GTK_MISC(BitrateLabel),1,0.5);
    gtk_misc_set_alignment(GTK_MISC(BitrateValueLabel),0,0.5);

    /* Translators: Please try to keep this string as short as possible as it
     * is shown in a narrow column. */
    SampleRateLabel = gtk_label_new(_("Freq.:"));
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), SampleRateLabel, 0, 2, 1, 1);
    SampleRateValueLabel = gtk_label_new("");
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), SampleRateValueLabel, 1, 2,
                     1, 1);
    gtk_misc_set_alignment(GTK_MISC(SampleRateLabel),1,0.5);
    gtk_misc_set_alignment(GTK_MISC(SampleRateValueLabel),0,0.5);

    Separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), Separator, 2, 0, 1, 4);

    ModeLabel = gtk_label_new(_("Mode:"));
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), ModeLabel, 3, 0, 1, 1);
    ModeValueLabel = gtk_label_new("");
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), ModeValueLabel, 4, 0, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(ModeLabel),1,0.5);
    gtk_misc_set_alignment(GTK_MISC(ModeValueLabel),0,0.5);

    SizeLabel = gtk_label_new(_("Size:"));
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), SizeLabel, 3, 1, 1, 1);
    SizeValueLabel = gtk_label_new("");
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), SizeValueLabel, 4, 1, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(SizeLabel),1,0.5);
    gtk_misc_set_alignment(GTK_MISC(SizeValueLabel),0,0.5);

    DurationLabel = gtk_label_new(_("Duration:"));
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), DurationLabel, 3, 2, 1, 1);
    DurationValueLabel = gtk_label_new("");
    gtk_grid_attach (GTK_GRID (HeaderInfosTable), DurationValueLabel, 4, 2, 1,
                     1);
    gtk_misc_set_alignment(GTK_MISC(DurationLabel),1,0.5);
    gtk_misc_set_alignment(GTK_MISC(DurationValueLabel),0,0.5);

    gtk_widget_show(FileFrame);
    gtk_widget_show(VBox);
    gtk_widget_show(HBox);
    gtk_widget_show(FileIndex);
    gtk_widget_show(FileEntry);
    gtk_widget_show_all(HeaderInfosTable);
    g_settings_bind (ETSettings, "file-show-header", HeaderInfosTable,
                     "visible", G_SETTINGS_BIND_GET);
    return FileFrame;
}

#include "data/pixmaps/sequence_track.xpm"
static GtkWidget *
Create_Tag_Area (void)
{
    GtkWidget *Separator;
    GtkWidget *Table;
    GtkWidget *Label;
    GtkWidget *Icon;
    GtkWidget *toolbar;
    GtkToolItem *toolitem;
    GIcon *icon;
    GtkWidget *image;
    GtkWidget *VBox;
    GList *focusable_widgets_list = NULL;
    //GtkWidget *ScrollWindow;
    //GtkTextBuffer *TextBuffer;
    GtkEntryCompletion *completion;
    gint MButtonSize = 13;
    gint TablePadding = 2;

    // For Picture
    static const GtkTargetEntry drops[] = { { "text/uri-list", 0, TARGET_URI_LIST } };
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;


    /* Main Frame */
    TagFrame = gtk_frame_new(_("Tag"));
    gtk_container_set_border_width(GTK_CONTAINER(TagFrame),2);

    /* Box for the notebook (only for setting a border) */
    VBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_container_add(GTK_CONTAINER(TagFrame),VBox);
    gtk_container_set_border_width(GTK_CONTAINER(VBox),2);

    /*
     * Note book
     */
    TagNoteBook = gtk_notebook_new();
    gtk_notebook_popup_enable(GTK_NOTEBOOK(TagNoteBook));
    //gtk_container_add(GTK_CONTAINER(TagFrame),TagNoteBook);
    gtk_box_pack_start(GTK_BOX(VBox),TagNoteBook,TRUE,TRUE,0);
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(TagNoteBook),GTK_POS_TOP);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(TagNoteBook),FALSE);
    gtk_notebook_popup_enable(GTK_NOTEBOOK(TagNoteBook));

    /*
     * 1 - Page for common tag fields
     */
    Label = gtk_label_new(_("Common"));

    Table = et_grid_new (11, 11);
    gtk_notebook_append_page (GTK_NOTEBOOK (TagNoteBook), Table, Label);
    gtk_container_set_border_width(GTK_CONTAINER(Table),2);

    /* Title */
    TitleLabel = gtk_label_new(_("Title:"));
    et_grid_attach_full (GTK_GRID (Table), TitleLabel, 0, 0, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(TitleLabel),1,0.5);

    TitleEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (TitleEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), TitleEntry, 1, 0, 9, 1, TRUE, TRUE,
                         TablePadding, TablePadding);

    g_signal_connect (G_OBJECT (TitleEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (TitleEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this title"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(TitleEntry));

    /* Artist */
    ArtistLabel = gtk_label_new(_("Artist:"));
    et_grid_attach_full (GTK_GRID (Table), ArtistLabel, 0, 1, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(ArtistLabel),1,0.5);

    ArtistEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (ArtistEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), ArtistEntry, 1, 1, 9, 1, TRUE, TRUE,
                         TablePadding,TablePadding);

    g_signal_connect (G_OBJECT (ArtistEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (ArtistEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this artist"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(ArtistEntry));

    /* Album Artist */
    AlbumArtistLabel = gtk_label_new(_("Album artist:"));
    et_grid_attach_full (GTK_GRID (Table), AlbumArtistLabel, 0, 2, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(AlbumArtistLabel),1,0.5);

    AlbumArtistEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (AlbumArtistEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), AlbumArtistEntry, 1, 2, 9, 1, TRUE,
                         TRUE, TablePadding, TablePadding);

    g_signal_connect (G_OBJECT (AlbumArtistEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (AlbumArtistEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this album artist"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(AlbumArtistEntry));

    /* Album */
    AlbumLabel = gtk_label_new(_("Album:"));
    et_grid_attach_full (GTK_GRID (Table), AlbumLabel, 0, 3, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(AlbumLabel),1,0.5);

    AlbumEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (AlbumEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), AlbumEntry, 1, 3, 6, 1, TRUE, TRUE,
                         TablePadding,TablePadding);

    g_signal_connect (G_OBJECT (AlbumEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (AlbumEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this album name"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(AlbumEntry));

    /* Disc Number */
    DiscNumberLabel = gtk_label_new(_("CD:"));
    et_grid_attach_full (GTK_GRID (Table), DiscNumberLabel, 8, 3, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(DiscNumberLabel),1,0.5);

    DiscNumberEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (DiscNumberEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), DiscNumberEntry, 9, 3, 1, 1, TRUE,
                         TRUE, TablePadding, TablePadding);
    gtk_entry_set_width_chars (GTK_ENTRY (DiscNumberEntry), 3);
    /* FIXME should allow to type only something like : 1/3. */
    /*g_signal_connect(G_OBJECT(GTK_ENTRY(DiscNumberEntry)),"insert_text",G_CALLBACK(Insert_Only_Digit),NULL); */

    g_signal_connect (G_OBJECT (DiscNumberEntry), "icon-release",
                      G_CALLBACK(Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (DiscNumberEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this disc number"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(DiscNumberEntry));

    /* Year */
    YearLabel = gtk_label_new(_("Year:"));
    et_grid_attach_full (GTK_GRID (Table), YearLabel, 0, 4, 1, 1, FALSE, FALSE,
                         TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(YearLabel),1,0.5);

    YearEntry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(YearEntry), 4);
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (YearEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), YearEntry, 1, 4, 1, 1, TRUE, TRUE,
                         TablePadding, TablePadding);
    gtk_entry_set_width_chars (GTK_ENTRY (YearEntry), 5);
    g_signal_connect (G_OBJECT (YearEntry), "insert-text",
                      G_CALLBACK (Insert_Only_Digit), NULL);
    g_signal_connect(G_OBJECT(YearEntry),"activate",G_CALLBACK(Parse_Date),NULL);
    g_signal_connect(G_OBJECT(YearEntry),"focus-out-event",G_CALLBACK(Parse_Date),NULL);

    g_signal_connect (G_OBJECT (YearEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked),NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (YearEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this year"));

    /* Small vertical separator */
    Separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    et_grid_attach_full (GTK_GRID (Table), Separator, 3, 4, 1, 1, FALSE, FALSE,
                         TablePadding,TablePadding);


    /* Track and Track total */
    TrackMButtonSequence = gtk_button_new();
    gtk_widget_set_size_request(TrackMButtonSequence,MButtonSize,MButtonSize);
    et_grid_attach_full (GTK_GRID (Table), TrackMButtonSequence, 4, 4, 1, 1,
                         FALSE, FALSE, TablePadding, TablePadding);
    g_signal_connect(G_OBJECT(TrackMButtonSequence),"clicked",G_CALLBACK(Mini_Button_Clicked),NULL);
    gtk_widget_set_tooltip_text(TrackMButtonSequence,_("Number selected tracks sequentially. "
                                                     "Starts at 01 in each subdirectory."));
    // Pixmap into TrackMButtonSequence button
    //Icon = gtk_image_new_from_stock("easytag-sequence-track", GTK_ICON_SIZE_BUTTON); // FIX ME : it doesn't display the good size of the '#'
    Icon = Create_Xpm_Image((const char **)sequence_track_xpm);
    gtk_container_add(GTK_CONTAINER(TrackMButtonSequence),Icon);
    gtk_widget_set_can_default(TrackMButtonSequence,TRUE); // To have enough space to display the icon
    gtk_widget_set_can_focus(TrackMButtonSequence,FALSE);   // To have enough space to display the icon

    TrackLabel = gtk_label_new(_("Track #:"));
    et_grid_attach_full (GTK_GRID (Table), TrackLabel, 5, 4, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(TrackLabel),1,0.5);

    if (TrackEntryComboModel != NULL)
        gtk_list_store_clear(TrackEntryComboModel);
    else
        TrackEntryComboModel = gtk_list_store_new(MISC_COMBO_COUNT, G_TYPE_STRING);

    TrackEntryCombo = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(TrackEntryComboModel));
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(TrackEntryCombo),MISC_COMBO_TEXT);
    et_grid_attach_full (GTK_GRID (Table), TrackEntryCombo, 6, 4, 1, 1, TRUE,
                         TRUE, TablePadding, TablePadding);
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(TrackEntryCombo),3); // Three columns to display track numbers list

    gtk_entry_set_width_chars (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (TrackEntryCombo))),
                               2);
    g_signal_connect(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(TrackEntryCombo)))),"insert_text",
        G_CALLBACK(Insert_Only_Digit),NULL);

    Label = gtk_label_new("/");
    et_grid_attach_full (GTK_GRID (Table), Label, 7, 4, 1, 1, FALSE, FALSE,
                         TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(Label),0.5,0.5);

    TrackMButtonNbrFiles = gtk_button_new();
    gtk_widget_set_size_request(TrackMButtonNbrFiles,MButtonSize,MButtonSize);
    et_grid_attach_full (GTK_GRID (Table), TrackMButtonNbrFiles, 8, 4, 1, 1,
                         FALSE, FALSE, TablePadding, TablePadding);
    g_signal_connect(G_OBJECT(TrackMButtonNbrFiles),"clicked",G_CALLBACK(Mini_Button_Clicked),NULL);
    gtk_widget_set_tooltip_text(TrackMButtonNbrFiles,_("Set the number of files, in the same directory of the displayed file, to the selected tracks."));
    // Pixmap into TrackMButtonNbrFiles button
    //Icon = gtk_image_new_from_stock("easytag-sequence-track", GTK_ICON_SIZE_BUTTON);
    Icon = Create_Xpm_Image((const char **)sequence_track_xpm);
    gtk_container_add(GTK_CONTAINER(TrackMButtonNbrFiles),Icon);
    gtk_widget_set_can_default(TrackMButtonNbrFiles,TRUE); // To have enough space to display the icon
    gtk_widget_set_can_focus(TrackMButtonNbrFiles,FALSE); // To have enough space to display the icon

    TrackTotalEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (TrackTotalEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), TrackTotalEntry, 9, 4, 1, 1, TRUE,
                         TRUE, TablePadding, TablePadding);
    gtk_entry_set_width_chars (GTK_ENTRY (TrackTotalEntry), 3);
    g_signal_connect (G_OBJECT (GTK_ENTRY (TrackTotalEntry)), "insert-text",
                      G_CALLBACK (Insert_Only_Digit), NULL);

    g_signal_connect (G_OBJECT (TrackTotalEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked) ,NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (TrackTotalEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this number of tracks"));


    /* Genre */
    GenreLabel = gtk_label_new(_("Genre:"));
    et_grid_attach_full (GTK_GRID (Table), GenreLabel, 0, 5, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(GenreLabel),1,0.5);

    if (GenreComboModel != NULL)
        gtk_list_store_clear(GenreComboModel);
    else
        GenreComboModel = gtk_list_store_new(MISC_COMBO_COUNT, G_TYPE_STRING);
    GenreCombo = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(GenreComboModel));
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(GenreCombo),MISC_COMBO_TEXT);
    completion = gtk_entry_completion_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (GenreCombo))),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    gtk_entry_set_completion(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(GenreCombo))), completion);
    g_object_unref(completion);
    gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(GenreComboModel));
    gtk_entry_completion_set_text_column(completion, 0);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(GenreComboModel), MISC_COMBO_TEXT, Combo_Alphabetic_Sort, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(GenreComboModel), MISC_COMBO_TEXT, GTK_SORT_ASCENDING);
    et_grid_attach_full (GTK_GRID (Table), GenreCombo, 1, 5, 9, 1, TRUE, TRUE,
                         TablePadding, TablePadding);
    Load_Genres_List_To_UI();
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(GenreCombo),2); // Two columns to display genres list

    g_signal_connect (G_OBJECT (gtk_bin_get_child (GTK_BIN (GenreCombo))),
                      "icon-release", G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (GenreCombo))),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this genre"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(GenreCombo))));

    /* Comment */
    CommentLabel = gtk_label_new(_("Comment:"));
    et_grid_attach_full (GTK_GRID (Table), CommentLabel, 0, 6, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(CommentLabel),1,0.5);

    CommentEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (CommentEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), CommentEntry, 1, 6, 9, 1, TRUE,
                         TRUE, TablePadding, TablePadding);

    // Use of a text view instead of an entry...
    /******ScrollWindow = gtk_scrolled_window_new(NULL,NULL);
    et_grid_attach_full(GTK_GRID(Table),ScrollWindow,1,5,9,1,FALSE,FALSE,TablePadding,TablePadding);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollWindow), GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(ScrollWindow), GTK_SHADOW_IN);
    gtk_widget_set_size_request(ScrollWindow,-1,52); // Display ~3 lines...
    TextBuffer = gtk_text_buffer_new(NULL);
    CommentView = gtk_text_view_new_with_buffer(TextBuffer);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(CommentView), GTK_WRAP_WORD); // To not display the horizontal scrollbar
    gtk_container_add(GTK_CONTAINER(ScrollWindow),CommentView);
    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(CommentView));
    *******/

    g_signal_connect (G_OBJECT (CommentEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (CommentEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this comment"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(CommentEntry));
    //Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(CommentView));


    /* Composer (name of the composers) */
    ComposerLabel = gtk_label_new(_("Composer:"));
    et_grid_attach_full (GTK_GRID (Table), ComposerLabel, 0, 7, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(ComposerLabel),1,0.5);

    ComposerEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (ComposerEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), ComposerEntry, 1, 7, 9, 1, TRUE,
                         TRUE, TablePadding, TablePadding);

    g_signal_connect (G_OBJECT (ComposerEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (ComposerEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this composer"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(ComposerEntry));


    /* Translators: Original Artist / Performer. Please try to keep this string
     * as short as possible, as it must fit into a narrow column. */
    OrigArtistLabel = gtk_label_new(_("Orig. artist:"));
    et_grid_attach_full (GTK_GRID (Table), OrigArtistLabel, 0, 8, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(OrigArtistLabel),1,0.5);

    OrigArtistEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (OrigArtistEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), OrigArtistEntry, 1, 8, 9, 1, TRUE,
                         TRUE,TablePadding,TablePadding);

    g_signal_connect (G_OBJECT (OrigArtistEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (OrigArtistEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this original artist"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(OrigArtistEntry));


    /* Copyright */
    CopyrightLabel = gtk_label_new(_("Copyright:"));
    et_grid_attach_full (GTK_GRID (Table), CopyrightLabel, 0, 9, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(CopyrightLabel),1,0.5);

    CopyrightEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (CopyrightEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), CopyrightEntry, 1, 9, 9, 1, TRUE,
                         TRUE, TablePadding, TablePadding);

    g_signal_connect (G_OBJECT (CopyrightEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (CopyrightEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this copyright"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(CopyrightEntry));


    /* URL */
    URLLabel = gtk_label_new(_("URL:"));
    et_grid_attach_full (GTK_GRID (Table), URLLabel, 0, 10, 1, 1, FALSE, FALSE,
                         TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(URLLabel),1,0.5);

    URLEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (URLEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), URLEntry, 1, 10, 9, 1, TRUE, TRUE,
                         TablePadding, TablePadding);

    g_signal_connect (G_OBJECT (URLEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (URLEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this URL"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(URLEntry));


    /* Encoded by */
    EncodedByLabel = gtk_label_new(_("Encoded by:"));
    et_grid_attach_full (GTK_GRID (Table), EncodedByLabel, 0, 11, 1, 1, FALSE,
                         FALSE, TablePadding, TablePadding);
    gtk_misc_set_alignment(GTK_MISC(EncodedByLabel),1,0.5);

    EncodedByEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (EncodedByEntry),
                                       GTK_ENTRY_ICON_SECONDARY, "insert-text");
    et_grid_attach_full (GTK_GRID (Table), EncodedByEntry, 1, 11, 9, 1, TRUE,
                         TRUE, TablePadding, TablePadding);

    g_signal_connect (G_OBJECT (EncodedByEntry), "icon-release",
                      G_CALLBACK (Mini_Button_Clicked), NULL);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (EncodedByEntry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Tag selected files with this encoder name"));

    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(EncodedByEntry));


    // Managing of entries when pressing the Enter key
    g_signal_connect_swapped(G_OBJECT(TitleEntry),      "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(ArtistEntry));
    g_signal_connect_swapped(G_OBJECT(ArtistEntry),     "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(AlbumEntry));
    g_signal_connect_swapped(G_OBJECT(AlbumEntry),      "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(DiscNumberEntry));
    g_signal_connect_swapped(G_OBJECT(DiscNumberEntry), "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(YearEntry));
    g_signal_connect_swapped(G_OBJECT(YearEntry),       "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(TrackEntryCombo)))));
    g_signal_connect_swapped(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(TrackEntryCombo)))),"activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(TrackTotalEntry));
    g_signal_connect_swapped(G_OBJECT(TrackTotalEntry), "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(gtk_bin_get_child(GTK_BIN(GenreCombo))));
    g_signal_connect_swapped(G_OBJECT(gtk_bin_get_child(GTK_BIN(GenreCombo))),"activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(CommentEntry));
    g_signal_connect_swapped(G_OBJECT(CommentEntry),    "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(ComposerEntry));
    g_signal_connect_swapped(G_OBJECT(ComposerEntry),   "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(OrigArtistEntry));
    g_signal_connect_swapped(G_OBJECT(OrigArtistEntry), "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(CopyrightEntry));
    g_signal_connect_swapped(G_OBJECT(CopyrightEntry),  "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(URLEntry));
    g_signal_connect_swapped(G_OBJECT(URLEntry),        "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(EncodedByEntry));
    g_signal_connect_swapped(G_OBJECT(EncodedByEntry),  "activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(AlbumArtistEntry));
    g_signal_connect_swapped(G_OBJECT(AlbumArtistEntry),"activate",G_CALLBACK(gtk_widget_grab_focus),G_OBJECT(TitleEntry));

    // Set focus chain
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,TitleEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,ArtistEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,AlbumArtistEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,AlbumEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,DiscNumberEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,YearEntry);
    //focusable_widgets_list = g_list_prepend(focusable_widgets_list,TrackMButtonSequence); // Doesn't work as focus disabled for this widget to have enought space to display icon
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,TrackEntryCombo);
    //focusable_widgets_list = g_list_prepend(focusable_widgets_list,TrackMButtonNbrFiles);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,TrackTotalEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,GenreCombo);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,CommentEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,ComposerEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,OrigArtistEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,CopyrightEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,URLEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,EncodedByEntry);
    focusable_widgets_list = g_list_prepend(focusable_widgets_list,TitleEntry); // To loop to the beginning
    /* More efficient than using g_list_append(), which must traverse the
     * whole list. */
    focusable_widgets_list = g_list_reverse(focusable_widgets_list);
    gtk_container_set_focus_chain(GTK_CONTAINER(Table),focusable_widgets_list);



    /*
     * 2 - Page for extra tag fields
     */
    Label = gtk_label_new (_("Images")); // As there is only the picture field... - also used in ET_Display_File_Tag_To_UI

    Table = et_grid_new (1, 2);
    gtk_notebook_append_page (GTK_NOTEBOOK (TagNoteBook), Table, Label);
    gtk_container_set_border_width(GTK_CONTAINER(Table),2);

    // Scroll window for PictureEntryView
    PictureScrollWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(PictureScrollWindow),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    et_grid_attach_full (GTK_GRID (Table), PictureScrollWindow, 0, 0, 1, 1,
                         TRUE, TRUE, TablePadding, TablePadding);

    PictureEntryModel = gtk_list_store_new(PICTURE_COLUMN_COUNT,
                                           GDK_TYPE_PIXBUF,
                                           G_TYPE_STRING,
                                           G_TYPE_POINTER);
    PictureEntryView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(PictureEntryModel));
    //gtk_tree_view_set_reorderable(GTK_TREE_VIEW(PictureEntryView),TRUE);
    gtk_container_add(GTK_CONTAINER(PictureScrollWindow), PictureEntryView);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(PictureEntryView), FALSE);
    gtk_widget_set_size_request(PictureEntryView, -1, 200);
    gtk_widget_set_tooltip_text (PictureEntryView,
                                 _("You can use drag and drop to add an image"));

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(PictureEntryView));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                        "pixbuf", PICTURE_COLUMN_PIC,
                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(PictureEntryView), column);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                        "text", PICTURE_COLUMN_TEXT,
                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(PictureEntryView), column);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    /* Activate Drag'n'Drop for the PictureEntryView. */
    gtk_drag_dest_set(GTK_WIDGET(PictureEntryView),
                      GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                      drops, sizeof(drops) / sizeof(GtkTargetEntry),
                      GDK_ACTION_COPY);
    g_signal_connect(G_OBJECT(PictureEntryView),"drag_data_received", G_CALLBACK(Tag_Area_Picture_Drag_Data), 0);
    g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(Picture_Selection_Changed_cb), NULL);
    g_signal_connect(G_OBJECT(PictureEntryView), "button_press_event",G_CALLBACK(Picture_Entry_View_Button_Pressed),NULL);
    g_signal_connect(G_OBJECT(PictureEntryView),"key_press_event", G_CALLBACK(Picture_Entry_View_Key_Pressed),NULL);

    /* Picture action toolbar. */
    toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_MENU);
    et_grid_attach_full (GTK_GRID (Table), toolbar, 0, 1, 1, 1, FALSE, FALSE,
                        TablePadding, TablePadding);

    /* TODO: Make the icons use the symbolic variants. */
    icon = g_themed_icon_new_with_default_fallbacks ("list-add");
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
    add_image_toolitem = gtk_tool_button_new (image, NULL);
    g_object_unref (icon);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), add_image_toolitem, -1);
    gtk_widget_set_tooltip_text (GTK_WIDGET (add_image_toolitem),
                                 _("Add images to the tag"));
    g_signal_connect (G_OBJECT (add_image_toolitem), "clicked",
                      G_CALLBACK (Picture_Add_Button_Clicked), NULL);

    /* Activate Drag'n'Drop for the add_image_toolitem. */
    gtk_drag_dest_set (GTK_WIDGET (add_image_toolitem),
                       GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                       drops, sizeof(drops) / sizeof(GtkTargetEntry),
                       GDK_ACTION_COPY);
    g_signal_connect (G_OBJECT (add_image_toolitem), "drag-data-received",
                      G_CALLBACK (Tag_Area_Picture_Drag_Data), 0);

    icon = g_themed_icon_new_with_default_fallbacks ("list-remove");
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
    remove_image_toolitem = gtk_tool_button_new (image, NULL);
    g_object_unref (icon);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), remove_image_toolitem, -1);
    gtk_widget_set_tooltip_text (GTK_WIDGET (remove_image_toolitem),
                                 _("Remove selected images from the tag"));
    gtk_widget_set_sensitive (GTK_WIDGET (remove_image_toolitem), FALSE);
    g_signal_connect (G_OBJECT (remove_image_toolitem), "clicked",
                      G_CALLBACK (Picture_Clear_Button_Clicked), NULL);

    toolitem = gtk_separator_tool_item_new ();
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);

    icon = g_themed_icon_new_with_default_fallbacks ("document-save");
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
    save_image_toolitem = gtk_tool_button_new (image, NULL);
    g_object_unref (icon);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), save_image_toolitem, -1);
    gtk_widget_set_tooltip_text (GTK_WIDGET (save_image_toolitem),
                                 _("Save the selected images to files"));
    gtk_widget_set_sensitive (GTK_WIDGET (save_image_toolitem), FALSE);
    g_signal_connect (G_OBJECT (save_image_toolitem), "clicked",
                      G_CALLBACK (Picture_Save_Button_Clicked), NULL);

    icon = g_themed_icon_new_with_default_fallbacks ("document-properties");
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
    image_properties_toolitem = gtk_tool_button_new (image, NULL);
    g_object_unref (icon);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), image_properties_toolitem, -1);
    gtk_widget_set_tooltip_text (GTK_WIDGET (image_properties_toolitem),
                                 _("Edit image properties"));
    gtk_widget_set_sensitive (GTK_WIDGET (image_properties_toolitem), FALSE);
    g_signal_connect (G_OBJECT (image_properties_toolitem), "clicked",
                      G_CALLBACK (Picture_Properties_Button_Clicked), NULL);

    toolitem = gtk_separator_tool_item_new ();
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);

    icon = g_themed_icon_new_with_default_fallbacks ("insert-image");
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
    apply_image_toolitem = gtk_tool_button_new (image, NULL);
    g_object_unref (icon);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), apply_image_toolitem, -1);
    gtk_widget_set_tooltip_text (GTK_WIDGET (apply_image_toolitem),
                                 _("Tag selected files with these images"));
    g_signal_connect (G_OBJECT (apply_image_toolitem), "clicked",
                      G_CALLBACK (Mini_Button_Clicked), NULL);

    //Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(PictureEntryView));

    gtk_widget_show_all(TagFrame);
    return TagFrame;
}


static void
Mini_Button_Clicked (GObject *object)
{
    GList *etfilelist = NULL;
    GList *selection_filelist = NULL;
    gchar *string_to_set = NULL;
    gchar *string_to_set1 = NULL;
    gchar *msg = NULL;
    ET_File *etfile;
    File_Tag *FileTag;
    GtkTreeSelection *selection;

    g_return_if_fail (ETCore->ETFileDisplayedList != NULL ||
                      BrowserList != NULL);

    // Save the current displayed data
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    // Warning : 'selection_filelist' is not a list of 'ETFile' items!
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
    selection_filelist = gtk_tree_selection_get_selected_rows(selection, NULL);
    // Create an 'ETFile' list from 'selection_filelist'
    while (selection_filelist)
    {
        etfile = Browser_List_Get_ETFile_From_Path(selection_filelist->data);
        etfilelist = g_list_append(etfilelist,etfile);

        if (!selection_filelist->next) break;
        selection_filelist = g_list_next(selection_filelist);
    }
    g_list_foreach(selection_filelist, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(selection_filelist);


    if (object == G_OBJECT (TitleEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(TitleEntry),0,-1); // The string to apply to all other files
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->title,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with title '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed title from selected files."));
    }
    else if (object == G_OBJECT (ArtistEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(ArtistEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->artist,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with artist '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed artist from selected files."));
    }
    else if (object == G_OBJECT (AlbumArtistEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(AlbumArtistEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->album_artist,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with album artist '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed album artist from selected files."));
    }
    else if (object == G_OBJECT (AlbumEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(AlbumEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->album,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with album '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed album name from selected files."));
    }
    else if (object == G_OBJECT (DiscNumberEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(DiscNumberEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->disc_number,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with disc number '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed disc number from selected files."));
    }
    else if (object == G_OBJECT (YearEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(YearEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->year,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with year '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed year from selected files."));
    }
    else if (object == G_OBJECT (TrackTotalEntry))
    {
        /* Used of Track and Total Track values */
        string_to_set = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(TrackEntryCombo)))));
        string_to_set1 = gtk_editable_get_chars(GTK_EDITABLE(TrackTotalEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);

            // We apply the TrackEntry field to all others files only if it is to delete
            // the field (string=""). Else we don't overwrite the track number
            if (!string_to_set || g_utf8_strlen(string_to_set, -1) == 0)
                ET_Set_Field_File_Tag_Item(&FileTag->track,string_to_set);
            ET_Set_Field_File_Tag_Item(&FileTag->track_total,string_to_set1);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }

        if ( string_to_set && g_utf8_strlen(string_to_set, -1) > 0 ) //&& atoi(string_to_set)>0 )
        {
            if ( string_to_set1 != NULL && g_utf8_strlen(string_to_set1, -1)>0 ) //&& atoi(string_to_set1)>0 )
            {
                msg = g_strdup_printf(_("Selected files tagged with track like 'xx/%s'."),string_to_set1);
            }else
            {
                msg = g_strdup_printf(_("Selected files tagged with track like 'xx'."));
            }
        }else
        {
            msg = g_strdup(_("Removed track number from selected files."));
        }
    }
    else if (object==G_OBJECT(TrackMButtonSequence))
    {
        /* This part doesn't set the same track number to all files, but sequence the tracks.
         * So we must browse the whole 'etfilelistfull' to get position of each selected file.
         * Note : 'etfilelistfull' and 'etfilelist' must be sorted in the same order */
        GList *etfilelistfull = NULL;
        gchar *path = NULL;
        gchar *path1 = NULL;
        gint i = 0;

        /* FIX ME!: see to fill also the Total Track (it's a good idea?) */
        etfilelistfull = g_list_first(ETCore->ETFileList);

        // Sort 'etfilelistfull' and 'etfilelist' in the same order
        etfilelist     = ET_Sort_File_List(etfilelist,SORTING_FILE_MODE);
        etfilelistfull = ET_Sort_File_List(etfilelistfull,SORTING_FILE_MODE);

        while (etfilelist && etfilelistfull)
        {
            // To get the path of the file
            File_Name *FileNameCur = (File_Name *)((ET_File *)etfilelistfull->data)->FileNameCur->data;
            // The ETFile in the selected file list
            etfile = etfilelist->data;

            // Restart counter when entering a new directory
            g_free(path1);
            path1 = g_path_get_dirname(FileNameCur->value);
            if ( path && path1 && strcmp(path,path1)!=0 )
                i = 0;
            if (g_settings_get_boolean (ETSettings, "tag-number-padded"))
                string_to_set = g_strdup_printf ("%.*d",
                                                 g_settings_get_uint (ETSettings, "tag-number-length"),
                                                 ++i);
            else
                string_to_set = g_strdup_printf("%d",++i);

            // The file is in the selection?
            if ( (ET_File *)etfilelistfull->data == etfile )
            {
                FileTag = ET_File_Tag_Item_New();
                ET_Copy_File_Tag_Item(etfile,FileTag);
                ET_Set_Field_File_Tag_Item(&FileTag->track,string_to_set);
                ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

                if (!etfilelist->next) break;
                etfilelist = g_list_next(etfilelist);
            }

            g_free(string_to_set);
            g_free(path);
            path = g_strdup(path1);

            etfilelistfull = g_list_next(etfilelistfull);
        }
        g_free(path);
        g_free(path1);
        //msg = g_strdup_printf(_("All %d tracks numbered sequentially."), ETCore->ETFileSelectionList_Length);
        msg = g_strdup_printf(_("Selected tracks numbered sequentially."));
    }
    else if (object==G_OBJECT(TrackMButtonNbrFiles))
    {
        /* Used of Track and Total Track values */
        while (etfilelist)
        {
            gchar *path_utf8, *filename_utf8;

            etfile        = (ET_File *)etfilelist->data;
            filename_utf8 = ((File_Name *)etfile->FileNameNew->data)->value_utf8;
            path_utf8     = g_path_get_dirname(filename_utf8);
            if (g_settings_get_boolean (ETSettings, "tag-number-padded"))
                string_to_set = g_strdup_printf ("%.*d",
                                                 g_settings_get_uint (ETSettings, "tag-number-length"),
                                                 ET_Get_Number_Of_Files_In_Directory (path_utf8));
            else
                string_to_set = g_strdup_printf("%d",ET_Get_Number_Of_Files_In_Directory(path_utf8));
            g_free(path_utf8);
            if (!string_to_set1)
                string_to_set1 = g_strdup(string_to_set); // Just for the message below...

            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->track_total,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }

        if ( string_to_set1 != NULL && g_utf8_strlen(string_to_set1, -1)>0 ) //&& atoi(string_to_set1)>0 )
        {
            msg = g_strdup_printf(_("Selected files tagged with track like 'xx/%s'."),string_to_set1);
        }else
        {
            msg = g_strdup(_("Removed track number from selected files."));
        }
    }
    else if (object == G_OBJECT (gtk_bin_get_child (GTK_BIN (GenreCombo))))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(GenreCombo))),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->genre,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with genre '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed genre from selected files."));
    }
    else if (object == G_OBJECT (CommentEntry))
    {
        //GtkTextBuffer *textbuffer;
        //GtkTextIter    start_iter;
        //GtkTextIter    end_iter;
        //textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(CommentView));
        //gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(textbuffer),&start_iter,&end_iter);
        //string_to_set = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(textbuffer),&start_iter,&end_iter,TRUE);

        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(CommentEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->comment,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with comment '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed comment from selected files."));
    }
    else if (object == G_OBJECT (ComposerEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(ComposerEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->composer,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with composer '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed composer from selected files."));
    }
    else if (object == G_OBJECT (OrigArtistEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(OrigArtistEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->orig_artist,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with original artist '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed original artist from selected files."));
    }
    else if (object == G_OBJECT (CopyrightEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(CopyrightEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->copyright,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with copyright '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed copyright from selected files."));
    }
    else if (object == G_OBJECT (URLEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(URLEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->url,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with URL '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed URL from selected files."));
    }
    else if (object == G_OBJECT (EncodedByEntry))
    {
        string_to_set = gtk_editable_get_chars(GTK_EDITABLE(EncodedByEntry),0,-1);
        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Item(&FileTag->encoded_by,string_to_set);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (string_to_set != NULL && g_utf8_strlen(string_to_set, -1)>0)
            msg = g_strdup_printf(_("Selected files tagged with encoder name '%s'."),string_to_set);
        else
            msg = g_strdup(_("Removed encoder name from selected files."));
    }
    else if (object==G_OBJECT(apply_image_toolitem))
    {
        Picture *res = NULL, *pic, *prev_pic = NULL;
        GtkTreeModel *model;
        GtkTreeIter iter;

        model = gtk_tree_view_get_model(GTK_TREE_VIEW(PictureEntryView));
        if (gtk_tree_model_get_iter_first(model, &iter))
        {
            do
            {
                gtk_tree_model_get(model, &iter, PICTURE_COLUMN_DATA, &pic, -1);
                pic = Picture_Copy_One(pic);
                if (!res)
                    res = pic;
                else
                    prev_pic->next = pic;
                prev_pic = pic;
            } while (gtk_tree_model_iter_next(model, &iter));
        }

        while (etfilelist)
        {
            etfile = (ET_File *)etfilelist->data;
            FileTag = ET_File_Tag_Item_New();
            ET_Copy_File_Tag_Item(etfile,FileTag);
            ET_Set_Field_File_Tag_Picture((Picture **)&FileTag->picture, res);
            ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

            if (!etfilelist->next) break;
            etfilelist = g_list_next(etfilelist);
        }
        if (res)
            msg = g_strdup (_("Selected files tagged with images."));
        else
            msg = g_strdup (_("Removed images from selected files."));
        Picture_Free(res);
    }

    g_list_free(etfilelist);

    // Refresh the whole list (faster than file by file) to show changes
    Browser_List_Refresh_Whole_List();

    /* Display the current file (Needed when sequencing tracks) */
    ET_Display_File_Data_To_UI(ETCore->ETFileDisplayed);

    if (msg)
    {
        Log_Print(LOG_OK,"%s",msg);
        Statusbar_Message(msg,TRUE);
        g_free(msg);
    }
    g_free(string_to_set);
    g_free(string_to_set1);

    /* To update state of Undo button */
    Update_Command_Buttons_Sensivity();
}




/*
 * Action when selecting all files
 */
void
et_on_action_select_all (void)
{
    GtkWidget *focused;

    /* Use the currently-focused widget and "select all" as appropriate.
     * https://bugzilla.gnome.org/show_bug.cgi?id=697515 */
    focused = gtk_window_get_focus (GTK_WINDOW (MainWindow));
    if (GTK_IS_EDITABLE (focused))
    {
        gtk_editable_select_region (GTK_EDITABLE (focused), 0, -1);
    }
    else if (focused == PictureEntryView)
    {
        GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (focused));
        gtk_tree_selection_select_all (selection);
    }
    else /* Assume that other widgets should select all in the file view. */
    {
        /* Save the current displayed data */
        ET_Save_File_Data_From_UI (ETCore->ETFileDisplayed);

        Browser_List_Select_All_Files ();
        Update_Command_Buttons_Sensivity ();
    }
}


/*
 * Action when unselecting all files
 */
void Action_Unselect_All_Files (void)
{
    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    Browser_List_Unselect_All_Files();
    ETCore->ETFileDisplayed = NULL;
}


/*
 * Action when inverting files selection
 */
void Action_Invert_Files_Selection (void)
{
    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    Browser_List_Invert_File_Selection();
    Update_Command_Buttons_Sensivity();
}



/*
 * Action when First button is selected
 */
void Action_Select_First_File (void)
{
    GList *etfilelist;

    if (!ETCore->ETFileDisplayedList)
        return;

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    /* Go to the first item of the list */
    etfilelist = ET_Displayed_File_List_First();
    if (etfilelist)
    {
        Browser_List_Unselect_All_Files(); // To avoid the last line still selected
        Browser_List_Select_File_By_Etfile((ET_File *)etfilelist->data,TRUE);
        ET_Display_File_Data_To_UI((ET_File *)etfilelist->data);
    }

    Update_Command_Buttons_Sensivity();
    Scan_Rename_File_Generate_Preview();
    Scan_Fill_Tag_Generate_Preview();

    if (!g_settings_get_boolean (ETSettings, "tag-preserve-focus"))
        gtk_widget_grab_focus(GTK_WIDGET(TitleEntry));
}


/*
 * Action when Prev button is selected
 */
void Action_Select_Prev_File (void)
{
    GList *etfilelist;

    if (!ETCore->ETFileDisplayedList || !ETCore->ETFileDisplayedList->prev)
        return;

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    /* Go to the prev item of the list */
    etfilelist = ET_Displayed_File_List_Previous();
    if (etfilelist)
    {
        Browser_List_Unselect_All_Files();
        Browser_List_Select_File_By_Etfile((ET_File *)etfilelist->data,TRUE);
        ET_Display_File_Data_To_UI((ET_File *)etfilelist->data);
    }

//    if (!ETFileList->prev)
//        gdk_beep(); // Warm the user

    Update_Command_Buttons_Sensivity();
    Scan_Rename_File_Generate_Preview();
    Scan_Fill_Tag_Generate_Preview();

    if (!g_settings_get_boolean (ETSettings, "tag-preserve-focus"))
        gtk_widget_grab_focus(GTK_WIDGET(TitleEntry));
}


/*
 * Action when Next button is selected
 */
void Action_Select_Next_File (void)
{
    GList *etfilelist;

    if (!ETCore->ETFileDisplayedList || !ETCore->ETFileDisplayedList->next)
        return;

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    /* Go to the next item of the list */
    etfilelist = ET_Displayed_File_List_Next();
    if (etfilelist)
    {
        Browser_List_Unselect_All_Files();
        Browser_List_Select_File_By_Etfile((ET_File *)etfilelist->data,TRUE);
        ET_Display_File_Data_To_UI((ET_File *)etfilelist->data);
    }

//    if (!ETFileList->next)
//        gdk_beep(); // Warm the user

    Update_Command_Buttons_Sensivity();
    Scan_Rename_File_Generate_Preview();
    Scan_Fill_Tag_Generate_Preview();

    if (!g_settings_get_boolean (ETSettings, "tag-preserve-focus"))
        gtk_widget_grab_focus(GTK_WIDGET(TitleEntry));
}


/*
 * Action when Last button is selected
 */
void Action_Select_Last_File (void)
{
    GList *etfilelist;

    if (!ETCore->ETFileDisplayedList || !ETCore->ETFileDisplayedList->next)
        return;

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    /* Go to the last item of the list */
    etfilelist = ET_Displayed_File_List_Last();
    if (etfilelist)
    {
        Browser_List_Unselect_All_Files();
        Browser_List_Select_File_By_Etfile((ET_File *)etfilelist->data,TRUE);
        ET_Display_File_Data_To_UI((ET_File *)etfilelist->data);
    }

    Update_Command_Buttons_Sensivity();
    Scan_Rename_File_Generate_Preview();
    Scan_Fill_Tag_Generate_Preview();

    if (!g_settings_get_boolean (ETSettings, "tag-preserve-focus"))
        gtk_widget_grab_focus(GTK_WIDGET(TitleEntry));
}


/*
 * Select a file in the "main list" using the ETFile adress of each item.
 */
void Action_Select_Nth_File_By_Etfile (ET_File *ETFile)
{
    if (!ETCore->ETFileDisplayedList)
        return;

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    /* Display the item */
    Browser_List_Select_File_By_Etfile(ETFile,TRUE);
    ET_Displayed_File_List_By_Etfile(ETFile); // Just to update 'ETFileDisplayedList'
    ET_Display_File_Data_To_UI(ETFile);

    Update_Command_Buttons_Sensivity();
    Scan_Rename_File_Generate_Preview();
    Scan_Fill_Tag_Generate_Preview();
}


/*
 * Action when Scan button is pressed
 */
void Action_Scan_Selected_Files (void)
{
    gint progress_bar_index;
    gint selectcount;
    gchar progress_bar_text[30];
    double fraction;
    GList *selfilelist = NULL;
    ET_File *etfile;
    GtkTreeSelection *selection;

    g_return_if_fail (ETCore->ETFileDisplayedList != NULL ||
                      BrowserList != NULL);

    /* Check if scanner window is opened */
    if (!ScannerWindow)
    {
        Open_ScannerWindow(SCANNER_TYPE);
        Statusbar_Message(_("Select Mode and Mask, and redo the same action"),TRUE);
        return;
    }

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    /* Initialize status bar */
    selectcount = gtk_tree_selection_count_selected_rows(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList)));
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar),0);
    progress_bar_index = 0;
    g_snprintf(progress_bar_text, 30, "%d/%d", progress_bar_index, selectcount);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), progress_bar_text);

    /* Set to unsensitive all command buttons (except Quit button) */
    Disable_Command_Buttons();

    progress_bar_index = 0;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
    selfilelist = gtk_tree_selection_get_selected_rows(selection, NULL);
    while (selfilelist)
    {
        etfile = Browser_List_Get_ETFile_From_Path(selfilelist->data);

        // Run the current scanner
        Scan_Select_Mode_And_Run_Scanner(etfile);

        fraction = (++progress_bar_index) / (double) selectcount;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), fraction);
        g_snprintf(progress_bar_text, 30, "%d/%d", progress_bar_index, selectcount);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), progress_bar_text);

        /* Needed to refresh status bar */
        while (gtk_events_pending())
            gtk_main_iteration();

        if (!selfilelist->next) break;
        selfilelist = g_list_next(selfilelist);
    }

    g_list_foreach(selfilelist, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(selfilelist);

    // Refresh the whole list (faster than file by file) to show changes
    Browser_List_Refresh_Whole_List();

    /* Display the current file */
    ET_Display_File_Data_To_UI(ETCore->ETFileDisplayed);

    /* To update state of command buttons */
    Update_Command_Buttons_Sensivity();

    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), "");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), 0);
    Statusbar_Message(_("All tags have been scanned"),TRUE);
}



/*
 * Action when Remove button is pressed
 */
void Action_Remove_Selected_Tags (void)
{
    GList *selfilelist = NULL;
    ET_File *etfile;
    File_Tag *FileTag;
    gint progress_bar_index;
    gint selectcount;
    double fraction;
    GtkTreeSelection *selection;

    g_return_if_fail (ETCore->ETFileDisplayedList != NULL ||
                      BrowserList != NULL);

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    /* Initialize status bar */
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), 0.0);
    selectcount = gtk_tree_selection_count_selected_rows(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList)));
    progress_bar_index = 0;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
    selfilelist = gtk_tree_selection_get_selected_rows(selection, NULL);
    while (selfilelist)
    {
        etfile = Browser_List_Get_ETFile_From_Path(selfilelist->data);
        FileTag = ET_File_Tag_Item_New();
        ET_Manage_Changes_Of_File_Data(etfile,NULL,FileTag);

        fraction = (++progress_bar_index) / (double) selectcount;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), fraction);
        /* Needed to refresh status bar */
        while (gtk_events_pending())
            gtk_main_iteration();

        if (!selfilelist->next) break;
        selfilelist = g_list_next(selfilelist);
    }

    g_list_foreach(selfilelist, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(selfilelist);

    // Refresh the whole list (faster than file by file) to show changes
    Browser_List_Refresh_Whole_List();

    /* Display the current file */
    ET_Display_File_Data_To_UI(ETCore->ETFileDisplayed);
    Update_Command_Buttons_Sensivity();

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), 0.0);
    Statusbar_Message(_("All tags have been removed"),TRUE);
}



/*
 * Action when Undo button is pressed.
 * Action_Undo_Selected_Files: Undo the last changes for the selected files.
 * Action_Undo_From_History_List: Undo the changes of the last modified file of the list.
 */
gint Action_Undo_Selected_Files (void)
{
    GList *selfilelist = NULL;
    gboolean state = FALSE;
    ET_File *etfile;
    GtkTreeSelection *selection;

    g_return_val_if_fail (ETCore->ETFileDisplayedList != NULL ||
                          BrowserList != NULL, FALSE);

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
    selfilelist = gtk_tree_selection_get_selected_rows(selection, NULL);
    while (selfilelist)
    {
        etfile = Browser_List_Get_ETFile_From_Path(selfilelist->data);
        state |= ET_Undo_File_Data(etfile);

        if (!selfilelist->next) break;
        selfilelist = g_list_next(selfilelist);
    }

    g_list_foreach(selfilelist, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(selfilelist);

    // Refresh the whole list (faster than file by file) to show changes
    Browser_List_Refresh_Whole_List();

    /* Display the current file */
    ET_Display_File_Data_To_UI(ETCore->ETFileDisplayed);
    Update_Command_Buttons_Sensivity();

    //ET_Debug_Print_File_List(ETCore->ETFileList,__FILE__,__LINE__,__FUNCTION__);

    return state;
}


void Action_Undo_From_History_List (void)
{
    ET_File *ETFile;

    g_return_if_fail (ETCore->ETFileList != NULL);

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    ETFile = ET_Undo_History_File_Data();
    if (ETFile)
    {
        ET_Display_File_Data_To_UI(ETFile);
        Browser_List_Select_File_By_Etfile(ETFile,TRUE);
        Browser_List_Refresh_File_In_List(ETFile);
    }

    Update_Command_Buttons_Sensivity();
}



/*
 * Action when Redo button is pressed.
 * Action_Redo_Selected_Files: Redo the last changes for the selected files.
 * Action_Redo_From_History_List: Redo the changes of the last modified file of the list.
 */
gint Action_Redo_Selected_File (void)
{
    GList *selfilelist = NULL;
    gboolean state = FALSE;
    ET_File *etfile;
    GtkTreeSelection *selection;

    g_return_val_if_fail (ETCore->ETFileDisplayedList != NULL ||
                          BrowserList != NULL, FALSE);

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
    selfilelist = gtk_tree_selection_get_selected_rows(selection, NULL);
    while (selfilelist)
    {
        etfile = Browser_List_Get_ETFile_From_Path(selfilelist->data);
        state |= ET_Redo_File_Data(etfile);

        if (!selfilelist->next) break;
        selfilelist = g_list_next(selfilelist);
    }

    g_list_foreach(selfilelist, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(selfilelist);

    // Refresh the whole list (faster than file by file) to show changes
    Browser_List_Refresh_Whole_List();

    /* Display the current file */
    ET_Display_File_Data_To_UI(ETCore->ETFileDisplayed);
    Update_Command_Buttons_Sensivity();

    return state;
}


void Action_Redo_From_History_List (void)
{
    ET_File *ETFile;

    g_return_if_fail (ETCore->ETFileDisplayedList != NULL);

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    ETFile = ET_Redo_History_File_Data();
    if (ETFile)
    {
        ET_Display_File_Data_To_UI(ETFile);
        Browser_List_Select_File_By_Etfile(ETFile,TRUE);
        Browser_List_Refresh_File_In_List(ETFile);
    }

    Update_Command_Buttons_Sensivity();
}




/*
 * Action when Save button is pressed
 */
void Action_Save_Selected_Files (void)
{
    Save_Selected_Files_With_Answer(FALSE);
}

void Action_Force_Saving_Selected_Files (void)
{
    Save_Selected_Files_With_Answer(TRUE);
}


/*
 * Will save the full list of file (not only the selected files in list)
 * and check if we must save also only the changed files or all files
 * (force_saving_files==TRUE)
 */
gint Save_All_Files_With_Answer (gboolean force_saving_files)
{
    GList *etfilelist;

    g_return_val_if_fail (ETCore != NULL || ETCore->ETFileList != NULL, FALSE);

    etfilelist = g_list_first (ETCore->ETFileList);

    return Save_List_Of_Files (etfilelist, force_saving_files);
}

/*
 * Will save only the selected files in the file list
 */
static gint
Save_Selected_Files_With_Answer (gboolean force_saving_files)
{
    gint toreturn;
    GList *etfilelist = NULL;
    GList *selfilelist = NULL;
    ET_File *etfile;
    GtkTreeSelection *selection;

    g_return_val_if_fail (BrowserList != NULL, FALSE);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
    selfilelist = gtk_tree_selection_get_selected_rows(selection, NULL);
    while (selfilelist)
    {
        etfile = Browser_List_Get_ETFile_From_Path(selfilelist->data);
        etfilelist = g_list_append(etfilelist, etfile);

        if (!selfilelist->next) break;
        selfilelist = selfilelist->next;
    }
    g_list_foreach(selfilelist, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(selfilelist);

    toreturn = Save_List_Of_Files(etfilelist, force_saving_files);
    g_list_free(etfilelist);
    return toreturn;
}

/*
 * Save_List_Of_Files: Function to save a list of files.
 *  - force_saving_files = TRUE => force saving the file even if it wasn't changed
 *  - force_saving_files = FALSE => force saving only the changed files
 */
static gint
Save_List_Of_Files (GList *etfilelist, gboolean force_saving_files)
{
    gint       progress_bar_index;
    gint       saving_answer;
    gint       nb_files_to_save;
    gint       nb_files_changed_by_ext_program;
    gchar     *msg;
    gchar      progress_bar_text[30];
    GList     *etfilelist_tmp;
    ET_File   *etfile_save_position = NULL;
    File_Tag  *FileTag;
    File_Name *FileNameNew;
    double     fraction;
    GtkAction *uiaction;
    GtkWidget *toggle_radio;
    GtkWidget *widget_focused;
    GtkTreePath *currentPath = NULL;

    g_return_val_if_fail (ETCore != NULL, FALSE);

    /* Save the current position in the list */
    etfile_save_position = ETCore->ETFileDisplayed;

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    /* Save widget that has current focus, to give it again the focus after saving */
    widget_focused = gtk_window_get_focus(GTK_WINDOW(MainWindow));

    /* Count the number of files to save */
    /* Count the number of files changed by an external program */
    nb_files_to_save = 0;
    nb_files_changed_by_ext_program = 0;
    etfilelist_tmp = etfilelist;
    while (etfilelist_tmp)
    {
        struct stat   statbuf;
        ET_File   *ETFile   = (ET_File *)etfilelist_tmp->data;
        File_Tag  *FileTag  = (File_Tag *)ETFile->FileTag->data;
        File_Name *FileName = (File_Name *)ETFile->FileNameNew->data;
        gchar *filename_cur = ((File_Name *)ETFile->FileNameCur->data)->value;
        gchar *filename_cur_utf8 = ((File_Name *)ETFile->FileNameCur->data)->value_utf8;
        gchar *basename_cur_utf8 = g_path_get_basename(filename_cur_utf8);

        // Count only the changed files or all files if force_saving_files==TRUE
        if ( force_saving_files
        || (FileName && FileName->saved==FALSE) || (FileTag && FileTag->saved==FALSE) )
            nb_files_to_save++;

        stat(filename_cur,&statbuf);
        if (ETFile->FileModificationTime != statbuf.st_mtime)
            nb_files_changed_by_ext_program++;
        g_free(basename_cur_utf8);

        etfilelist_tmp = etfilelist_tmp->next;
    }

    /* Initialize status bar */
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar),0);
    progress_bar_index = 0;
    g_snprintf(progress_bar_text, 30, "%d/%d", progress_bar_index, nb_files_to_save);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), progress_bar_text);

    /* Set to unsensitive all command buttons (except Quit button) */
    Disable_Command_Buttons();
    Browser_Area_Set_Sensitive(FALSE);
    Tag_Area_Set_Sensitive(FALSE);
    File_Area_Set_Sensitive(FALSE);

    /* Show msgbox (if needed) to ask confirmation ('SF' for Save File) */
    SF_HideMsgbox_Write_Tag = FALSE;
    SF_HideMsgbox_Rename_File = FALSE;

    Main_Stop_Button_Pressed = FALSE;
    uiaction = gtk_ui_manager_get_action(UIManager, "/ToolBar/Stop"); // Activate the stop button
    g_object_set(uiaction, "sensitive", FALSE, NULL);

    /*
     * Check if file was changed by an external program
     */
    if (nb_files_changed_by_ext_program > 0)
    {
        // Some files were changed by other program than EasyTAG
        GtkWidget *msgdialog = NULL;
        gint response;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_WARNING,
                                           GTK_BUTTONS_NONE,
                                           ngettext("A file was changed by an external program","%d files were changed by an external program.",nb_files_changed_by_ext_program),
                                           nb_files_changed_by_ext_program);
        gtk_dialog_add_buttons(GTK_DIALOG(msgdialog),GTK_STOCK_DISCARD,GTK_RESPONSE_NO,GTK_STOCK_SAVE,GTK_RESPONSE_YES,NULL);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",_("Do you want to continue saving the file?"));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Quit"));

        response = gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);

        switch (response)
        {
            case GTK_RESPONSE_YES:
                break;
            case GTK_RESPONSE_NO:
            case GTK_RESPONSE_NONE:
                /* Skip the following loop. */
                Main_Stop_Button_Pressed = TRUE;
                break;
        }
    }

    etfilelist_tmp = etfilelist;
    while (etfilelist_tmp && !Main_Stop_Button_Pressed)
    {
        FileTag     = ((ET_File *)etfilelist_tmp->data)->FileTag->data;
        FileNameNew = ((ET_File *)etfilelist_tmp->data)->FileNameNew->data;

        /* We process only the files changed and not saved, or we force to save all
         * files if force_saving_files==TRUE */
        if ( force_saving_files
        || FileTag->saved == FALSE || FileNameNew->saved == FALSE )
        {
            // ET_Display_File_Data_To_UI((ET_File *)etfilelist_tmp->data);
            // Use of 'currentPath' to try to increase speed. Indeed, in many
            // cases, the next file to select, is the next in the list
            currentPath = Browser_List_Select_File_By_Etfile2((ET_File *)etfilelist_tmp->data,FALSE,currentPath);

            fraction = (++progress_bar_index) / (double) nb_files_to_save;
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), fraction);
            g_snprintf(progress_bar_text, 30, "%d/%d", progress_bar_index, nb_files_to_save);
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), progress_bar_text);

            /* Needed to refresh status bar */
            while (gtk_events_pending())
                gtk_main_iteration();

            // Save tag and rename file
            saving_answer = Save_File((ET_File *)etfilelist_tmp->data, (nb_files_to_save>1) ? TRUE : FALSE, force_saving_files);

            if (saving_answer == -1)
            {
                /* Stop saving files + reinit progress bar */
                gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), "");
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), 0.0);
                Statusbar_Message (_("Saving files was stopped"), TRUE);
                /* To update state of command buttons */
                Update_Command_Buttons_Sensivity();
                Browser_Area_Set_Sensitive(TRUE);
                Tag_Area_Set_Sensitive(TRUE);
                File_Area_Set_Sensitive(TRUE);

                return -1; /* We stop all actions */
            }
        }

        etfilelist_tmp = etfilelist_tmp->next;
        if (Main_Stop_Button_Pressed)
            break;

    }

    if (currentPath)
        gtk_tree_path_free(currentPath);

    if (Main_Stop_Button_Pressed)
        msg = g_strdup (_("Saving files was stopped"));
    else
        msg = g_strdup (_("All files have been saved"));

    Main_Stop_Button_Pressed = FALSE;
    uiaction = gtk_ui_manager_get_action(UIManager, "/ToolBar/Stop");
    g_object_set(uiaction, "sensitive", FALSE, NULL);

    /* Return to the saved position in the list */
    ET_Display_File_Data_To_UI(etfile_save_position);
    Browser_List_Select_File_By_Etfile(etfile_save_position,TRUE);

    /* Browser is on mode : Artist + Album list */
    toggle_radio = gtk_ui_manager_get_widget (UIManager,
                                              "/ToolBar/ArtistViewMode");
    if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (toggle_radio)))
        Browser_Display_Tree_Or_Artist_Album_List();

    /* To update state of command buttons */
    Update_Command_Buttons_Sensivity();
    Browser_Area_Set_Sensitive(TRUE);
    Tag_Area_Set_Sensitive(TRUE);
    File_Area_Set_Sensitive(TRUE);

    /* Give again focus to the first entry, else the focus is passed to another */
    gtk_widget_grab_focus(GTK_WIDGET(widget_focused));

    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), "");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), 0);
    Statusbar_Message(msg,TRUE);
    g_free(msg);
    Browser_List_Refresh_Whole_List();
    return TRUE;
}



/*
 * Delete a file on the hard disk
 */
void Action_Delete_Selected_Files (void)
{
    Delete_Selected_Files_With_Answer();
}


static gint
Delete_Selected_Files_With_Answer (void)
{
    GList *selfilelist;
    GList *rowreflist = NULL;
    gint   progress_bar_index;
    gint   saving_answer;
    gint   nb_files_to_delete;
    gint   nb_files_deleted = 0;
    gchar *msg;
    gchar progress_bar_text[30];
    double fraction;
    GtkTreeModel *treemodel;
    GtkTreeRowReference *rowref;
    GtkTreeSelection *selection;

    g_return_val_if_fail (ETCore->ETFileDisplayedList != NULL ||
                          BrowserList != NULL, FALSE);

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    /* Number of files to save */
    nb_files_to_delete = gtk_tree_selection_count_selected_rows(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList)));

    /* Initialize status bar */
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar),0);
    progress_bar_index = 0;
    g_snprintf(progress_bar_text, 30, "%d/%d", progress_bar_index, nb_files_to_delete);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), progress_bar_text);

    /* Set to unsensitive all command buttons (except Quit button) */
    Disable_Command_Buttons();
    Browser_Area_Set_Sensitive(FALSE);
    Tag_Area_Set_Sensitive(FALSE);
    File_Area_Set_Sensitive(FALSE);

    /* Show msgbox (if needed) to ask confirmation */
    SF_HideMsgbox_Delete_File = 0;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
    selfilelist = gtk_tree_selection_get_selected_rows(selection, &treemodel);
    while (selfilelist)
    {
        rowref = gtk_tree_row_reference_new(treemodel, selfilelist->data);
        rowreflist = g_list_append(rowreflist, rowref);

        if (!selfilelist->next) break;
        selfilelist = selfilelist->next;
    }

    while (rowreflist)
    {
        GtkTreePath *path;
        ET_File *ETFile;

        path = gtk_tree_row_reference_get_path(rowreflist->data);
        ETFile = Browser_List_Get_ETFile_From_Path(path);
        gtk_tree_path_free(path);

        ET_Display_File_Data_To_UI(ETFile);
        Browser_List_Select_File_By_Etfile(ETFile,FALSE);
        fraction = (++progress_bar_index) / (double) nb_files_to_delete;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), fraction);
        g_snprintf(progress_bar_text, 30, "%d/%d", progress_bar_index, nb_files_to_delete);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), progress_bar_text);
         /* Needed to refresh status bar */
        while (gtk_events_pending())
            gtk_main_iteration();

        saving_answer = Delete_File(ETFile,(nb_files_to_delete>1)?TRUE:FALSE);

        switch (saving_answer)
        {
            case 1:
                nb_files_deleted += saving_answer;
                // Remove file in the browser (corresponding line in the clist)
                Browser_List_Remove_File(ETFile);
                // Remove file from file list
                ET_Remove_File_From_File_List(ETFile);
                break;
            case 0:
                break;
            case -1:
                // Stop deleting files + reinit progress bar
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar),0.0);
                // To update state of command buttons
                Update_Command_Buttons_Sensivity();
                Browser_Area_Set_Sensitive(TRUE);
                Tag_Area_Set_Sensitive(TRUE);
                File_Area_Set_Sensitive(TRUE);

                g_list_foreach(selfilelist, (GFunc) gtk_tree_path_free, NULL);
                g_list_free(selfilelist);
                return -1; // We stop all actions
        }

        if (!rowreflist->next) break;
        rowreflist = rowreflist->next;
    }

    g_list_foreach(selfilelist, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(selfilelist);
    g_list_foreach(rowreflist, (GFunc) gtk_tree_row_reference_free, NULL);
    g_list_free(rowreflist);

    if (nb_files_deleted < nb_files_to_delete)
        msg = g_strdup (_("Files have been partially deleted"));
    else
        msg = g_strdup (_("All files have been deleted"));

    // It's important to displayed the new item, as it'll check the changes in Browser_Display_Tree_Or_Artist_Album_List
    if (ETCore->ETFileDisplayed)
        ET_Display_File_Data_To_UI(ETCore->ETFileDisplayed);
    /*else if (ET_Displayed_File_List_Current())
        ET_Display_File_Data_To_UI((ET_File *)ET_Displayed_File_List_Current()->data);*/

    // Load list...
    Browser_List_Load_File_List(ETCore->ETFileDisplayedList, NULL);
    // Rebuild the list...
    Browser_Display_Tree_Or_Artist_Album_List();

    /* To update state of command buttons */
    Update_Command_Buttons_Sensivity();
    Browser_Area_Set_Sensitive(TRUE);
    Tag_Area_Set_Sensitive(TRUE);
    File_Area_Set_Sensitive(TRUE);

    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), "");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), 0);
    Statusbar_Message(msg,TRUE);
    g_free(msg);

    return TRUE;
}



/*
 * Save changes of the ETFile (write tag and rename file)
 *  - multiple_files = TRUE  : when saving files, a msgbox appears with ability
 *                             to do the same action for all files.
 *  - multiple_files = FALSE : appears only a msgbox to ask confirmation.
 */
static gint
Save_File (ET_File *ETFile, gboolean multiple_files,
           gboolean force_saving_files)
{
    File_Tag  *FileTag;
    File_Name *FileNameNew;
    gint stop_loop = 0;
    //struct stat   statbuf;
    //gchar *filename_cur = ((File_Name *)ETFile->FileNameCur->data)->value;
    gchar *filename_cur_utf8 = ((File_Name *)ETFile->FileNameCur->data)->value_utf8;
    gchar *filename_new_utf8 = ((File_Name *)ETFile->FileNameNew->data)->value_utf8;
    gchar *basename_cur_utf8, *basename_new_utf8;
    gchar *dirname_cur_utf8, *dirname_new_utf8;

    g_return_val_if_fail (ETFile != NULL, 0);

    basename_cur_utf8 = g_path_get_basename(filename_cur_utf8);
    basename_new_utf8 = g_path_get_basename(filename_new_utf8);

    /* Save the current displayed data */
    //ET_Save_File_Data_From_UI((ET_File *)ETFileList->data); // Not needed, because it was done before
    FileTag     = ETFile->FileTag->data;
    FileNameNew = ETFile->FileNameNew->data;

    /*
     * Check if file was changed by an external program
     */
    /*stat(filename_cur,&statbuf);
    if (ETFile->FileModificationTime != statbuf.st_mtime)
    {
        // File was changed
        GtkWidget *msgbox = NULL;
        gint response;

        msg = g_strdup_printf(_("The file '%s' was changed by an external program.\nDo you want to continue?"),basename_cur_utf8);
        msgbox = msg_box_new(_("Write File"),
                             GTK_WINDOW(MainWindow),
                             NULL,
                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                             msg,
                             GTK_STOCK_DIALOG_WARNING,
                             GTK_STOCK_NO,  GTK_RESPONSE_NO,
                             GTK_STOCK_YES, GTK_RESPONSE_YES,
                             NULL);
        g_free(msg);

        response = gtk_dialog_run(GTK_DIALOG(msgbox));
        gtk_widget_destroy(msgbox);

        switch (response)
        {
            case GTK_RESPONSE_YES:
                break;
            case GTK_RESPONSE_NO:
            case GTK_RESPONSE_NONE:
                stop_loop = -1;
                return stop_loop;
                break;
        }
    }*/


    /*
     * First part: write tag information (artist, title,...)
     */
    // Note : the option 'force_saving_files' is only used to save tags
    if ( force_saving_files
    || FileTag->saved == FALSE ) // This tag had been already saved ?
    {
        GtkWidget *msgdialog = NULL;
        GtkWidget *msgdialog_check_button = NULL;
        gint response;

        if (g_settings_get_boolean (ETSettings, "confirm-write-tags")
            && !SF_HideMsgbox_Write_Tag)
        {
            // ET_Display_File_Data_To_UI(ETFile);

            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               _("Do you want to write the tag of file '%s'?"),
                                               basename_cur_utf8);
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Confirm Tag Writing"));
            if (multiple_files)
            {
                GtkWidget *message_area;
                message_area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(msgdialog));
                msgdialog_check_button = gtk_check_button_new_with_label(_("Repeat action for the remaining files"));
                gtk_container_add(GTK_CONTAINER(message_area),msgdialog_check_button);
                gtk_widget_show (msgdialog_check_button);
                gtk_dialog_add_buttons(GTK_DIALOG(msgdialog),GTK_STOCK_DISCARD,GTK_RESPONSE_NO,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_SAVE,GTK_RESPONSE_YES,NULL);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(msgdialog_check_button), TRUE); // Checked by default
            }else
            {
                gtk_dialog_add_buttons(GTK_DIALOG(msgdialog),GTK_STOCK_DISCARD,GTK_RESPONSE_NO,GTK_STOCK_SAVE,GTK_RESPONSE_YES,NULL);
            }

            SF_ButtonPressed_Write_Tag = response = gtk_dialog_run(GTK_DIALOG(msgdialog));
            // When check button in msgbox was activated : do not display the message again
            if (msgdialog_check_button && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button)))
                SF_HideMsgbox_Write_Tag = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button));
            gtk_widget_destroy(msgdialog);
        }else
        {
            if (SF_HideMsgbox_Write_Tag)
                response = SF_ButtonPressed_Write_Tag;
            else
                response = GTK_RESPONSE_YES;
        }

        switch (response)
        {
            case GTK_RESPONSE_YES:
            {
                gboolean rc;

                // if 'SF_HideMsgbox_Write_Tag is TRUE', then errors are displayed only in log
                rc = Write_File_Tag(ETFile,SF_HideMsgbox_Write_Tag);
                // if an error occurs when 'SF_HideMsgbox_Write_Tag is TRUE', we don't stop saving...
                if (rc != TRUE && !SF_HideMsgbox_Write_Tag)
                {
                    stop_loop = -1;
                    return stop_loop;
                }
                break;
            }
            case GTK_RESPONSE_NO:
                break;
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_NONE:
                stop_loop = -1;
                return stop_loop;
                break;
        }
    }


    /*
     * Second part: rename the file
     */
    // Do only if changed! (don't take force_saving_files into account)
    if ( FileNameNew->saved == FALSE ) // This filename had been already saved ?
    {
        GtkWidget *msgdialog = NULL;
        GtkWidget *msgdialog_check_button = NULL;
        gint response;

        if (g_settings_get_boolean (ETSettings, "confirm-rename-file")
            && !SF_HideMsgbox_Rename_File)
        {
            gchar *msgdialog_title = NULL;
            gchar *msg = NULL;
            gchar *msg1 = NULL;
            // ET_Display_File_Data_To_UI(ETFile);

            dirname_cur_utf8 = g_path_get_dirname(filename_cur_utf8);
            dirname_new_utf8 = g_path_get_dirname(filename_new_utf8);

            // Directories were renamed? or only filename?
            if (g_utf8_collate(dirname_cur_utf8,dirname_new_utf8) != 0)
            {
                if (g_utf8_collate(basename_cur_utf8,basename_new_utf8) != 0)
                {
                    // Directories and filename changed
                    msgdialog_title = g_strdup (_("Rename File and Directory"));
                    msg = g_strdup(_("File and directory rename confirmation required"));
                    msg1 = g_strdup_printf(_("Do you want to rename the file and directory '%s' to '%s'?"),
                                           filename_cur_utf8, filename_new_utf8);
                }else
                {
                    // Only directories changed
                    msgdialog_title = g_strdup (_("Rename Directory"));
                    msg = g_strdup(_("Directory rename confirmation required"));
                    msg1 = g_strdup_printf(_("Do you want to rename the directory '%s' to '%s'?"),
                                           dirname_cur_utf8, dirname_new_utf8);
                }
            }else
            {
                // Only filename changed
                msgdialog_title = g_strdup (_("Rename File"));
                msg = g_strdup(_("File rename confirmation required"));
                msg1 = g_strdup_printf(_("Do you want to rename the file '%s' to '%s'?"),
                                       basename_cur_utf8, basename_new_utf8);
            }

            g_free(dirname_cur_utf8);
            g_free(dirname_new_utf8);

            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               "%s",
                                               msg);
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",msg1);
            gtk_window_set_title(GTK_WINDOW(msgdialog),msgdialog_title);
            if (multiple_files)
            {
                GtkWidget *message_area;
                message_area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(msgdialog));
                msgdialog_check_button = gtk_check_button_new_with_label(_("Repeat action for the remaining files"));
                gtk_container_add(GTK_CONTAINER(message_area),msgdialog_check_button);
                gtk_widget_show (msgdialog_check_button);
                gtk_dialog_add_buttons(GTK_DIALOG(msgdialog),GTK_STOCK_DISCARD,GTK_RESPONSE_NO,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_SAVE,GTK_RESPONSE_YES,NULL);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(msgdialog_check_button), TRUE); // Checked by default
            }else
            {
                gtk_dialog_add_buttons(GTK_DIALOG(msgdialog),GTK_STOCK_DISCARD,GTK_RESPONSE_NO,GTK_STOCK_SAVE,GTK_RESPONSE_YES,NULL);
            }
            g_free(msg);
            g_free(msg1);
            g_free(msgdialog_title);
            SF_ButtonPressed_Rename_File = response = gtk_dialog_run(GTK_DIALOG(msgdialog));
            if (msgdialog_check_button && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button)))
                SF_HideMsgbox_Rename_File = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button));
            gtk_widget_destroy(msgdialog);
        }else
        {
            if (SF_HideMsgbox_Rename_File)
                response = SF_ButtonPressed_Rename_File;
            else
                response = GTK_RESPONSE_YES;
        }

        switch(response)
        {
            case GTK_RESPONSE_YES:
            {
                gboolean rc;

                // if 'SF_HideMsgbox_Rename_File is TRUE', then errors are displayed only in log
                rc = Rename_File(ETFile,SF_HideMsgbox_Rename_File);
                // if an error occurs when 'SF_HideMsgbox_Rename_File is TRUE', we don't stop saving...
                if (rc != TRUE && !SF_HideMsgbox_Rename_File)
                {
                    stop_loop = -1;
                    return stop_loop;
                }
                break;
            }
            case GTK_RESPONSE_NO:
                break;
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_NONE:
                stop_loop = -1;
                return stop_loop;
                break;
        }
    }

    g_free(basename_cur_utf8);
    g_free(basename_new_utf8);

    /* Refresh file into browser list */
    // Browser_List_Refresh_File_In_List(ETFile);

    return 1;
}


/*
 * Write tag of the ETFile
 * Return TRUE => OK
 *        FALSE => error
 */
static gboolean
Write_File_Tag (ET_File *ETFile, gboolean hide_msgbox)
{
    gchar *cur_filename_utf8 = ((File_Name *)ETFile->FileNameCur->data)->value_utf8;
    gchar *msg;
    gchar *msg1;
    gchar *basename_utf8;
    GtkWidget *msgdialog;

    basename_utf8 = g_path_get_basename(cur_filename_utf8);
    msg = g_strdup_printf(_("Writing tag of '%s'"),basename_utf8);
    Statusbar_Message(msg,TRUE);
    g_free(msg);

    if (ET_Save_File_Tag_To_HD(ETFile))
    {
        Statusbar_Message(_("Tag(s) written"),TRUE);
        g_free (basename_utf8);
        return TRUE;
    }

    switch ( ((ET_File_Description *)ETFile->ETFileDescription)->TagType)
    {
#ifdef ENABLE_OGG
        case OGG_TAG:
            // Special for Ogg Vorbis because the error is defined into 'vcedit_error(state)'
            msg = ogg_error_msg;
            msg1 = g_strdup_printf(_("Cannot write tag in file '%s' (%s)"),
                                  basename_utf8,ogg_error_msg);
            break;
#endif
        default:
            msg = g_strdup (g_strerror (errno));
            msg1 = g_strdup_printf (_("Cannot write tag in file '%s' (%s)"),
                                    basename_utf8, msg);
    }

    Log_Print(LOG_ERROR,"%s", msg1);
    g_free(msg1);

    if (!hide_msgbox)
    {
        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                             GTK_MESSAGE_ERROR,
                             GTK_BUTTONS_CLOSE,
                             _("Cannot write tag in file '%s'"),
                             basename_utf8);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",msg);
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Tag Write Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
    }
    g_free(msg);
    g_free(basename_utf8);

    return FALSE;
}


/*
 * Make dir and all parents with permission mode
 */
static gboolean
Make_Dir (const gchar *dirname_old, const gchar *dirname_new)
{
    gchar *parent, *temp;
    struct stat dirstat;
#ifdef G_OS_WIN32
    gboolean first = TRUE;
#endif /* G_OS_WIN32 */

    // Use same permissions as the old directory
    stat(dirname_old,&dirstat);

    temp = parent = g_strdup(dirname_new);
    for (temp++;*temp;temp++)
    {
        if (*temp!=G_DIR_SEPARATOR)
            continue;

#ifdef G_OS_WIN32
        if (first)
        {
            first = FALSE;
            continue;
        }
#endif /* G_OS_WIN32 */

        *temp=0; // To truncate temporarly dirname_new

        if (mkdir(parent,dirstat.st_mode)==-1 && errno!=EEXIST)
        {
            g_free(parent);
            return FALSE;
        }
        *temp=G_DIR_SEPARATOR; // To cancel the '*temp=0;'
    }
    g_free(parent);

    if (mkdir(dirname_new,dirstat.st_mode)==-1 && errno!=EEXIST)
        return FALSE;

    return TRUE;
}

/*
 * Remove old directories after renaming the file
 * Badly coded, but works....
 */
static gboolean
Remove_Dir (const gchar *dirname_old, const gchar *dirname_new)
{
    gchar *temp_old, *temp_new;
    gchar *temp_end_old, *temp_end_new;

    temp_old = g_strdup(dirname_old);
    temp_new = g_strdup(dirname_new);

    while (temp_old && temp_new && strcmp(temp_old,temp_new)!=0 )
    {
        if (rmdir(temp_old)==-1)
        {
            // Patch from vdaghan : ENOTEMPTY & EEXIST are synonymous and used by some systems
            if (errno != ENOTEMPTY
            &&  errno != EEXIST)
            {
                g_free(temp_old);
                g_free(temp_new);
                return FALSE;
            }else
            {
                break;
            }
        }

        temp_end_old = temp_old + strlen(temp_old) - 1;
        temp_end_new = temp_new + strlen(temp_new) - 1;

        while (*temp_end_old && *temp_end_old!=G_DIR_SEPARATOR)
        {
            temp_end_old--;
        }
        *temp_end_old=0;
        while (*temp_end_new && *temp_end_new!=G_DIR_SEPARATOR)
        {
            temp_end_new--;
        }
        *temp_end_new=0;
    }
    g_free(temp_old);
    g_free(temp_new);

    return TRUE;
}


/*
 * Rename the file ETFile
 * Return TRUE => OK
 *        FALSE => error
 */
static gboolean
Rename_File (ET_File *ETFile, gboolean hide_msgbox)
{
    FILE  *file;
    gchar *tmp_filename = NULL;
    gchar *cur_filename = ((File_Name *)ETFile->FileNameCur->data)->value;
    gchar *new_filename = ((File_Name *)ETFile->FileNameNew->data)->value;
    gchar *cur_filename_utf8 = ((File_Name *)ETFile->FileNameCur->data)->value_utf8;      // Filename + path
    gchar *new_filename_utf8 = ((File_Name *)ETFile->FileNameNew->data)->value_utf8;
    gchar *cur_basename_utf8 = g_path_get_basename(cur_filename_utf8); // Only filename
    gchar *new_basename_utf8 = g_path_get_basename(new_filename_utf8);
    gint   fd_tmp;
    gchar *msg;
    gchar *dirname_cur;
    gchar *dirname_new;
    gchar *dirname_cur_utf8;
    gchar *dirname_new_utf8;

    msg = g_strdup_printf(_("Renaming file '%s'"),cur_filename_utf8);
    Statusbar_Message(msg,TRUE);
    g_free(msg);

    /* We use two stages to rename file, to avoid problem with some system
     * that doesn't allow to rename the file if only the case has changed. */
    tmp_filename = g_strdup_printf("%s.XXXXXX",cur_filename);
    if ( (fd_tmp = mkstemp(tmp_filename)) >= 0 )
    {
        close(fd_tmp);
        unlink(tmp_filename);
    }

    // Rename to the temporary name
    if ( rename(cur_filename,tmp_filename)!=0 ) // => rename() fails
    {
        gchar *msg;
        GtkWidget *msgdialog;

        /* Renaming file to the temporary filename has failed */
        msg = g_strdup_printf(_("Cannot rename file '%s' to '%s' (%s)"),
                              cur_basename_utf8,new_basename_utf8,g_strerror(errno));
        if (!hide_msgbox)
        {
            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               _("Cannot rename file '%s' to '%s'"),
                                               cur_basename_utf8,
                                               new_basename_utf8);
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename File Error"));
            gtk_dialog_run(GTK_DIALOG(msgdialog));
            gtk_widget_destroy(msgdialog);
        }

        Log_Print(LOG_ERROR,"%s", msg);
        g_free(msg);

        Statusbar_Message (_("File(s) not renamed"), TRUE);
        g_free(tmp_filename);
        g_free(cur_basename_utf8);
        g_free(new_basename_utf8);
        return FALSE;
    }

    /* Check if the new filename already exists. Must be done after changing
     * filename to the temporary name, else we can't detect the problem under
     * Linux when renaming a file 'aa.mp3' to 'AA.mp3' and if the last one
     * already exists */
    if ( (file=fopen(new_filename,"r"))!=NULL )
    {
        GtkWidget *msgdialog;

        fclose(file);

        // Restore the initial name
        if ( rename(tmp_filename,cur_filename)!=0 ) // => rename() fails
        {
            gchar *msg;

            /* Renaming file from the temporary filename has failed */
            if (!hide_msgbox)
            {
                msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     _("Cannot rename file '%s' to '%s'"),
                                     new_basename_utf8,
                                     cur_basename_utf8);
                gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
                gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename File Error"));

                gtk_dialog_run(GTK_DIALOG(msgdialog));
                gtk_widget_destroy(msgdialog);
            }

            msg = g_strdup_printf(_("Cannot rename file '%s' to '%s': %s"),
                                    new_basename_utf8,cur_basename_utf8,g_strerror(errno));
            Log_Print(LOG_ERROR,"%s", msg);
            g_free(msg);

            Statusbar_Message (_("File(s) not renamed"), TRUE);
            g_free(tmp_filename);
            g_free(cur_basename_utf8);
            g_free(new_basename_utf8);
            return FALSE;
        }

        if (!hide_msgbox)
        {
            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               _("Cannot rename file '%s' to '%s'"),
                                               new_basename_utf8,
                                               cur_basename_utf8);
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename File Error"));
            gtk_dialog_run(GTK_DIALOG(msgdialog));
            gtk_widget_destroy(msgdialog);
        }

        msg = g_strdup_printf(_("Cannot rename file '%s' because the following "
                    "file already exists: '%s'"),cur_basename_utf8,new_basename_utf8);
        Log_Print(LOG_ERROR,"%s", msg);
        g_free(msg);

        Statusbar_Message (_("File(s) not renamed"), TRUE);

        g_free (tmp_filename);
        g_free(new_basename_utf8);
        g_free(cur_basename_utf8);
        return FALSE;
    }

    /* If files are in different directories, we need to create the new
     * destination directory */
    dirname_cur = g_path_get_dirname(tmp_filename);
    dirname_new = g_path_get_dirname(new_filename);

    if (dirname_cur && dirname_new && strcmp(dirname_cur,dirname_new)) /* Need to create target directory? */
    {
        if (!Make_Dir(dirname_cur,dirname_new))
        {
            gchar *msg;
            GtkWidget *msgdialog;

            /* Renaming file has failed, but we try to set the initial name */
            rename(tmp_filename,cur_filename);

            dirname_new_utf8 = filename_to_display(dirname_new);
            if (!hide_msgbox)
            {
                msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   _("Cannot create target directory '%s'"),
                                                   dirname_new_utf8);
                gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
                gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename File Error"));
                gtk_dialog_run(GTK_DIALOG(msgdialog));
                gtk_widget_destroy(msgdialog);
            }

            msg = g_strdup_printf(_("Cannot create target directory '%s': %s"),
                                  dirname_new_utf8,g_strerror(errno));
            Log_Print(LOG_ERROR,"%s", msg);
            g_free(dirname_new_utf8);
            g_free(msg);

            g_free(tmp_filename);
            g_free(cur_basename_utf8);
            g_free(new_basename_utf8);
            g_free(dirname_cur);
            g_free(dirname_new);
            return FALSE;
        }
    }

    /* Now, we rename the file to his final name */
    if ( rename(tmp_filename,new_filename)==0 )
    {
        /* Renaming file has succeeded */
        Log_Print(LOG_OK,_("Renamed file '%s' to '%s'"),cur_basename_utf8,new_basename_utf8);

        ETFile->FileNameCur = ETFile->FileNameNew;
        /* Now the file was renamed, so mark his state */
        ET_Mark_File_Name_As_Saved(ETFile);

        Statusbar_Message (_("File(s) renamed"), TRUE);

        /* Remove the of directory (check automatically if it is empty) */
        if (!Remove_Dir(dirname_cur,dirname_new))
        {
            gchar *msg;
            GtkWidget *msgdialog;

            /* Removing directories failed */
            dirname_cur_utf8 = filename_to_display(dirname_cur);
            if (!hide_msgbox)
            {
                msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   _("Cannot remove old directory '%s'"),
                                                   dirname_cur_utf8);
                gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
                gtk_window_set_title(GTK_WINDOW(msgdialog),_("Remove Directory Error"));
                gtk_dialog_run(GTK_DIALOG(msgdialog));
                gtk_widget_destroy(msgdialog);
            }

            msg = g_strdup_printf(_("Cannot remove old directory '%s': %s"),
                                  dirname_cur_utf8,g_strerror(errno));
            g_free(dirname_cur_utf8);
            Log_Print(LOG_ERROR,"%s", msg);
            g_free(msg);

            g_free(tmp_filename);
            g_free(cur_basename_utf8);
            g_free(new_basename_utf8);
            g_free(dirname_cur);
            g_free(dirname_new);
            return FALSE;
        }
    }else if ( errno==EXDEV )
    {
        /* For the error "Invalid cross-device link" during renaming, when the
         * new filename isn't on the same device of the omd filename. In this
         * case, we need to move the file, and not only to update the hard disk
         * index table. For example : 'renaming' /mnt/D/file.mp3 to /mnt/E/file.mp3
         *
         * So, we need to copy the old file to the new location, and then to
         * deleted the old file */
        if ( Copy_File(tmp_filename,new_filename) )
        {
            /* Delete the old file */
            unlink(tmp_filename);

            /* Renaming file has succeeded */
            Log_Print(LOG_OK,_("Moved file '%s' to '%s'"),cur_basename_utf8,new_basename_utf8);

            ETFile->FileNameCur = ETFile->FileNameNew;
            /* Now the file was renamed, so mark his state */
            ET_Mark_File_Name_As_Saved(ETFile);

            Statusbar_Message (_("File(s) moved"), TRUE);

            /* Remove the of directory (check automatically if it is empty) */
            if (!Remove_Dir(dirname_cur,dirname_new))
            {
                gchar *msg;
                GtkWidget *msgdialog;

                /* Removing directories failed */
                dirname_cur_utf8 = filename_to_display(dirname_cur);
                if (!hide_msgbox)
                {
                    msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                       GTK_MESSAGE_ERROR,
                                                       GTK_BUTTONS_CLOSE,
                                                       _("Cannot remove old directory '%s'"),
                                                       dirname_cur_utf8);
                    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
                    gtk_window_set_title(GTK_WINDOW(msgdialog),_("Remove Directory Error"));

                    gtk_dialog_run(GTK_DIALOG(msgdialog));
                    gtk_widget_destroy(msgdialog);
                }
                msg = g_strdup_printf(_("Cannot remove old directory '%s': (%s)"),
                                      dirname_cur_utf8,g_strerror(errno));
                g_free(dirname_cur_utf8);

                Log_Print(LOG_ERROR,"%s", msg);
                g_free(msg);

                g_free(tmp_filename);
                g_free(cur_basename_utf8);
                g_free(new_basename_utf8);
                g_free(dirname_cur);
                g_free(dirname_new);
                return FALSE;
            }
        }else
        {
            gchar *msg;
            GtkWidget *msgdialog;

            /* Moving file has failed */
            if (!hide_msgbox)
            {
                msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   _("Cannot move file '%s' to '%s'"),
                                                   cur_basename_utf8,
                                                   new_basename_utf8);
                gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
                gtk_window_set_title(GTK_WINDOW(msgdialog),_("File Move Error"));

                gtk_dialog_run(GTK_DIALOG(msgdialog));
                gtk_widget_destroy(msgdialog);
            }

            msg = g_strdup_printf(_("Cannot move file '%s' to '%s': (%s)"),
                                  cur_basename_utf8,new_basename_utf8,g_strerror(errno));
            Log_Print(LOG_ERROR,"%s", msg);
            g_free(msg);

            Statusbar_Message (_("File(s) not moved"), TRUE);

            g_free(tmp_filename);
            g_free(cur_basename_utf8);
            g_free(new_basename_utf8);
            g_free(dirname_cur);
            g_free(dirname_new);
            return FALSE;
        }
    }else
    {
        GtkWidget *msgdialog;

        /* Renaming file has failed, but we try to set the initial name */
        rename(tmp_filename,cur_filename);

        if (!hide_msgbox)
        {
            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               _("Cannot rename file '%s' to '%s'"),
                                               cur_basename_utf8,
                                               new_basename_utf8);
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename File Error"));

            gtk_dialog_run(GTK_DIALOG(msgdialog));
            gtk_widget_destroy(msgdialog);
        }

        Log_Print (LOG_ERROR, _("Cannot rename file '%s' to '%s': %s"),
                              cur_basename_utf8, new_basename_utf8,
                              g_strerror (errno));

        Statusbar_Message (_("File(s) not renamed"), TRUE);

        g_free(tmp_filename);
        g_free(cur_basename_utf8);
        g_free(new_basename_utf8);
        g_free(dirname_cur);
        g_free(dirname_new);
        return FALSE;
    }

    return TRUE;
}

/*
 * Delete the file ETFile
 */
static gint
Delete_File (ET_File *ETFile, gboolean multiple_files)
{
    GtkWidget *msgdialog;
    GtkWidget *msgdialog_check_button = NULL;
    gchar *cur_filename;
    gchar *cur_filename_utf8;
    gchar *basename_utf8;
    gint response;
    gint stop_loop;

    g_return_val_if_fail (ETFile != NULL, FALSE);

    /* Filename of the file to delete. */
    cur_filename      = ((File_Name *)(ETFile->FileNameCur)->data)->value;
    cur_filename_utf8 = ((File_Name *)(ETFile->FileNameCur)->data)->value_utf8;
    basename_utf8 = g_path_get_basename (cur_filename_utf8);

    /*
     * Remove the file
     */
    if (g_settings_get_boolean (ETSettings, "confirm-delete-file")
        && !SF_HideMsgbox_Delete_File)
    {
        if (multiple_files)
        {
            GtkWidget *message_area;
            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               _("Do you really want to delete the file '%s'?"),
                                               basename_utf8);
            message_area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(msgdialog));
            msgdialog_check_button = gtk_check_button_new_with_label(_("Repeat action for the remaining files"));
            gtk_container_add(GTK_CONTAINER(message_area),msgdialog_check_button);
            gtk_dialog_add_buttons(GTK_DIALOG(msgdialog),GTK_STOCK_NO,GTK_RESPONSE_NO,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_DELETE,GTK_RESPONSE_YES,NULL);
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Delete File"));
            //GTK_TOGGLE_BUTTON(msgbox_check_button)->active = TRUE; // Checked by default
        }else
        {
            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               _("Do you really want to delete the file '%s'?"),
                                               basename_utf8);
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Delete File"));
            gtk_dialog_add_buttons(GTK_DIALOG(msgdialog),GTK_STOCK_CANCEL,GTK_RESPONSE_NO,GTK_STOCK_DELETE,GTK_RESPONSE_YES,NULL);
        }
        SF_ButtonPressed_Delete_File = response = gtk_dialog_run(GTK_DIALOG(msgdialog));
        if (msgdialog_check_button && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button)))
            SF_HideMsgbox_Delete_File = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button));
        gtk_widget_destroy(msgdialog);
    }else
    {
        if (SF_HideMsgbox_Delete_File)
            response = SF_ButtonPressed_Delete_File;
        else
            response = GTK_RESPONSE_YES;
    }

    switch (response)
    {
        case GTK_RESPONSE_YES:
            if (remove(cur_filename)==0)
            {
                gchar *msg = g_strdup_printf(_("File '%s' deleted"), basename_utf8);
                Statusbar_Message(msg,FALSE);
                g_free(msg);
                g_free(basename_utf8);
                return 1;
            }
            break;
        case GTK_RESPONSE_NO:
            break;
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_NONE:
            stop_loop = -1;
            g_free(basename_utf8);
            return stop_loop;
            break;
    }

    g_free(basename_utf8);
    return 0;
}

/*
 * Copy a file to a new location
 */
static gboolean
Copy_File (const gchar *fileold, const gchar *filenew)
{
    FILE* fOld;
    FILE* fNew;
    gchar buffer[512];
    gint NbRead;
    struct stat    statbuf;
    struct utimbuf utimbufbuf;

    if ( (fOld=fopen(fileold, "rb")) == NULL )
    {
        return FALSE;
    }

    if ( (fNew=fopen(filenew, "wb")) == NULL )
    {
        fclose(fOld);
        return FALSE;
    }

    while ( (NbRead=fread(buffer, 1, 512, fOld)) != 0 )
    {
        fwrite(buffer, 1, NbRead, fNew);
    }

    fclose(fNew);
    fclose(fOld);

    // Copy properties of the old file to the new one.
    stat(fileold,&statbuf);
    chmod(filenew,statbuf.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO));
    chown(filenew,statbuf.st_uid,statbuf.st_gid);
    utimbufbuf.actime  = statbuf.st_atime; // Last access time
    utimbufbuf.modtime = statbuf.st_mtime; // Last modification time
    utime(filenew,&utimbufbuf);

    return TRUE;
}

void
Action_Select_Browser_Style (void)
{
    g_return_if_fail (ETCore->ETFileDisplayedList != NULL);

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    Browser_Display_Tree_Or_Artist_Album_List();

    Update_Command_Buttons_Sensivity();
}





/*
 * Scans the specified directory: and load files into a list.
 * If the path doesn't exist, we free the previous loaded list of files.
 */
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
gboolean Read_Directory (gchar *path_real)
{
    DIR   *dir;
    gchar *msg;
    gchar  progress_bar_text[30];
    guint  nbrfile = 0;
    double fraction;
    GList *FileList = NULL;
    gint   progress_bar_index = 0;
    GtkAction *uiaction;
    GtkWidget *artist_radio;

    g_return_val_if_fail (path_real != NULL, FALSE);

    ReadingDirectory = TRUE;    /* A flag to avoid to start another reading */

    /* Initialize file list */
    ET_Core_Free();
    ET_Core_Initialize();
    Update_Command_Buttons_Sensivity();

    /* Initialize browser list */
    Browser_List_Clear();

    /* Clear entry boxes  */
    Clear_File_Entry_Field();
    Clear_Header_Fields();
    Clear_Tag_Entry_Fields();
    gtk_label_set_text(GTK_LABEL(FileIndex),"0/0:");

    artist_radio = gtk_ui_manager_get_widget (UIManager, "/ToolBar/ArtistViewMode");
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (artist_radio),
                                       FALSE);
    //Browser_Display_Tree_Or_Artist_Album_List(); // To show the corresponding lists...

    // Set to unsensitive the Browser Area, to avoid to select another file while loading the first one
    Browser_Area_Set_Sensitive(FALSE);

    /* Placed only here, to empty the previous list of files */
    if ((dir=opendir(path_real)) == NULL)
    {
        // Message if the directory doesn't exist...
        GtkWidget *msgdialog;
        gchar *path_utf8 = filename_to_display(path_real);

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("Cannot read directory '%s'"),
                                           path_utf8);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Directory Read Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        g_free(path_utf8);

        ReadingDirectory = FALSE; //Allow a new reading
        Browser_Area_Set_Sensitive(TRUE);
        return FALSE;
    }
    closedir(dir);

    /* Open the window to quit recursion (since 27/04/2007 : not only into recursion mode) */
    Set_Busy_Cursor();
    uiaction = gtk_ui_manager_get_action(UIManager, "/ToolBar/Stop");
    g_settings_bind (ETSettings, "browse-subdir", uiaction, "sensitive",
                     G_SETTINGS_BIND_GET);
    Open_Quit_Recursion_Function_Window();

    /* Read the directory recursively */
    msg = g_strdup_printf(_("Search in progress…"));
    Statusbar_Message(msg,FALSE);
    g_free(msg);
    /* Search the supported files. */
    FileList = Read_Directory_Recursively (FileList, path_real,
                                           g_settings_get_boolean (ETSettings,
                                                                   "browse-subdir"));
    nbrfile = g_list_length(FileList);

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), 0.0);
    g_snprintf(progress_bar_text, 30, "%d/%d", 0, nbrfile);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), progress_bar_text);

    // Load the supported files (Extension recognized)
    while (FileList)
    {
        gchar *filename_real = FileList->data; // Contains real filenames
        gchar *filename_utf8 = filename_to_display(filename_real);

        msg = g_strdup_printf(_("File: '%s'"),filename_utf8);
        Statusbar_Message(msg,FALSE);
        g_free(msg);
        g_free(filename_utf8);

        // Warning: Do not free filename_real because ET_Add_File.. uses it for internal structures
        ET_Add_File_To_File_List(filename_real);

        // Update the progress bar
        fraction = (++progress_bar_index) / (double) nbrfile;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), fraction);
        g_snprintf(progress_bar_text, 30, "%d/%d", progress_bar_index, nbrfile);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), progress_bar_text);
        while (gtk_events_pending())
            gtk_main_iteration();

        if (!FileList->next || Main_Stop_Button_Pressed) break;
        FileList = FileList->next;
    }
    if (FileList) g_list_free(FileList);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ProgressBar), "");

    /* Close window to quit recursion */
    Destroy_Quit_Recursion_Function_Window();
    Main_Stop_Button_Pressed = FALSE;
    uiaction = gtk_ui_manager_get_action(UIManager, "/ToolBar/Stop");
    g_object_set(uiaction, "sensitive", FALSE, NULL);

    //ET_Debug_Print_File_List(ETCore->ETFileList,__FILE__,__LINE__,__FUNCTION__);

    if (ETCore->ETFileList)
    {
        //GList *etfilelist;
        /* Load the list of file into the browser list widget */
        Browser_Display_Tree_Or_Artist_Album_List();

        /* Load the list attached to the TrackEntry */
        Load_Track_List_To_UI();

        /* Display the first file */
        //No need to select first item, because Browser_Display_Tree_Or_Artist_Album_List() does this
        //etfilelist = ET_Displayed_File_List_First();
        //if (etfilelist)
        //{
        //    ET_Display_File_Data_To_UI((ET_File *)etfilelist->data);
        //    Browser_List_Select_File_By_Etfile((ET_File *)etfilelist->data,FALSE);
        //}

        /* Prepare message for the status bar */
        if (g_settings_get_boolean (ETSettings, "browse-subdir"))
            msg = g_strdup_printf(_("Found %d file(s) in this directory and subdirectories."),ETCore->ETFileDisplayedList_Length);
        else
            msg = g_strdup_printf(_("Found %d file(s) in this directory."),ETCore->ETFileDisplayedList_Length);

    }else
    {
        /* Clear entry boxes */
        Clear_File_Entry_Field();
        Clear_Header_Fields();
        Clear_Tag_Entry_Fields();

        if (FileIndex)
            gtk_label_set_text(GTK_LABEL(FileIndex),"0/0:");



	/* Translators: No files, as in "0 files". */
        Browser_Label_Set_Text(_("No files")); /* See in ET_Display_Filename_To_UI */

        /* Prepare message for the status bar */
        if (g_settings_get_boolean (ETSettings, "browse-subdir"))
            msg = g_strdup(_("No file found in this directory and subdirectories"));
        else
            msg = g_strdup(_("No file found in this directory"));
    }

    /* Update sensitivity of buttons and menus */
    Update_Command_Buttons_Sensivity();

    Browser_Area_Set_Sensitive(TRUE);

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), 0.0);
    Statusbar_Message(msg,FALSE);
    g_free(msg);
    Set_Unbusy_Cursor();
    ReadingDirectory = FALSE;

    return TRUE;
}



/*
 * Recurse the path to create a list of files. Return a GList of the files found.
 */
static GList *
Read_Directory_Recursively (GList *file_list, const gchar *path_real,
                            gboolean recurse)
{
    DIR *dir;
    struct dirent *dirent;
    struct stat statbuf;
    gchar *filename;

    if ((dir = opendir(path_real)) == NULL)
        return file_list;

    while ((dirent = readdir(dir)) != NULL)
    {
        if (Main_Stop_Button_Pressed)
        {
            closedir(dir);
            return file_list;
        }

        /* We do not read the directories '.' and '..', but may read hidden
         * directories like '.mydir'. */
        if ((g_ascii_strcasecmp (dirent->d_name, "..") != 0)
            && ((g_ascii_strncasecmp (dirent->d_name, ".", 1) != 0)
            || (g_settings_get_boolean (ETSettings, "browse-show-hidden")
            && strlen (dirent->d_name) > 1)))
        {
            if (path_real[strlen(path_real)-1]!=G_DIR_SEPARATOR)
                filename = g_strconcat(path_real,G_DIR_SEPARATOR_S,dirent->d_name,NULL);
            else
                filename = g_strconcat(path_real,dirent->d_name,NULL);

            if (stat(filename, &statbuf) == -1)
            {
                g_free(filename);
                continue;
            }

            if (S_ISDIR(statbuf.st_mode))
            {
                if (recurse)
                    file_list = Read_Directory_Recursively(file_list,filename,recurse);
            //}else if ( (S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) && ET_File_Is_Supported(filename))
            }else if ( S_ISREG(statbuf.st_mode) && ET_File_Is_Supported(filename))
            {
                file_list = g_list_append(file_list,g_strdup(filename));
            }
            g_free(filename);

            // Just to not block X events
            while (gtk_events_pending())
                gtk_main_iteration();
        }
    }

    closedir(dir);
    return file_list;
}


/*
 * Window with the 'STOP' button to stop recursion when reading directories
 */
static void
Open_Quit_Recursion_Function_Window (void)
{
    GtkWidget *button;
    GtkWidget *frame;

    if (QuitRecursionWindow != NULL)
        return;
    QuitRecursionWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(QuitRecursionWindow),_("Searching…"));
    gtk_window_set_transient_for(GTK_WINDOW(QuitRecursionWindow),GTK_WINDOW(MainWindow));
    //gtk_window_set_policy(GTK_WINDOW(QuitRecursionWindow),FALSE,FALSE,TRUE);

    // Just center on mainwindow
    gtk_window_set_position(GTK_WINDOW(QuitRecursionWindow), GTK_WIN_POS_CENTER_ON_PARENT);

    g_signal_connect(G_OBJECT(QuitRecursionWindow),"destroy",
                     G_CALLBACK(Quit_Recursion_Function_Button_Pressed),NULL);
    g_signal_connect(G_OBJECT(QuitRecursionWindow),"delete_event",
                     G_CALLBACK(Quit_Recursion_Function_Button_Pressed),NULL);
    g_signal_connect(G_OBJECT(QuitRecursionWindow),"key_press_event",
                     G_CALLBACK(Quit_Recursion_Window_Key_Press),NULL);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(QuitRecursionWindow),frame);
    gtk_container_set_border_width(GTK_CONTAINER(frame),2);

    // Button to stop...
    button = Create_Button_With_Icon_And_Label(GTK_STOCK_STOP,_("  STOP the search...  "));
    gtk_container_set_border_width(GTK_CONTAINER(button),8);
    gtk_container_add(GTK_CONTAINER(frame),button);
    g_signal_connect(G_OBJECT(button),"clicked",G_CALLBACK(Quit_Recursion_Function_Button_Pressed),NULL);

    gtk_widget_show_all(QuitRecursionWindow);
}

static void
Destroy_Quit_Recursion_Function_Window (void)
{
    if (QuitRecursionWindow)
    {
        gtk_widget_destroy(QuitRecursionWindow);
        QuitRecursionWindow = NULL;
        /*Statusbar_Message(_("Recursive file search interrupted."),FALSE);*/
    }
}

static void
Quit_Recursion_Function_Button_Pressed (void)
{
    Action_Main_Stop_Button_Pressed();
    Destroy_Quit_Recursion_Function_Window();
}

static void
Quit_Recursion_Window_Key_Press (GtkWidget *window, GdkEvent *event)
{
    GdkEventKey *kevent;

    if (event && event->type == GDK_KEY_PRESS)
    {
        kevent = (GdkEventKey *)event;
        switch(kevent->keyval)
        {
            case GDK_KEY_Escape:
                Destroy_Quit_Recursion_Function_Window();
                break;
        }
    }
}


/*
 * To stop the recursive search within directories or saving files
 */
void Action_Main_Stop_Button_Pressed (void)
{
    GtkAction *uiaction;
    Main_Stop_Button_Pressed = TRUE;
    uiaction = gtk_ui_manager_get_action(UIManager, "/ToolBar/Stop");
    g_object_set(uiaction, "sensitive", FALSE, NULL);
}

static void
ui_widget_set_sensitive (const gchar *menu, const gchar *action, gboolean sensitive)
{
    GtkAction *uiaction;
    gchar *path;

    path = g_strconcat("/MenuBar/", menu,"/", action, NULL);

    uiaction = gtk_ui_manager_get_action(UIManager, path);
    if (uiaction)
        g_object_set(uiaction, "sensitive", sensitive, NULL);
    g_free(path);
}

/*
 * Update_Command_Buttons_Sensivity: Set to sensitive/unsensitive the state of each button into
 * the commands area and menu items in function of state of the "main list".
 */
void Update_Command_Buttons_Sensivity (void)
{
    GtkAction *uiaction;

    if (!ETCore->ETFileDisplayedList)
    {
        /* No file found */

        /* File and Tag frames */
        File_Area_Set_Sensitive(FALSE);
        Tag_Area_Set_Sensitive(FALSE);

        /* Tool bar buttons (the others are covered by the menu) */
        uiaction = gtk_ui_manager_get_action(UIManager, "/ToolBar/Stop");
        g_object_set(uiaction, "sensitive", FALSE, NULL);

        /* Scanner Window */
        if (SWScanButton)
            gtk_widget_set_sensitive(GTK_WIDGET(SWScanButton),FALSE);

        /* Menu commands */
        ui_widget_set_sensitive(MENU_FILE, AM_OPEN_FILE_WITH, FALSE);
        ui_widget_set_sensitive (MENU_FILE, AM_SELECT_ALL, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_UNSELECT_ALL_FILES, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_INVERT_SELECTION, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_DELETE_FILE, FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_ASCENDING_FILENAME, FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_DESCENDING_FILENAME,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_ASCENDING_CREATION_DATE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_DESCENDING_CREATION_DATE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_ASCENDING_TRACK_NUMBER,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_DESCENDING_TRACK_NUMBER,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_ASCENDING_TITLE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_DESCENDING_TITLE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_ASCENDING_ARTIST,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_DESCENDING_ARTIST,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_ASCENDING_ALBUM,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_DESCENDING_ALBUM,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_ASCENDING_YEAR,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_DESCENDING_YEAR,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_ASCENDING_GENRE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_DESCENDING_GENRE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_ASCENDING_COMMENT,FALSE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH, AM_SORT_DESCENDING_COMMENT,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_ASCENDING_FILE_TYPE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_DESCENDING_FILE_TYPE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_ASCENDING_FILE_SIZE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_DESCENDING_FILE_SIZE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_ASCENDING_FILE_DURATION,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_DESCENDING_FILE_DURATION,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_ASCENDING_FILE_BITRATE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_DESCENDING_FILE_BITRATE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_ASCENDING_FILE_SAMPLERATE,FALSE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH, AM_SORT_DESCENDING_FILE_SAMPLERATE,FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_PREV, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_NEXT, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_FIRST, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_LAST, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_SCAN, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_REMOVE, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_UNDO, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_REDO, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_SAVE, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_SAVE_FORCED, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_UNDO_HISTORY, FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_REDO_HISTORY, FALSE);
        ui_widget_set_sensitive(MENU_MISC, AM_SEARCH_FILE, FALSE);
        ui_widget_set_sensitive(MENU_MISC, AM_FILENAME_FROM_TXT, FALSE);
        ui_widget_set_sensitive(MENU_MISC, AM_WRITE_PLAYLIST, FALSE);
        ui_widget_set_sensitive(MENU_MISC, AM_RUN_AUDIO_PLAYER, FALSE);
        ui_widget_set_sensitive(MENU_SCANNER, AM_SCANNER_FILL_TAG, FALSE);
        ui_widget_set_sensitive(MENU_SCANNER, AM_SCANNER_RENAME_FILE, FALSE);
        ui_widget_set_sensitive(MENU_SCANNER, AM_SCANNER_PROCESS_FIELDS, FALSE);

        return;
    }else
    {
        GList *selfilelist = NULL;
        ET_File *etfile;
        gboolean has_undo = FALSE;
        gboolean has_redo = FALSE;
        //gboolean has_to_save = FALSE;
        GtkTreeSelection *selection;

        /* File and Tag frames */
        File_Area_Set_Sensitive(TRUE);
        Tag_Area_Set_Sensitive(TRUE);

        /* Tool bar buttons */
        uiaction = gtk_ui_manager_get_action(UIManager, "/ToolBar/Stop");
        g_object_set(uiaction, "sensitive", FALSE, NULL);

        /* Scanner Window */
        if (SWScanButton)    gtk_widget_set_sensitive(GTK_WIDGET(SWScanButton),TRUE);

        /* Commands into menu */
        ui_widget_set_sensitive(MENU_FILE, AM_OPEN_FILE_WITH,TRUE);
        ui_widget_set_sensitive (MENU_FILE, AM_SELECT_ALL, TRUE);
        ui_widget_set_sensitive(MENU_FILE, AM_UNSELECT_ALL_FILES,TRUE);
        ui_widget_set_sensitive(MENU_FILE, AM_INVERT_SELECTION,TRUE);
        ui_widget_set_sensitive(MENU_FILE, AM_DELETE_FILE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_ASCENDING_FILENAME,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_DESCENDING_FILENAME,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_ASCENDING_CREATION_DATE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_DESCENDING_CREATION_DATE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_ASCENDING_TRACK_NUMBER,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_DESCENDING_TRACK_NUMBER,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_ASCENDING_TITLE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_DESCENDING_TITLE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_ASCENDING_ARTIST,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_DESCENDING_ARTIST,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_ASCENDING_ALBUM,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_DESCENDING_ALBUM,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_ASCENDING_YEAR,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_DESCENDING_YEAR,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_ASCENDING_GENRE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_DESCENDING_GENRE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_ASCENDING_COMMENT,TRUE);
        ui_widget_set_sensitive(MENU_SORT_TAG_PATH,AM_SORT_DESCENDING_COMMENT,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_ASCENDING_FILE_TYPE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_DESCENDING_FILE_TYPE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_ASCENDING_FILE_SIZE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_DESCENDING_FILE_SIZE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_ASCENDING_FILE_DURATION,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_DESCENDING_FILE_DURATION,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_ASCENDING_FILE_BITRATE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_DESCENDING_FILE_BITRATE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_ASCENDING_FILE_SAMPLERATE,TRUE);
        ui_widget_set_sensitive(MENU_SORT_PROP_PATH,AM_SORT_DESCENDING_FILE_SAMPLERATE,TRUE);
        ui_widget_set_sensitive(MENU_FILE,AM_SCAN,TRUE);
        ui_widget_set_sensitive(MENU_FILE,AM_REMOVE,TRUE);
        ui_widget_set_sensitive(MENU_MISC,AM_SEARCH_FILE,TRUE);
        ui_widget_set_sensitive(MENU_MISC,AM_FILENAME_FROM_TXT,TRUE);
        ui_widget_set_sensitive(MENU_MISC,AM_WRITE_PLAYLIST,TRUE);
        ui_widget_set_sensitive(MENU_MISC,AM_RUN_AUDIO_PLAYER,TRUE);
        ui_widget_set_sensitive(MENU_SCANNER,AM_SCANNER_FILL_TAG,TRUE);
        ui_widget_set_sensitive(MENU_SCANNER,AM_SCANNER_RENAME_FILE,TRUE);
        ui_widget_set_sensitive(MENU_SCANNER,AM_SCANNER_PROCESS_FIELDS,TRUE);

        /* Check if one of the selected files has undo or redo data */
        if (BrowserList)
        {
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
            selfilelist = gtk_tree_selection_get_selected_rows(selection, NULL);
            while (selfilelist)
            {
                etfile = Browser_List_Get_ETFile_From_Path(selfilelist->data);
                has_undo    |= ET_File_Data_Has_Undo_Data(etfile);
                has_redo    |= ET_File_Data_Has_Redo_Data(etfile);
                //has_to_save |= ET_Check_If_File_Is_Saved(etfile);
                if ((has_undo && has_redo /*&& has_to_save*/) || !selfilelist->next) // Useless to check the other files
                    break;
                selfilelist = g_list_next(selfilelist);
            }
            g_list_foreach(selfilelist, (GFunc) gtk_tree_path_free, NULL);
            g_list_free(selfilelist);
        }

        /* Enable undo commands if there are undo data */
        if (has_undo)
            ui_widget_set_sensitive(MENU_FILE, AM_UNDO, TRUE);
        else
            ui_widget_set_sensitive(MENU_FILE, AM_UNDO, FALSE);

        /* Enable redo commands if there are redo data */
        if (has_redo)
            ui_widget_set_sensitive(MENU_FILE, AM_REDO, TRUE);
        else
            ui_widget_set_sensitive(MENU_FILE, AM_REDO, FALSE);

        /* Enable save file command if file has been changed */
        // Desactivated because problem with only one file in the list, as we can't change the selected file => can't mark file as changed
        /*if (has_to_save)
            ui_widget_set_sensitive(MENU_FILE, AM_SAVE, FALSE);
        else*/
            ui_widget_set_sensitive(MENU_FILE, AM_SAVE, TRUE);
        
        ui_widget_set_sensitive(MENU_FILE, AM_SAVE_FORCED, TRUE);

        /* Enable undo command if there are data into main undo list (history list) */
        if (ET_History_File_List_Has_Undo_Data())
            ui_widget_set_sensitive(MENU_FILE, AM_UNDO_HISTORY, TRUE);
        else
            ui_widget_set_sensitive(MENU_FILE, AM_UNDO_HISTORY, FALSE);

        /* Enable redo commands if there are data into main redo list (history list) */
        if (ET_History_File_List_Has_Redo_Data())
            ui_widget_set_sensitive(MENU_FILE, AM_REDO_HISTORY, TRUE);
        else
            ui_widget_set_sensitive(MENU_FILE, AM_REDO_HISTORY, FALSE);
    }

    if (!ETCore->ETFileDisplayedList->prev)    /* Is it the 1st item ? */
    {
        ui_widget_set_sensitive(MENU_FILE, AM_PREV,FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_FIRST,FALSE);
    }else
    {
        ui_widget_set_sensitive(MENU_FILE, AM_PREV,TRUE);
        ui_widget_set_sensitive(MENU_FILE, AM_FIRST,TRUE);
    }
    if (!ETCore->ETFileDisplayedList->next)    /* Is it the last item ? */
    {
        ui_widget_set_sensitive(MENU_FILE, AM_NEXT,FALSE);
        ui_widget_set_sensitive(MENU_FILE, AM_LAST,FALSE);
    }else
    {
        ui_widget_set_sensitive(MENU_FILE, AM_NEXT,TRUE);
        ui_widget_set_sensitive(MENU_FILE, AM_LAST,TRUE);
    }
}

/*
 * Just to disable buttons when we are saving files (do not disable Quit button)
 */
static void
Disable_Command_Buttons (void)
{
    /* Scanner Window */
    if (SWScanButton)
        gtk_widget_set_sensitive(SWScanButton,FALSE);

    /* "File" menu commands */
    ui_widget_set_sensitive(MENU_FILE,AM_OPEN_FILE_WITH,FALSE);
    ui_widget_set_sensitive (MENU_FILE, AM_SELECT_ALL, FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_UNSELECT_ALL_FILES,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_INVERT_SELECTION,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_DELETE_FILE,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_FIRST,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_PREV,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_NEXT,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_LAST,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_SCAN,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_REMOVE,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_UNDO,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_REDO,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_SAVE,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_SAVE_FORCED,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_UNDO_HISTORY,FALSE);
    ui_widget_set_sensitive(MENU_FILE,AM_REDO_HISTORY,FALSE);

    /* "Scanner" menu commands */
    ui_widget_set_sensitive(MENU_SCANNER,AM_SCANNER_FILL_TAG,FALSE);
    ui_widget_set_sensitive(MENU_SCANNER,AM_SCANNER_RENAME_FILE,FALSE);
    ui_widget_set_sensitive(MENU_SCANNER,AM_SCANNER_PROCESS_FIELDS,FALSE);

}

/*
 * Disable (FALSE) / Enable (TRUE) all user widgets in the tag area
 */
static void
Tag_Area_Set_Sensitive (gboolean activate)
{
    g_return_if_fail (TagArea != NULL);

    /* TAG Area (entries + buttons). */
    gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(TagArea)),activate);
}

/*
 * Disable (FALSE) / Enable (TRUE) all user widgets in the file area
 */
static void
File_Area_Set_Sensitive (gboolean activate)
{
    g_return_if_fail (FileArea != NULL);

    /* File Area. */
    gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(FileArea)),activate);
    /*gtk_widget_set_sensitive(GTK_WIDGET(FileEntry),activate);*/
}

/*
 * Display controls according the kind of tag... (Hide some controls if not available for a tag type)
 */
void Tag_Area_Display_Controls (ET_File *ETFile)
{
    g_return_if_fail (ETFile != NULL || ETFile->ETFileDescription != NULL ||
                      TitleLabel != NULL);

    /* Common controls for all tags. */
    gtk_widget_show(GTK_WIDGET(TitleLabel));
    gtk_widget_show(GTK_WIDGET(TitleEntry));
    gtk_widget_show(GTK_WIDGET(ArtistLabel));
    gtk_widget_show(GTK_WIDGET(ArtistEntry));
    gtk_widget_show(GTK_WIDGET(AlbumArtistLabel));
    gtk_widget_show(GTK_WIDGET(AlbumArtistEntry));
    gtk_widget_show(GTK_WIDGET(AlbumLabel));
    gtk_widget_show(GTK_WIDGET(AlbumEntry));
    gtk_widget_show(GTK_WIDGET(YearLabel));
    gtk_widget_show(GTK_WIDGET(YearEntry));
    gtk_widget_show(GTK_WIDGET(TrackLabel));
    gtk_widget_show(GTK_WIDGET(TrackEntryCombo));
    gtk_widget_show(GTK_WIDGET(TrackTotalEntry));
    gtk_widget_show(GTK_WIDGET(TrackMButtonSequence));
    gtk_widget_show(GTK_WIDGET(TrackMButtonNbrFiles));
    gtk_widget_show(GTK_WIDGET(GenreLabel));
    gtk_widget_show(GTK_WIDGET(GenreCombo));
    gtk_widget_show(GTK_WIDGET(CommentLabel));
    gtk_widget_show(GTK_WIDGET(CommentEntry));

    // Special controls to display or not!
    switch (ETFile->ETFileDescription->TagType)
    {
        case ID3_TAG:
            if (!g_settings_get_boolean (ETSettings, "id3v2-enabled"))
            {
                // ID3v1 : Hide specifics ID3v2 fields if not activated!
                gtk_widget_hide(GTK_WIDGET(DiscNumberLabel));
                gtk_widget_hide(GTK_WIDGET(DiscNumberEntry));
                gtk_widget_hide(GTK_WIDGET(ComposerLabel));
                gtk_widget_hide(GTK_WIDGET(ComposerEntry));
                gtk_widget_hide(GTK_WIDGET(OrigArtistLabel));
                gtk_widget_hide(GTK_WIDGET(OrigArtistEntry));
                gtk_widget_hide(GTK_WIDGET(CopyrightLabel));
                gtk_widget_hide(GTK_WIDGET(CopyrightEntry));
                gtk_widget_hide(GTK_WIDGET(URLLabel));
                gtk_widget_hide(GTK_WIDGET(URLEntry));
                gtk_widget_hide(GTK_WIDGET(EncodedByLabel));
                gtk_widget_hide(GTK_WIDGET(EncodedByEntry));
                gtk_widget_hide(GTK_WIDGET(PictureScrollWindow));
                gtk_widget_hide (GTK_WIDGET (apply_image_toolitem));
                gtk_widget_hide (GTK_WIDGET (remove_image_toolitem));
                gtk_widget_hide (GTK_WIDGET (add_image_toolitem));
                gtk_widget_hide (GTK_WIDGET (save_image_toolitem));
                gtk_widget_hide (GTK_WIDGET (image_properties_toolitem));
            }else
            {
                gtk_widget_show(GTK_WIDGET(DiscNumberLabel));
                gtk_widget_show(GTK_WIDGET(DiscNumberEntry));
                gtk_widget_show(GTK_WIDGET(ComposerLabel));
                gtk_widget_show(GTK_WIDGET(ComposerEntry));
                gtk_widget_show(GTK_WIDGET(OrigArtistLabel));
                gtk_widget_show(GTK_WIDGET(OrigArtistEntry));
                gtk_widget_show(GTK_WIDGET(CopyrightLabel));
                gtk_widget_show(GTK_WIDGET(CopyrightEntry));
                gtk_widget_show(GTK_WIDGET(URLLabel));
                gtk_widget_show(GTK_WIDGET(URLEntry));
                gtk_widget_show(GTK_WIDGET(EncodedByLabel));
                gtk_widget_show(GTK_WIDGET(EncodedByEntry));
                gtk_widget_show(GTK_WIDGET(PictureScrollWindow));
                gtk_widget_show (GTK_WIDGET (apply_image_toolitem));
                gtk_widget_show (GTK_WIDGET (remove_image_toolitem));
                gtk_widget_show (GTK_WIDGET (add_image_toolitem));
                gtk_widget_show (GTK_WIDGET (save_image_toolitem));
                gtk_widget_show (GTK_WIDGET (image_properties_toolitem));
            }
            break;

#ifdef ENABLE_OGG
        case OGG_TAG:
            gtk_widget_show(GTK_WIDGET(DiscNumberLabel));
            gtk_widget_show(GTK_WIDGET(DiscNumberEntry));
            gtk_widget_show(GTK_WIDGET(ComposerLabel));
            gtk_widget_show(GTK_WIDGET(ComposerEntry));
            gtk_widget_show(GTK_WIDGET(OrigArtistLabel));
            gtk_widget_show(GTK_WIDGET(OrigArtistEntry));
            gtk_widget_show(GTK_WIDGET(CopyrightLabel));
            gtk_widget_show(GTK_WIDGET(CopyrightEntry));
            gtk_widget_show(GTK_WIDGET(URLLabel));
            gtk_widget_show(GTK_WIDGET(URLEntry));
            gtk_widget_show(GTK_WIDGET(EncodedByLabel));
            gtk_widget_show(GTK_WIDGET(EncodedByEntry));
            gtk_widget_show(GTK_WIDGET(PictureScrollWindow));
            gtk_widget_show (GTK_WIDGET (apply_image_toolitem));
            gtk_widget_show (GTK_WIDGET (remove_image_toolitem));
            gtk_widget_show (GTK_WIDGET (add_image_toolitem));
            gtk_widget_show (GTK_WIDGET (save_image_toolitem));
            gtk_widget_show (GTK_WIDGET (image_properties_toolitem));
            break;
#endif

#ifdef ENABLE_FLAC
        case FLAC_TAG:
            gtk_widget_show(GTK_WIDGET(DiscNumberLabel));
            gtk_widget_show(GTK_WIDGET(DiscNumberEntry));
            gtk_widget_show(GTK_WIDGET(ComposerLabel));
            gtk_widget_show(GTK_WIDGET(ComposerEntry));
            gtk_widget_show(GTK_WIDGET(OrigArtistLabel));
            gtk_widget_show(GTK_WIDGET(OrigArtistEntry));
            gtk_widget_show(GTK_WIDGET(CopyrightLabel));
            gtk_widget_show(GTK_WIDGET(CopyrightEntry));
            gtk_widget_show(GTK_WIDGET(URLLabel));
            gtk_widget_show(GTK_WIDGET(URLEntry));
            gtk_widget_show(GTK_WIDGET(EncodedByLabel));
            gtk_widget_show(GTK_WIDGET(EncodedByEntry));
            gtk_widget_show(GTK_WIDGET(PictureScrollWindow));
            gtk_widget_show (GTK_WIDGET (apply_image_toolitem));
            gtk_widget_show (GTK_WIDGET (remove_image_toolitem));
            gtk_widget_show (GTK_WIDGET (add_image_toolitem));
            gtk_widget_show (GTK_WIDGET (save_image_toolitem));
            gtk_widget_show (GTK_WIDGET (image_properties_toolitem));
            break;
#endif

        case APE_TAG:
            gtk_widget_show(GTK_WIDGET(DiscNumberLabel));
            gtk_widget_show(GTK_WIDGET(DiscNumberEntry));
            gtk_widget_show(GTK_WIDGET(ComposerLabel));
            gtk_widget_show(GTK_WIDGET(ComposerEntry));
            gtk_widget_show(GTK_WIDGET(OrigArtistLabel));
            gtk_widget_show(GTK_WIDGET(OrigArtistEntry));
            gtk_widget_show(GTK_WIDGET(CopyrightLabel));
            gtk_widget_show(GTK_WIDGET(CopyrightEntry));
            gtk_widget_show(GTK_WIDGET(URLLabel));
            gtk_widget_show(GTK_WIDGET(URLEntry));
            gtk_widget_show(GTK_WIDGET(EncodedByLabel));
            gtk_widget_show(GTK_WIDGET(EncodedByEntry));
            gtk_widget_hide(GTK_WIDGET(PictureScrollWindow));
            gtk_widget_hide (GTK_WIDGET (apply_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (remove_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (add_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (save_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (image_properties_toolitem));
            break;

#ifdef ENABLE_MP4
        case MP4_TAG:
            gtk_widget_hide(GTK_WIDGET(DiscNumberLabel));
            gtk_widget_hide(GTK_WIDGET(DiscNumberEntry));
            gtk_widget_hide(GTK_WIDGET(ComposerLabel));
            gtk_widget_hide(GTK_WIDGET(ComposerEntry));
            gtk_widget_hide(GTK_WIDGET(OrigArtistLabel));
            gtk_widget_hide(GTK_WIDGET(OrigArtistEntry));
            gtk_widget_hide(GTK_WIDGET(CopyrightLabel));
            gtk_widget_hide(GTK_WIDGET(CopyrightEntry));
            gtk_widget_hide(GTK_WIDGET(URLLabel));
            gtk_widget_hide(GTK_WIDGET(URLEntry));
            gtk_widget_hide(GTK_WIDGET(EncodedByLabel));
            gtk_widget_hide(GTK_WIDGET(EncodedByEntry));
            gtk_widget_hide(GTK_WIDGET(PictureScrollWindow));
            gtk_widget_hide (GTK_WIDGET (apply_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (remove_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (add_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (save_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (image_properties_toolitem));
            break;
#endif

#ifdef ENABLE_WAVPACK
        case WAVPACK_TAG:
            gtk_widget_show(GTK_WIDGET(DiscNumberLabel));
            gtk_widget_show(GTK_WIDGET(DiscNumberEntry));
            gtk_widget_show(GTK_WIDGET(ComposerLabel));
            gtk_widget_show(GTK_WIDGET(ComposerEntry));
            gtk_widget_show(GTK_WIDGET(OrigArtistLabel));
            gtk_widget_show(GTK_WIDGET(OrigArtistEntry));
            gtk_widget_show(GTK_WIDGET(CopyrightLabel));
            gtk_widget_show(GTK_WIDGET(CopyrightEntry));
            gtk_widget_show(GTK_WIDGET(URLLabel));
            gtk_widget_show(GTK_WIDGET(URLEntry));
            gtk_widget_show(GTK_WIDGET(EncodedByLabel));
            gtk_widget_show(GTK_WIDGET(EncodedByEntry));
            gtk_widget_hide(GTK_WIDGET(PictureScrollWindow));
            gtk_widget_hide (GTK_WIDGET (apply_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (remove_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (add_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (save_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (image_properties_toolitem));
            break;
#endif /* ENABLE_WAVPACK */

        case UNKNOWN_TAG:
        default:
            gtk_widget_hide(GTK_WIDGET(DiscNumberLabel));
            gtk_widget_hide(GTK_WIDGET(DiscNumberEntry));
            gtk_widget_hide(GTK_WIDGET(ComposerLabel));
            gtk_widget_hide(GTK_WIDGET(ComposerEntry));
            gtk_widget_hide(GTK_WIDGET(OrigArtistLabel));
            gtk_widget_hide(GTK_WIDGET(OrigArtistEntry));
            gtk_widget_hide(GTK_WIDGET(CopyrightLabel));
            gtk_widget_hide(GTK_WIDGET(CopyrightEntry));
            gtk_widget_hide(GTK_WIDGET(URLLabel));
            gtk_widget_hide(GTK_WIDGET(URLEntry));
            gtk_widget_hide(GTK_WIDGET(EncodedByLabel));
            gtk_widget_hide(GTK_WIDGET(EncodedByEntry));
            gtk_widget_hide(GTK_WIDGET(PictureScrollWindow));
            gtk_widget_hide (GTK_WIDGET (apply_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (remove_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (add_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (save_image_toolitem));
            gtk_widget_hide (GTK_WIDGET (image_properties_toolitem));
            break;
    }
}


/*
 * Clear the entries of tag area
 */
void Clear_Tag_Entry_Fields (void)
{
    /* GtkTextBuffer *textbuffer; */

    g_return_if_fail (TitleEntry != NULL);

    gtk_entry_set_text(GTK_ENTRY(TitleEntry),                       "");
    gtk_entry_set_text(GTK_ENTRY(ArtistEntry),                      "");
    gtk_entry_set_text(GTK_ENTRY(AlbumArtistEntry),                 "");
    gtk_entry_set_text(GTK_ENTRY(AlbumEntry),                       "");
    gtk_entry_set_text(GTK_ENTRY(DiscNumberEntry),                  "");
    gtk_entry_set_text(GTK_ENTRY(YearEntry),                        "");
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(TrackEntryCombo))),  "");
    gtk_entry_set_text(GTK_ENTRY(TrackTotalEntry),                  "");
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(GenreCombo))),       "");
    gtk_entry_set_text(GTK_ENTRY(CommentEntry),                     "");
    /* textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(CommentView));
     * gtk_text_buffer_set_text(GTK_TEXT_BUFFER(textbuffer),           "", -1); */
    gtk_entry_set_text(GTK_ENTRY(ComposerEntry),                    "");
    gtk_entry_set_text(GTK_ENTRY(OrigArtistEntry),                  "");
    gtk_entry_set_text(GTK_ENTRY(CopyrightEntry),                   "");
    gtk_entry_set_text(GTK_ENTRY(URLEntry),                         "");
    gtk_entry_set_text(GTK_ENTRY(EncodedByEntry),                   "");
    PictureEntry_Clear();
}


/*
 * Clear the entry of file area
 */
void
Clear_File_Entry_Field (void)
{
    g_return_if_fail (FileEntry != NULL);

    gtk_entry_set_text (GTK_ENTRY (FileEntry),"");
}


/*
 * Clear the header information
 */
void Clear_Header_Fields (void)
{
    g_return_if_fail (VersionValueLabel != NULL);

    /* Default values are MPs data */
    gtk_label_set_text(GTK_LABEL(VersionLabel),        _("Encoder:"));
    gtk_label_set_text(GTK_LABEL(VersionValueLabel),   "");
    gtk_label_set_text(GTK_LABEL(BitrateValueLabel),   "");
    gtk_label_set_text(GTK_LABEL(SampleRateValueLabel),"");
    gtk_label_set_text(GTK_LABEL(ModeLabel),           _("Mode:"));
    gtk_label_set_text(GTK_LABEL(ModeValueLabel),      "");
    gtk_label_set_text(GTK_LABEL(SizeValueLabel),      "");
    gtk_label_set_text(GTK_LABEL(DurationValueLabel),  "");
}




/*
 * Load the default directory when the user interface is completely displayed
 * to avoid bad visualization effect at startup.
 */
static void
Init_Load_Default_Dir (void)
{
    //ETCore->ETFileList = NULL;
    ET_Core_Free();
    ET_Core_Initialize();

    // Open the scanner window
    if (g_settings_get_boolean (ETSettings, "scan-startup"))
        Open_ScannerWindow(SCANNER_TYPE); // Open the last selected scanner

    if (INIT_DIRECTORY)
    {
        Browser_Tree_Select_Dir(INIT_DIRECTORY);
        Browser_Reload_Directory();
    }else
    {
        Statusbar_Message(_("Select a directory to browse"),FALSE);
        Browser_Load_Default_Directory();
    }

    // To set sensivity of buttons in the case if the default directory is invalid
    Update_Command_Buttons_Sensivity();

    g_source_remove(idle_handler_id);
}



static void
Convert_P20_And_Underscore_Into_Spaces (GtkWidget *entry)
{
    gchar *string = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

    Scan_Convert_Underscore_Into_Space(string);
    Scan_Convert_P20_Into_Space(string);
    gtk_entry_set_text(GTK_ENTRY(entry),string);
    g_free(string);
}

static void
Convert_Space_Into_Underscore (GtkWidget *entry)
{
    gchar *string = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

    Scan_Convert_Space_Into_Undescore(string);
    gtk_entry_set_text(GTK_ENTRY(entry),string);
    g_free(string);
}

static void
Convert_All_Uppercase (GtkWidget *entry)
{
    gchar *string = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

    Scan_Process_Fields_All_Uppercase(string);
    gtk_entry_set_text(GTK_ENTRY(entry),string);
    g_free(string);
}

static void
Convert_All_Lowercase (GtkWidget *entry)
{
    gchar *string = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

    Scan_Process_Fields_All_Downcase(string);
    gtk_entry_set_text(GTK_ENTRY(entry),string);
    g_free(string);
}

static void
Convert_Letter_Uppercase (GtkWidget *entry)
{
    gchar *string = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

    Scan_Process_Fields_Letter_Uppercase(string);
    gtk_entry_set_text(GTK_ENTRY(entry),string);
    g_free(string);
}

static void
Convert_First_Letters_Uppercase (GtkWidget *entry)
{
    gchar *string = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

    Scan_Process_Fields_First_Letters_Uppercase(string);
    gtk_entry_set_text(GTK_ENTRY(entry),string);
    g_free(string);
}

static void
Convert_Remove_Space (GtkWidget *entry)
{
    gchar *string = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

    Scan_Process_Fields_Remove_Space(string);
    gtk_entry_set_text(GTK_ENTRY(entry),string);
    g_free(string);
}

static void
Convert_Insert_Space (GtkWidget *entry)
{
    // FIX ME : we suppose that it will not grow more than 2 times its size...
    guint string_length = 2 * strlen(gtk_entry_get_text(GTK_ENTRY(entry)));
    gchar *string       = g_malloc(string_length+1);
    strncpy(string,gtk_entry_get_text(GTK_ENTRY(entry)),string_length);
    string[string_length]='\0';

    Scan_Process_Fields_Insert_Space(&string);
    gtk_entry_set_text(GTK_ENTRY(entry),string);
    g_free(string);
}

static void
Convert_Only_One_Space (GtkWidget *entry)
{
    gchar *string = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

    Scan_Process_Fields_Keep_One_Space(string);
    gtk_entry_set_text(GTK_ENTRY(entry),string);
    g_free(string);
}

static void
Convert_Remove_All_Text (GtkWidget *entry)
{
    gtk_entry_set_text (GTK_ENTRY (entry), "");
}

/*
 * Entry_Popup_Menu_Handler: show the popup menu when the third mouse button is pressed.
 */
static gboolean
Entry_Popup_Menu_Handler (GtkMenu *menu, GdkEventButton *event)
{
    if (event && (event->type==GDK_BUTTON_PRESS) && (event->button==3))
    {
        /* FIX ME : this is not very clean, but if we use 'event->button' (contains value of
         * the 3rd button) instead of '1', we need to click two times the left mouse button
         * to activate an item of the opened popup menu (when menu is attached to an entry). */
        //gtk_menu_popup(menu,NULL,NULL,NULL,NULL,event->button,event->time);
        gtk_menu_popup(menu,NULL,NULL,NULL,NULL,1,event->time);
        return TRUE;
    }
    return FALSE;
}

/*
 * Popup menu attached to all entries of tag + filename + rename combobox.
 * Displayed when pressing the right mouse button and contains functions to process ths strings.
 */
void Attach_Popup_Menu_To_Tag_Entries (GtkEntry *entry)
{
    GtkWidget *PopupMenu;
    GtkWidget *Image;
    GtkWidget *MenuItem;


    PopupMenu = gtk_menu_new();
    g_signal_connect_swapped(G_OBJECT(entry),"button_press_event",
        G_CALLBACK(Entry_Popup_Menu_Handler),G_OBJECT(PopupMenu));

    /* Menu items */
    MenuItem = gtk_image_menu_item_new_with_label(_("Tag selected files with this field"));
    Image = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO,GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(MenuItem),Image);
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);
    g_signal_connect_swapped (G_OBJECT (MenuItem), "activate",
                              G_CALLBACK (Mini_Button_Clicked),
                              G_OBJECT (entry));

    /* Separator */
    MenuItem = gtk_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);

    MenuItem = gtk_image_menu_item_new_with_label(_("Convert '_' and '%20' to spaces"));
    Image = gtk_image_new_from_stock(GTK_STOCK_CONVERT,GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(MenuItem),Image);
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate",
        G_CALLBACK(Convert_P20_And_Underscore_Into_Spaces),G_OBJECT(entry));

    MenuItem = gtk_image_menu_item_new_with_label(_("Convert ' ' to '_'"));
    Image = gtk_image_new_from_stock(GTK_STOCK_CONVERT,GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(MenuItem),Image);
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate",
        G_CALLBACK(Convert_Space_Into_Underscore),G_OBJECT(entry));

    /* Separator */
    MenuItem = gtk_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);

    MenuItem = gtk_image_menu_item_new_with_label(_("All uppercase"));
    Image = gtk_image_new_from_stock("easytag-all-uppercase",GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(MenuItem),Image);
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate",
        G_CALLBACK(Convert_All_Uppercase),G_OBJECT(entry));

    MenuItem = gtk_image_menu_item_new_with_label(_("All lowercase"));
    Image = gtk_image_new_from_stock("easytag-all-downcase",GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(MenuItem),Image);
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate",
        G_CALLBACK(Convert_All_Lowercase),G_OBJECT(entry));

    MenuItem = gtk_image_menu_item_new_with_label(_("First letter uppercase"));
    Image = gtk_image_new_from_stock("easytag-first-letter-uppercase",GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(MenuItem),Image);
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate",
        G_CALLBACK(Convert_Letter_Uppercase),G_OBJECT(entry));

    MenuItem = gtk_image_menu_item_new_with_label(_("First letter uppercase of each word"));
    Image = gtk_image_new_from_stock("easytag-first-letter-uppercase-word",GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(MenuItem),Image);
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate",
        G_CALLBACK(Convert_First_Letters_Uppercase),G_OBJECT(entry));

    /* Separator */
    MenuItem = gtk_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);

    MenuItem = gtk_image_menu_item_new_with_label(_("Remove spaces"));
    Image = gtk_image_new_from_stock(GTK_STOCK_REMOVE,GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(MenuItem),Image);
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate",
        G_CALLBACK(Convert_Remove_Space),G_OBJECT(entry));

    MenuItem = gtk_image_menu_item_new_with_label(_("Insert space before uppercase letter"));
    Image = gtk_image_new_from_stock(GTK_STOCK_ADD,GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(MenuItem),Image);
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate",
        G_CALLBACK(Convert_Insert_Space),G_OBJECT(entry));

    MenuItem = gtk_image_menu_item_new_with_label(_("Remove duplicate spaces or underscores"));
    Image = gtk_image_new_from_stock(GTK_STOCK_REMOVE,GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(MenuItem),Image);
    gtk_menu_shell_append(GTK_MENU_SHELL(PopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate",
        G_CALLBACK(Convert_Only_One_Space),G_OBJECT(entry));

    MenuItem = gtk_image_menu_item_new_with_label (_("Remove all text"));
    Image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(MenuItem), Image);
    gtk_menu_shell_append (GTK_MENU_SHELL(PopupMenu), MenuItem);
    g_signal_connect_swapped (G_OBJECT (MenuItem), "activate",
                              G_CALLBACK (Convert_Remove_All_Text),
                              G_OBJECT (entry));

    gtk_widget_show_all(PopupMenu);
}



/*
 * Function to manage the received signals (specially for segfaults)
 * Handle crashs
 */
#ifndef G_OS_WIN32
static void
Handle_Crash (gint signal_id)
{
    //gchar commmand[256];

    Log_Print(LOG_ERROR,_("EasyTAG version %s: Abnormal exit (PID: %d)"),PACKAGE_VERSION,getpid());
    Log_Print(LOG_ERROR,_("Received signal %s (%d)"),signal_to_string(signal_id),signal_id);

    Log_Print(LOG_ERROR,_("You have probably found a bug in EasyTAG. Please, "
                          "file a bug report with a GDB backtrace ('gdb "
			  "easytag core' then 'bt' and 'l') and information "
			  "to reproduce it at: %s"),PACKAGE_BUGREPORT);

    // To send messages to the console...
    g_print(_("EasyTAG version %s: Abnormal exit (PID: %d)."),PACKAGE_VERSION,getpid());
    g_print("\n");
    g_print(_("Received signal %s (%d)\a"),signal_to_string(signal_id),signal_id);
    g_print("\n");
    g_print(_("You have probably found a bug in EasyTAG. Please, file a bug "
            "report with a GDB backtrace ('gdb easytag core' then 'bt' and "
            "'l') and information to reproduce it at: %s"),PACKAGE_BUGREPORT);
    g_print("\n");

    signal(signal_id,SIG_DFL); // Let the OS handle recursive seg faults
    //signal(SIGTSTP, exit);
    //snprintf(commmand,sizeof(commmand),"gdb -x /root/core.txt easytag %d", getpid());
    //system(commmand);
}

static const gchar *
signal_to_string (gint signal)
{
#ifdef SIGHUP
    if (signal == SIGHUP)     return ("SIGHUP");
#endif
#ifdef SIGINT
    if (signal == SIGINT)     return ("SIGINT");
#endif
#ifdef SIGQUIT
    if (signal == SIGQUIT)    return ("SIGQUIT");
#endif
#ifdef SIGILL
    if (signal == SIGILL)     return ("SIGILL");
#endif
#ifdef SIGTRAP
    if (signal == SIGTRAP)    return ("SIGTRAP");
#endif
#ifdef SIGABRT
    if (signal == SIGABRT)    return ("SIGABRT");
#endif
#ifdef SIGIOT
    if (signal == SIGIOT)     return ("SIGIOT");
#endif
#ifdef SIGEMT
    if (signal == SIGEMT)     return ("SIGEMT");
#endif
#ifdef SIGFPE
    if (signal == SIGFPE)     return ("SIGFPE");
#endif
#ifdef SIGKILL
    if (signal == SIGKILL)    return ("SIGKILL");
#endif
#ifdef SIGBUS
    if (signal == SIGBUS)     return ("SIGBUS");
#endif
#ifdef SIGSEGV
    if (signal == SIGSEGV)    return ("SIGSEGV");
#endif
#ifdef SIGSYS
    if (signal == SIGSYS)     return ("SIGSYS");
#endif
#ifdef SIGPIPE
    if (signal == SIGPIPE)    return ("SIGPIPE");
#endif
#ifdef SIGALRM
    if (signal == SIGALRM)    return ("SIGALRM");
#endif
#ifdef SIGTERM
    if (signal == SIGTERM)    return ("SIGTERM");
#endif
#ifdef SIGUSR1
    if (signal == SIGUSR1)    return ("SIGUSR1");
#endif
#ifdef SIGUSR2
    if (signal == SIGUSR2)    return ("SIGUSR2");
#endif
#ifdef SIGCHLD
    if (signal == SIGCHLD)    return ("SIGCHLD");
#endif
#ifdef SIGCLD
    if (signal == SIGCLD)     return ("SIGCLD");
#endif
#ifdef SIGPWR
    if (signal == SIGPWR)     return ("SIGPWR");
#endif
#ifdef SIGVTALRM
    if (signal == SIGVTALRM)  return ("SIGVTALRM");
#endif
#ifdef SIGPROF
    if (signal == SIGPROF)    return ("SIGPROF");
#endif
#ifdef SIGIO
    if (signal == SIGIO)      return ("SIGIO");
#endif
#ifdef SIGPOLL
    if (signal == SIGPOLL)    return ("SIGPOLL");
#endif
#ifdef SIGWINCH
    if (signal == SIGWINCH)   return ("SIGWINCH");
#endif
#ifdef SIGWINDOW
    if (signal == SIGWINDOW)  return ("SIGWINDOW");
#endif
#ifdef SIGSTOP
    if (signal == SIGSTOP)    return ("SIGSTOP");
#endif
#ifdef SIGTSTP
    if (signal == SIGTSTP)    return ("SIGTSTP");
#endif
#ifdef SIGCONT
    if (signal == SIGCONT)    return ("SIGCONT");
#endif
#ifdef SIGTTIN
    if (signal == SIGTTIN)    return ("SIGTTIN");
#endif
#ifdef SIGTTOU
    if (signal == SIGTTOU)    return ("SIGTTOU");
#endif
#ifdef SIGURG
    if (signal == SIGURG)     return ("SIGURG");
#endif
#ifdef SIGLOST
    if (signal == SIGLOST)    return ("SIGLOST");
#endif
#ifdef SIGRESERVE
    if (signal == SIGRESERVE) return ("SIGRESERVE");
#endif
#ifdef SIGDIL
    if (signal == SIGDIL)     return ("SIGDIL");
#endif
#ifdef SIGXCPU
    if (signal == SIGXCPU)    return ("SIGXCPU");
#endif
#ifdef SIGXFSZ
    if (signal == SIGXFSZ)    return ("SIGXFSZ");
#endif
    return (_("Unknown signal"));
}
#endif /* !G_OS_WIN32 */


/*
 * Exit the program
 */
static void
EasyTAG_Exit (void)
{
    ET_Core_Destroy();
    Charset_Insert_Locales_Destroy();
    Log_Print(LOG_OK,_("EasyTAG: Normal exit."));
    gtk_main_quit();
#ifdef G_OS_WIN32
    weasytag_cleanup();
#endif /* G_OS_WIN32 */
    exit(0);
}

static void
Quit_MainWindow_Confirmed (void)
{
    // Save the configuration when exiting...
    Save_Changes_Of_UI();
    
    // Quit EasyTAG
    EasyTAG_Exit();
}

static void
Quit_MainWindow_Save_And_Quit (void)
{
    /* Save modified tags */
    if (Save_All_Files_With_Answer(FALSE) == -1)
        return;
    Quit_MainWindow_Confirmed();
}

void Quit_MainWindow (void)
{
    GtkWidget *msgbox;
    gint response;

    /* If you change the displayed data and quit immediately */
    if (ETCore->ETFileList)
        ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed); // To detect change before exiting

    /* Save combobox history list before exit */
    Save_Path_Entry_List(BrowserEntryModel, MISC_COMBO_TEXT);

    /* Check if all files have been saved before exit */
    if (g_settings_get_boolean (ETSettings, "confirm-when-unsaved-files")
        && ET_Check_If_All_Files_Are_Saved() != TRUE)
    {
        /* Some files haven't been saved */
        msgbox = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_QUESTION,
                                        GTK_BUTTONS_NONE,
                                        "%s",
                                        _("Some files have been modified but not saved"));
        gtk_dialog_add_buttons(GTK_DIALOG(msgbox),GTK_STOCK_DISCARD,GTK_RESPONSE_NO,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_SAVE,GTK_RESPONSE_YES,NULL);
        gtk_window_set_title(GTK_WINDOW(msgbox),_("Quit"));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgbox),"%s",_("Do you want to save them before quitting?"));
        response = gtk_dialog_run(GTK_DIALOG(msgbox));
        gtk_widget_destroy(msgbox);
        switch (response)
        {
            case GTK_RESPONSE_YES:
                Quit_MainWindow_Save_And_Quit();
                break;
            case GTK_RESPONSE_NO:
                Quit_MainWindow_Confirmed();
                break;
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_NONE:
                return;
        }

    } else if (g_settings_get_boolean (ETSettings, "confirm-quit"))
    {
        msgbox = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_QUESTION,
                                         GTK_BUTTONS_NONE,
                                         "%s",
                                         _("Do you really want to quit?"));
         gtk_dialog_add_buttons(GTK_DIALOG(msgbox),GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_QUIT,GTK_RESPONSE_CLOSE,NULL);
        gtk_window_set_title(GTK_WINDOW(msgbox),_("Quit"));
        response = gtk_dialog_run(GTK_DIALOG(msgbox));
        gtk_widget_destroy(msgbox);
        switch (response)
        {
            case GTK_RESPONSE_CLOSE:
                Quit_MainWindow_Confirmed();
                break;
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_NONE:
                return;
                break;
        }
    }else
    {
        Quit_MainWindow_Confirmed();
    }

}

/*
 * For the configuration file...
 */
void MainWindow_Apply_Changes (void)
{
    GdkWindow *window;

    g_return_if_fail(MainWindow !=NULL);

    window = gtk_widget_get_window(MainWindow);

    if ( window && gdk_window_is_visible(window) && gdk_window_get_state(window)!=GDK_WINDOW_STATE_MAXIMIZED )
    {
        gint x, y, width, height;

        // Position and Origin of the window
        gdk_window_get_root_origin(window,&x,&y);
        MAIN_WINDOW_X = x;
        MAIN_WINDOW_Y = y;
        width = gdk_window_get_width(window);
        height = gdk_window_get_height(window);
        MAIN_WINDOW_WIDTH  = width;
        MAIN_WINDOW_HEIGHT = height;

        // Handle panes position
        PANE_HANDLE_POSITION1 = gtk_paned_get_position(GTK_PANED(MainWindowHPaned));
        PANE_HANDLE_POSITION2 = gtk_paned_get_position(GTK_PANED(BrowserHPaned));
        PANE_HANDLE_POSITION3 = gtk_paned_get_position(GTK_PANED(ArtistAlbumVPaned));
        PANE_HANDLE_POSITION4 = gtk_paned_get_position(GTK_PANED(MainWindowVPaned));
    }

}
