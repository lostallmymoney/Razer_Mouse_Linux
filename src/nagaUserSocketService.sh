#!/bin/bash
SOCKET_PATH="/run/user/$(id -u)/nagaUserCmd.sock"
rm -f "$SOCKET_PATH"

# Create a UNIX socket and handle incoming commands
while true; do
    # Wait for a connection and read the command
    if exec 3<>"$SOCKET_PATH"; then
        # Read command from socket
        read -r cmd <&3
        if [[ -n "$cmd" ]]; then
            # Run command in a subshell, pipe output back
            bash -c "$cmd" 2>&1 | while IFS= read -r line; do
                echo "$line" >&3
            done
        fi
        exec 3>&-
    fi
done
