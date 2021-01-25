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

#include "dhcpcd.h"
#include "dhcpcd-gtk.h"

#define PAGE_INTRO 0
#define PAGE_LOCALE 1
#define PAGE_PASSWD 2
#define PAGE_OSCAN 3
#define PAGE_WIFIAP 4
#define PAGE_WIFIPSK 5
#define PAGE_UPDATE 6
#define PAGE_DONE 7

#define NEXT_BTN 0
#define SKIP_BTN 1
#define PREV_BTN 2

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

#define FLAGFILE "/tmp/.wlflag"

typedef struct {
    char *msg;
    int prog;
} prog_msg;

static prog_msg pm;

/* Controls */

static GtkWidget *main_dlg, *msg_dlg, *msg_msg, *msg_pb, *msg_btn;
static GtkWidget *err_dlg, *err_msg, *err_btn;
static GtkWidget *wizard_nb, *next_btn, *prev_btn, *skip_btn;
static GtkWidget *country_cb, *language_cb, *timezone_cb;
static GtkWidget *ap_tv, *psk_label, *prompt, *ip_label;
static GtkWidget *pwd1_te, *pwd2_te, *psk_te;
static GtkWidget *pwd_hide, *psk_hide, *eng_chk, *uskey_chk, *uscan_chk;

/* Lists for localisation */

GtkListStore *country_list, *locale_list, *tz_list;
GtkTreeModelSort *slang, *scity;
GtkTreeModelFilter *fcount;

/* List of APs */

GtkListStore *ap_list;

/* Globals */

char *wifi_if, *init_country, *init_lang, *init_kb, *init_var, *init_tz;
char *cc, *lc, *city, *ext, *lay, *var;
char *ssid;
gint conn_timeout = 0, pulse_timer = 0;
gboolean reboot, uscan, is_pi = TRUE;
int last_btn = NEXT_BTN;
int calls;

/* Map from country code to keyboard */

static const char keyboard_map[][13] = {
	"",	    "AL",	"al",	    "",
	"",	    "AZ",	"az",	    "",
	"",	    "BD",	"bd",	    "",     //us
	"",	    "BE",	"be",	    "",
	"",	    "BG",	"bg",	    "",     //us
	"",	    "BR",	"br",	    "",
	"",	    "BT",	"bt",	    "",     //us
	"",	    "BY",	"by",	    "",     //us
	"fr",	"CA",	"ca",	    "",
	"",	    "CA",	"us",	    "",
	"de",	"CH",	"ch",	    "",
	"fr",	"CH",	"ch",	    "fr",
	"",	    "CH",	"ch",	    "",
	"",	    "CZ",	"cz",	    "",
	"",	    "DK",	"dk",	    "",
	"",	    "EE",	"ee",	    "",
	"ast",	"ES",	"es",	    "ast",
	"bo",	"",	    "cn",	    "tib",  //us
	"ca",	"ES",	"es",	    "cat",
	"",	    "ES",	"es",	    "",
	"",	    "ET",	"et",	    "",     //us
	"se",	"FI",	"fi",	    "smi",
	"",	    "FI",	"fi",	    "",
	"",	    "FR",	"fr",	    "latin9",
	"",	    "GB",	"gb",	    "",
	"",	    "GG",	"gb",	    "",
	"",	    "HU",	"hu",	    "",
	"",	    "IE",	"ie",	    "",
	"",	    "IL",	"il",	    "",     //us
	"",	    "IM",	"gb",	    "",
	"",	    "IR",	"ir",	    "",     //us
	"",	    "IS",	"is",	    "",
	"",	    "IT",	"it",	    "",
	"",	    "JE",	"gb",	    "",
	"",	    "JP",	"jp",	    "",
	"",	    "LT",	"lt",	    "",
	"",	    "LV",	"lv",	    "",
	"",	    "KG",	"kg",	    "",     //us
	"",	    "KH",	"kh",	    "",     //us
	"",	    "KR",	"kr",	    "kr104",
	"",	    "KZ",	"kz",	    "",     //us
	"",	    "LK",	"lk",	    "",     //us
	"",	    "MA",	"ma",	    "",     //us
	"",	    "MK",	"mk",	    "",     //us
	"",	    "NL",	"us",	    "",
	"",	    "MM",	"mm",	    "",     //us
	"",	    "MN",	"mn",	    "",     //us
	"",	    "MT",	"mt",	    "",
	"se",	"NO",	"no",	    "smi",
	"",	    "NO",	"no",	    "",
	"",	    "NP",	"np",	    "",     //us
	"",	    "PH",	"ph",	    "",
	"",	    "PL",	"pl",	    "",
	"",	    "PT",	"pt",	    "",
	"",	    "RO",	"ro",	    "",
	"",	    "RU",	"ru",	    "",     //us
	"se",	"SE",	"se",	    "smi",
	"",	    "SK",	"sk",	    "",
	"",	    "SI",	"si",	    "",
	"tg",	"",	    "tj",	    "",     //us
	"",	    "TJ",	"tj",	    "",     //us
	"",	    "TH",	"th",	    "",     //us
	"ku",	"TR",	"tr",	    "ku",
	"",	    "TR",	"tr",	    "",
	"",	    "UA",	"ua",	    "",     //us
	"en",	"US",	"us",	    "",
	"",	    "VN",	"us",	    "",
	"",	    "ZA",	"za",	    "",
	"",	    "AR",	"latam",	"",
	"",	    "BO",	"latam",	"",
	"",	    "CL",	"latam",	"",
	"",	    "CO",	"latam",	"",
	"",	    "CR",	"latam",	"",
	"",	    "DO",	"latam",	"",
	"",	    "EC",	"latam",	"",
	"",	    "GT",	"latam",	"",
	"",	    "HN",	"latam",	"",
	"",	    "MX",	"latam",	"",
	"",	    "NI",	"latam",	"",
	"",	    "PA",	"latam",	"",
	"",	    "PE",	"latam",	"",
	"es",	"PR",	"latam",	"",
	"",	    "PY",	"latam",	"",
	"",	    "SV",	"latam",	"",
	"es",	"US",	"latam",	"",
	"",	    "UY",	"latam",	"",
	"",	    "VE",	"latam",    "",
	"ar",	"",	    "ara",	    "",     //us
	"bn",	"",	    "in",	    "ben",  //us
	"bs",	"",	    "ba",	    "",
	"de",	"LI",	"ch",	    "",
	"de",	"",	    "de",	    "",
	"el",	"",	    "gr",	    "",     //us
	"eo",	"",     "epo",      "",
	"fr",	"",	    "fr",	    "latin9",
	"gu",	"",	    "in",	    "guj",  //us
	"hi",	"",	    "in",	    "",     //us
	"hr",	"",	    "hr",	    "",
	"hy",	"",	    "am",	    "",     //us
	"ka",	"",	    "ge",	    "",     //us
	"kn",	"",	    "in",	    "kan",  //us
	"ku",	"",	    "tr",	    "ku",
	"lo",	"",	    "la",	    "",     //us
	"mr",	"",	    "in",	    "",     //us
	"ml",	"",	    "in",	    "mal",  //us
	"my",	"",	    "mm",	    "",     //us
	"ne",	"",	    "np",	    "",     //us
	"os",	"",	    "ru",	    "os",
	"pa",	"",	    "in",	    "guru", //us
	"si",	"",	    "si",	    "sin_phonetic",     //us
	"sr",	"",	    "rs",	    "latin",
	"sv",	"",	    "se",	    "",
	"ta",	"",	    "in",	    "tam",  //us
	"te",	"",	    "in",	    "tel",  //us
	"tg",	"",	    "tj",	    "",     //us
	"the",	"",	    "np",	    "",     //us
	"tl",	"",	    "ph",	    "",
	"ug",	"",	    "cn",	    "ug",   //us
	"zh",	"",	    "cn",	    "",
	"",	    "",	    "us",	    ""
};

#define MAX_KBS 15
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
    "IL"
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
    "he"
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
    "Europe/Jerusalem"
};

/* In dhcpcd-gtk/main.c */

void init_dhcpcd (void);
extern DHCPCD_CONNECTION *con;

/* Local prototypes */

static char *get_shell_string (char *cmd, gboolean all);
static char *get_string (char *cmd);
static int get_status (char *cmd);
static char *get_quoted_param (char *path, char *fname, char *toseek);
static int vsystem (const char *fmt, ...);
static void error_box (char *msg);
static gboolean cb_error (gpointer data);
static void thread_error (char *msg);
static void message (char *msg, int wait, int dest_page, int prog, gboolean pulse);
static gboolean cb_message (gpointer data);
static void thread_message (char *msg, int prog);
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
static void escape_passwd (const char *in, char **out);
static void scans_add (char *str, int match, int secure, int signal, int connected);
static int find_line (char **lssid, int *secure, int *connected);
void connect_success (void);
static gint connect_failure (gpointer data);
static gboolean select_ssid (char *lssid, const char *psk);
static char *find_psk_for_network (char *ssid);
static void progress (PkProgress *progress, PkProgressType *type, gpointer data);
static void do_updates_done (PkTask *task, GAsyncResult *res, gpointer data);
static gboolean filter_fn (PkPackage *package, gpointer user_data);
static void check_updates_done (PkTask *task, GAsyncResult *res, gpointer data);
static void install_lang_done (PkTask *task, GAsyncResult *res, gpointer data);
static void resolve_lang_done (PkTask *task, GAsyncResult *res, gpointer data);
static void refresh_cache_done (PkTask *task, GAsyncResult *res, gpointer data);
static gpointer refresh_update_cache (gpointer data);
static gboolean clock_synced (void);
static void resync (void);
static gboolean ntp_check (gpointer data);
static void page_changed (GtkNotebook *notebook, GtkWidget *page, int pagenum, gpointer data);
static void next_page (GtkButton* btn, gpointer ptr);
static void prev_page (GtkButton* btn, gpointer ptr);
static void skip_page (GtkButton* btn, gpointer ptr);
static gboolean show_ip (void);
static void set_marketing_serial (void);
static gboolean net_available (void);
static int get_pi_keyboard (void);
static gboolean srprompt (gpointer data);

/* Helpers */

static char *get_shell_string (char *cmd, gboolean all)
{
    char *line = NULL, *res = NULL;
    size_t len = 0;
    FILE *fp = popen (cmd, "r");

    if (fp == NULL) return g_strdup ("");
    if (getline (&line, &len, fp) > 0)
    {
        g_strdelimit (line, "\n\r", 0);
        if (!all)
        {
            res = line;
            while (*res++) if (g_ascii_isspace (*res)) *res = 0;
        }
        res = g_strdup (line);
    }
    pclose (fp);
    g_free (line);
    return res ? res : g_strdup ("");
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
        if (sscanf (buf, "%d", &res) == 1)
        {
            val = res;
        }
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

    res = get_string ("hexdump -n 1 -e '1/1 \"%d\"' /proc/device-tree/chosen/rpi-country-code 2> /dev/null");
    if (res)
    {
        if (sscanf (res, "%x", &val) == 1) ret = val;
        g_free (res);
        if (ret) return ret;
    }

    res = get_string ("lsusb -v -d 04d9:0006 | grep \"RPI Wired Keyboard\" | rev");
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
    int dest_page = PAGE_DONE;

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
        gtk_window_set_default_size (GTK_WINDOW (err_dlg), 400, 200);

        err_msg = (GtkWidget *) gtk_builder_get_object (builder, "err_lbl");
        err_btn = (GtkWidget *) gtk_builder_get_object (builder, "err_btn");

        gtk_label_set_text (GTK_LABEL (err_msg), msg);

        gtk_button_set_label (GTK_BUTTON (err_btn), "_OK");
        g_signal_connect (err_btn, "clicked", G_CALLBACK (ok_clicked), (void *) dest_page);

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

static void message (char *msg, int wait, int dest_page, int prog, gboolean pulse)
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
    if (wait)
    {
        gtk_widget_hide (msg_pb);
        gtk_widget_show (msg_btn);
        g_signal_connect (msg_btn, "clicked", G_CALLBACK (ok_clicked), (void *) dest_page);
        gtk_widget_grab_focus (msg_btn);
    }
    else
    {
        gtk_widget_hide (msg_btn);
        gtk_widget_show (msg_pb);
        if (prog == -1)
        {
            if (pulse)
            {
                gtk_progress_bar_pulse (GTK_PROGRESS_BAR (msg_pb));
                pulse_timer = g_timeout_add (200, pulse_pb, NULL);
            }
        }
        else
        {
            float progress = prog / 100.0;
            gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (msg_pb), progress);
        }
    }

    gtk_widget_show (msg_dlg);
}

static gboolean cb_message (gpointer data)
{
    prog_msg *pm = (prog_msg *) data;
    if (pm->prog == -2)
        message (pm->msg, 1, PAGE_DONE, -1, FALSE);
    else
        message (pm->msg, 0, 0, pm->prog, FALSE);
    return FALSE;
}

static void thread_message (char *msg, int prog)
{
    pm.msg = msg;
    pm.prog = prog;
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
    if (data) gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), (int) data);
    return FALSE;
}

static gboolean loc_done (gpointer data)
{
    char *lang, *language, *lcall, *loc;
    gint x, y;

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

        gtk_window_get_position (GTK_WINDOW (main_dlg), &x, &y);
        loc = g_strdup_printf ("%d:%d", x, y);

#ifdef HOMESCHOOL
        execl ("/usr/bin/piwizhs", "piwizhs", "--langset", loc, NULL);
#else
        execl ("/usr/bin/piwiz", "piwiz", "--langset", loc, NULL);
#endif
        g_free (loc);
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
    FILE *fp;

    // set timezone
    if (g_strcmp0 (init_tz, city))
    {
        fp = fopen ("/etc/timezone", "wb");
        if (fp)
        {
            fprintf (fp, "%s\n", city);
            fclose (fp);
        }
        vsystem ("rm /etc/localtime");
        vsystem ("dpkg-reconfigure --frontend noninteractive tzdata");
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
        fp = fopen ("/etc/default/keyboard", "wb");
        if (fp)
        {
            fprintf (fp, "XKBMODEL=pc105\nXKBLAYOUT=%s\nXKBVARIANT=%s\nXKBOPTIONS=\nBACKSPACE=guess", lay, var);
            fclose (fp);
        }
        vsystem ("setxkbmap -layout %s -variant \"%s\" -option \"\"", lay, var);
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
        vsystem ("sed -i /etc/locale.gen -e 's/^\\([^#].*\\)/# \\1/g'");
        vsystem ("sed -i /etc/locale.gen -e 's/^# \\(%s_%s[\\. ].*UTF-8\\)/\\1/g'", lc, cc);
        vsystem ("locale-gen");
        vsystem ("LC_ALL=%s_%s%s LANG=%s_%s%s LANGUAGE=%s_%s%s update-locale LANG=%s_%s%s LC_ALL=%s_%s%s LANGUAGE=%s_%s%s", lc, cc, ext, lc, cc, ext, lc, cc, ext, lc, cc, ext, lc, cc, ext, lc, cc, ext);
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

static void read_locales (void)
{
    char *cname, *lname, *buffer, *cptr, *cptr1, *cptr2;
    GtkTreeModelSort *scount;
    GtkTreeIter iter;
    FILE *fp;
    size_t len;
    int ext;

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
                if (strstr (buffer, ".UTF-8")) ext = 1;
                else ext = 0;

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

                    gtk_list_store_append (locale_list, &iter);
                    gtk_list_store_set (locale_list, &iter, LL_LCODE, cptr1, LL_CCODE, cptr2, LL_LNAME, lname, LL_CHARS, ext ? ".UTF-8" : "", -1);
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

                    gtk_list_store_append (tz_list, &iter);
                    gtk_list_store_set (tz_list, &iter, TL_ZONE, cptr2, TL_CCODE, cptr1, TL_CITY, cname, -1);
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

static void country_changed (GtkComboBox *cb, gpointer ptr)
{
    GtkTreeModel *model;
    GtkTreeModelFilter *flang, *fcity;
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
    fcity = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (tz_list), NULL));
    gtk_tree_model_filter_set_visible_func (fcity, (GtkTreeModelFilterVisibleFunc) match_country, str, NULL);
    scity = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (fcity)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (scity), TL_CITY, GTK_SORT_ASCENDING);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (timezone_cb), GTK_TREE_MODEL (scity));
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
    init_kb = get_string ("grep XKBLAYOUT /etc/default/keyboard | cut -d = -f 2 | tr -d '\"\n'");
    init_var = get_string ("grep XKBVARIANT /etc/default/keyboard | cut -d = -f 2 | tr -d '\"\n'");
    buffer = get_string ("grep LC_ALL /etc/default/locale | cut -d = -f 2");
    if (!buffer[0]) buffer = get_string ("grep LANGUAGE /etc/default/locale | cut -d = -f 2");
    if (!buffer[0]) buffer = get_string ("grep LANG /etc/default/locale | cut -d = -f 2");
    if (buffer[0])
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
            g_free (val);
            return;
        }
        g_free (val);
        if (!gtk_tree_model_iter_next (model, &iter)) break;
    }
}

/* Password */

static void escape_passwd (const char *in, char **out)
{
    const char *ip;
    char *op;

    ip = in;
    *out = malloc (2 * strlen (in) + 1);    // allocate for worst case...
    op = *out;
    while (*ip)
    {
        if (*ip == '$' || *ip == '"' || *ip == '\\' || *ip == '`')
            *op++ = '\\';
        *op++ = *ip++;
    }
    *op = 0;
}

/* WiFi */

static void scans_add (char *str, int match, int secure, int signal, int connected)
{
    GtkTreeIter iter;
    GdkPixbuf *sec_icon = NULL, *sig_icon = NULL;
    char *icon;
    int dsig;

    gtk_list_store_append (ap_list, &iter);
    if (secure)
        sec_icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default(), "network-wireless-encrypted", 16, 0, NULL);
    if (signal >= 0)
    {
        if (signal > 80) dsig = 100;
        else if (signal > 55) dsig = 75;
        else if (signal > 30) dsig = 50;
        else if (signal > 5) dsig = 25;
        else dsig = 0;

        icon = g_strdup_printf ("network-wireless-connected-%02d", dsig);
        sig_icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default(), icon, 16, 0, NULL);
        g_free (icon);
    }
    gtk_list_store_set (ap_list, &iter, 0, str, 1, sec_icon, 2, sig_icon, 3, secure, 4, connected, -1);

    if (match)
        gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (ap_tv)), &iter);
}

static int find_line (char **lssid, int *secure, int *connected)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *sel;

    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (ap_tv));
    if (sel && gtk_tree_selection_get_selected (sel, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, lssid, 3, secure, 4, connected, -1);
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
        gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_UPDATE);
    }
}

static gint connect_failure (gpointer data)
{
    conn_timeout = 0;
    hide_message ();
    message (_("Failed to connect to network."), 1, 0, -1, FALSE);
    return FALSE;
}

static gboolean select_ssid (char *lssid, const char *psk)
{
    DHCPCD_WI_SCAN scan, *s;
    WI_SCAN *w;

    TAILQ_FOREACH (w, &wi_scans, next)
    {
        for (s = w->scans; s; s = s->next)
        {
            if (!g_strcmp0 (lssid, s->ssid))
            {
                DHCPCD_CONNECTION *dcon = dhcpcd_if_connection (w->interface);
                if (!dcon) return FALSE;

                DHCPCD_WPA *wpa = dhcpcd_wpa_find (dcon, w->interface->ifname);
                if (!wpa) return FALSE;

                /* Take a copy of scan in case it's destroyed by a scan update */
                memcpy (&scan, s, sizeof (scan));
                scan.next = NULL;

                if (!(scan.flags & WSF_PSK))
                    dhcpcd_wpa_configure (wpa, &scan, NULL);
                else if (*psk == '\0')
                    dhcpcd_wpa_select (wpa, &scan);
                else
                    dhcpcd_wpa_configure (wpa, &scan, psk);

                return TRUE;
            }
        }
    }
    return FALSE;
}

void menu_update_scans (WI_SCAN *wi, DHCPCD_WI_SCAN *scans)
{
    DHCPCD_WI_SCAN *s;
    char *lssid = NULL;
    int active, selected, connected;

    // get the selected line in the list of SSIDs - values in active and connected are ignored here
    selected = find_line (&lssid, &active, &connected);

    // erase the current list
    gtk_list_store_clear (ap_list);

    // loop through scan results
    for (s = scans; s; s = s->next)
    {
        // only include SSIDs which have either PSK or no security
        if (s->flags & WSF_SECURE && !(s->flags & WSF_PSK)) continue;

        // if this AP matches the SSID previously selected, select it in the new list
        if (!g_strcmp0 (lssid, s->ssid)) active = 1;
        else active = 0;

        // is this network already active? - potentially confusing; not convinced this is a good idea... !!!!
        connected = dhcpcd_wi_associated (wi->interface, s);
        if (!selected && connected) active = 1;

        // add this SSID to the new list
        scans_add (s->ssid, active, (s->flags & WSF_SECURE) && (s->flags & WSF_PSK), s->strength.value, connected);
    }

    if (lssid) g_free (lssid);
    dhcpcd_wi_scans_free (wi->scans);
    wi->scans = scans;
}

static char *find_psk_for_network (char *ssid)
{
    FILE *fp;
    char *line = NULL, *seek, *res, *ret = NULL;
    size_t len = 0;
    int state = 0;

    seek = g_strdup_printf ("ssid=\"%s\"", ssid);
    fp = fopen ("/etc/wpa_supplicant/wpa_supplicant.conf", "rb");
    if (fp)
    {
        while (getline (&line, &len, fp) > 0)
        {
            // state : 1 in a network block; 2 in network block with matching ssid; 0 otherwise
            if (strstr (line, "network={")) state = 1;
            else if (strstr (line, "}")) state = 0;
            else if (state)
            {
                if (strstr (line, seek)) state = 2;
                else if (state == 2 && (res = strstr (line, "psk=")))
                {
                    strtok (res, "\"");
                    ret = g_strdup (strtok (NULL, "\""));
                    break;
                }
            }
        }
        g_free (line);
        fclose (fp);
    }
    g_free (seek);
    return ret;
}

/* Updates */

static void progress (PkProgress *progress, PkProgressType *type, gpointer data)
{
    int role = pk_progress_get_role (progress);

    if (msg_dlg)
    {
        switch (pk_progress_get_status (progress))
        {
            case PK_STATUS_ENUM_DOWNLOAD :  if ((int) type == PK_PROGRESS_TYPE_PERCENTAGE)
                                            {
                                                if (role == PK_ROLE_ENUM_REFRESH_CACHE)
                                                    thread_message (_("Reading update list - please wait..."), pk_progress_get_percentage (progress));
                                                else if (role == PK_ROLE_ENUM_UPDATE_PACKAGES)
                                                    thread_message (_("Downloading updates - please wait..."), pk_progress_get_percentage (progress));
                                            }
                                            break;

            case PK_STATUS_ENUM_INSTALL :   if ((int) type == PK_PROGRESS_TYPE_PERCENTAGE)
                                            {
                                                if (role == PK_ROLE_ENUM_UPDATE_PACKAGES)
                                                    thread_message (_("Installing updates - please wait..."), pk_progress_get_percentage (progress));
                                                else if (role == PK_ROLE_ENUM_INSTALL_PACKAGES)
                                                    thread_message (_("Installing languages - please wait..."), pk_progress_get_percentage (progress));
                                            }
                                            break;

            default :                       gtk_progress_bar_pulse (GTK_PROGRESS_BAR (msg_pb));
                                            break;
        }
    }
}

static void do_updates_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error getting updates.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    // re-set the serial number in case a Chromium update was installed
    set_marketing_serial ();
    thread_message (_("System is up to date"), -2);
}

static gboolean filter_fn (PkPackage *package, gpointer user_data)
{
    if (is_pi) return TRUE;
    if (strstr (pk_package_get_arch (package), "amd64")) return FALSE;
    return TRUE;
}

static void check_updates_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);
    gchar **ids;

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error comparing versions.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    PkPackageSack *sack = pk_results_get_package_sack (results);
    PkPackageSack *fsack = pk_package_sack_filter (sack, filter_fn, NULL);

    if (pk_package_sack_get_size (fsack) > 0)
    {
        reboot = TRUE;
        thread_message (_("Getting updates - please wait..."), -1);

        ids = pk_package_sack_get_ids (fsack);
        pk_task_update_packages_async (task, ids, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) do_updates_done, NULL);
        g_strfreev (ids);
    }
    else thread_message (_("System is up to date"), -2);

    g_object_unref (sack);
    g_object_unref (fsack);
}

static void install_lang_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error installing languages.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    thread_message (_("Comparing versions - please wait..."), -1);
    pk_client_get_updates_async (PK_CLIENT (task), PK_FILTER_ENUM_NONE, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) check_updates_done, NULL);
}

static void resolve_lang_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);
    PkPackage *item;
    gchar *package_id, *arch;
    gchar **ids;
    int i;

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error finding languages.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    PkPackageSack *sack = pk_results_get_package_sack (results);
    PkPackageSack *fsack = pk_package_sack_filter (sack, filter_fn, NULL);
    GPtrArray *array = pk_package_sack_get_array (fsack);

    // remove armhf packages for which there is an arm64 equivalent...
    for (i = 0; i < array->len; i++)
    {
        item = g_ptr_array_index (array, i);
        g_object_get (item, "package-id", &package_id, NULL);
        if (arch = strstr (package_id, "arm64"))
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
        reboot = TRUE;
        thread_message (_("Installing languages - please wait..."), -1);

        ids = pk_package_sack_get_ids (fsack);
        pk_task_install_packages_async (task, ids, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) install_lang_done, NULL);
        g_strfreev (ids);
    }
    else
    {
        thread_message (_("Comparing versions - please wait..."), -1);
        pk_client_get_updates_async (PK_CLIENT (task), PK_FILTER_ENUM_NONE, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) check_updates_done, NULL);
    }

    g_object_unref (sack);
    g_object_unref (fsack);
}

static void refresh_cache_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);
    gchar **pack_array;
    gchar *lpack, *buf;

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error checking for updates.\n%s"), error->message);
        thread_error (buffer);
        g_error_free (error);
        return;
    }

    buf = g_strdup_printf ("check-language-support -l %s_%s", lc, cc);
    lpack = get_shell_string (buf, TRUE);
    if (strlen (lpack))
    {
        pack_array = g_strsplit (lpack, " ", -1);

        thread_message (_("Finding languages - please wait..."), -1);
        pk_client_resolve_async (PK_CLIENT (task), 0, pack_array, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) resolve_lang_done, NULL);
        g_strfreev (pack_array);
    }
    else
    {
        thread_message (_("Comparing versions - please wait..."), -1);
        pk_client_get_updates_async (PK_CLIENT (task), PK_FILTER_ENUM_NONE, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) check_updates_done, NULL);
    }
    g_free (buf);
    g_free (lpack);
}

static gpointer refresh_update_cache (gpointer data)
{
    PkTask *task = pk_task_new ();
    pk_client_refresh_cache_async (PK_CLIENT (task), TRUE, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) refresh_cache_done, NULL);
    return NULL;
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
        system ("/etc/init.d/ntp stop; ntpd -gq; /etc/init.d/ntp start");
    }
    else
    {
        system ("systemctl -q stop systemd-timesyncd 2> /dev/null; systemctl -q start systemd-timesyncd 2> /dev/null");
    }
}

static gboolean ntp_check (gpointer data)
{
    if (clock_synced ())
    {
        message (_("Checking for updates - please wait..."), 0, 0, -1, FALSE);
        g_thread_new (NULL, refresh_update_cache, NULL);
        return FALSE;
    }
    // trigger a resync
    if (calls == 0) resync ();

    if (calls++ > 120)
    {
        message (_("Could not sync time - unable to check for updates"), 1, PAGE_DONE, -1, FALSE);
        return FALSE;
    }

    return TRUE;
}

/* Page management */

static void page_changed (GtkNotebook *notebook, GtkWidget *page, int pagenum, gpointer data)
{
    gtk_button_set_label (GTK_BUTTON (next_btn), _("_Next"));
    gtk_button_set_label (GTK_BUTTON (prev_btn), _("_Back"));
    gtk_button_set_label (GTK_BUTTON (skip_btn), _("_Skip"));
    gtk_widget_set_visible (skip_btn, FALSE);

    switch (pagenum)
    {
        case PAGE_INTRO :   gtk_button_set_label (GTK_BUTTON (prev_btn), _("_Cancel"));
                            break;

        case PAGE_DONE :    if (reboot || uscan)
                            {
                                gtk_widget_show (prompt);
                                gtk_button_set_label (GTK_BUTTON (skip_btn), _("_Later"));
                                gtk_button_set_label (GTK_BUTTON (next_btn), _("_Restart"));
                                gtk_widget_set_visible (skip_btn, TRUE);
                            }
                            else
                            {
                                gtk_widget_hide (prompt);
                                gtk_button_set_label (GTK_BUTTON (next_btn), _("_Done"));
                            }
                            break;

        case PAGE_WIFIAP :  if (!con)
                            {
                                init_dhcpcd ();
                                gtk_list_store_clear (ap_list);
                                scans_add (_("Searching for networks - please wait..."), 0, 0, -1, 0);
                            }
                            gtk_widget_set_visible (skip_btn, TRUE);
                            break;

        case PAGE_WIFIPSK : gtk_widget_set_visible (skip_btn, TRUE);
                            break;

        case PAGE_UPDATE :  gtk_widget_set_visible (skip_btn, TRUE);
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

static void next_page (GtkButton* btn, gpointer ptr)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    const char *psk;
    char *pw1, *pw2, *wc;
    char *text;
    int secure, connected;

    last_btn = NEXT_BTN;
    switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)))
    {
        case PAGE_LOCALE :  // get the combo entries and look up relevant codes in database
                            model = gtk_combo_box_get_model (GTK_COMBO_BOX (language_cb));
                            gtk_combo_box_get_active_iter (GTK_COMBO_BOX (language_cb), &iter);
                            gtk_tree_model_get (model, &iter, LL_LCODE, &lc, -1);
                            gtk_tree_model_get (model, &iter, LL_CCODE, &cc, -1);
                            gtk_tree_model_get (model, &iter, LL_CHARS, &ext, -1);
                            wc = g_strdup (cc);
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

                            model = gtk_combo_box_get_model (GTK_COMBO_BOX (timezone_cb));
                            gtk_combo_box_get_active_iter (GTK_COMBO_BOX (timezone_cb), &iter);
                            gtk_tree_model_get (model, &iter, TL_ZONE, &city, -1);

                            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uskey_chk)))
                            {
                                lay = g_strdup ("us");
                                var = g_strdup ("");
                            }
                            else lookup_keyboard (cc, lc, &lay, &var);

                            // set wifi country - this is quick, so no need for warning
                            if (wifi_if[0])
                            {
                                vsystem ("raspi-config nonint do_wifi_country %s", wc);
                            }
                            g_free (wc);

                            if (g_strcmp0 (init_tz, city) || g_strcmp0 (init_country, cc)
                                || g_strcmp0 (init_lang, lc) || g_ascii_strcasecmp (init_kb, lay)
                                || g_strcmp0 (init_var, var))
                            {
                                message (_("Setting location - please wait..."), 0, 0, -1, TRUE);
                                g_thread_new (NULL, set_locale, NULL);
                            }
                            else gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_PASSWD);
                            break;

        case PAGE_PASSWD :  escape_passwd (gtk_entry_get_text (GTK_ENTRY (pwd1_te)), &pw1);
                            escape_passwd (gtk_entry_get_text (GTK_ENTRY (pwd2_te)), &pw2);
                            if (strlen (pw1) || strlen (pw2))
                            {
                                if (g_strcmp0 (pw1, pw2))
                                {
                                    message (_("The two passwords entered do not match."), 1, 0, -1, FALSE);
                                    break;
                                }
                                vsystem ("echo \"$SUDO_USER:%s\" | chpasswd", pw1);
                            }
                            g_free (pw1);
                            g_free (pw2);
                            if (is_pi) gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_OSCAN);
                            else
                            {
                                if (!wifi_if[0])
                                    gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_UPDATE);
                                else
                                    gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_WIFIAP);
                            }
                            break;

        case PAGE_OSCAN :   uscan = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uscan_chk));
                            if (!wifi_if[0])
                                gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_UPDATE);
                            else
                                gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_WIFIAP);
                            break;

        case PAGE_WIFIAP :  if (ssid) g_free (ssid);
                            ssid = NULL;
                            if (!find_line (&ssid, &secure, &connected))
                                gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_UPDATE);
                            else
                            {
                                if (connected)
                                {
                                    gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_UPDATE);
                                    break;
                                }
                                if (secure)
                                {
                                    text = g_strdup_printf (_("Enter the password for the WiFi network \"%s\"."), ssid);
                                    gtk_label_set_text (GTK_LABEL (psk_label), text);
                                    g_free (text);

                                    text = find_psk_for_network (ssid);
                                    gtk_entry_set_text (GTK_ENTRY (psk_te), text ? text : "");
                                    g_free (text);

                                    gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_WIFIPSK);
                                }
                                else
                                {
                                    if (select_ssid (ssid, NULL))
                                    {
                                        message (_("Connecting to WiFi network - please wait..."), 0, 0, -1, TRUE);
                                        conn_timeout = g_timeout_add (30000, connect_failure, NULL);
                                    }
                                    else message (_("Could not connect to this network"), 1, 0, -1, FALSE);
                                }
                            }
                            break;

        case PAGE_WIFIPSK : psk = gtk_entry_get_text (GTK_ENTRY (psk_te));
                            if (select_ssid (ssid, psk))
                            {
                                message (_("Connecting to WiFi network - please wait..."), 0, 0, -1, TRUE);
                                conn_timeout = g_timeout_add (30000, connect_failure, NULL);
                            }
                            else message (_("Could not connect to this network"), 1, 0, -1, FALSE);
                            break;

        case PAGE_DONE :    gtk_dialog_response (GTK_DIALOG (main_dlg), GTK_RESPONSE_OK);
                            break;

        case PAGE_UPDATE :  if (net_available ())
                            {
                                if (clock_synced ())
                                {
                                    message (_("Checking for updates - please wait..."), 0, 0, -1, FALSE);
                                    g_thread_new (NULL, refresh_update_cache, NULL);
                                }
                                else
                                {
                                    message (_("Synchronising clock - please wait..."), 0, 0, -1, TRUE);
                                    calls = 0;
                                    g_timeout_add_seconds (1, ntp_check, NULL);
                                }
                            }
                            else message (_("No network connection found - unable to check for updates"), 1, PAGE_DONE, -1, FALSE);
                            break;

        default :           gtk_notebook_next_page (GTK_NOTEBOOK (wizard_nb));
                            break;
    }
}

static void prev_page (GtkButton* btn, gpointer ptr)
{
    last_btn = PREV_BTN;
    switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)))
    {
        case PAGE_UPDATE :  if (wifi_if[0]) gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_WIFIAP);
                            else
                            {
                                if (is_pi)
                                    gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_OSCAN);
                                else
                                    gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_PASSWD);
                            }
                            break;

        case PAGE_INTRO :   gtk_dialog_response (GTK_DIALOG (main_dlg), GTK_RESPONSE_CANCEL);
                            break;

        case PAGE_WIFIAP :  if (is_pi)
                                gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_OSCAN);
                            else
                                gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_PASSWD);
                            break;

        default :           gtk_notebook_prev_page (GTK_NOTEBOOK (wizard_nb));
                            break;
    }
}

static void skip_page (GtkButton* btn, gpointer ptr)
{
    last_btn = SKIP_BTN;
    switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)))
    {
        case PAGE_WIFIAP :  gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_UPDATE);
                            break;

        case PAGE_UPDATE :  gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_DONE);
                            break;

        case PAGE_DONE :    gtk_dialog_response (GTK_DIALOG (main_dlg), GTK_RESPONSE_CLOSE);
                            break;

        default :           gtk_notebook_next_page (GTK_NOTEBOOK (wizard_nb));
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
    if (ip && strlen (ip))
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

static void set_marketing_serial (void)
{
    if (is_pi)
    {
        if (access ("/usr/lib/chromium-browser/master_preferences", F_OK) != -1)
        {
            if (system ("grep -q \"^Revision\\s*:\\s*[ 123][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]11[0-9a-fA-F]$\" /proc/cpuinfo") == 0)
                vsystem ("sed -i /usr/lib/chromium-browser/master_preferences -e s/UNIDENTIFIED/`vcgencmd otp_dump | grep ^6[45] | sha256sum | cut -d ' ' -f 1`/g");
            else
                vsystem ("sed -i /usr/lib/chromium-browser/master_preferences -e s/UNIDENTIFIED/`cat /proc/cpuinfo | grep Serial | sha256sum | cut -d ' ' -f 1`/g");
        }
        else if (access ("/etc/chromium/master_preferences", F_OK) != -1)
        {
            if (system ("grep -q \"^Revision\\s*:\\s*[ 123][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]11[0-9a-fA-F]$\" /proc/cpuinfo") == 0)
                vsystem ("sed -i /etc/chromium/master_preferences -e s/UNIDENTIFIED/`vcgencmd otp_dump | grep ^6[45] | sha256sum | cut -d ' ' -f 1`/g");
            else
                vsystem ("sed -i /etc/chromium/master_preferences -e s/UNIDENTIFIED/`cat /proc/cpuinfo | grep Serial | sha256sum | cut -d ' ' -f 1`/g");
        }
    }
}

static gboolean net_available (void)
{
    char *ip;
    gboolean val = FALSE;

    ip = get_string ("hostname -I | tr ' ' \\\\n | grep \\\\. | tr \\\\n ','");
    if (ip)
    {
        if (strlen (ip)) val = TRUE;
        g_free (ip);
    }
    return val;
}

static gboolean srprompt (gpointer data)
{
    if (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)) == PAGE_INTRO)
    {
        if (net_available () && clock_synced ())
        {
            char *buf = g_strdup_printf ("aplay %s/srprompt.wav", PACKAGE_DATA_DIR);
            system (buf);
            g_free (buf);
        }
        return TRUE;
    }
    return FALSE;
}


/* The dialog... */

int main (int argc, char *argv[])
{
    GtkBuilder *builder;
    GtkWidget *wid;
    GtkCellRenderer *col;
    int res, kbd;

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    if (system ("raspi-config nonint is_pi")) is_pi = FALSE;

    // set the audio output to HDMI if there is one, otherwise the analog jack
    system ("if sudo -u $SUDO_USER XDG_RUNTIME_DIR=/run/user/$SUDO_UID pactl list short sinks | grep -q bcm2835_audio.digital-stereo ; then sudo -u $SUDO_USER XDG_RUNTIME_DIR=/run/user/$SUDO_UID pactl set-default-sink alsa_output.platform-bcm2835_audio.digital-stereo ; else sudo -u $SUDO_USER XDG_RUNTIME_DIR=/run/user/$SUDO_UID pactl set-default-sink alsa_output.platform-bcm2835_audio.analog-stereo ; fi");

    // read country code from Pi keyboard, if any
    kbd = get_pi_keyboard ();
    if (kbd > MAX_KBS - 1) kbd = 0;

#ifdef HOMESCHOOL
    vsystem ("familyshield on");
    reboot = TRUE;
#else
    reboot = FALSE;
#endif
    uscan = FALSE;
    read_inits ();

    set_marketing_serial ();

    // GTK setup
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    // create the master databases
    locale_list = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    country_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    tz_list = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    ap_list = gtk_list_store_new (5, G_TYPE_STRING, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF, G_TYPE_INT, G_TYPE_INT);

    // build the UI
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/piwiz.ui");

    msg_dlg = NULL;
    err_dlg = NULL;
    main_dlg = (GtkWidget *) gtk_builder_get_object (builder, "wizard_dlg");
    wizard_nb = (GtkWidget *) gtk_builder_get_object (builder, "wizard_nb");
    g_signal_connect (wizard_nb, "switch-page", G_CALLBACK (page_changed), NULL);

    next_btn = (GtkWidget *) gtk_builder_get_object (builder, "next_btn");
    g_signal_connect (next_btn, "clicked", G_CALLBACK (next_page), NULL);

    prev_btn = (GtkWidget *) gtk_builder_get_object (builder, "prev_btn");
    g_signal_connect (prev_btn, "clicked", G_CALLBACK (prev_page), NULL);

    skip_btn = (GtkWidget *) gtk_builder_get_object (builder, "skip_btn");
    g_signal_connect (skip_btn, "clicked", G_CALLBACK (skip_page), NULL);

    ip_label = (GtkWidget *) gtk_builder_get_object (builder, "p0ip");
    pwd1_te = (GtkWidget *) gtk_builder_get_object (builder, "p2pwd1");
    pwd2_te = (GtkWidget *) gtk_builder_get_object (builder, "p2pwd2");
    psk_te = (GtkWidget *) gtk_builder_get_object (builder, "p4psk");
    psk_label = (GtkWidget *) gtk_builder_get_object (builder, "p4info");
    prompt = (GtkWidget *) gtk_builder_get_object (builder, "p6prompt");

    pwd_hide = (GtkWidget *) gtk_builder_get_object (builder, "p2check");
    g_signal_connect (pwd_hide, "toggled", G_CALLBACK (pwd_toggle), NULL);
    psk_hide = (GtkWidget *) gtk_builder_get_object (builder, "p4check");
    g_signal_connect (psk_hide, "toggled", G_CALLBACK (psk_toggle), NULL);
    eng_chk = (GtkWidget *) gtk_builder_get_object (builder, "p1check1");
    uskey_chk = (GtkWidget *) gtk_builder_get_object (builder, "p1check2");
    uscan_chk = (GtkWidget *) gtk_builder_get_object (builder, "p7check");

    gtk_entry_set_visibility (GTK_ENTRY (pwd1_te), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (pwd2_te), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (psk_te), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwd_hide), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (psk_hide), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (eng_chk), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uskey_chk), FALSE);

    // set up the locale combo boxes
    read_locales ();
    wid = (GtkWidget *) gtk_builder_get_object (builder, "p1table");
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
    set_init (GTK_TREE_MODEL (scity), timezone_cb, TL_ZONE, kbd ? kb_tzs[kbd] : init_tz);

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
    gtk_tree_view_set_model (GTK_TREE_VIEW (ap_tv), GTK_TREE_MODEL (ap_list));

    col = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ap_tv), 1, "Security", col, "pixbuf", 1, NULL);
    gtk_tree_view_column_set_sizing (gtk_tree_view_get_column (GTK_TREE_VIEW (ap_tv), 1), GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width (gtk_tree_view_get_column (GTK_TREE_VIEW (ap_tv), 1), 30);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ap_tv), 2, "Signal", col, "pixbuf", 2, NULL);
    gtk_tree_view_column_set_sizing (gtk_tree_view_get_column (GTK_TREE_VIEW (ap_tv), 2), GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width (gtk_tree_view_get_column (GTK_TREE_VIEW (ap_tv), 2), 30);

    /* start timed event to detect IP address being available */
    g_timeout_add (1000, (GSourceFunc) show_ip, NULL);

    /* start timed event to prompt for screen reader install if not already installed */
    res = system ("dpkg -l orca 2> /dev/null | grep -q ii");
    if (res) g_timeout_add_seconds (15, srprompt, NULL);

    /* if restarting after language set, skip to password page */
    if (argc == 3 && !g_strcmp0 (argv[1], "--langset"))
    {
        gint x, y;
        sscanf (argv[2], "%d:%d", &x, &y);
        gtk_window_move (GTK_WINDOW (main_dlg), x, y);

        gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_PASSWD);

        /* touch the flag file to close the old window on restart */
        fclose (fopen (FLAGFILE, "wb"));
    }

    res = gtk_dialog_run (GTK_DIALOG (main_dlg));

    g_object_unref (builder);
    gtk_widget_destroy (main_dlg);

    if (res == GTK_RESPONSE_CANCEL || res == GTK_RESPONSE_OK || res == GTK_RESPONSE_CLOSE)
    {
#ifdef HOMESCHOOL
        vsystem ("cp /usr/share/raspi-ui-overrides/applications/chromium-browser.desktop /etc/xdg/autostart/");
        vsystem ("echo \"[Desktop Entry]\nType=Link\nName=Browse the Internet\nIcon=applications-internet\nURL=/usr/share/raspi-ui-overrides/applications/chromium-browser.desktop\" > /home/pi/Desktop/chromium-browser.desktop");
#endif
        vsystem ("rm -f /etc/xdg/autostart/piwiz.desktop");
    }

    if (res == GTK_RESPONSE_OK || res == GTK_RESPONSE_CLOSE)
    {
        if (uscan) vsystem ("raspi-config nonint do_overscan 1");
    }

    if (res == GTK_RESPONSE_OK)
    {
        if (reboot || uscan) vsystem ("sync;reboot");
    }

    return 0;
}


