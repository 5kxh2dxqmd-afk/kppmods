#!/bin/sh
set -eu
# Remove only this package's associations. Book records and covers remain intact.
sqlite3 -cmd '.timeout 5000' /var/local/appreg.db '
BEGIN IMMEDIATE;
DELETE FROM associations WHERE handlerId="org.kindle.epub.extractor";
DELETE FROM properties WHERE handlerId="org.kindle.epub.extractor";
DELETE FROM handlerIds WHERE handlerId="org.kindle.epub.extractor";
DELETE FROM associations WHERE handlerId="com.lab126.booklet.reader" AND interface="application" AND contentId="MT:application/epub+zip";
COMMIT;'
