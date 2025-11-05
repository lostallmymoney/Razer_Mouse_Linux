#!/bin/bash
# Usage: usb_rebind_helper.sh <interface_id>
# Unbinds, sleeps, then rebinds the given USB interface

INTERFACE_ID="$1"
UNBIND_PATH="/sys/bus/usb/drivers/usb/unbind"
BIND_PATH="/sys/bus/usb/drivers/usb/bind"

if [ ! -w "$UNBIND_PATH" ] || [ ! -w "$BIND_PATH" ]; then
  echo "Cannot write to $UNBIND_PATH or $BIND_PATH. Need to set the right permissions." >&2
  exit 3
fi

echo "$INTERFACE_ID" > "$UNBIND_PATH"
sleep 0.1
echo "$INTERFACE_ID" > "$BIND_PATH"
