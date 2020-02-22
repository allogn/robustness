#!/bin/bash
cd src/SatGreedy/build/
make
# make test
cd ../../../
python3 -m unittest discover -s ./tests -p "*.py"
