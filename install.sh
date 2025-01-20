#!/bin/bash

sudo make && sudo make install

sudo cp ./dotfile/slock.service /etc/systemd/system/slock.service
sudo systemctl enable slock.service
sudo systemctl daemon-reload
