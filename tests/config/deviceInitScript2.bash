#!/bin/bash

if [[ -f "produceDevice2InitError" ]]; then
    echo Simulating error in second script: `cat produceDevice2InitError`
    exit 1
fi

if [[ -f "produceDevice2InitSecondLine" ]]; then
    # Create additional output line and wait until the file is removed
    echo Just another output line...
    while [[ -f "produceDevice2InitSecondLine" ]]; do
        sleep 0.1
    done
fi

echo just a second script