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
#define PAGE_WIFIAP 3
#define PAGE_WIFIPSK 4
#define PAGE_UPDATE 5
#define PAGE_DONE 6

#define NEXT_BTN 0
#define SKIP_BTN 1
#define PREV_BTN 2

#define LABEL_WIDTH(name, w) {GtkWidget *l = (GtkWidget *) gtk_builder_get_object (builder, name); gtk_widget_set_size_request (l, w, -1);}

/* Controls */

static GtkWidget *main_dlg, *msg_dlg, *msg_msg, *msg_pb, *msg_btn;
static GtkWidget *wizard_nb, *next_btn, *prev_btn, *skip_btn;
static GtkWidget *country_cb, *language_cb, *timezone_cb;
static GtkWidget *ap_tv, *psk_label, *prompt, *ip_label;
static GtkWidget *pwd1_te, *pwd2_te, *psk_te;
static GtkWidget *pwd_hide, *psk_hide, *us_key;

/* Lists for localisation */

GtkListStore *locale_list, *tz_list;
GtkTreeModelSort *slang, *scity;
GtkTreeModelFilter *fcount;

/* List of APs */

GtkListStore *ap_list;

/* Globals */

char *wifi_if, *init_country, *init_lang, *init_kb, *init_var, *init_tz;
char *cc, *lc, *city, *ext, *lay, *var;
char *ssid;
gint conn_timeout = 0, pulse_timer = 0;
gboolean reboot;
int last_btn = NEXT_BTN;

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

/* In dhcpcd-gtk/main.c */

void init_dhcpcd (void);
extern DHCPCD_CONNECTION *con;

/* Local prototypes */

static char *get_shell_string (char *cmd, gboolean all);
static char *get_string (char *cmd);
static char *get_quoted_param (char *path, char *fname, char *toseek);
static int vsystem (const char *fmt, ...);
static void message (char *msg, int wait, int dest_page, int prog, gboolean pulse);
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
static void set_init (GtkTreeModel *model, GtkWidget *cb, int pos, char *init);
static void escape_passwd (const char *in, char **out);
static void scans_add (char *str, int match, int secure, int signal, int connected);
static int find_line (char **lssid, int *secure, int *connected);
void connect_success (void);
static gint connect_failure (gpointer data);
static gboolean select_ssid (char *lssid, const char *psk);
static char *find_psk_for_network (char *ssid);
static void progress (PkProgress *progress, PkProgressType *type, gpointer data);
static void do_updates_done (PkTask *task, GAsyncResult *res, gpointer data);
static void check_updates_done (PkTask *task, GAsyncResult *res, gpointer data);
static void install_lang_done (PkTask *task, GAsyncResult *res, gpointer data);
static void resolve_lang_done (PkTask *task, GAsyncResult *res, gpointer data);
static void refresh_cache_done (PkTask *task, GAsyncResult *res, gpointer data);
static gpointer refresh_update_cache (gpointer data);
static void page_changed (GtkNotebook *notebook, GtkNotebookPage *page, int pagenum, gpointer data);
static void next_page (GtkButton* btn, gpointer ptr);
static void prev_page (GtkButton* btn, gpointer ptr);
static void skip_page (GtkButton* btn, gpointer ptr);
static gboolean show_ip (void);

/* Helpers */

GtkWidget *gtk_box_new (GtkOrientation o, gint s)
{
	if (o == GTK_ORIENTATION_HORIZONTAL)
		return gtk_hbox_new (false, s);
	else
		return gtk_vbox_new (false, s);
}

static char *get_shell_string (char *cmd, gboolean all)
{
    char *line = NULL, *res = NULL;
    int len = 0;
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

static char *get_quoted_param (char *path, char *fname, char *toseek)
{
    char *pathname, *linebuf, *cptr, *dptr, *res;
    int len;

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

static void message (char *msg, int wait, int dest_page, int prog, gboolean pulse)
{
    if (!msg_dlg)
    {
        GtkBuilder *builder;
        GtkWidget *wid;
        GdkColor col;

        builder = gtk_builder_new ();
        gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/piwiz.ui", NULL);

        msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "msg");
        gtk_window_set_modal (GTK_WINDOW (msg_dlg), TRUE);
        gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));
        gtk_window_set_position (GTK_WINDOW (msg_dlg), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_destroy_with_parent (GTK_WINDOW (msg_dlg), TRUE);

        wid = (GtkWidget *) gtk_builder_get_object (builder, "msg_eb");
        gdk_color_parse ("#FFFFFF", &col);
        gtk_widget_modify_bg (wid, GTK_STATE_NORMAL, &col);

        msg_msg = (GtkWidget *) gtk_builder_get_object (builder, "msg_lbl");
        msg_pb = (GtkWidget *) gtk_builder_get_object (builder, "msg_pb");
        msg_btn = (GtkWidget *) gtk_builder_get_object (builder, "msg_btn");

        gtk_label_set_text (GTK_LABEL (msg_msg), msg);

        gtk_widget_set_sensitive (prev_btn, FALSE);
        gtk_widget_set_sensitive (next_btn, FALSE);
        gtk_widget_set_sensitive (skip_btn, FALSE);

        gtk_widget_show_all (msg_dlg);
        g_object_unref (builder);
    }
    else gtk_label_set_text (GTK_LABEL (msg_msg), msg);

    if (pulse_timer) g_source_remove (pulse_timer);
    pulse_timer = 0;
    if (wait)
    {
        g_signal_connect (msg_btn, "clicked", G_CALLBACK (ok_clicked), (void *) dest_page);
        gtk_widget_set_visible (msg_pb, FALSE);
        gtk_widget_set_visible (msg_btn, TRUE);
        gtk_widget_grab_focus (msg_btn);
    }
    else
    {
        gtk_widget_set_visible (msg_btn, FALSE);
        gtk_widget_set_visible (msg_pb, TRUE);
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
}

static void hide_message (void)
{
    gtk_widget_destroy (GTK_WIDGET (msg_dlg));
    msg_dlg = NULL;
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
    hide_message ();
    gtk_notebook_next_page (GTK_NOTEBOOK (wizard_nb));
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
    g_free (ext);

    g_idle_add (loc_done, NULL);
    return NULL;
}

static void read_locales (void)
{
    char *cname, *lname, *buffer, *cptr, *cptr1, *cptr2;
    GtkTreeModelSort *scount;
    GtkTreeIter iter;
    FILE *fp;
    int len, ext;

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
                    gtk_list_store_set (locale_list, &iter, 0, cptr1, 1, cptr2, 2, lname, 3, cname, 4, ext ? ".UTF-8" : "", -1);
                    g_free (cname);
                    g_free (lname);
                }
            }
        }
        g_free (buffer);
        fclose (fp);
    }

    // sort and filter the database to produce the list for the country combo
    scount = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (locale_list)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (scount), 3, GTK_SORT_ASCENDING);
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
                    gtk_list_store_set (tz_list, &iter, 0, cptr2, 1, cptr1, 2, cname, -1);
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
    gtk_tree_model_get (model, iter, 1, &str1, -1);
    gtk_tree_model_get (model, &next, 1, &str2, -1);
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
    gtk_tree_model_get (model, &iter, 1, &str, -1);

    // filter and sort the master database for entries matching this code
    flang = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (locale_list), NULL));
    gtk_tree_model_filter_set_visible_func (flang, (GtkTreeModelFilterVisibleFunc) match_country, str, NULL);
    slang = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (flang)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (slang), 2, GTK_SORT_ASCENDING);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (language_cb), GTK_TREE_MODEL (slang));
    gtk_combo_box_set_active (GTK_COMBO_BOX (language_cb), 0);

    // set the timezones for the country
    fcity = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (tz_list), NULL));
    gtk_tree_model_filter_set_visible_func (fcity, (GtkTreeModelFilterVisibleFunc) match_country, str, NULL);
    scity = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (fcity)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (scity), 2, GTK_SORT_ASCENDING);

    // set up the combo box from the sorted and filtered list
    gtk_combo_box_set_model (GTK_COMBO_BOX (timezone_cb), GTK_TREE_MODEL (scity));
    gtk_combo_box_set_active (GTK_COMBO_BOX (timezone_cb), 0);

    g_free (str);
}

static gboolean match_country (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    char *str;
    gboolean res;

    gtk_tree_model_get (model, iter, 1, &str, -1);
    if (!g_strcmp0 (str, (char *) data)) res = TRUE;
    else res = FALSE;
    g_free (str);
    return res;
}

static void read_inits (void)
{
    char *buffer, *lc, *cc;

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
        lc = strtok (buffer, "_");
        cc = strtok (NULL, ". ");
        if (lc && cc)
        {
            init_country = g_strdup (cc);
            init_lang = g_strdup (lc);
        }
        g_free (buffer);
    }
}

static void set_init (GtkTreeModel *model, GtkWidget *cb, int pos, char *init)
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
        gtk_timeout_remove (conn_timeout);
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
    int len = 0, state = 0;

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
                                                    message (_("Reading update list - please wait..."), 0, 0, pk_progress_get_percentage (progress), FALSE);
                                                else if (role == PK_ROLE_ENUM_UPDATE_PACKAGES)
                                                    message (_("Downloading updates - please wait..."), 0, 0, pk_progress_get_percentage (progress), FALSE);
                                            }
                                            break;

            case PK_STATUS_ENUM_INSTALL :   if ((int) type == PK_PROGRESS_TYPE_PERCENTAGE)
                                            {
                                                if (role == PK_ROLE_ENUM_UPDATE_PACKAGES)
                                                    message (_("Installing updates - please wait..."), 0, 0, pk_progress_get_percentage (progress), FALSE);
                                                else if (role == PK_ROLE_ENUM_INSTALL_PACKAGES)
                                                    message (_("Installing languages - please wait..."), 0, 0, pk_progress_get_percentage (progress), FALSE);
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
        message (buffer, 1, PAGE_DONE, -1, FALSE);
        g_free (buffer);
        g_error_free (error);
        return;
    }

    message (_("System is up to date"), 1, PAGE_DONE, -1, FALSE);
}

static void check_updates_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error comparing versions.\n%s"), error->message);
        message (buffer, 1, PAGE_DONE, -1, FALSE);
        g_free (buffer);
        g_error_free (error);
        return;
    }

    PkPackageSack *sack = pk_results_get_package_sack (results);
    GPtrArray *array = pk_package_sack_get_array (sack);

    if (array->len > 0)
    {
        reboot = TRUE;
        message (_("Getting updates - please wait..."), 0, 0, -1, FALSE);
        pk_task_update_packages_async (task, pk_package_sack_get_ids (sack), NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) do_updates_done, NULL);
    }
    else message (_("System is up to date"), 1, PAGE_DONE, -1, FALSE);
}

static void install_lang_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error installing languages.\n%s"), error->message);
        message (buffer, 1, PAGE_DONE, -1, FALSE);
        g_free (buffer);
        g_error_free (error);
        return;
    }

    message (_("Comparing versions - please wait..."), 0, 0, -1, FALSE);
    pk_client_get_updates_async (PK_CLIENT (task), PK_FILTER_ENUM_NONE, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) check_updates_done, NULL);
}

static void resolve_lang_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        char *buffer = g_strdup_printf (_("Error finding languages.\n%s"), error->message);
        message (buffer, 1, PAGE_DONE, -1, FALSE);
        g_free (buffer);
        g_error_free (error);
        return;
    }

    PkPackageSack *sack = pk_results_get_package_sack (results);
    GPtrArray *array = pk_package_sack_get_array (sack);

    if (array->len > 0)
    {
        reboot = TRUE;
        message (_("Installing languages - please wait..."), 0, 0, -1, FALSE);
        pk_task_install_packages_async (task, pk_package_sack_get_ids (sack), NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) install_lang_done, NULL);
    }
    else
    {
        message (_("Comparing versions - please wait..."), 0, 0, -1, FALSE);
        pk_client_get_updates_async (PK_CLIENT (task), PK_FILTER_ENUM_NONE, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) check_updates_done, NULL);
    }
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
        message (buffer, 1, PAGE_DONE, -1, FALSE);
        g_free (buffer);
        g_error_free (error);
        return;
    }

    buf = g_strdup_printf ("check-language-support -l %s_%s", lc, cc);
    lpack = get_shell_string (buf, TRUE);
    if (strlen (lpack))
    {
        pack_array = g_strsplit (lpack, " ", -1);

        message (_("Finding languages - please wait..."), 0, 0, -1, FALSE);
        pk_client_resolve_async (PK_CLIENT (task), 0, pack_array, NULL, (PkProgressCallback) progress, NULL, (GAsyncReadyCallback) resolve_lang_done, NULL);
        g_strfreev (pack_array);
    }
    else
    {
        message (_("Comparing versions - please wait..."), 0, 0, -1, FALSE);
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

/* Page management */

static void page_changed (GtkNotebook *notebook, GtkNotebookPage *page, int pagenum, gpointer data)
{
    gtk_button_set_label (GTK_BUTTON (next_btn), _("_Next"));
    gtk_button_set_label (GTK_BUTTON (prev_btn), _("_Back"));
    gtk_widget_set_visible (skip_btn, FALSE);

    switch (pagenum)
    {
        case PAGE_INTRO :   gtk_button_set_label (GTK_BUTTON (prev_btn), _("_Cancel"));
                            break;

        case PAGE_DONE :    if (reboot)
                            {
                                gtk_widget_show (prompt);
                                gtk_button_set_label (GTK_BUTTON (next_btn), _("_Reboot"));
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
    if (last_btn == PREV_BTN) gtk_widget_grab_focus (prev_btn);
    else if (last_btn == SKIP_BTN && gtk_widget_get_visible (skip_btn)) gtk_widget_grab_focus (skip_btn);
    else gtk_widget_grab_focus (next_btn);
}

static void next_page (GtkButton* btn, gpointer ptr)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    const char *psk;
    char *pw1, *pw2;
    char *text;
    int secure, connected;

    last_btn = NEXT_BTN;
    switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)))
    {
        case PAGE_LOCALE :  // get the combo entries and look up relevant codes in database
                            model = gtk_combo_box_get_model (GTK_COMBO_BOX (language_cb));
                            gtk_combo_box_get_active_iter (GTK_COMBO_BOX (language_cb), &iter);
                            gtk_tree_model_get (model, &iter, 0, &lc, -1);
                            gtk_tree_model_get (model, &iter, 1, &cc, -1);
                            gtk_tree_model_get (model, &iter, 4, &ext, -1);

                            model = gtk_combo_box_get_model (GTK_COMBO_BOX (timezone_cb));
                            gtk_combo_box_get_active_iter (GTK_COMBO_BOX (timezone_cb), &iter);
                            gtk_tree_model_get (model, &iter, 0, &city, -1);

                            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (us_key)))
                            {
                                lay = g_strdup ("us");
                                var = g_strdup ("");
                            }
                            else lookup_keyboard (cc, lc, &lay, &var);

                            // set wifi country - this is quick, so no need for warning
                            if (wifi_if[0])
                            {
                                vsystem ("wpa_cli -i %s set country %s >> /dev/null", wifi_if, cc);
                                vsystem ("iw reg set %s", cc);
                                vsystem ("wpa_cli -i %s save_config >> /dev/null", wifi_if);
                            }

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
                                vsystem ("(echo \"%s\" ; echo \"%s\") | passwd $SUDO_USER", pw1, pw2);
                            }
                            g_free (pw1);
                            g_free (pw2);
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
                                        conn_timeout = gtk_timeout_add (30000, connect_failure, NULL);
                                    }
                                    else message (_("Could not connect to this network"), 1, 0, -1, FALSE);
                                }
                            }
                            break;

        case PAGE_WIFIPSK : psk = gtk_entry_get_text (GTK_ENTRY (psk_te));
                            if (select_ssid (ssid, psk))
                            {
                                message (_("Connecting to WiFi network - please wait..."), 0, 0, -1, TRUE);
                                conn_timeout = gtk_timeout_add (30000, connect_failure, NULL);
                            }
                            else message (_("Could not connect to this network"), 1, 0, -1, FALSE);
                            break;

        case PAGE_DONE :    gtk_dialog_response (GTK_DIALOG (main_dlg), GTK_RESPONSE_OK);
                            break;

        case PAGE_UPDATE :  message (_("Checking for updates - please wait..."), 0, 0, -1, FALSE);
                            g_thread_new (NULL, refresh_update_cache, NULL);
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
        case PAGE_UPDATE :  gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_WIFIAP);
                            break;

        case PAGE_INTRO :   gtk_dialog_response (GTK_DIALOG (main_dlg), GTK_RESPONSE_CANCEL);
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
    ip = get_string ("ifconfig eth0 | grep inet[^6] | tr -s ' ' | cut -d ' ' -f 3");
    if (ip && strlen (ip))
    {
        buf = g_strdup_printf (_("<span font_desc=\"10.0\">IP : %s</span>"), ip);
        gtk_label_set_markup (GTK_LABEL (ip_label), buf);
        g_free (buf);
        g_free (ip);
        return FALSE;
    }
    g_free (ip);
    return TRUE;
}

/* The dialog... */

int main (int argc, char *argv[])
{
    GtkBuilder *builder;
    GtkWidget *wid;
    GtkCellRenderer *col;
    int res;

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    reboot = FALSE;
    read_inits ();

    // GTK setup
    gdk_threads_init ();
    gdk_threads_enter ();
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    // create the master databases
    locale_list = gtk_list_store_new (5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    tz_list = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    ap_list = gtk_list_store_new (5, G_TYPE_STRING, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF, G_TYPE_INT, G_TYPE_INT);

    // build the UI
    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/piwiz.ui", NULL);

    msg_dlg = NULL;
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
    us_key = (GtkWidget *) gtk_builder_get_object (builder, "p1check");

    gtk_entry_set_visibility (GTK_ENTRY (pwd1_te), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (pwd2_te), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (psk_te), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwd_hide), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (psk_hide), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (us_key), FALSE);

    // set up the locale combo boxes
    read_locales ();
    wid = (GtkWidget *) gtk_builder_get_object (builder, "p1table");
    country_cb = gtk_combo_box_new_with_model (GTK_TREE_MODEL (fcount));
    language_cb = gtk_combo_box_new ();
    timezone_cb = gtk_combo_box_new ();
    gtk_table_attach (GTK_TABLE (wid), GTK_WIDGET (country_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (wid), GTK_WIDGET (language_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (wid), GTK_WIDGET (timezone_cb), 1, 2, 2, 3, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_set_tooltip_text (GTK_WIDGET (country_cb), _("Set the country in which you are using your Pi"));
    gtk_widget_set_tooltip_text (GTK_WIDGET (language_cb), _("Set the language in which applications should appear"));
    gtk_widget_set_tooltip_text (GTK_WIDGET (timezone_cb), _("Set the closest city to your location"));

    // set up cell renderers to associate list columns with combo boxes
    col = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (country_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (country_cb), col, "text", 3);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (language_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (language_cb), col, "text", 2);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (timezone_cb), col, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (timezone_cb), col, "text", 2);

    // initialise the country combo
    g_signal_connect (country_cb, "changed", G_CALLBACK (country_changed), NULL);
    set_init (GTK_TREE_MODEL (fcount), country_cb, 1, init_country ? init_country : "GB");
    set_init (GTK_TREE_MODEL (slang), language_cb, 0, init_lang ? init_lang : "en");
    set_init (GTK_TREE_MODEL (scity), timezone_cb, 0, init_tz[0] ? init_tz : "Europe/London");

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

    // GTK word wrapping is great; no, really; I'm not being sarcastic. Honest. Why don't you believe me?
    res = 480;
    LABEL_WIDTH ("p0label", res);
    LABEL_WIDTH ("p1info", res);
    LABEL_WIDTH ("p2info", res);
    LABEL_WIDTH ("p3info", res);
    LABEL_WIDTH ("p4info", res);
    LABEL_WIDTH ("p5info", res);
    LABEL_WIDTH ("p6info", res);
    LABEL_WIDTH ("p1prompt", res);
    LABEL_WIDTH ("p2prompt", res);
    LABEL_WIDTH ("p3prompt", res);
    LABEL_WIDTH ("p4prompt", res);
    LABEL_WIDTH ("p6prompt", res);

    /* start timed event to detect IP address being available */
    g_timeout_add (1000, (GSourceFunc) show_ip, NULL);

    res = gtk_dialog_run (GTK_DIALOG (main_dlg));

    g_object_unref (builder);
    gtk_widget_destroy (main_dlg);
    gdk_threads_leave ();
    if (res == GTK_RESPONSE_CANCEL || res == GTK_RESPONSE_OK)
    {
        vsystem ("rm /etc/xdg/autostart/piwiz.desktop");
    }

    if (res == GTK_RESPONSE_OK && reboot) vsystem ("reboot");
    return 0;
}


