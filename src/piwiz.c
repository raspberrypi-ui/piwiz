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
#define PAGE_DONE 5

/* Controls */

static GtkWidget *main_dlg, *msg_dlg;
static GtkWidget *wizard_nb, *next_btn, *prev_btn, *skip_btn;
static GtkWidget *country_cb, *language_cb, *timezone_cb;
static GtkWidget *ap_tv, *psk_label;
static GtkWidget *pwd1_te, *pwd2_te, *psk_te;

/* Lists for localisation */

GtkListStore *locale_list, *tz_list;
GtkTreeModelSort *scount, *slang, *scity;
GtkTreeModelFilter *fcount, *flang, *fcity;

/* List of APs */

GtkListStore *ap_list;

/* Globals */

char *wifi_if, *init_country, *init_lang, *init_kb, *init_tz;
char *ssid;
gint conn_timeout = 0;

/* In dhcpcd-gtk/main.c */

void init_dhcpcd (void);
extern DHCPCD_CONNECTION *con;

/* Local prototypes */

static char *get_string (char *cmd);
static char *get_quoted_param (char *path, char *fname, char *toseek);
static int vsystem (const char *fmt, ...);
static void message (char *msg, int wait);
static gboolean ok_clicked (gpointer data);
static gboolean close_msg (gpointer data);
static gpointer set_locale (gpointer data);
static void read_locales (void);
static gboolean unique_rows (GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static void country_changed (GtkComboBox *cb, gpointer ptr);
static gboolean match_country (GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static void read_inits (void);
static void set_init (GtkTreeModel *model, GtkWidget *cb, int pos, char *init);
static void scans_add (char *str, int match, int secure, int signal);
static int find_line (char **ssid, int *sec);
void connect_success (void);
static gint connect_failure (gpointer data);
static gboolean select_ssid (char *lssid, const char *psk);
static void page_changed (GtkNotebook *notebook, GtkNotebookPage *page, int pagenum, gpointer data);
static void next_page (GtkButton* btn, gpointer ptr);
static void prev_page (GtkButton* btn, gpointer ptr);
static void skip_page (GtkButton* btn, gpointer ptr);

/* Helpers */

GtkWidget *gtk_box_new (GtkOrientation o, gint s)
{
	if (o == GTK_ORIENTATION_HORIZONTAL)
		return gtk_hbox_new (false, s);
	else
		return gtk_vbox_new (false, s);
}

static char *get_string (char *cmd)
{
    char *line = NULL, *res = NULL;
    int len = 0;
    FILE *fp = popen (cmd, "r");

    if (fp == NULL) return NULL;
    if (getline (&line, &len, fp) > 0)
    {
        res = line;
        while (*res++) if (g_ascii_isspace (*res)) *res = 0;
        res = g_strdup (line);
    }
    pclose (fp);
    g_free (line);
    return res;
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

static void message (char *msg, int wait)
{
    GdkColor col;
    GtkWidget *wid;
    GtkBuilder *builder = gtk_builder_new ();

    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/piwiz.ui", NULL);

    msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "msg");
    gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "msg_eb");
    gdk_color_parse ("#FFFFFF", &col);
    gtk_widget_modify_bg (wid, GTK_STATE_NORMAL, &col);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "msg_lbl");
    gtk_label_set_text (GTK_LABEL (wid), msg);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "msg_bb");

    gtk_widget_show_all (msg_dlg);
    if (!wait) gtk_widget_set_visible (wid, FALSE);
    else
    {
        wid = (GtkWidget *) gtk_builder_get_object (builder, "msg_btn");
        g_signal_connect (wid, "clicked", G_CALLBACK (ok_clicked), NULL);
    }
    g_object_unref (builder);
}

static gboolean ok_clicked (gpointer data)
{
    gtk_widget_destroy (GTK_WIDGET (msg_dlg));
    return FALSE;
}

static gboolean close_msg (gpointer data)
{
    gtk_widget_destroy (GTK_WIDGET (msg_dlg));
    gtk_notebook_next_page (GTK_NOTEBOOK (wizard_nb));
    return FALSE;
}

/* Internationalisation */

static gpointer set_locale (gpointer data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    char *cc, *lc, *city, *ext, *lcc;
    FILE *fp;
    
    // get the combo entries and look up relevant codes in database
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (language_cb));
    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (language_cb), &iter);
    gtk_tree_model_get (model, &iter, 0, &lc, -1);
    gtk_tree_model_get (model, &iter, 1, &cc, -1);
    gtk_tree_model_get (model, &iter, 4, &ext, -1);
    lcc = g_ascii_strdown (cc, -1);

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (timezone_cb));
    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (timezone_cb), &iter);
    gtk_tree_model_get (model, &iter, 0, &city, -1);

    // set wifi country
    vsystem ("wpa_cli -i %s set country %s >> /dev/null", wifi_if, cc);
    vsystem ("iw reg set %s", cc);
    vsystem ("wpa_cli -i %s save_config >> /dev/null", wifi_if);

    // set timezone
    if (g_strcmp0 (init_tz, city))
    {
        fp = fopen ("/etc/timezone", "wb");
        fprintf (fp, "%s\n", city);
        fclose (fp);
        if (init_tz)
        {
            g_free (init_tz);
            init_tz = g_strdup (city);
        }
    }

    // set keyboard
    if (g_strcmp0 (init_kb, lcc))
    {
        fp = fopen ("/etc/default/keyboard", "wb");
        fprintf (fp, "XKBMODEL=pc105\nXKBLAYOUT=%s\nXKBVARIANT=\nXKBOPTIONS=\nBACKSPACE=guess", lcc);
        fclose (fp);
        vsystem ("setxkbmap -layout %s -variant \"\" -option \"\"", lcc);
        if (init_kb)
        {
            g_free (init_kb);
            init_kb = g_strdup (lcc);
        }
    }

    // set locale
    if (g_strcmp0 (init_country, cc) || g_strcmp0 (init_lang, lc))
    {
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

    g_free (cc);
    g_free (lc);
    g_free (city);
    g_free (ext);
    g_free (lcc);

    g_idle_add (close_msg, NULL);
    return NULL;
}

static void read_locales (void)
{
    char *cname, *lname, *buffer, *cptr, *cptr1, *cptr2;
    GtkTreeIter iter;
    FILE *fp;
    int len, ext;

    // populate the locale database
    buffer = NULL;
    len = 0;
    fp = fopen ("/usr/share/i18n/SUPPORTED", "rb");
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
    fclose (fp);
    g_free (buffer);

    // sort and filter the database to produce the list for the country combo
    scount = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (locale_list)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (scount), 3, GTK_SORT_ASCENDING);
    fcount = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (scount), NULL));
    gtk_tree_model_filter_set_visible_func (fcount, (GtkTreeModelFilterVisibleFunc) unique_rows, NULL, NULL);

    // populate the timezone database
    buffer = NULL;
    len = 0;
    fp = fopen ("/usr/share/zoneinfo/zone.tab", "rb");
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
    fclose (fp);
    g_free (buffer);
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
    init_kb = get_string ("grep XKBLAYOUT /etc/default/keyboard | cut -d = -f 2");
    buffer = get_string ("grep LC_ALL /etc/default/locale | cut -d = -f 2");
    if (!buffer) buffer = get_string ("grep LANGUAGE /etc/default/locale | cut -d = -f 2");
    if (!buffer) buffer = get_string ("grep LANG /etc/default/locale | cut -d = -f 2");
    if (buffer)
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

/* WiFi */

static void scans_add (char *str, int match, int secure, int signal)
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
    gtk_list_store_set (ap_list, &iter, 0, str, 1, sec_icon, 2, sig_icon, 3, secure, -1);

    if (match)
        gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (ap_tv)), &iter);
}

static int find_line (char **lssid, int *sec)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *sel;

    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (ap_tv));
    if (sel && gtk_tree_selection_get_selected (sel, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, lssid, 3, sec, -1);
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
        gtk_widget_destroy (GTK_WIDGET (msg_dlg));
        gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_DONE);
    }
}

static gint connect_failure (gpointer data)
{
    conn_timeout = 0;
    gtk_widget_destroy (GTK_WIDGET (msg_dlg));
    message (_("Failed to connect to network."), 1);
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
    int active;

    // get the selected line in the list of SSIDs
    find_line (&lssid, &active);

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

        // add this SSID to the new list
        scans_add (s->ssid, active, (s->flags & WSF_SECURE) && (s->flags & WSF_PSK), s->strength.value);
    }

    if (lssid) g_free (lssid);
    dhcpcd_wi_scans_free (wi->scans);
    wi->scans = scans;
}

/* Page management */

static void page_changed (GtkNotebook *notebook, GtkNotebookPage *page, int pagenum, gpointer data)
{
    gtk_button_set_label (GTK_BUTTON (next_btn), _("Next"));
    gtk_widget_set_visible (prev_btn, TRUE);
    gtk_widget_set_visible (skip_btn, FALSE);

    switch (pagenum)
    {
        case PAGE_INTRO :   gtk_widget_set_visible (prev_btn, FALSE);
                            break;

        case PAGE_DONE :    gtk_button_set_label (GTK_BUTTON (next_btn), _("Reboot"));
                            break;

        case PAGE_WIFIAP :  if (!con)
                            {
                                init_dhcpcd ();
                                gtk_list_store_clear (ap_list);
                                scans_add (_("Searching for networks - please wait..."), 0, 0, -1);
                            }
                            gtk_widget_set_visible (skip_btn, TRUE);
                            break;

        case PAGE_WIFIPSK : gtk_widget_set_visible (skip_btn, TRUE);
                            break;
    }
}

static void next_page (GtkButton* btn, gpointer ptr)
{
    const char *psk;
    int sec;
    char *text;
    const char *pw1, *pw2;

    switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)))
    {
        case PAGE_LOCALE :  message (_("Setting locale - please wait..."), 0);
                            g_thread_new (NULL, set_locale, NULL);
                            break;

        case PAGE_PASSWD :  pw1 = gtk_entry_get_text (GTK_ENTRY (pwd1_te));
                            pw2 = gtk_entry_get_text (GTK_ENTRY (pwd2_te));
                            if (strlen (pw1) || strlen (pw2))
                            {
                                if (strlen (pw1) != strlen (pw2) || g_strcmp0 (pw1, pw2))
                                {
                                    message (_("The two passwords entered do not match."), 1);
                                    break;
                                }
                                vsystem ("(echo \"%s\" ; echo \"%s\") | passwd $SUDO_USER", pw1, pw2);
                            }
                            if (!wifi_if[0]) gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_DONE);
                            else gtk_notebook_next_page (GTK_NOTEBOOK (wizard_nb));
                            break;

        case PAGE_WIFIAP :  if (ssid) g_free (ssid);
                            ssid = NULL;
                            if (!find_line (&ssid, &sec))
                                gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_DONE);
                            else
                            {
                                if (sec)
                                {
                                    text = g_strdup_printf (_("Enter the password for the WiFi network \"%s\"."), ssid);
                                    gtk_label_set_text (GTK_LABEL (psk_label), text);
                                    g_free (text);
                                    gtk_notebook_next_page (GTK_NOTEBOOK (wizard_nb));
                                }
                                else
                                {
                                    if (select_ssid (ssid, NULL))
                                    {
                                        message (_("Connecting to WiFi network - please wait..."), 0);
                                        conn_timeout = gtk_timeout_add (30000, connect_failure, NULL);
                                    }
                                    else message (_("Could not connect to this network"), 1);
                                }
                            }
                            break;

        case PAGE_WIFIPSK : psk = gtk_entry_get_text (GTK_ENTRY (psk_te));
                            if (select_ssid (ssid, psk))
                            {
                                message (_("Connecting to WiFi network - please wait..."), 0);
                                conn_timeout = gtk_timeout_add (30000, connect_failure, NULL);
                            }
                            else message (_("Could not connect to this network"), 1);
                            break;

        case PAGE_DONE :    gtk_dialog_response (GTK_DIALOG (main_dlg), 10);
                            break;

        default :           gtk_notebook_next_page (GTK_NOTEBOOK (wizard_nb));
                            break;
    }
}

static void prev_page (GtkButton* btn, gpointer ptr)
{
    if (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)) == PAGE_DONE)
        gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_WIFIAP);
    else
        gtk_notebook_prev_page (GTK_NOTEBOOK (wizard_nb));
}

static void skip_page (GtkButton* btn, gpointer ptr)
{
    switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard_nb)))
    {
        case PAGE_LOCALE :
        case PAGE_PASSWD :
        case PAGE_WIFIPSK : gtk_notebook_next_page (GTK_NOTEBOOK (wizard_nb));
                            break;

        case PAGE_WIFIAP :  gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard_nb), PAGE_DONE);
                            break;
    }
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

    read_inits ();

    // GTK setup
    gdk_threads_init ();
    gdk_threads_enter ();
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    // create the master databases
    locale_list = gtk_list_store_new (5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    tz_list = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    ap_list = gtk_list_store_new (4, G_TYPE_STRING, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF, G_TYPE_INT);

    // build the UI
    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/piwiz.ui", NULL);

    main_dlg = (GtkWidget *) gtk_builder_get_object (builder, "wizard_dlg");
    wizard_nb = (GtkWidget *) gtk_builder_get_object (builder, "wizard_nb");
    g_signal_connect (wizard_nb, "switch-page", G_CALLBACK (page_changed), NULL);

    next_btn = (GtkWidget *) gtk_builder_get_object (builder, "next_btn");
    g_signal_connect (next_btn, "clicked", G_CALLBACK (next_page), NULL);

    prev_btn = (GtkWidget *) gtk_builder_get_object (builder, "prev_btn");
    g_signal_connect (prev_btn, "clicked", G_CALLBACK (prev_page), NULL);

    skip_btn = (GtkWidget *) gtk_builder_get_object (builder, "skip_btn");
    g_signal_connect (skip_btn, "clicked", G_CALLBACK (skip_page), NULL);

    pwd1_te = (GtkWidget *) gtk_builder_get_object (builder, "p2pwd1");
    pwd2_te = (GtkWidget *) gtk_builder_get_object (builder, "p2pwd2");
    psk_te = (GtkWidget *) gtk_builder_get_object (builder, "p4psk");
    psk_label = (GtkWidget *) gtk_builder_get_object (builder, "p4info");

    gtk_entry_set_visibility (GTK_ENTRY (pwd1_te), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (pwd2_te), FALSE);

    // set up the locale combo boxes
    read_locales ();
    wid = (GtkWidget *) gtk_builder_get_object (builder, "p1table");
    country_cb = gtk_combo_box_new_with_model (GTK_TREE_MODEL (fcount));
    language_cb = gtk_combo_box_new ();
    timezone_cb = gtk_combo_box_new ();
    gtk_table_attach (GTK_TABLE (wid), GTK_WIDGET (country_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (wid), GTK_WIDGET (language_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (wid), GTK_WIDGET (timezone_cb), 1, 2, 2, 3, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);

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
    set_init (GTK_TREE_MODEL (fcount), country_cb, 1, init_country[0] ? init_country : "GB");
    set_init (GTK_TREE_MODEL (slang), language_cb, 0, init_lang[0] ? init_lang : "en");
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

    res = gtk_dialog_run (GTK_DIALOG (main_dlg));

    g_object_unref (builder);
    gtk_widget_destroy (main_dlg);
    gdk_threads_leave ();

    if (res == 10) system ("reboot");
    return 0;
}


