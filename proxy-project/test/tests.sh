#!/bin/bash

# Settings
PROXY="127.0.0.1:8888"
FILTER_FILE="config/filters.txt"

echo "--- Starting Proxy Tests ---"

# 1. Test Connection (Forwarding)
echo "Testing Connection..."
curl -v -x $PROXY http://httpbin.org/get > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "PASS: Connection successful"
else
    echo "FAIL: Could not connect"
fi

# 2. Test Blocking
echo "Testing Blocking..."
echo "example.org" >> $FILTER_FILE
sleep 1 # Wait for reload

# We expect a 403 error here
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -x $PROXY http://example.org)

if [ "$HTTP_CODE" == "403" ]; then
    echo "PASS: Site blocked successfully (403)"
else
    echo "FAIL: Site was not blocked (Got $HTTP_CODE)"
fi

# Remove the blocked site from the file
grep -v "example.org" $FILTER_FILE > temp.txt && mv temp.txt $FILTER_FILE

# 3. Test Malformed Request
echo "Testing Malformed Request..."
# Note: -q 1 waits 1 second for response (Linux). Use -w 1 on Mac.
RESPONSE=$(echo -e "GARBAGE_DATA\r\n\r\n" | nc -q 1 127.0.0.1 8888 2>/dev/null)

if [[ "$RESPONSE" == *"400 Bad Request"* ]] || [[ "$RESPONSE" == *"408 Request Timeout"* ]]; then
    echo "PASS: Proxy handled garbage data correctly"
else
    echo "FAIL: Proxy did not return Error (Got: '$RESPONSE')"
fi

# 4. Test Concurrency (10 Users)
echo "Testing Concurrency (10 users)..."
SUCCESS_COUNT=0
PIDS=""

# Function to make a request and exit with status
make_req() {
    # Check if we get a 200 OK response
    CODE=$(curl -s -o /dev/null -w "%{http_code}" -x $PROXY http://httpbin.org/get)
    if [ "$CODE" == "200" ]; then
        exit 0
    else
        exit 1
    fi
}

# Loop to launch 10 background processes
for i in {1..10}; do
    make_req &
    PIDS="$PIDS $!"
done

# Wait for all processes and count successes
for pid in $PIDS; do
    wait $pid
    if [ $? -eq 0 ]; then
        ((SUCCESS_COUNT++))
    fi
done

if [ "$SUCCESS_COUNT" -eq 10 ]; then
    echo "PASS: All 10 concurrent requests succeeded"
else
    echo "FAIL: Only $SUCCESS_COUNT/10 requests succeeded"
fi

echo "--- Tests Complete ---"