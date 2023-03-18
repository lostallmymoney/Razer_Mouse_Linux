#!/bin/sh
NAGAID2=$(xinput | grep Naga | grep pointer | cut -d= -f2 | cut -f1)
if [ "$(echo "$NAGAID2" | wc -w)" -eq "2" ]; then
	if [ "$(xinput get-button-map "$(echo "$NAGAID2" | head -n 1)" | grep -c 10)" -eq 1 ]; then
		xinput set-button-map "$(echo "$NAGAID2" | head -n 1)" 1 2 3 4 5 6 7 11 10 8 9 13 14 15 275 276
	else
		xinput set-button-map "$(echo "$NAGAID2" | tail -n 1)" 1 2 3 4 5 6 7 11 10 8 9 13 14 15 275 276
	fi
else
	xinput set-button-map "$NAGAID2" 1 2 3 4 5 6 7 11 10 8 9 13 14 15 275 276
fi
