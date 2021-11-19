#!/bin/bash

echo starting device1 init

if [[ -f "produceDevice1InitError" ]]; then
    exit 1
fi

while  [[ ! -f "continueDevice1Init" ]]; do
    sleep 1
done

touch device1Init.success

echo device1 init successful
       
