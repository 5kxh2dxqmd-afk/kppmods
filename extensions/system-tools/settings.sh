#!/bin/sh
set -eu
# A string in the bundle can outlive the Settings module that used it.
settings_modules=$(/var/run/eink-sdk/usr/bin/kppmod scan /app/KPPMainApp/js/KPPMainApp.js.hbc)
if printf '%s\n' "$settings_modules" | grep -Fq '"settingsRoot":'; then
    settings_uri='app://com.lab126.KPPMainApp/?view=KPP_SETTINGS'
else
    settings_uri='app://com.lab126.booklet.settings'
fi
exec /usr/bin/lipc-set-prop com.lab126.appmgrd start "$settings_uri"
