#!/bin/bash

readonly QRREADER_PATH=$(whereis qrreader | awk {'print $2'})
readonly DEFAULT_SETTINGS_PATH=$(whereis defaultsettings.sh | awk {'print $2'})
readonly VERSION=$(lsb_release -sr)
readonly MIN_VERSION=${VERSION:0:2}
readonly DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ORIGINAL_USER=$(pstree -lu -s $$ | grep --max-count=1 -o '([^)]*)' | head -n 1 |sed -r "s/[()]+//g")
DISPLAY_NUM=$(sudo -u $ORIGINAL_USER echo $DISPLAY)

function install_qr(){
	if pgrep qrreader; then
		sudo pkill qrreader
	fi
	echo "Radim update"
    sudo apt update -q=2
	sudo apt-get --fix-broken install -y -q=1
    sudo apt install xclip -y -q=2
	sudo pkill qrreader
	sudo -u "$1" XDG_RUNTIME_DIR="/run/user/$(id -u $1)" systemctl --user disable --now qrreader
    sudo cp "$DIR/$2" /usr/local/bin/qrreader
    sudo chmod 755 /usr/local/bin/qrreader
}

function install_qr_service(){
	sudo cp -v $DIR/qrreader.service /etc/systemd/user/qrreader.service
	sudo -u "$1" XDG_RUNTIME_DIR="/run/user/$(id -u $1)" systemctl --user enable --now qrreader
	sudo -u "$1" XDG_RUNTIME_DIR="/run/user/$(id -u $1)" systemctl --user restart --now qrreader
}

function remove_udev_rule(){
    #$1 je parameter funkcije, u ovom slucaju $ORIGINAL_USER
    sudo rm -v /etc/udev/rules.d/82-qr.rules
    sudo rm -v /etc/udev/rules.d/80-qr.rules
	sudo udevadm control --reload-rules && udevadm trigger
}
function stop_mcafee(){
	echo "Zaustavljam Mcafee servise"
	sudo systemctl stop mfetpd.service
	sudo systemctl stop mfeespd.service
	sudo systemctl stop mcafee.ma.service
}

function remove_qr_from_default_settings(){
	if [ -n "$DEFAULT_SETTINGS_PATH" ]; then
		qrreader_line=$(cat $DEFAULT_SETTINGS_PATH | grep qr)
		if [ ! -z "$qrreader_line" ]; then
			echo "Onemogucavam automatsko startovanje iz $DEFAULT_SETTINGS_PATH"
			sudo sed -i '/qrreader/ s/^/#/' "$DEFAULT_SETTINGS_PATH"
		fi
	fi
}

function add_qr_to_default_settings(){
	if [ -n "$DEFAULT_SETTINGS_PATH" ]; then
		qrreader_line=$(cat $DEFAULT_SETTINGS_PATH | grep qr)
		if [ ! -z "$qrreader_line" ]; then
			echo "Automatsko pokretanje vec omoguceno $DEFAULT_SETTINGS_PATH"
			#sudo sed -i '/qrreader/ s/^/#/' "$DEFAULT_SETTINGS_PATH"
		elif [ -z "$qrreader_line" ]; then
			printf "\nsleep 30 && qrreader &" | sudo tee -a $DEFAULT_SETTINGS_PATH
		fi
	fi
}

#sudo sed -i '/:0/ s//:2/' /etc/systemd/user/qrreader.service

case $MIN_VERSION in
"20"|"22"|"24")
	stop_mcafee
	sudo adduser $ORIGINAL_USER dialout
	if [ -n "$QRREADER_PATH" ]; then
		echo "Vec Postoji stari qrreader, uklanjam"
		remove_udev_rule $ORIGINAL_USER
		install_qr $ORIGINAL_USER "qrreader-new-v4-slow"
	else
		echo "Instaliram qrreader"
		install_qr $ORIGINAL_USER "qrreader-new-v4-slow"
	fi
	if [ "$DISPLAY" == ":0" ];then
		echo "Instaliram qr service"
		install_qr_service $ORIGINAL_USER "qrreader-new-v4-slow"
		remove_qr_from_default_settings
	else
		add_qr_to_default_settings
	fi
;;
*)
	echo "Nepodrzana verzija Ubuntua, podrzane su 20.04, 22.04 i 24.04"
	exit 1
	;;
esac
