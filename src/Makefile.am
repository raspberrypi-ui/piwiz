bin_PROGRAMS = piwiz \
			   piwizhs

dist_bin_SCRIPTS = hdmi-audio-select

piwiz_CFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/piwiz"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/piwiz/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS) \
	-Wall

piwiz_SOURCES = piwiz.c

if USE_DHCPCD
piwiz_SOURCES += \
		libdhcpcd/config.c \
		libdhcpcd/dhcpcd.c \
		libdhcpcd/wpa.c \
		dhcpcd-gtk/wpa.c \
		dhcpcd-gtk/main.c

piwiz_CFLAGS += \
	-DUSE_DHCPCD \
	-I$(srcdir)/libdhcpcd \
	-I$(srcdir)/dhcpcd-gtk
endif

piwiz_includedir = $(includedir)/piwiz

piwiz_include_HEADERS =

piwiz_LDFLAGS = $(DYNAMIC_FLAGS) -lcrypt
piwiz_DEPENDENCIES_EXTRA = $(BUILTIN_PLUGINS)

piwiz_LDADD = \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(X11_LIBS) \
		$(INTLLIBS)

piwizhs_CFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/piwiz"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/piwiz/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DHOMESCHOOL \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS) \
	-Wall

piwizhs_SOURCES = piwiz.c

if USE_DHCPCD
piwizhs_SOURCES += \
		libdhcpcd/config.c \
		libdhcpcd/dhcpcd.c \
		libdhcpcd/wpa.c \
		dhcpcd-gtk/wpa.c \
		dhcpcd-gtk/main.c

piwizhs_CFLAGS += \
	-DUSE_DHCPCD \
	-I$(srcdir)/libdhcpcd \
	-I$(srcdir)/dhcpcd-gtk
endif

piwizhs_includedir = $(includedir)/piwiz

piwizhs_include_HEADERS =

piwizhs_LDFLAGS = $(DYNAMIC_FLAGS) -lcrypt
piwizhs_DEPENDENCIES_EXTRA = $(BUILTIN_PLUGINS)

piwizhs_LDADD = \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(X11_LIBS) \
		$(INTLLIBS)
		
EXTRA_DIST =
