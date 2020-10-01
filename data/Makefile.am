uidir = $(datadir)/piwiz

ui_in_files = 	piwiz.ui \
				raspberry-pi-logo.png \
				corner.png \
				srprompt.wav

ui_DATA = $(ui_in_files)

desktopdir=/etc/xdg/autostart

desktop_in_files= \
	piwiz.desktop.in \
	$(NULL)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(ui_in_files) \
			$(desktop_in_files) \
			$(desktop_DATA) \
			$(NULL)
