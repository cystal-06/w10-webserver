#!/bin/sh
printf "Content-Type: text/plain\r\n\r\n"
echo "=== SWS CGI Environment Dump (RFC 3875) ==="
env | sort
