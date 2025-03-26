#!/bin/bash

echo starting device1 init

while  [[ -f "blockDevice1Init" ]]; do
    sleep 1
done

touch device1Init.complete

echo device1 init complete
