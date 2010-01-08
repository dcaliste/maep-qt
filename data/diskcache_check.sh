#!/bin/sh
# From Maep-1.1 and later, the tile cache has been moved from
# /home/user/.osm-gps-map to /home/user/MyDocs/maps. This script
# converts this old location into the new one and installs a symlink
# for old applications

# check if this really is a filesystem looking like a maemo5 one
if [ "`mount | grep MyDocs | grep mmcblk0p1`" == "" ]; then
    echo "Not a Maemo5 filesystem, doing nothing."
    exit 0;
fi

echo "Maemo5 filesystem detected"

SRC=/home/user/.osm-gps-map
DEST=/home/user/MyDocs/.maps

# make sure source isn't already a symlink (already converted)
if [ -h "$SRC" ]; then
    echo "OK: source path already is a symlink, doing nothing"
    exit 0;
fi

# make sure source is actually present (data has already been cached)
if [ ! -e "$SRC" ]; then
    echo "OK: source path does not exist, doing nothing"
    exit 0;
fi

# try to figure out how long this will take
SIZE=`du -s $SRC | cut -f1`
echo "Size is $SIZE"
# copying is about 25MBytes/minute
TIME_REQ=$(($SIZE/25000))

# make this a nice string
if [ $TIME_REQ -lt 1 ]; then
    TIME_REQ_MSG="less than one minute"
elif [ $TIME_REQ -lt 2 ]; then
    TIME_REQ_MSG="one minute"
else
    TIME_REQ_MSG="$TIME_REQ minutes"
fi

# create full message
cat > /tmp/msg << EOF
The filesystem structure used by this application has changed and will now be updated automatically. $SIZE kilobytes of data have to be converted. 

This will take approx. $TIME_REQ_MSG.
EOF

if maemo-confirm-text "Updating filesystem" /tmp/msg; then
  # create destination if it doesn't exist yet
  if [ ! -e "$DEST" ]; then
      echo "Creating $DEST"
      mkdir $DEST
  fi

  # moving files
  echo "Moving files ..."
  mv $SRC/* $DEST

  # create symlink for applications not aware of this change
  # (e.g. old versions of maep and gpxview)
  rm -rf $SRC
  ln -s $DEST $SRC

  echo "done."
else
  echo "User cancelled conversion"
fi

rm /tmp/msg

