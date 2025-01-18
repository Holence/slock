#!/bin/bash
sudo cp ./slock.service /etc/systemd/system/slock.service
sudo systemctl enable slock.service
sudo systemctl daemon-reload
