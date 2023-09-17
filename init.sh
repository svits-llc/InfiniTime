#!/bin/bash
git submodule update --init

npm install lv_font_conv

python -m venv .venv
source .venv/bin/activate
pythom -m pip install wheel
pythom -m pip install -r tools/mcuboot/requirements.txt