#!/bin/bash
# This script runs multiple applications in the background.
# It waits for all of them to finish before exiting.
./app-io ../A-5.csv &
./app-io ../B-5.csv &
./app-io ../C-5.csv &
