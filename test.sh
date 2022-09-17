make debug -j12
sudo cp build/uinhibitd /tmp/uinhibitd
sudo chown root /tmp/uinhibitd
sudo chmod 4755 /tmp/uinhibitd
/tmp/uinhibitd $@
