#!/bin/sh
printf "Content-Type: text/plain\r\n\r\n"
echo "Hello from SWS CGI!"
echo "Server protocol: $SERVER_PROTOCOL"
echo "Request method:  $REQUEST_METHOD"
echo "Remote address:  $REMOTE_ADDR"
echo "Script name:     $SCRIPT_NAME"
echo "Query string:    $QUERY_STRING"
