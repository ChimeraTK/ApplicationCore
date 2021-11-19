#!/bin/bash

if [[ -f "produceDevice2InitError" ]]; then
    echo Simulating error in second script
    exit 1
fi

echo just a second script
       
