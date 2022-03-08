#!/bin/sh

# create the first boot user
sudo adduser rpi-first-boot-wizard --gecos ',,,,'
sudo adduser rpi-first-boot-wizard adm
sudo adduser rpi-first-boot-wizard sudo
sudo adduser rpi-first-boot-wizard audio
sudo adduser rpi-first-boot-wizard video
sudo adduser rpi-first-boot-wizard users
sudo adduser rpi-first-boot-wizard input
sudo adduser rpi-first-boot-wizard netdev
echo "rpi-first-boot-wizard ALL=(ALL) NOPASSWD: ALL" | sudo tee -a /etc/sudoers.d/010_wiz-nopasswd > /dev/null

# set autologin
sudo systemctl set-default graphical.target
sudo ln -fs /lib/systemd/system/getty@.service /etc/systemd/system/getty.target.wants/getty@tty1.service
echo "[Service]\nExecStart=\nExecStart=-/sbin/agetty --autologin rpi-first-boot-wizard --noclear %I \$TERM" | sudo tee /etc/systemd/system/getty@tty1.service.d/autologin.conf > /dev/null
sudo sed /etc/lightdm/lightdm.conf -i -e "s/^\(#\|\)autologin-user=.*/autologin-user=rpi-first-boot-wizard/"

# create the wizard autostart...
echo "[Desktop Entry]\nType=Application\nName=Raspberry Pi First-Run Wizard\nExec=sudo -AE piwiz --useronly $USER\nStartupNotify=true" | sudo tee /etc/xdg/autostart/piwiz.desktop > /dev/null


