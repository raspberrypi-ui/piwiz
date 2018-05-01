bin_PROGRAMS = piwiz

piwiz_CFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/piwiz"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/piwiz/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(srcdir)/libdhcpcd \
	-I$(srcdir)/dhcpcd-gtk \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

piwiz_SOURCES = piwiz.c \
		libdhcpcd/config.c \
		libdhcpcd/dhcpcd.c \
		libdhcpcd/wpa.c \
		dhcpcd-gtk/wpa.c \
		dhcpcd-gtk/main.c


piwiz_includedir = $(includedir)/piwiz

piwiz_include_HEADERS =

piwiz_LDFLAGS = $(DYNAMIC_FLAGS)
piwiz_DEPENDENCIES_EXTRA = $(BUILTIN_PLUGINS)

piwiz_LDADD = \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(X11_LIBS) \
		$(INTLLIBS)

EXTRA_DIST =
