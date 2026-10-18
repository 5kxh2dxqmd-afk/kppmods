#!/bin/sh
set -eu
package_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
[ -r /usr/lib/ccat/libEBridge.so ] || { echo 'Stock CCAT bridge unavailable' >&2; exit 1; }
/usr/bin/register "$package_dir/epub.install"
# Older KPP applications provide Home while books use the Java/WebReader path.
# Enable the native reader only when its extension parser is present.
kpp_old='_ZN4krpp7feature15documentContext25DocumentContextParserImpl25getFormatTypeForExtensionERKSs'
kpp_new='_ZNK4krpp7feature15documentContext25DocumentContextParserImpl25getFormatTypeForExtensionERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE'
for executable in /app/bin/KPPMainAppV2 /app/bin/KPPMainApp; do
    [ -r "$executable" ] || continue
    if LC_ALL=C grep -aFq "$kpp_old" "$executable" ||
       LC_ALL=C grep -aFq "$kpp_new" "$executable"; then
        touch /var/local/ENABLE_KPPREADER
        mkdir -p /var/local/kft/field_test_toggles
        touch /var/local/kft/field_test_toggles/LegacyFormatMigration
        break
    fi
done
