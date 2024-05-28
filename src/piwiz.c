/*
Copyright (c) 2018 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#define I_KNOW_THE_PACKAGEKIT_GLIB2_API_IS_SUBJECT_TO_CHANGE
#include <packagekit-glib2/packagekit.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <libintl.h>
#include <crypt.h>

#include <NetworkManager.h>

#define PAGE_INTRO 0
#define PAGE_LOCALE 1
#define PAGE_PASSWD 2
#define PAGE_OSCAN 3
#define PAGE_WIFIAP 4
#define PAGE_WIFIPSK 5
#define PAGE_BROWSER 6
#define PAGE_RPC 7
#define PAGE_UPDATE 8
#define PAGE_DONE 9

#define NEXT_BTN 0
#define SKIP_BTN 1
#define PREV_BTN 2

#define FORWARD 1
#define BACKWARD -1

#define MSG_PULSE -1
#define MSG_WAIT -2
#define MSG_TERM -3

// columns in localisation list stores

#define CL_CNAME 0
#define CL_CCODE 1

#define LL_LNAME 0
#define LL_LCODE 1
#define LL_CCODE 2
#define LL_CHARS 3

#define TL_CITY  0
#define TL_ZONE  1
#define TL_CCODE 2  // must be the same as LL_CCODE to allow match_country to be used for both
#define TL_PREF  3

#define AP_SSID      0
#define AP_SEC_ICON  1
#define AP_SIG_ICON  2
#define AP_SECURE    3
#define AP_CONNECTED 4
#define AP_DEVICE    5
#define AP_AP        6

#define FLAGFILE "/tmp/.wlflag"

#define JAPAN_FONTS "fonts-vlgothic fonts-mplus"

typedef struct {
    char *msg;
    int type;
} prog_msg;

static prog_msg pm;

/* Controls */

static GtkWidget *main_dlg, *msg_dlg, *msg_msg, *msg_pb, *msg_btn;
static GtkWidget *err_dlg, *err_msg, *err_btn;
static GtkWidget *wizard_nb, *next_btn, *prev_btn, *skip_btn;
static GtkWidget *country_cb, *language_cb, *timezone_cb;
static GtkWidget *ap_tv, *psk_label, *prompt, *ip_label, *bt_prompt;
static GtkWidget *user_te, *pwd1_te, *pwd2_te, *psk_te;
static GtkWidget *pwd_hide, *psk_hide, *eng_chk, *uskey_chk;
static GtkWidget *uscan1_sw, *uscan2_sw, *uscan2_box, *rpc_sw;
static GtkWidget *rename_title, *rename_info, *rename_prompt;
static GtkWidget *done_title, *done_info, *done_prompt;
static GtkWidget *chromium_rb, *firefox_rb, *uninstall_chk;

/* Lists for localisation */

GtkListStore *country_list, *locale_list, *tz_list;
GtkTreeModelSort *slang;
GtkTreeModelFilter *fcount, *fcity;

/* List of APs */

GtkListStore *ap_list;
GtkTreeModel *sap, *fap;

/* Globals */

char *wifi_if, *init_country, *init_lang, *init_kb, *init_var, *init_tz;
char *cc, *lc, *city, *ext, *lay, *var;
char *ssid;
char *user = NULL, *pw = NULL, *chuser = NULL;
gint conn_timeout = 0, pulse_timer = 0;
gboolean reboot = TRUE, is_pi = TRUE;
int last_btn = NEXT_BTN;
int calls;
gboolean browser = TRUE;
gboolean rpc = TRUE;

typedef enum {
    WM_OPENBOX,
    WM_WAYFIRE,
    WM_LABWC } wm_type;
static wm_type wm;

typedef enum {
    INSTALL_LANGUAGES,
    UNINSTALL_BROWSER,
    INSTALL_UPDATES } update_type;

NMClient *nm_client = NULL;
gboolean nm_scanning = FALSE;
NMDevice *nm_dev;
char *nm_ap_id = NULL;
NMConnection *nm_wcon;

/* Map from country code to keyboard */

static const char keyboard_map[][13] = {
    "",     "AL",   "al",       "",
    "",     "AZ",   "az",       "",
    "",     "BD",   "bd",       "",     //us
    "",     "BE",   "be",       "",
    "",     "BG",   "bg",       "",     //us
    "",     "BR",   "br",       "",
    "",     "BT",   "bt",       "",     //us
    "",     "BY",   "by",       "",     //us
    "fr",   "CA",   "ca",       "",
    "",     "CA",   "us",       "",
    "de",   "CH",   "ch",       "",
    "fr",   "CH",   "ch",       "fr",
    "",     "CH",   "ch",       "",
    "",     "CZ",   "cz",       "",
    "",     "DK",   "dk",       "",
    "",     "EE",   "ee",       "",
    "ast",  "ES",   "es",       "ast",
    "bo",   "",     "cn",       "tib",  //us
    "ca",   "ES",   "es",       "cat",
    "",     "ES",   "es",       "",
    "",     "ET",   "et",       "",     //us
    "se",   "FI",   "fi",       "smi",
    "",     "FI",   "fi",       "",
    "",     "FR",   "fr",       "latin9",
    "",     "GB",   "gb",       "",
    "",     "GG",   "gb",       "",
    "",     "HU",   "hu",       "",
    "",     "IE",   "ie",       "",
    "",     "IL",   "il",       "",     //us
    "",     "IM",   "gb",       "",
    "",     "IR",   "ir",       "",     //us
    "",     "IS",   "is",       "",
    "",     "IT",   "it",       "",
    "",     "JE",   "gb",       "",
    "",     "JP",   "jp",       "",
    "",     "LT",   "lt",       "",
    "",     "LV",   "lv",       "",
    "",     "KG",   "kg",       "",     //us
    "",     "KH",   "kh",       "",     //us
    "",     "KR",   "kr",       "kr104",
    "",     "KZ",   "kz",       "",     //us
    "",     "LK",   "lk",       "",     //us
    "",     "MA",   "ma",       "",     //us
    "",     "MK",   "mk",       "",     //us
    "",     "NL",   "us",       "",
    "",     "MM",   "mm",       "",     //us
    "",     "MN",   "mn",       "",     //us
    "",     "MT",   "mt",       "",
    "se",   "NO",   "no",       "smi",
    "",     "NO",   "no",       "",
    "",     "NP",   "np",       "",     //us
    "",     "PH",   "ph",       "",
    "",     "PL",   "pl",       "",
    "",     "PT",   "pt",       "",
    "",     "RO",   "ro",       "",
    "",     "RU",   "ru",       "",     //us
    "se",   "SE",   "se",       "smi",
    "",     "SK",   "sk",       "",
    "",     "SI",   "si",       "",
    "tg",   "",     "tj",       "",     //us
    "",     "TJ",   "tj",       "",     //us
    "",     "TH",   "th",       "",     //us
    "ku",   "TR",   "tr",       "ku",
    "",     "TR",   "tr",       "",
    "",     "UA",   "ua",       "",     //us
    "en",   "US",   "us",       "",
    "",     "VN",   "us",       "",
    "",     "ZA",   "za",       "",
    "",     "AR",   "latam",    "",
    "",     "BO",   "latam",    "",
    "",     "CL",   "latam",    "",
    "",     "CO",   "latam",    "",
    "",     "CR",   "latam",    "",
    "",     "DO",   "latam",    "",
    "",     "EC",   "latam",    "",
    "",     "GT",   "latam",    "",
    "",     "HN",   "latam",    "",
    "",     "MX",   "latam",    "",
    "",     "NI",   "latam",    "",
    "",     "PA",   "latam",    "",
    "",     "PE",   "latam",    "",
    "es",   "PR",   "latam",    "",
    "",     "PY",   "latam",    "",
    "",     "SV",   "latam",    "",
    "es",   "US",   "latam",    "",
    "",     "UY",   "latam",    "",
    "",     "VE",   "latam",    "",
    "ar",   "",     "ara",      "",     //us
    "bn",   "",     "in",       "ben",  //us
    "bs",   "",     "ba",       "",
    "de",   "LI",   "ch",       "",
    "de",   "",     "de",       "",
    "el",   "",     "gr",       "",     //us
    "eo",   "",     "epo",      "",
    "fr",   "",     "fr",       "latin9",
    "gu",   "",     "in",       "guj",  //us
    "hi",   "",     "in",       "",     //us
    "hr",   "",     "hr",       "",
    "hy",   "",     "am",       "",     //us
    "ka",   "",     "ge",       "",     //us
    "kn",   "",     "in",       "kan",  //us
    "ku",   "",     "tr",       "ku",
    "lo",   "",     "la",       "",     //us
    "mr",   "",     "in",       "",     //us
    "ml",   "",     "in",       "mal",  //us
    "my",   "",     "mm",       "",     //us
    "ne",   "",     "np",       "",     //us
    "os",   "",     "ru",       "os",
    "pa",   "",     "in",       "guru", //us
    "si",   "",     "si",       "sin_phonetic",     //us
    "sr",   "",     "rs",       "latin",
    "sv",   "",     "se",       "",
    "ta",   "",     "in",       "tam",  //us
    "te",   "",     "in",       "tel",  //us
    "tg",   "",     "tj",       "",     //us
    "the",  "",     "np",       "",     //us
    "tl",   "",     "ph",       "",
    "ug",   "",     "cn",       "ug",   //us
    "zh",   "",     "cn",       "",
    "",     "",     "us",       ""
};

#define MAX_KBS 17
static const char kb_countries[MAX_KBS][3] = {
    "GB",   // default if no Pi keyboard found
    "GB",
    "FR",
    "ES",
    "US",
    "DE",
    "IT",
    "JP",
    "PT",
    "NO",
    "SE",
    "DK",
    "RU",
    "TR",
    "IL",
    "HU",
    "KR"
};

static const char kb_langs[MAX_KBS][3] = {
    "en",   // default if no Pi keyboard found
    "en",
    "fr",
    "es",
    "en",
    "de",
    "it",
    "jp",
    "pt",
    "nn",
    "se",
    "fi",
    "ru",
    "tr",
    "he",
    "hu",
    "ko"
};

static const char kb_tzs[MAX_KBS][20] = {
    "Europe/London",    // default if no Pi keyboard found
    "Europe/London",
    "Europe/Paris",
    "Europe/Madrid",
    "America/New_York",
    "Europe/Berlin",
    "Europe/Rome",
    "Asia/Tokyo",
    "Europe/Lisbon",
    "Europe/Oslo",
    "Europe/Stockholm",
    "Europe/Helsinki",
    "Europe/Moscow",
    "Europe/Istanbul",
    "Europe/Jerusalem",
    "Europe/Budapest",
    "Asia/Seoul"
};

/* Local prototypes */

static char *get_shell_string (char *cmd, gboolean all);
static char *get_string (char *cmd);
static int get_status (char *cmd);
static char *get_quoted_param (char *path, char *fname, char *toseek);
static int vsystem (const char *fmt, ...);
static void error_box (char *msg);
static gboolean cb_error (gpointer data);
static void thread_error (char *msg);
static void message (char *msg, int type);
static gboolean cb_message (gpointer data);
static void thread_message (char *msg, int type);
static void hide_message (void);
static gboolean ok_clicked (GtkButton *button, gpointer data);
static gboolean loc_done (gpointer data);
static void lookup_keyboard (char *country, char *language, char **layout, char **variant);
static gpointer set_locale (gpointer data);
static void read_locales (void);
static gboolean unique_rows (GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static void country_changed (GtkComboBox *cb, gpointer ptr);
static gboolean match_country (GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static void read_inits (void);
static void set_init (GtkTreeModel *model, GtkWidget *cb, int pos, const char *init);
static void set_init_tz (GtkTreeModel *model, GtkWidget *cb, int pos, const char *init);
static int find_line (char **lssid, int *secure, int *connected);
void connect_success (void);
static gint connect_failure (gpointer data);
static void nm_start_scan (void);
static void nm_stop_scan (void);
static void nm_req_scan_finish_cb (GObject *device, GAsyncResult *result, gpointer data);
static void nm_ap_changed_cb (NMDeviceWifi *device, NMAccessPoint *unused, gpointer data);
static void nm_ap_add (NMDeviceWifi *dev, NMAccessPoint *ap);
static gboolean nm_select_matched_ssid (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static gboolean nm_set_connected_flag (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void nm_connect_wifi (const char *password);
static void nm_find_conn (gpointer data, gpointer user_data);
static void nm_connection_active_cb (GObject *client, GAsyncResult *result, gpointer data);
static void nm_changes_committed_cb (GObject *connection, GAsyncResult *result, gpointer data);
static gboolean nm_check_connection (gpointer data);
static char *nm_ap_get_id (NMAccessPoint *ap);
static const char *nm_ap_get_path (char *id);
static gboolean nm_filter_dup_ap (GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static char *nm_find_psk_for_network (char *ssid);
static void progress (PkProgress *progress, PkProgressType type, gpointer data);
static void do_updates_done (PkTask *task, GAsyncResult *res, gpointer data);
static gboolean inst_filter_fn (PkPackage *package, gpointer user_data);
static gboolean rem_filter_fn (PkPackage *package, gpointer user_data);
static gboolean upd_filter_fn (PkPackage *package, gpointer user_data);
static void next_update (PkTask *task, update_type update_stage);
static void check_updates_done (PkTask *task, GAsyncResult *res, gpointer data);
static void install_lang_done (PkTask *task, GAsyncResult *res, gpointer data);
static void resolve_lang_done (PkTask *task, GAsyncResult *res, gpointer data);
static void uninstall_browser_done (PkTask *task, GAsyncResult *res, gpointer data);
static void resolve_browser_done (PkTask *task, GAsyncResult *res, gpointer data);
static void refresh_cache_done (PkTask *task, GAsyncResult *res, gpointer data);
static gpointer refresh_update_cache (gpointer data);
static gboolean clock_synced (void);
static void resync (void);
static gboolean ntp_check (gpointer data);
static void page_changed (GtkNotebook *notebook, GtkWidget *page, int pagenum, gpointer data);
static gboolean page_shown (int page);
static void change_page (int dir);
static void next_page (GtkButton* btn, gpointer ptr);
static void prev_page (GtkButton* btn, gpointer ptr);
static void skip_page (GtkButton* btn, gpointer ptr);
static gboolean show_ip (void);
#ifndef HOMESCHOOL
static void set_marketing_serial (const char *file);
#else
static void set_hs_serial (void);
#endif
static gboolean net_available (void);
static int get_pi_keyboard (void);
static gboolean srprompt (gpointer data);
static void uscan_toggle (GtkSwitch *sw, gpointer ptr);

/* Helpers */

static char *get_shell_string (char *cmd, gboolean all)
{
    char *line = NULL, *res = NULL;
    size_t len = 0;
    FILE *fp = popen (cmd, "r");

    if (fp == NULL) return NULL;
    if (getline (&line, &len, fp) > 0)
    {
        g_strdelimit (line, "\n\r", 0);
        if (!all)
        {
            res = line;
            while (*res++) if (g_ascii_isspace (*res)) *res = 0;
        }
        if (line[0])
            res = g_strdup (line);
        else
            res = NULL;
    }
    pclose (fp);
    g_free (line);
    return res;
}

static char *get_string (char *cmd)
{
    return get_shell_string (cmd, FALSE);
}

static int get_status (char *cmd)
{
    FILE *fp = popen (cmd, "r");
    char *buf = NULL;
    size_t res = 0;
    int val = 0;

    if (fp == NULL) return 0;
    if (getline (&buf, &res, fp) > 0)
    {
        if (sscanf (buf, "%d", &val) != 1) val = 0;
    }
    pclose (fp);
    g_free (buf);
    return val;
}

static char *get_quoted_param (char *path, char *fname, char *toseek)
{
    char *pathname, *linebuf, *cptr, *dptr, *res;
    size_t len;

    pathname = g_strdup_printf ("%s/%s", path, fname);
    FILE *fp = fopen (pathname, "rb");
    g_free (pathname);
    if (!fp) return NULL;

    linebuf = NULL;
    len = 0;
    while (getline (&linebuf, &len, fp) > 0)
    {
        // skip whitespace at line start
        cptr = linebuf;
        while (*cptr == ' ' || *cptr == '\t') cptr++;

        // compare against string to find
        if (!strncmp (cptr, toseek, strlen (toseek)))
        {
            // find string in quotes
            strtok (cptr, "\"");
            dptr = strtok (NULL, "\"\n\r");

            // copy to dest
            if (dptr) res = g_strdup (dptr);
            else res = NULL;

            // done
            g_free (linebuf);
            fclose (fp);
            return res;
        }
    }

    // end of file with no match
    g_free (linebuf);
    fclose (fp);
    return NULL;
}

static int vsystem (const char *fmt, ...)
{
    char *cmdline;
    int res;

    va_list arg;
    va_start (arg, fmt);
    g_vasprintf (&cmdline, fmt, arg);
    va_end (arg);
    res = system (cmdline);
    g_free (cmdline);
    return res;
}

/* Keyboard detection */

static int get_pi_keyboard (void)
{
    int val, ret = 0;
    char *res;

    res = get_string ("hexdump -n 1 -s 3 -e '1/1 \"%d\"' /proc/device-tree/chosen/rpi-country-code 2> /dev/null");
    if (res)
    {
        if (sscanf (res, "%d", &val) == 1) ret = val;
        g_free (res);
        if (ret) return ret;
    }

    res = get_string ("lsusb -v -d 04d9:0006 2> /dev/null | grep iProduct | rev");
    if (res)
    {
        if (sscanf (res, "%x", &val) == 1) ret = val;
        g_free (res);
    }
    return ret;
}


/* Message boxes */

static gboolean pulse_pb (gpointer data)
{
    if (msg_dlg)
    {
        gtk_progress_bar_pulse (GTK_PROGRESS_BAR (msg_pb));
        return TRUE;
    }
    else return FALSE;
}

static void error_box (char *msg)
{
    if (msg_dlg)
    {
        // clear any existing message box
        gtk_widget_destroy (GTK_WIDGET (msg_dlg));
        msg_dlg = NULL;
    }

    if (!err_dlg)
    {
        GtkBuilder *builder;

        builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/piwiz.ui");

        err_dlg = (GtkWidget *) gtk_builder_get_object (builder, "error");
        gtk_window_set_transient_for (GTK_WINDOW (err_dlg), GTK_WINDOW (main_dlg));

        err_msg = (GtkWidget *) gtk_builder_get_object (builder, "err_lbl");
        err_btn = (GtkWidget *) gtk_builder_get_object (builder, "err_btn");

        gtk_label_set_text (GTK_LABEL (err_msg), msg);

        g_signal_connect (err_btn, "clicked", G_CALLBACK (ok_clicked), (void *) 1);

        gtk_widget_show_all (err_dlg);
        g_object_unref (builder);
    }
    else gtk_label_set_text (GTK_LABEL (err_msg), msg);
}

static gboolean cb_error (gpointer data)
{
    error_box ((char *) data);
    return FALSE;
}

static void thread_error (char *msg)
{
    gdk_threads_add_idle (cb_error, msg);
}

static void message (char *msg, int type)
{
    GtkWidget *wid;

    if (!msg_dlg)
    {
        GtkBuilder *builder;

        builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/piwiz.ui");

        msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal");
        gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));

        msg_msg = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
        msg_pb = (GtkWidget *) gtk_builder_get_object (builder, "modal_pb");
        msg_btn = (GtkWidget *) gtk_builder_get_object (builder, "modal_ok");
        wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_cancel");
        gtk_widget_hide (wid);

        gtk_label_set_text (GTK_LABEL (msg_msg), msg);

        gtk_widget_set_sensitive (prev_btn, FALSE);
        gtk_widget_set_sensitive (next_btn, FALSE);
        gtk_widget_set_sensitive (skip_btn, FALSE);

        g_object_unref (builder);
    }
    else gtk_label_set_text (GTK_LABEL (msg_msg), msg);

    if (pulse_timer) g_source_remove (pulse_timer);
    pulse_timer = 0;

    if (type == MSG_WAIT || type == MSG_TERM)
    {
        // type = MSG_WAIT is a dialog waiting for OK to be clicked, at which point it closes
        // type = MSG_TERM is the same, but on closing the wizard jumps to the last page
        gtk_widget_hide (msg_pb);
        gtk_widget_show (msg_btn);
        g_signal_connect (msg_btn, "clicked", G_CALLBACK (ok_clicked), type == MSG_TERM ? (void *) 1 : (void *) 0);
        gtk_widget_grab_focus (msg_btn);
    }
    else if (type == MSG_PULSE)
    {
        // type = MSG_PULSE shows a pulsing progress bar
        gtk_widget_hide (msg_btn);
        gtk_widget_show (msg_pb);
        gtk_progress_bar_pulse (GTK_PROGRESS_BAR (msg_pb));
        pulse_timer = g_timeout_add (200, pulse_pb, NULL);
    }
    else
    {
        // any other values of type are a percentage progress value
        gtk_widget_hide (msg_btn);
        gtk_widget_show (msg_pb);
        float fprogress = type / 100.0;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (msg_pb), fprogress);
    }

    gtk_widget_show (msg_dlg);
}

static gboolean cb_message (gpointer data)
{
    prog_msg *pm = (prog_msg *) data;
    message (pm->msg, pm->type);
    return FALSE;
}

static void thread_message (char *msg, int type)
{
    pm.msg = msg;
    pm.type = type;
    gdk_threads_add_idle (cb_message, &pm);
}

static void hide_message (void)
{
    if (msg_dlg)
    {
        gtk_widget_destroy (GTK_WIDGET (msg_dlg));
        msg_dlg = NULL;
    }
    if (err_dlg)
    {
        gtk_widget_destroy (GTK_WIDGET (err_dlg));
        err_dlg = NULL;
    }
    gtk_widget_set_sensitive (prev_btn, TRUE);
    gtk_widget_set_sensitive (next_btn, TRUE);
    gtk_widget_set_sensitive (skip_btn, TRUE);
}

static gboolean ok_clicked (GtkButton *button, gpointer data)
{
    hide_message ();
    if (data) gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_DONE);
    return FALSE;
}

static gboolean loc_done (gpointer data)
{
    char *lang, *language, *lcall;

    remove (FLAGFILE);
    sync ();

    if (fork () == 0)
    {
        // new child process - set the new locale environment variables and then restart the wizard
        lang = g_strdup_printf ("LANG=%s_%s%s", lc, cc, ext);
        language = g_strdup_printf ("LANGUAGE=%s_%s%s", lc, cc, ext);
        lcall = g_strdup_printf ("LC_ALL=%s_%s%s", lc, cc, ext);
        putenv (lang);
        putenv (language);
        putenv (lcall);

        execl ("/usr/bin/piwiz", "piwiz", "--langset", lc, cc, NULL);
        exit (0);
    }
    else
    {
        while (access (FLAGFILE, F_OK) == -1);
        exit (0);
    }
    return FALSE;
}

/* Internationalisation */

static void lookup_keyboard (char *country, char *language, char **layout, char **variant)
{
    int n = 0;

    // this will always exit as long as the last entry in the map has two nulls...
    while (1)
    {
        if ((*keyboard_map[n] == 0 || !g_strcmp0 (language, keyboard_map[n])) &&
            (*keyboard_map[n + 1] == 0 || !g_strcmp0 (country, keyboard_map[n + 1])))
        {
            *layout = g_strdup (keyboard_map[n + 2]);
            *variant = g_strdup (keyboard_map[n + 3]);
            return;
        }
        n += 4;
    }
}

static gpointer set_locale (gpointer data)
{
    // set timezone
    if (g_strcmp0 (init_tz, city))
    {
        vsystem ("sudo raspi-config nonint do_change_timezone_rc_gui %s", city);
        if (init_tz)
        {
            g_free (init_tz);
            init_tz = g_strdup (city);
        }
    }

    // set keyboard
    if (g_strcmp0 (init_kb, lay) || g_strcmp0 (init_var, var))
    {
        reboot = TRUE;
        vsystem ("sudo raspi-config nonint do_change_keyboard_rc_gui pc105 %s %s", lay, var);
        if (init_kb)
        {
            g_free (init_kb);
            init_kb = g_strdup (lay);
        }
        if (init_var)
        {
            g_free (init_var);
            init_var = g_strdup (var);
        }
    }

    // set locale
    if (g_strcmp0 (init_country, cc) || g_strcmp0 (init_lang, lc))
    {
        reboot = TRUE;
        vsystem ("sudo raspi-config nonint do_change_locale_rc_gui %s_%s%s", lc, cc, ext);
        if (init_country)
        {
            g_free (init_country);
            init_country = g_strdup (cc);
        }
        if (init_lang)
        {
            g_free (init_lang);
            init_lang = g_strdup (lc);
        }
    }

    g_free (lay);
    g_free (var);
    g_free (city);

    g_idle_add (loc_done, NULL);
    return NULL;
}

static void deunicode (char **str)
{
    if (*str && strchr (*str, '<'))
    {
        int val;
        char *tmp = g_strdup (*str);
        char *pos = strchr (tmp, '<');

        if (sscanf (pos, "<U00%X>", &val) == 1)
        {
            *pos++ = val >= 0xC0 ? 0xC3 : 0xC2;
            *pos++ = val >= 0xC0 ? val - 0x40 : val;
            sprintf (pos, "%s", strchr (*str, '>') + 1);
            g_free (*str);
            *str = tmp;
        }
    }
}

static void read_locales (void)
{
    char *cname, *lname, *buffer, *buffer1, *cptr, *cptr1, *cptr2, *cptr3, *cptr4, *cname1;
    GtkTreeModelSort *scount;
    GtkTreeIter iter;
    FILE *fp, *fp1;
    size_t len, len1;
    int has_ext;

    // populate the locale database
    buffer = NULL;
    len = 0;
    fp = fopen ("/usr/share/i18n/SUPPORTED", "rb");
    if (fp)
    {
        while (getline (&buffer, &len, fp) > 0)
        {
            // does the line contain UTF-8; ignore lines with an @
            if (strstr (buffer, "UTF-8") && !strstr (buffer, "@"))
            {
                if (strstr (buffer, ".UTF-8")) has_ext = 1;
                else has_ext = 0;

                // split into lang and country codes
                cptr1 = strtok (buffer, "_");
                cptr2 = strtok (NULL, ". ");

                if (cptr1 && cptr2)
                {
                    // read names from locale file
                    cptr = g_strdup_printf ("%s_%s", cptr1, cptr2);
                    cname = get_quoted_param ("/usr/share/i18n/locales", cptr, "territory");
                    lname = get_quoted_param ("/usr/share/i18n/locales", cptr, "language");
                    g_free (cptr);

                    // deal with the likes of "malta"...
                    cname[0] = g_ascii_toupper (cname[0]);
                    lname[0] = g_ascii_toupper (lname[0]);

                    // deal with Curacao and Bokmal
                    deunicode (&cname);
                    deunicode (&lname);

                    gtk_list_store_append (locale_list, &iter);
                    gtk_list_store_set (locale_list, &iter, LL_LCODE, cptr1, LL_CCODE, cptr2, LL_LNAME, lname, LL_CHARS, has_ext ? ".UTF-8" : "", -1);
                    gtk_list_store_append (country_list, &iter);
                    gtk_list_store_set (country_list, &iter, CL_CNAME, cname, CL_CCODE, cptr2, -1);
                    g_free (cname);
                    g_free (lname);
                }
            }
        }
        g_free (buffer);
        fclose (fp);
    }

    // sort and filter the database to produce the list for the country combo
    scount = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (country_list)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (scount), CL_CNAME, GTK_SORT_ASCENDING);
    fcount = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (scount), NULL));
    gtk_tree_model_filter_set_visible_func (fcount, (GtkTreeModelFilterVisibleFunc) unique_rows, NULL, NULL);

    // populate the timezone database
    buffer = NULL;
    len = 0;
    fp = fopen ("/usr/share/zoneinfo/zone.tab", "rb");
    if (fp)
    {
        while (getline (&buffer, &len, fp) > 0)
        {
            // ignore lines starting #
            if (buffer[0] != '#')
            {
                // split on tabs
                cptr1 = strtok (buffer, "\t");
                strtok (NULL, "\t");
                cptr2 = strtok (NULL, "\t\n\r");

                if (cptr1 && cptr2)
                {
                    // split off the part after the final / and replace _ with space
                    if (strrchr (cptr2, '/')) cname = g_strdup (strrchr (cptr2, '/') + 1);
                    else cname = g_strdup (cptr2);
                    cptr = cname;
                    while (*cptr++) if (*cptr == '_') *cptr = ' ';
                    cptr4 = g_strdup_printf ("L %s", cptr2);

                    // zone1970.tab can include multiple comma-separated country codes...
                    cptr3 = strtok (cptr1, ",");
                    while (cptr3)
                    {
                        gtk_list_store_append (tz_list, &iter);
                        gtk_list_store_set (tz_list, &iter, TL_ZONE, cptr2, TL_CCODE, cptr3, TL_CITY, cname, TL_PREF, TRUE, -1);
                        buffer1 = NULL;
                        len1 = 0;
                        fp1 = fopen ("/usr/share/zoneinfo/tzdata.zi", "rb");
                        while (getline (&buffer1, &len1, fp1) > 0)
                        {
                            if (!strncmp (buffer1, cptr4, strlen (cptr4)))
                            {
                                // matching remap line in yet another database file
                                if (strrchr (strrchr (buffer1, ' '), '/'))
                                {
                                    cname1 = g_strdup (strrchr (strrchr (buffer1, ' '), '/') + 1);
                                    cptr = cname1;
                                    while (*cptr++)
                                    {
                                        if (*cptr == '_') *cptr = ' ';
                                        if (*cptr == '\n') *cptr = 0;
                                    }
                                    gtk_list_store_append (tz_list, &iter);
                                    gtk_list_store_set (tz_list, &iter, TL_ZONE, cptr2, TL_CCODE, cptr3, TL_CITY, cname1, TL_PREF, FALSE, -1);
                                    g_free (cname1);
                                }
                            }
                        }
                        fclose (fp1);
                        cptr3 = strtok (NULL, ",");
                    }
                    g_free (cptr4);
                    g_free (cname);
                }
            }
        }
        g_free (buffer);
        fclose (fp);
    }
}

static gboolean unique_rows (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    GtkTreeIter next = *iter;
    char *str1, *str2;
    gboolean res;

    if (!gtk_tree_model_iter_next (model, &next)) return TRUE;
    gtk_tree_model_get (model, iter, CL_CCODE, &str1, -1);
    gtk_tree_model_get (model, &next, CL_CCODE, &str2, -1);
    if (!g_strcmp0 (str1, str2)) res = FALSE;
    else res = TRUE;
    g_free (str1);
    g_free (str2);
    return res;
}

static gboolean unique_tzs (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    GtkTreeIter next = *iter;
    char *str1, *str2;
    gboolean res;

    if (!gtk_tree_model_iter_next (model, &next)) return TRUE;
    gtk_tree_model_get (model, iter, TL_CITY, &str1, -1);
    gtk_tree_model_get (model, &next, TL_CITY, &str2, -1);
    if (!g_strcmp0 (str1, str2)) res = FALSE;
    else res = TRUE;
    g_free (str1);
    g_free (str2);
    return res;
}

static void country_changed (GtkComboBox *cb, gpointer ptr)
{
    GtkTreeModel *model;
    GtkTreeModelFilter *flang, *fcity1;
    GtkTreeModelSort *scity;
    GtkTreeIter iter;
    char *str;

    // get the current country code from the combo box
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (country_cb));
    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (country_cb), &iter);
    gtk_tree_model_get (model, &iter, CL_CCODE, &str, -1);

    // filter and sort the master database for entries matching this code
    flang = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (locale_list), NULL));
    gtk_tree_model_filter_set_visible_func (flang, (GtkTreeModelFilterVisibleFunc) match_country, str, NULL);
    slang = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (flang)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (slang), LL_LNAME, GTK_SORT_ASCENDING);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (language_cb), GTK_TREE_MODEL (slang));
    gtk_combo_box_set_active (GTK_COMBO_BOX (language_cb), 0);

    // set the timezones for the country
    fcity1 = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (tz_list), NULL));
    gtk_tree_model_filter_set_visible_func (fcity1, (GtkTreeModelFilterVisibleFunc) match_country, str, NULL);
    scity = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (fcity1)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (scity), TL_CITY, GTK_SORT_ASCENDING);
    fcity = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (scity), NULL));
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (fcity), (GtkTreeModelFilterVisibleFunc) unique_tzs, NULL, NULL);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (timezone_cb), GTK_TREE_MODEL (fcity));
    gtk_combo_box_set_active (GTK_COMBO_BOX (timezone_cb), 0);

    g_free (str);
}

static gboolean match_country (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    char *str;
    gboolean res;

    gtk_tree_model_get (model, iter, LL_CCODE, &str, -1);
    if (!g_strcmp0 (str, (char *) data)) res = TRUE;
    else res = FALSE;
    g_free (str);
    return res;
}

static void read_inits (void)
{
    char *buffer, *llc, *lcc;

    init_country = NULL;
    init_lang = NULL;

    wifi_if = get_string ("for dir in /sys/class/net/*/wireless; do if [ -d \"$dir\" ] ; then basename \"$(dirname \"$dir\")\" ; fi ; done | head -n 1");
    init_tz = get_string ("cat /etc/timezone");
    init_kb = get_string ("grep XKBLAYOUT /etc/default/keyboard 2> /dev/null | cut -d = -f 2 | tr -d '\"\n'");
    init_var = get_string ("grep XKBVARIANT /etc/default/keyboard 2> /dev/null | cut -d = -f 2 | tr -d '\"\n'");
    if (init_kb && !init_var) init_var = g_strdup ("");
    buffer = get_string ("grep LC_ALL /etc/default/locale 2> /dev/null | cut -d = -f 2");
    if (!buffer) buffer = get_string ("grep LANGUAGE /etc/default/locale 2> /dev/null | cut -d = -f 2");
    if (!buffer) buffer = get_string ("grep LANG /etc/default/locale 2> /dev/null | cut -d = -f 2");
    if (buffer)
    {
        llc = strtok (buffer, "_");
        lcc = strtok (NULL, ":. ");
        if (llc && lcc)
        {
            init_country = g_strdup (lcc);
            init_lang = g_strdup (llc);
        }
        g_free (buffer);
    }
    else
    {
        init_country = g_strdup ("GB");
        init_lang = g_strdup ("en");
    }
}

static void set_init (GtkTreeModel *model, GtkWidget *cb, int pos, const char *init)
{
    GtkTreeIter iter;
    char *val;

    gtk_tree_model_get_iter_first (model, &iter);
    while (1)
    {
        gtk_tree_model_get (model, &iter, pos, &val, -1);
        if (!g_strcmp0 (init, val))
        {
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cb), &iter);
        }
        g_free (val);
        if (!gtk_tree_model_iter_next (model, &iter)) break;
    }
}

static void set_init_tz (GtkTreeModel *model, GtkWidget *cb, int pos, const char *init)
{
    GtkTreeIter iter;
    char *val;
    gboolean pref;

    gtk_tree_model_get_iter_first (model, &iter);
    while (1)
    {
        gtk_tree_model_get (model, &iter, pos, &val, TL_PREF, &pref, -1);
        if (!g_strcmp0 (init, val) && pref)
        {
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cb), &iter);
        }
        g_free (val);
        if (!gtk_tree_model_iter_next (model, &iter)) break;
    }
}

/* WiFi */

static int find_line (char **lssid, int *secure, int *connected)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *sel;

    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (ap_tv));
    if (sel && gtk_tree_selection_get_selected (sel, &model, &iter))
    {
        if (nm_ap_id) g_free (nm_ap_id);
        gtk_tree_model_get (model, &iter, AP_SSID, lssid, AP_SECURE, secure, AP_CONNECTED, connected, AP_DEVICE, &nm_dev, AP_AP, &nm_ap_id, -1);
        if (g_strcmp0 (*lssid, _("Searching for networks - please wait..."))) return 1;
    } 
    return 0;
}

void connect_success (void)
{
    if (conn_timeout)
    {
        g_source_remove (conn_timeout);
        conn_timeout = 0;
        hide_message ();
        change_page (FORWARD);
    }
}

static gint connect_failure (gpointer data)
{
    conn_timeout = 0;
    hide_message ();
    message (_("Failed to connect to network."), MSG_WAIT);
    return FALSE;
}

/* Network Manager - scanning */

// trigger a scan for APs
static void nm_start_scan (void)
{
    if (!wifi_if) return;
    if (!nm_scanning)
    {
        const GPtrArray *devices = nm_client_get_devices (nm_client);
        for (int i = 0; devices && i < devices->len; i++)
        {
            NMDevice *device = g_ptr_array_index (devices, i);
            if (NM_IS_DEVICE_WIFI (device))
            {
                g_signal_connect (device, "access-point-added", G_CALLBACK (nm_ap_changed_cb), NULL);
                g_signal_connect (device, "access-point-removed", G_CALLBACK (nm_ap_changed_cb), NULL);
                nm_device_wifi_request_scan_async (NM_DEVICE_WIFI (device), NULL, nm_req_scan_finish_cb, NULL);
            }
        }
        nm_scanning = TRUE;
    }
}

// stop a scan for APs
static void nm_stop_scan (void)
{
    if (!wifi_if) return;
    if (nm_scanning)
    {
        const GPtrArray *devices = nm_client_get_devices (nm_client);
        for (int i = 0; devices && i < devices->len; i++)
        {
            NMDevice *device = g_ptr_array_index (devices, i);
            if (NM_IS_DEVICE_WIFI (device))
            {
                g_signal_handlers_disconnect_by_func (device, G_CALLBACK (nm_ap_changed_cb), NULL);
            }
        }
        nm_scanning = FALSE;
    }
}

// callback to end async request_scan call
static void nm_req_scan_finish_cb (GObject *device, GAsyncResult *result, gpointer data)
{
    GError *error;
    nm_device_wifi_request_scan_finish (NM_DEVICE_WIFI (device), result, &error);
}

// callback generated during scan indicating that the list of APs has changed - update the list store
static void nm_ap_changed_cb (NMDeviceWifi *device, NMAccessPoint *unused, gpointer data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *sel;
    NMAccessPoint *ap;
    char *sel_ap, *active_ap;

    ap = nm_device_wifi_get_active_access_point (device);
    if (NM_IS_ACCESS_POINT (ap)) active_ap = nm_ap_get_id (ap);
    else active_ap = NULL;

    // get the selected line in the list of SSIDs
    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (ap_tv));
    if (sel && gtk_tree_selection_get_selected (sel, &model, &iter))
        gtk_tree_model_get (model, &iter, AP_AP, &sel_ap, -1);
    else sel_ap = NULL;

    // delete and recreate the list
    gtk_list_store_clear (ap_list);
    const GPtrArray *aps = nm_device_wifi_get_access_points (device);
    for (int i = 0; aps && i < aps->len; i++)
    {
        ap = g_ptr_array_index (aps, i);
        nm_ap_add (device, ap);
    }

    // if no selection has been made, select the active AP - always select the active AP after a connection...
    if (data || (!sel_ap && active_ap)) sel_ap = active_ap;

    // reselect the selected line
    if (sel_ap) gtk_tree_model_foreach (gtk_tree_view_get_model (GTK_TREE_VIEW (ap_tv)), nm_select_matched_ssid, sel_ap);

    // update connection state for the non-active entries in the list
    if (active_ap) gtk_tree_model_foreach (GTK_TREE_MODEL (ap_list), nm_set_connected_flag, active_ap);

    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (fap));
    if (gtk_tree_model_iter_n_children (fap, NULL)) gtk_widget_set_sensitive (ap_tv, TRUE);

    if (sel_ap != active_ap && sel_ap) g_free (sel_ap);
    if (active_ap) g_free (active_ap);
}

// add an AP to the list store
static void nm_ap_add (NMDeviceWifi *dev, NMAccessPoint *ap)
{
    GtkTreeIter iter;
    GdkPixbuf *sec_icon = NULL, *sig_icon = NULL;
    char *icon, *ssid_txt, *id;
    int signal, dsig, secure = 0;
    GBytes *ssid;

    if (!NM_IS_ACCESS_POINT (ap)) return;

    ssid = nm_access_point_get_ssid (ap);
    if (!ssid) return;

    ssid_txt = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
    if (!ssid_txt) return;

    id = nm_ap_get_id (ap);

    if ((nm_access_point_get_flags (ap) & NM_802_11_AP_FLAGS_PRIVACY) || nm_access_point_get_wpa_flags (ap)
        || nm_access_point_get_rsn_flags (ap))
    {
        sec_icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), "network-wireless-encrypted", 16, 0, NULL);
        secure = 1;
    }

    signal = nm_access_point_get_strength (ap);
    if (signal > 80) dsig = 100;
    else if (signal > 55) dsig = 75;
    else if (signal > 30) dsig = 50;
    else if (signal > 5) dsig = 25;
    else dsig = 0;

    icon = g_strdup_printf ("network-wireless-connected-%02d", dsig);
    sig_icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), icon, 16, 0, NULL);
    g_free (icon);

    gtk_list_store_append (ap_list, &iter);
    gtk_list_store_set (ap_list, &iter, AP_SSID, ssid_txt, AP_SEC_ICON, sec_icon, AP_SIG_ICON, sig_icon,
        AP_SECURE, secure, AP_DEVICE, dev, AP_AP, id, -1);
    g_free (ssid_txt);
    g_free (id);
}

// selects a list store item if it matches the supplied AP
static gboolean nm_select_matched_ssid (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    char *ap;
    gboolean res = FALSE;

    gtk_tree_model_get (model, iter, AP_AP, &ap, -1);

    if (!g_strcmp0 (ap, (char *) data))
    {
        gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (ap_tv)), iter);
        res = TRUE;
    }
    g_free (ap);
    return res;
}

// sets the connected flag for a list store item if it matches the supplied AP
static gboolean nm_set_connected_flag (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    char *ap;

    gtk_tree_model_get (model, iter, AP_AP, &ap, -1);

    if (!g_strcmp0 (ap, (char *) data)) gtk_list_store_set (GTK_LIST_STORE (model), iter, AP_CONNECTED, 1, -1);
    g_free (ap);
    return FALSE;
}


/* Network Manager - connection */

// request a connection to the AP stored in the nm_ap global
static void nm_connect_wifi (const char *password)
{
    NMSettingWirelessSecurity *sec = NULL;
    GPtrArray *conns;
    const char *path = nm_ap_get_path (nm_ap_id);

    if (!path)
    {
        if (conn_timeout)
        {
            g_source_remove (conn_timeout);
            conn_timeout = 0;
        }
        message (_("Failed to connect - access point not available."), MSG_WAIT);
        return;
    }

    /* create the security object from the password */
    if (password)
    {
        sec = NM_SETTING_WIRELESS_SECURITY (nm_setting_wireless_security_new ());
        g_object_set (G_OBJECT (sec), NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk",
            NM_SETTING_WIRELESS_SECURITY_PSK, password, NULL);
    }

    /* do we already have a connection to the desired AP from a previous failed attempt? */
    nm_wcon = NULL;
    conns = (GPtrArray*) nm_client_get_connections (nm_client);
    g_ptr_array_foreach (conns, nm_find_conn, nm_ap_id);

    if (nm_wcon)
    {
        /* pre-existing connection - update security and try again */
        if (sec)
        {
            nm_connection_add_setting (nm_wcon, NM_SETTING (sec));
            nm_remote_connection_commit_changes_async (NM_REMOTE_CONNECTION (nm_wcon), TRUE, NULL, nm_changes_committed_cb, (gpointer) path);
        }
        else
            nm_client_activate_connection_async (nm_client, nm_wcon, nm_dev, path, NULL, nm_connection_active_cb, (gpointer) 1);
    }
    else
    {
        /* new connection */
        nm_wcon = nm_simple_connection_new ();
        if (sec) nm_connection_add_setting (nm_wcon, NM_SETTING (sec));
        nm_client_add_and_activate_connection_async (nm_client, nm_wcon, nm_dev, path, NULL, nm_connection_active_cb, (gpointer) 0);
    }
}

// find existing connection so it can be edited and reused
static void nm_find_conn (gpointer data, gpointer user_data)
{
    const char *mode;
    char *ssid_txt, *id;
    NMConnection *conn = (NMConnection *) data;
    NMSettingWireless *wset = nm_connection_get_setting_wireless (conn);
    if (wset)
    {
        GBytes *ssid = nm_setting_wireless_get_ssid (wset);
        if (!ssid) return;
        ssid_txt = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
        if (!ssid_txt) return;

        mode = nm_setting_wireless_get_mode (wset);
        NM80211Mode nm = NM_802_11_MODE_INFRA;
        if (!g_strcmp0 (mode, "adhoc")) nm = NM_802_11_MODE_ADHOC;
        if (!g_strcmp0 (mode, "ap")) nm = NM_802_11_MODE_AP;
        if (!g_strcmp0 (mode, "mesh")) nm = NM_802_11_MODE_MESH;

        id = g_strdup_printf ("%s_%d", ssid_txt, nm);

        if (!strncmp (id, (char *) user_data, strlen (id))) nm_wcon = conn;

        g_free (ssid_txt);
        g_free (id);
    }
}

// callback to end async add_and_activate_connection call
static void nm_connection_active_cb (GObject *client, GAsyncResult *result, gpointer data)
{
    NMActiveConnection *active;
    GError *error = NULL;

    if (data)
        active = nm_client_activate_connection_finish (NM_CLIENT (client), result, &error);
    else
        active = nm_client_add_and_activate_connection_finish (NM_CLIENT (client), result, &error);

    if (error)
    {
        g_print ("Error adding connection: %s", error->message);
        g_error_free (error);
        connect_failure (NULL);
    }
    else g_timeout_add (1000, nm_check_connection, active);
}

// callback to end async commit_changes call
static void nm_changes_committed_cb (GObject *connection, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;

    if (nm_remote_connection_commit_changes_finish (NM_REMOTE_CONNECTION (connection), result, &error))
        nm_client_activate_connection_async (nm_client, NM_CONNECTION (connection), nm_dev,
            (char *) data, NULL, nm_connection_active_cb, (gpointer) 1);
    else
    {
        g_print ("Error committing changes: %s", error->message);
        g_error_free (error);
        connect_failure (NULL);
    }
}

// polled function to check that a newly-activated connection has actually become active
static gboolean nm_check_connection (gpointer data)
{
    NMActiveConnection *active = NM_ACTIVE_CONNECTION (data);
    NMActiveConnectionState state = nm_active_connection_get_state (active);

    if (state == NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
    {
        const GPtrArray *devices = nm_active_connection_get_devices (active);
        for (int i = 0; devices && i < devices->len; i++)
        {
            NMDevice *device = g_ptr_array_index (devices, i);
            nm_ap_changed_cb (NM_DEVICE_WIFI (device), NULL, (void *) 1);
        }
        g_object_unref (active);

        connect_success ();
        return FALSE;
    }
    return TRUE;
}

// helper function to create unique string for an AP
static char *nm_ap_get_id (NMAccessPoint *ap)
{
    GBytes *ssid = nm_access_point_get_ssid (ap);
    if (!ssid) return NULL;
    char *ssid_txt = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
    if (!ssid_txt) return NULL;

    NM80211Mode mode = nm_access_point_get_mode (ap);
    NM80211ApFlags flags = nm_access_point_get_flags (ap);
    NM80211ApSecurityFlags wpa_flags = nm_access_point_get_wpa_flags (ap);
    NM80211ApSecurityFlags rsn_flags = nm_access_point_get_rsn_flags (ap);

    char *res = g_strdup_printf ("%s_%d_%d_%d_%d", ssid_txt, mode, flags, wpa_flags, rsn_flags);
    g_free (ssid_txt);
    return res;
}

// helper function to get the object path for the AP with the supplied ID
static const char *nm_ap_get_path (char *id)
{
    NMAccessPoint *ap;
    char *ap_id;

    const GPtrArray *aps = nm_device_wifi_get_access_points (NM_DEVICE_WIFI (nm_dev));
    for (int i = 0; aps && i < aps->len; i++)
    {
        ap = NM_ACCESS_POINT (g_ptr_array_index (aps, i));
        ap_id = nm_ap_get_id (ap);
        if (!g_strcmp0 (ap_id, id))
        {
            g_free (ap_id);
            return nm_object_get_path (NM_OBJECT (ap));
        }
        else g_free (ap_id);
    }
    return NULL;
}

// filter function used to determine if an AP is already in the list store
static gboolean nm_filter_dup_ap (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    GtkTreeIter it2 = *iter;
    char *ap1, *ap2;
    gboolean res = TRUE;

    gtk_tree_model_get (model, iter, AP_AP, &ap1, -1);
    if (!ap1) return res;

    while (gtk_tree_model_iter_previous (model, &it2))
    {
        gtk_tree_model_get (model, &it2, AP_AP, &ap2, -1);
        if (!g_strcmp0 (ap1, ap2)) res = FALSE;
        g_free (ap2);
        if (!res) break;
    }

    g_free (ap1);
    return res;
}

// extract PSK for already-known SSID
static char *nm_find_psk_for_network (char *ssid)
{
    char *cmd, *fname, *res;

    // find the file with a matching SSID
    cmd = g_strdup_printf ("grep -l 'ssid=%s$' /etc/NetworkManager/system-connections/*.nmconnection 2> /dev/null", ssid);
    fname = get_shell_string (cmd, TRUE);
    g_free (cmd);

    if (!fname) return NULL;

    cmd = g_strdup_printf ("grep 'psk=' '%s' | cut -d = -f 2", fname);
    res = get_shell_string (cmd, TRUE);
    g_free (cmd);
    g_free (fname);

    return res;
}

// initiate scan for networks
static void start_scanning (void)
{
    if (!wifi_if) return;

    if (!nm_client)
    {
        GtkTreeIter iter;
        nm_client = nm_client_new (NULL, NULL);
        gtk_list_store_clear (ap_list);
        gtk_list_store_append (ap_list, &iter);
        gtk_list_store_set (ap_list, &iter, AP_SSID, _("Searching for networks - please wait..."),
            AP_SEC_ICON, NULL, AP_SIG_ICON, NULL, AP_AP, NULL, -1);
        gtk_widget_set_sensitive (ap_tv, FALSE);
    }
    nm_start_scan ();
}


/* Updates */

static void progress (PkProgress *progress, PkProgressType type, gpointer data)
{
    int role = pk_progress_get_role (progress);
    int status = pk_progress_get_status (progress);
    int percent = pk_progress_get_percentage (progress);

    if (msg_dlg)
    {
        if ((type == PK_PROGRESS_TYPE_PERCENTAGE || type == PK_PROGRESS_TYPE_ITEM_PROGRESS
            || type == PK_PROGRESS_TYPE_PACKAGE || type == PK_PROGRESS_TYPE_PACKAGE_ID
            || type == PK_PROGRESS_TYPE_DOWNLOAD_SIZE_REMAINING) && percent >= 0 && percent <= 100)
        {
            switch (role)
            {
                case PK_ROLE_ENUM_UPDATE_PACKAGES :     if (status == PK_STATUS_ENUM_DOWNLOAD)
                                                            message (_("Downloading updates - please wait..."), percent);
                                                        else if (status == PK_STATUS_ENUM_RUNNING)
                                                            message (_("Installing updates - please wait..."), percent);
                                                        else
                                                            gtk_progress_bar_pulse (GTK_PROGRESS_BAR (msg_pb));
                                                        break;

                case PK_ROLE_ENUM_INSTALL_PACKAGES :    if (status == PK_STATUS_ENUM_DOWNLOAD)
                                                            message (_("Downloading languages - please wait..."), percent);
                                                        else if (status == PK_STATUS_ENUM_RUNNING)
                                                            message (_("Installing languages - please wait..."), percent);
                                                        else
                                                            gtk_progress_bar_pulse (GTK_PROGRESS_BAR (msg_pb));
                                                        break;

                case PK_ROLE_ENUM_REMOVE_PACKAGES :     if (status == PK_STATUS_ENUM_RUNNING)
                                                            message (_("Uninstalling browser - please wait..."), percent);
                                                        else
                                                            gtk_progress_bar_pulse (GTK_PROGRESS_BAR (msg_pb));
                                                        break;

                default :                               gtk_progress_bar_pulse (GTK_PROGRESS_BAR (msg_pb));
                                                        break;
            }
        }
        else gtk_progress_bar_pulse (GTK_PROGRESS_BAR (msg_pb));
    }
}

static gboolean inst_filter_fn (PkPackage *package, gpointer user_data)
{
    if (!is_pi && strstr (pk_package_get_arch (package), "amd64")) return FALSE;

    PkInfoEnum info = pk_package_get_info (package);
	switch (info)
    {
        case PK_INFO_ENUM_AVAILABLE:    return TRUE;
                                        break;

        default:                        return FALSE;
                                        break;
    }
}

static gboolean rem_filter_fn (PkPackage *package, gpointer user_data)
{
    if (!is_pi && strstr (pk_package_get_arch (package), "amd64")) return FALSE;

    PkInfoEnum info = pk_package_get_info (package);
	switch (info)
    {
        case PK_INFO_ENUM_INSTALLED:    return TRUE;
                                        break;

        default:                        return FALSE;
                                        break;
    }
}

static gboolean upd_filter_fn (PkPackage *package, gpointer user_data)
{
    if (!is_pi && strstr (pk_package_get_arch (package), "amd64")) return FALSE;

    PkInfoEnum info = pk_package_get_info (package);
	switch (info)
    {
        case PK_INFO_ENUM_LOW:
        case PK_INFO_ENUM_NORMAL:
        case PK_INFO_ENUM_IMPORTANT:
        case PK_INFO_ENUM_SECURITY:
        case PK_INFO_ENUM_BUGFIX:
        case PK_INFO_ENUM_ENHANCEMENT:
        case PK_INFO_ENUM_BLOCKED:      return TRUE;
                                        break;

        default:                        return FALSE;
                                        break;
    }
}

static void next_update (PkTask *task, update_type update_stage)
{
    gchar **pack_array;
    gchar *lpack, *buf, *tmp;

    switch (update_stage)
    {
        case INSTALL_LANGUAGES:
            buf = g_strdup_printf ("check-language-support -l %s_%s", lc, cc);
            lpack = get_shell_string (buf, TRUE);
            g_free (buf);

            if (!g_strcmp0 (lc, "ja"))
            {
                tmp = g_strdup_printf ("%s%s%s", JAPAN_FONTS, lpack ? " " : "", lpack);
                g_free (lpack);
                lpack = tmp;
            }

            if (lpack)
            {
                pack_array = g_strsplit (lpack, " ", -1);

                thread_message (_("Installing languages - please wait..."), MSG_PULSE);
                pk_client_resolve_async (PK_CLIENT (task), 0, pack_array, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) resolve_lang_done, NULL);
                g_strfreev (pack_array);
                g_free (lpack);
                break;
            }

        case UNINSTALL_BROWSER:
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uninstall_chk)))
            {
                if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chromium_rb))) lpack = g_strdup ("firefox");
                else lpack = g_strdup ("chromium-browser");
                pack_array = g_strsplit (lpack, " ", -1);
                thread_message (_("Uninstalling browser - please wait..."), MSG_PULSE);
                pk_client_resolve_async (PK_CLIENT (task), 0, pack_array, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) resolve_browser_done, NULL);
                g_strfreev (pack_array);
                g_free (lpack);
                break;
            }

        case INSTALL_UPDATES:
            thread_message (_("Getting updates - please wait..."), MSG_PULSE);
            pk_client_get_updates_async (PK_CLIENT (task), PK_FILTER_ENUM_NONE, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) check_updates_done, NULL);
    }
}

static gpointer refresh_update_cache (gpointer data)
{
    PkTask *task = pk_task_new ();
    pk_client_refresh_cache_async (PK_CLIENT (task), TRUE, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) refresh_cache_done, NULL);
    return NULL;
}

static void refresh_cache_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error checking for updates.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    next_update (task, INSTALL_LANGUAGES);
}

static void resolve_lang_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);
    PkPackage *item;
    gchar **ids;
    int i;

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error installing languages.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    PkPackageSack *sack = pk_results_get_package_sack (results);
    PkPackageSack *fsack = pk_package_sack_filter (sack, inst_filter_fn, NULL);

    // remove armhf packages for which there is an arm64 equivalent...
    GPtrArray *array = pk_package_sack_get_array (fsack);
    for (i = 0; i < array->len; i++)
    {
        gchar *package_id, *arch;
        item = g_ptr_array_index (array, i);
        g_object_get (item, "package-id", &package_id, NULL);
        if ((arch = strstr (package_id, "arm64")))
        {
            *(arch + 3) = 'h';
            *(arch + 4) = 'f';
            item = pk_package_sack_find_by_id_name_arch (fsack, package_id);
            if (item) pk_package_sack_remove_package (fsack, item);
        }
        g_free (package_id);
    }
    g_ptr_array_unref (array);

    if (pk_package_sack_get_size (fsack) > 0)
    {
        thread_message (_("Installing languages - please wait..."), MSG_PULSE);

        ids = pk_package_sack_get_ids (fsack);
        pk_task_install_packages_async (task, ids, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) install_lang_done, NULL);
        g_strfreev (ids);
    }
    else next_update (task, UNINSTALL_BROWSER);

    g_object_unref (sack);
    g_object_unref (fsack);
}

static void install_lang_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error installing languages.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    next_update (task, UNINSTALL_BROWSER);
}

static void resolve_browser_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);
    gchar **ids;

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error uninstalling browser.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    PkPackageSack *sack = pk_results_get_package_sack (results);
    PkPackageSack *fsack = pk_package_sack_filter (sack, rem_filter_fn, NULL);

    if (pk_package_sack_get_size (fsack) > 0)
    {
        thread_message (_("Uninstalling browser - please wait..."), MSG_PULSE);

        ids = pk_package_sack_get_ids (fsack);
        pk_task_remove_packages_async (task, ids, TRUE, TRUE, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) uninstall_browser_done, NULL);
        g_strfreev (ids);
    }
    else next_update (task, INSTALL_UPDATES);

    g_object_unref (sack);
    g_object_unref (fsack);
}

static void uninstall_browser_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error uninstalling browser.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    next_update (task, INSTALL_UPDATES);
}

static void check_updates_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);
    gchar **ids;

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error getting updates.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    PkPackageSack *sack = pk_results_get_package_sack (results);
    PkPackageSack *fsack = pk_package_sack_filter (sack, upd_filter_fn, NULL);

    if (pk_package_sack_get_size (fsack) > 0)
    {
        thread_message (_("Getting updates - please wait..."), MSG_PULSE);

        ids = pk_package_sack_get_ids (fsack);
        pk_task_update_packages_async (task, ids, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) do_updates_done, NULL);
        g_strfreev (ids);
    }
    else
    {
        // check reboot flag set by install process
        if (!access ("/run/reboot-required", F_OK)) reboot = TRUE;

        thread_message (_("System is up to date"), MSG_TERM);
    }

    g_object_unref (sack);
    g_object_unref (fsack);
}

static void do_updates_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error getting updates.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    // check reboot flag set by install process
    if (!access ("/run/reboot-required", F_OK)) reboot = TRUE;

    // re-set the serial number in case a browser update was installed
#if HOMESCHOOL
    set_hs_serial ();
#else
    set_marketing_serial ("/etc/chromium/master_preferences");
    set_marketing_serial ("/usr/share/firefox/distribution/distribution.ini");
#endif
    thread_message (_("System is up to date"), MSG_TERM);
}

static gboolean clock_synced (void)
{
    if (system ("test -e /usr/sbin/ntpd") == 0)
    {
        if (system ("ntpq -p | grep -q ^\\*") == 0) return TRUE;
    }
    else
    {
        if (system ("timedatectl status | grep -q \"synchronized: yes\"") == 0) return TRUE;
    }
    return FALSE;
}

static void resync (void)
{
    if (system ("test -e /usr/sbin/ntpd") == 0)
    {
        vsystem ("sudo /etc/init.d/ntp stop; sudo ntpd -gq; sudo /etc/init.d/ntp start");
    }
    else
    {
        vsystem ("sudo systemctl -q stop systemd-timesyncd 2> /dev/null; sudo systemctl -q start systemd-timesyncd 2> /dev/null");
    }
}

static gboolean ntp_check (gpointer data)
{
    if (clock_synced ())
    {
        message (_("Checking for updates - please wait..."), MSG_PULSE);
        g_thread_new (NULL, refresh_update_cache, NULL);
        return FALSE;
    }
    // trigger a resync
    if (calls == 0) resync ();

    if (calls++ > 120)
    {
        message (_("Could not sync time - unable to check for updates"), MSG_TERM);
        return FALSE;
    }

    return TRUE;
}

/* Final configuration */

static gpointer final_setup (gpointer ptr)
{
#ifdef HOMESCHOOL
    if (chuser == NULL)
    {
        vsystem ("sudo cp /usr/share/applications/chromium-browser.desktop /etc/xdg/autostart/");
        vsystem ("sudo mkdir -p /home/pi/Desktop; sudo chown pi:pi /home/pi/Desktop/");
        vsystem ("echo \"[Desktop Entry]\nType=Link\nName=Web Browser\nIcon=applications-internet\nURL=/usr/share/applications/chromium-browser.desktop\" | sudo tee /home/pi/Desktop/chromium-browser.desktop; sudo chown pi:pi /home/pi/Desktop/chromium-browser.desktop");
    }
#endif
    // copy the wayfire config file for the new user
    if (!chuser) vsystem ("sudo mkdir -p /home/pi/.config/; sudo chown pi:pi /home/pi/.config/; sudo cp /home/rpi-first-boot-wizard/.config/wayfire.ini /home/pi/.config/wayfire.ini; sudo chown pi:pi /home/pi/.config/wayfire.ini");

    // rename the pi user to the new user and set the password
    vsystem ("sudo /usr/lib/userconf-pi/userconf %s %s '%s'", chuser ? chuser : "pi", user, pw);

    // set an autostart to set HDMI audio on reboot as new user
    if (chuser == NULL) vsystem ("echo \"[Desktop Entry]\nType=Application\nName=Select HDMI Audio\nExec=sh -c '/usr/bin/hdmi-audio-select; sudo rm /etc/xdg/autostart/hdmiaudio.desktop'\" | sudo tee /etc/xdg/autostart/hdmiaudio.desktop", user);

    if (reboot) vsystem ("sync; sudo reboot");
    gtk_main_quit ();
    return NULL;
}

/* Page management */

static void page_changed (GtkNotebook *notebook, GtkWidget *page, int pagenum, gpointer data)
{
    gtk_button_set_label (GTK_BUTTON (next_btn), _("_Next"));
    gtk_button_set_label (GTK_BUTTON (prev_btn), _("_Back"));
    gtk_button_set_label (GTK_BUTTON (skip_btn), _("_Skip"));
    gtk_widget_hide (skip_btn);
    gtk_widget_show (prev_btn);

    switch (pagenum)
    {
        case PAGE_INTRO :   gtk_widget_hide (prev_btn);
                            break;

        case PAGE_PASSWD :  if (chuser != NULL)
                            {
                                gtk_label_set_markup (GTK_LABEL (rename_title), _("<b>Rename User</b>"));
                                char *msg = g_strdup_printf (_("Your current user '%s' will be renamed.\n\nThe new username can only contain lower-case letters, digits and hyphens, and must start with a letter."), chuser);
                                gtk_label_set_text (GTK_LABEL (rename_info), msg);
                                g_free (msg);
                                gtk_label_set_text (GTK_LABEL (rename_prompt), _("Press 'Next' to rename the user."));
                                gtk_button_set_label (GTK_BUTTON (prev_btn), _("_Cancel"));
                            }
                            else start_scanning ();
                            break;

        case PAGE_DONE :    if (reboot)
                            {
                                gtk_widget_show (prompt);
                                gtk_button_set_label (GTK_BUTTON (next_btn), _("_Restart"));
                                if (chuser != NULL)
                                {
                                    gtk_label_set_markup (GTK_LABEL (done_title), _("<b>User Renamed</b>"));
                                    char *msg = g_strdup_printf (_("The '%s' user has been renamed to '%s' and the new password set."), chuser, user);
                                    gtk_label_set_text (GTK_LABEL (done_info), msg);
                                    g_free (msg);
                                    gtk_label_set_text (GTK_LABEL (done_prompt), _("Press 'Restart' to reboot and login as the new user."));
                                }
                            }
                            else
                            {
                                gtk_widget_hide (prompt);
                                gtk_button_set_label (GTK_BUTTON (next_btn), _("_Done"));
                            }
                            break;

        case PAGE_WIFIAP :  gtk_widget_show (skip_btn);
                            break;

        case PAGE_WIFIPSK : gtk_widget_show (skip_btn);
                            break;

        case PAGE_UPDATE :  gtk_widget_show (skip_btn);
                            break;

        case PAGE_BROWSER : gtk_widget_set_visible (uninstall_chk, net_available ());
                            break;
    }

    // restore the keyboard focus
    if (pagenum == PAGE_DONE) gtk_widget_grab_focus (next_btn);
    else
    {
        if (last_btn == PREV_BTN) gtk_widget_grab_focus (prev_btn);
        else if (last_btn == SKIP_BTN && gtk_widget_get_visible (skip_btn)) gtk_widget_grab_focus (skip_btn);
        else gtk_widget_grab_focus (next_btn);
    }
}

static gboolean page_shown (int page)
{
    switch (page)
    {
        case PAGE_OSCAN :   return (is_pi && wm == WM_OPENBOX);

        case PAGE_WIFIAP :  if (wifi_if) return TRUE;
                            else return FALSE;

        case PAGE_BROWSER : return browser;

        case PAGE_RPC :     return rpc;

        case PAGE_WIFIPSK : return FALSE;

        case PAGE_INTRO :
        case PAGE_LOCALE :
        case PAGE_PASSWD :
        case PAGE_UPDATE :
        case PAGE_DONE :
        default :           return TRUE;
    }
}

static void change_page (int dir)
{
    int page;

    for (page = gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)) + dir;
        dir == BACKWARD ? page >= PAGE_INTRO : page < PAGE_DONE; page += dir)
    {
        if (page_shown (page))
        {
            gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), page);
            break;
        }
    }
}

static void next_page (GtkButton* btn, gpointer ptr)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    const char *ccptr;
    char *wc, *text;
    int secure, connected;

    last_btn = NEXT_BTN;
    switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)))
    {
        case PAGE_INTRO :   // disable Bluetooth autoconnect after first page
                            if (!system ("rfkill list bluetooth | grep -q Bluetooth"))
                            {
                                if (wm != WM_OPENBOX) vsystem ("wfpanelctl bluetooth apstop");
                                else vsystem ("lxpanelctl command bluetooth apstop");
                            }
                            change_page (FORWARD);
                            break;

        case PAGE_LOCALE :  // get the combo entries and look up relevant codes in database
                            model = gtk_combo_box_get_model (GTK_COMBO_BOX (language_cb));
                            gtk_combo_box_get_active_iter (GTK_COMBO_BOX (language_cb), &iter);
                            gtk_tree_model_get (model, &iter, LL_LCODE, &lc, -1);
                            gtk_tree_model_get (model, &iter, LL_CCODE, &cc, -1);
                            gtk_tree_model_get (model, &iter, LL_CHARS, &ext, -1);
                            wc = g_strdup (cc);

                            model = gtk_combo_box_get_model (GTK_COMBO_BOX (timezone_cb));
                            gtk_combo_box_get_active_iter (GTK_COMBO_BOX (timezone_cb), &iter);
                            gtk_tree_model_get (model, &iter, TL_ZONE, &city, -1);

                            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uskey_chk)))
                            {
                                lay = g_strdup ("us");
                                var = g_strdup ("");
                            }
                            else lookup_keyboard (cc, lc, &lay, &var);

                            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (eng_chk)))
                            {
                                // override language setting
                                g_free (lc);
                                g_free (cc);
                                g_free (ext);
                                lc = g_strdup ("en");
                                cc = g_strdup ("US");
                                ext = g_strdup (".UTF-8");
                            }

                            // set wifi country - this is quick, so no need for warning
                            if (wifi_if)
                            {
                                vsystem ("sudo raspi-config nonint do_wifi_country %s > /dev/null", wc);
                            }
                            g_free (wc);

                            if (g_strcmp0 (init_tz, city) || g_strcmp0 (init_country, cc)
                                || g_strcmp0 (init_lang, lc) || g_ascii_strcasecmp (init_kb, lay)
                                || g_strcmp0 (init_var, var))
                            {
                                message (_("Setting location - please wait..."), MSG_PULSE);
                                g_thread_new (NULL, set_locale, NULL);
                            }
                            else change_page (FORWARD);
                            break;

        case PAGE_PASSWD :  if (user != NULL)
                            {
                                g_free (user);
                                user = NULL;
                            }
                            if (pw != NULL)
                            {
                                g_free (pw);
                                pw = NULL;
                            }
                            ccptr = gtk_entry_get_text (GTK_ENTRY (user_te));
                            if (!strlen (ccptr))
                            {
                                message (_("The username is blank."), MSG_WAIT);
                                break;
                            }
                            if (strlen (ccptr) > 32)
                            {
                                message (_("The username must be 32 characters or shorter."), MSG_WAIT);
                                break;
                            }
                            if (*ccptr < 'a' || *ccptr > 'z')
                            {
                                message (_("The first character of the username must be a lower-case letter."), MSG_WAIT);
                                break;
                            }
                            if (!g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (user_te)), "rpi-first-boot-wizard"))
                            {
                                message (_("This username is used by the system and cannot be used for a user account."), MSG_WAIT);
                                break;
                            }
                            if (!g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (user_te)), "root"))
                            {
                                message (_("This username is used by the system and cannot be used for a user account."), MSG_WAIT);
                                break;
                            }
                            while (*++ccptr)
                            {
                                if ((*ccptr < 'a' || *ccptr > 'z') && (*ccptr < '0' || *ccptr > '9') && *ccptr != '-') break;
                            }
                            if (*ccptr)
                            {
                                message (_("Usernames can only contain lower-case letters, digits and hyphens."), MSG_WAIT);
                                break;
                            }
                            if (!strlen (gtk_entry_get_text (GTK_ENTRY (pwd1_te))) || !strlen (gtk_entry_get_text (GTK_ENTRY (pwd2_te))))
                            {
                                message (_("The password is blank."), MSG_WAIT);
                                break;
                            }
                            if (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (pwd1_te)), gtk_entry_get_text (GTK_ENTRY (pwd2_te))))
                            {
                                message (_("The two passwords entered do not match."), MSG_WAIT);
                                break;
                            }
                            if (!g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (user_te)), "pi") || !g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (pwd1_te)), "raspberry"))
                            {
                                message (_("You have used a known default value for the username or password.\n\nWe strongly recommend you go back and choose something else."), MSG_WAIT);
                            }
                            user = g_strdup (gtk_entry_get_text (GTK_ENTRY (user_te)));
                            pw = g_strdup (crypt (gtk_entry_get_text (GTK_ENTRY (pwd1_te)), crypt_gensalt (NULL, 0, NULL, 0)));
                            if (chuser != NULL) gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_DONE);
                            else change_page (FORWARD);
                            break;

        case PAGE_OSCAN :   change_page (FORWARD);
                            break;

        case PAGE_WIFIAP :  if (ssid) g_free (ssid);
                            ssid = NULL;
                            if (!find_line (&ssid, &secure, &connected)) change_page (FORWARD);
                            else
                            {
                                if (connected) change_page (FORWARD);
                                else if (secure)
                                {
                                    text = g_strdup_printf (_("Enter the password for the WiFi network \"%s\"."), ssid);
                                    gtk_label_set_text (GTK_LABEL (psk_label), text);
                                    g_free (text);
                                    text = nm_find_psk_for_network (ssid);
                                    gtk_entry_set_text (GTK_ENTRY (psk_te), text ? text : "");
                                    if (text) g_free (text);
                                    gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_WIFIPSK);
                                }
                                else
                                {
                                    message (_("Connecting to WiFi network - please wait..."), MSG_PULSE);
                                    conn_timeout = g_timeout_add (30000, connect_failure, NULL);
                                    nm_connect_wifi (NULL);
                                }
                            }
                            break;

        case PAGE_WIFIPSK : ccptr = gtk_entry_get_text (GTK_ENTRY (psk_te));
                            message (_("Connecting to WiFi network - please wait..."), MSG_PULSE);
                            conn_timeout = g_timeout_add (30000, connect_failure, NULL);
                            nm_connect_wifi (ccptr);
                            break;

        case PAGE_BROWSER : if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chromium_rb)))
                                vsystem ("sudo raspi-config nonint do_browser chromium-browser pi");
                            else
                                vsystem ("sudo raspi-config nonint do_browser firefox pi");
                            change_page (FORWARD);
                            break;

        case PAGE_RPC :     if (gtk_switch_get_active (GTK_SWITCH (rpc_sw)))
                                vsystem ("sudo systemctl --global enable rpi-connect;sudo systemctl --global enable rpi-connect-wayvnc");
                            else
                                vsystem ("sudo systemctl --global disable rpi-connect;sudo systemctl --global disable rpi-connect-wayvnc");
                            change_page (FORWARD);
                            break;

        case PAGE_UPDATE :  if (net_available ())
                            {
                                if (clock_synced ())
                                {
                                    message (_("Checking for updates - please wait..."), MSG_PULSE);
                                    g_thread_new (NULL, refresh_update_cache, NULL);
                                }
                                else
                                {
                                    message (_("Synchronising clock - please wait..."), MSG_PULSE);
                                    calls = 0;
                                    g_timeout_add_seconds (1, ntp_check, NULL);
                                }
                            }
                            else message (_("No network connection found - unable to check for updates"), MSG_TERM);
                            break;

        case PAGE_DONE :    nm_stop_scan ();
                            message (_("Restarting - please wait..."), MSG_PULSE);
                            gtk_widget_hide (main_dlg);
                            g_thread_new (NULL, final_setup, NULL);
                            break;
    }
}

static void prev_page (GtkButton* btn, gpointer ptr)
{
    last_btn = PREV_BTN;

    if (chuser != NULL)
    {
        int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb));
        if (page == PAGE_DONE) gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_PASSWD);
        else if (page == PAGE_PASSWD)
        {
            // call the script to cancel renaming and reset original user
            vsystem ("sudo /usr/bin/cancel-rename %s", chuser);
            vsystem ("sync;reboot");
            gtk_main_quit ();
        }
    }
    else change_page (BACKWARD);
}

static void skip_page (GtkButton* btn, gpointer ptr)
{
    last_btn = SKIP_BTN;
    switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)))
    {
        case PAGE_WIFIAP :
        case PAGE_WIFIPSK : change_page (FORWARD);
                            break;

        case PAGE_UPDATE :  gchar *buf = g_strdup_printf ("check-language-support -l %s_%s", lc, cc);
                            gchar *lpack = get_shell_string (buf, TRUE);
                            gboolean uninst = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uninstall_chk));
                            if (lpack && uninst)
                                message (_("If installing updates is skipped, translation files will not be installed, and the unused browser will not be uninstalled."), MSG_TERM);
                            else if (lpack)
                                message (_("If installing updates is skipped, translation files will not be installed."), MSG_TERM);
                            else if (uninst)
                                message (_("If installing updates is skipped, the unused browser will not be uninstalled."), MSG_TERM);
                            else change_page (FORWARD);
                            g_free (buf);
                            g_free (lpack);
                            break;
    }
}

static void pwd_toggle (GtkButton *btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pwd_hide)))
    {
        gtk_entry_set_visibility (GTK_ENTRY (pwd1_te), FALSE);
        gtk_entry_set_visibility (GTK_ENTRY (pwd2_te), FALSE);
    }
    else
    {
        gtk_entry_set_visibility (GTK_ENTRY (pwd1_te), TRUE);
        gtk_entry_set_visibility (GTK_ENTRY (pwd2_te), TRUE);
    }
}

static void psk_toggle (GtkButton *btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (psk_hide)))
    {
        gtk_entry_set_visibility (GTK_ENTRY (psk_te), FALSE);
    }
    else
    {
        gtk_entry_set_visibility (GTK_ENTRY (psk_te), TRUE);
    }
}

static gboolean show_ip (void)
{
    char *ip, *buf;

    // display the ip address on the first page
    ip = get_string ("hostname -I | tr ' ' \\\\n | grep \\\\. | tr \\\\n ','");
    if (ip)
    {
        buf = ip;
        do
        {
            if (*buf == ',')
            {
                if (*(buf + 1)) *buf = '\t';
                else *buf = 0;
            }
        }
        while (*buf++);
        buf = g_strdup_printf (_("<span font_desc=\"10.0\">IP : %s</span>"), ip);
        gtk_label_set_markup (GTK_LABEL (ip_label), buf);
        g_free (buf);
        g_free (ip);
        return FALSE;
    }
    g_free (ip);
    return TRUE;
}

#ifndef HOMESCHOOL
static void set_marketing_serial (const char *file)
{
    if (is_pi)
    {
        if (access (file, F_OK) != -1)
        {
            vsystem ("sudo sed -i %s -e s/UNIDENTIFIED/`cat /proc/cpuinfo | grep Serial | sha256sum | cut -d ' ' -f 1`/g", file);
        }
    }
}
#else
static void set_hs_serial (void)
{
    if (is_pi)
    {
        if (access ("/etc/chromium/master_preferences", F_OK) != -1)
        {
            vsystem ("sudo sed -i /etc/chromium/master_preferences -e s/UNIDENTIFIED/`cat /proc/cpuinfo | grep Serial | sha256sum | cut -d ' ' -f 1`/g");
        }
    }
}
#endif

static gboolean net_available (void)
{
    char *ip;
    gboolean val = FALSE;

    ip = get_string ("hostname -I | tr ' ' \\\\n | grep \\\\. | tr \\\\n ','");
    if (ip)
    {
        val = TRUE;
        g_free (ip);
    }
    return val;
}

static gboolean check_bluetooth (void)
{
    if (!system ("rfkill list bluetooth | grep -q Bluetooth")) gtk_widget_show (bt_prompt);
    return FALSE;
}

static gboolean srprompt (gpointer data)
{
    if (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)) == PAGE_INTRO)
    {
        if (net_available () && clock_synced ())
        {
            char *args[3] = { "/usr/bin/aplay", "srprompt.wav", NULL };
            g_spawn_async (PACKAGE_DATA_DIR, args, NULL, 0, NULL, NULL, NULL, NULL);
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_main_quit ();
    return TRUE;
}

/* Underscan */

static void uscan_toggle (GtkSwitch *sw, gpointer ptr)
{
    int enable = gtk_switch_get_active (sw) ? 0 : 1;
    if (GTK_WIDGET (sw) == uscan2_sw)
        vsystem ("sudo raspi-config nonint do_overscan_kms 2 %d", enable);
    else
        vsystem ("sudo raspi-config nonint do_overscan_kms 1 %d", enable);
}

static int num_screens (void)
{
    if (wm != WM_OPENBOX)
        return get_status ("wlr-randr | grep -cv '^ '");
    else
        return get_status ("xrandr -q | grep -cw connected");
}


/* The dialog... */

int main (int argc, char *argv[])
{
    GtkBuilder *builder;
    GtkCellRenderer *col;
    int res, kbd;

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    if (system ("raspi-config nonint is_pi")) is_pi = FALSE;
    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE")) wm = WM_WAYFIRE;
        else wm = WM_LABWC;
    }
    else wm = WM_OPENBOX;

    // set the audio output to HDMI if there is one, otherwise the analog jack
    vsystem ("hdmi-audio-select");

    // read country code from Pi keyboard, if any
    kbd = get_pi_keyboard ();
    if (kbd > MAX_KBS - 1) kbd = 0;

#ifdef HOMESCHOOL
    vsystem ("sudo familyshield on");
#endif

    reboot = TRUE;
    read_inits ();

#ifdef HOMESCHOOL
    browser = FALSE;
    set_hs_serial ();
#else
    if (vsystem ("raspi-config nonint is_installed chromium-browser")) browser = FALSE;
    if (vsystem ("raspi-config nonint is_installed firefox")) browser = FALSE;
    set_marketing_serial ("/etc/chromium/master_preferences");
    set_marketing_serial ("/usr/share/firefox/distribution/distribution.ini");
#endif

    if (vsystem ("raspi-config nonint is_installed rpi-connect")) rpc = FALSE;
    if (wm == WM_OPENBOX) rpc = FALSE;

    // GTK setup
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    // create the master databases
    locale_list = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    country_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    tz_list = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    ap_list = gtk_list_store_new (7, G_TYPE_STRING, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF, G_TYPE_INT, G_TYPE_INT, G_TYPE_POINTER, G_TYPE_STRING);

    // build the UI
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/piwiz.ui");

    msg_dlg = NULL;
    err_dlg = NULL;
    main_dlg = (GtkWidget *) gtk_builder_get_object (builder, "wizard_dlg");
    g_signal_connect (main_dlg, "delete_event", G_CALLBACK (close_prog), NULL);

    wizard_nb = (GtkWidget *) gtk_builder_get_object (builder, "wizard_nb");
    g_signal_connect (wizard_nb, "switch-page", G_CALLBACK (page_changed), NULL);

    next_btn = (GtkWidget *) gtk_builder_get_object (builder, "next_btn");
    g_signal_connect (next_btn, "clicked", G_CALLBACK (next_page), NULL);

    prev_btn = (GtkWidget *) gtk_builder_get_object (builder, "prev_btn");
    g_signal_connect (prev_btn, "clicked", G_CALLBACK (prev_page), NULL);

    skip_btn = (GtkWidget *) gtk_builder_get_object (builder, "skip_btn");
    g_signal_connect (skip_btn, "clicked", G_CALLBACK (skip_page), NULL);

    bt_prompt = (GtkWidget *) gtk_builder_get_object (builder, "p0prompt");
    ip_label = (GtkWidget *) gtk_builder_get_object (builder, "p0ip");
    user_te = (GtkWidget *) gtk_builder_get_object (builder, "p2user");
    pwd1_te = (GtkWidget *) gtk_builder_get_object (builder, "p2pwd1");
    pwd2_te = (GtkWidget *) gtk_builder_get_object (builder, "p2pwd2");
    psk_te = (GtkWidget *) gtk_builder_get_object (builder, "p4psk");
    psk_label = (GtkWidget *) gtk_builder_get_object (builder, "p4info");
    prompt = (GtkWidget *) gtk_builder_get_object (builder, "p6prompt");
    rename_title = (GtkWidget *) gtk_builder_get_object (builder, "p2title");
    rename_info = (GtkWidget *) gtk_builder_get_object (builder, "p2info");
    rename_prompt = (GtkWidget *) gtk_builder_get_object (builder, "p2prompt");
    done_title = (GtkWidget *) gtk_builder_get_object (builder, "p6title");
    done_info = (GtkWidget *) gtk_builder_get_object (builder, "p6info");
    done_prompt = (GtkWidget *) gtk_builder_get_object (builder, "p6prompt");
    chromium_rb = (GtkWidget *) gtk_builder_get_object (builder, "p8rbcr");
    firefox_rb = (GtkWidget *) gtk_builder_get_object (builder, "p8rbff");
    uninstall_chk = (GtkWidget *) gtk_builder_get_object (builder, "p8chuninst");
    rpc_sw = (GtkWidget *) gtk_builder_get_object (builder, "p9rpcsw");

    pwd_hide = (GtkWidget *) gtk_builder_get_object (builder, "p2check");
    g_signal_connect (pwd_hide, "toggled", G_CALLBACK (pwd_toggle), NULL);
    psk_hide = (GtkWidget *) gtk_builder_get_object (builder, "p4check");
    g_signal_connect (psk_hide, "toggled", G_CALLBACK (psk_toggle), NULL);
    eng_chk = (GtkWidget *) gtk_builder_get_object (builder, "p1check1");
    uskey_chk = (GtkWidget *) gtk_builder_get_object (builder, "p1check2");

    gtk_entry_set_visibility (GTK_ENTRY (pwd1_te), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (pwd2_te), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (psk_te), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwd_hide), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (psk_hide), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (eng_chk), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uskey_chk), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chromium_rb), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uninstall_chk), FALSE);
    gtk_switch_set_active (GTK_SWITCH (rpc_sw), FALSE);

    // set up underscan
    uscan1_sw = (GtkWidget *) gtk_builder_get_object (builder, "p7switch1");
    uscan2_sw = (GtkWidget *) gtk_builder_get_object (builder, "p7switch2");
    uscan2_box  = (GtkWidget *) gtk_builder_get_object (builder, "p7hbox2");
    gtk_switch_set_active (GTK_SWITCH (uscan1_sw), !get_status ("raspi-config nonint get_overscan_kms 1"));
    gtk_switch_set_active (GTK_SWITCH (uscan2_sw), !get_status ("raspi-config nonint get_overscan_kms 2"));
    g_signal_connect (uscan1_sw, "state-set", G_CALLBACK (uscan_toggle), NULL);
    g_signal_connect (uscan2_sw, "state-set", G_CALLBACK (uscan_toggle), NULL);
    if (num_screens () != 2) gtk_widget_hide (uscan2_box);

    // set up the locale combo boxes
    read_locales ();
    country_cb = (GtkWidget *) gtk_builder_get_object (builder, "p1comb1");
    language_cb = (GtkWidget *) gtk_builder_get_object (builder, "p1comb2");
    timezone_cb = (GtkWidget *) gtk_builder_get_object (builder, "p1comb3");
    gtk_combo_box_set_model (GTK_COMBO_BOX (country_cb), GTK_TREE_MODEL (fcount));

    // set up cell renderers to associate list columns with combo boxes
    col = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (country_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (country_cb), col, "text", CL_CNAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (language_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (language_cb), col, "text", LL_LNAME);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (timezone_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (timezone_cb), col, "text", TL_CITY);

    // initialise the country combo
    g_signal_connect (country_cb, "changed", G_CALLBACK (country_changed), NULL);
    set_init (GTK_TREE_MODEL (fcount), country_cb, CL_CCODE, kbd ? kb_countries[kbd] : init_country);
    set_init (GTK_TREE_MODEL (slang), language_cb, LL_LCODE, kbd ? kb_langs[kbd] : init_lang);
    set_init_tz (GTK_TREE_MODEL (fcity), timezone_cb, TL_ZONE, kbd ? kb_tzs[kbd] : init_tz);

    // make an educated guess as to whether a US keyboard override was set
    char *ilay = NULL, *ivar = NULL;
    if (init_country && init_lang) lookup_keyboard (init_country, init_lang, &ilay, &ivar);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uskey_chk), (!g_strcmp0 (init_kb, "us") && g_strcmp0 (ilay, "us")));

    gtk_widget_show_all (GTK_WIDGET (country_cb));
    gtk_widget_show_all (GTK_WIDGET (language_cb));
    gtk_widget_show_all (GTK_WIDGET (timezone_cb));

    // set up the wifi AP list
    ap_tv = (GtkWidget *) gtk_builder_get_object (builder, "p3networks");
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ap_tv), 0, "AP", col, "text", 0, NULL);
    gtk_tree_view_column_set_expand (gtk_tree_view_get_column (GTK_TREE_VIEW (ap_tv), 0), TRUE);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ap_tv), FALSE);
    sap = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (ap_list));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sap), AP_SSID, GTK_SORT_ASCENDING);
    fap = gtk_tree_model_filter_new (GTK_TREE_MODEL (sap), NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (fap), (GtkTreeModelFilterVisibleFunc) nm_filter_dup_ap, NULL, NULL);
    gtk_tree_view_set_model (GTK_TREE_VIEW (ap_tv), fap);

    col = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ap_tv), 1, "Security", col, "pixbuf", 1, NULL);
    gtk_tree_view_column_set_sizing (gtk_tree_view_get_column (GTK_TREE_VIEW (ap_tv), 1), GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width (gtk_tree_view_get_column (GTK_TREE_VIEW (ap_tv), 1), 30);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ap_tv), 2, "Signal", col, "pixbuf", 2, NULL);
    gtk_tree_view_column_set_sizing (gtk_tree_view_get_column (GTK_TREE_VIEW (ap_tv), 2), GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width (gtk_tree_view_get_column (GTK_TREE_VIEW (ap_tv), 2), 30);

    // set idle event to check for Bluetooth hardware
    gtk_widget_hide (bt_prompt);
    g_idle_add ((GSourceFunc) check_bluetooth, NULL);

    /* start timed event to detect IP address being available */
    g_timeout_add (1000, (GSourceFunc) show_ip, NULL);

    /* start timed event to prompt for screen reader install if not already installed */
    res = system ("dpkg -l orca 2> /dev/null | grep -q ii");
    if (res) g_timeout_add_seconds (15, srprompt, NULL);

    /* if restarting after language set, skip to password page */
    if (argc >= 3 && !g_strcmp0 (argv[1], "--langset"))
    {
        if (argc >= 3) lc = g_strdup (argv[2]);
        if (argc >= 4) cc = g_strdup (argv[3]);

        gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_PASSWD);

        /* touch the flag file to close the old window on restart */
        fclose (fopen (FLAGFILE, "wb"));
    }
    else if (argc == 3 && !g_strcmp0 (argv[1], "--useronly"))
    {
        chuser = g_strdup (argv[2]);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_PASSWD);
    }
    else gtk_widget_hide (prev_btn);

    g_object_unref (builder);

    gtk_widget_show (main_dlg);
    gtk_window_set_default_size (GTK_WINDOW (main_dlg), 1, 1);
    gtk_main ();

    return 0;
}


