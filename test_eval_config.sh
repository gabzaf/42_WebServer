#!/bin/bash

BASE_URL="http://localhost"
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

function check_status() {
    url=$1
    expected=$2
    extra_args=$3
    echo -n "Testing $url... "
    status=$(curl -s -o /dev/null -w "%{http_code}" $extra_args "$url")
    if [ "$status" == "$expected" ]; then
        echo -e "${GREEN}PASS${NC} ($status)"
    else
        echo -e "${RED}FAIL${NC} (Expected $expected, got $status)"
    fi
}

function check_content() {
    url=$1
    expected_content=$2
    extra_args=$3
    echo -n "Testing content of $url... "
    content=$(curl -s $extra_args "$url")
    if [[ "$content" == *"$expected_content"* ]]; then
        echo -e "${GREEN}PASS${NC}"
    else
        echo -e "${RED}FAIL${NC} (Content mismatch)"
        echo "Got: $content"
    fi
}

echo "=== Server 1 (Port 1234) ==="
check_status "$BASE_URL:1234/" 200
check_content "$BASE_URL:1234/" "Server 1"

echo -e "\n=== Server 2 (Port 1235) ==="
check_status "$BASE_URL:1235/" 200
check_content "$BASE_URL:1235/" "Server 2"

echo -e "\n=== Same Port (Port 1236) ==="
echo "Testing sameport1..."
check_content "$BASE_URL:1236/" "Same Port 1" "-H Host:sameport1"
echo "Testing sameport2..."
check_content "$BASE_URL:1236/" "Same Port 2" "-H Host:sameport2"

echo -e "\n=== Error & Body Limit (Port 1237) ==="
echo "Testing Custom 404..."
check_status "$BASE_URL:1237/nonexistent" 404
check_content "$BASE_URL:1237/nonexistent" "Custom 404 Error"

echo "Testing Body Limit (Client Max Body Size 10)..."
# Sending 20 bytes
check_status "$BASE_URL:1237/" 413 "-X POST -d '12345678901234567890'"

echo -e "\n=== Routes (Port 1238) ==="
echo "Testing /someLocation/ (mapped to tests/)..."
check_content "$BASE_URL:1238/someLocation/" "Routes Test"

echo -e "\n=== Methods (Port 1239) ==="
echo "Testing GET on / (Allowed)..."
check_status "$BASE_URL:1239/" 200
echo "Testing DELETE on / (Not Allowed)..."
check_status "$BASE_URL:1239/" 405 "-X DELETE"
check_content "$BASE_URL:1239/" "Custom 405 Error" "-X DELETE"

echo "Testing POST on /dumpster/ (Allowed)..."
# Upload small file to dumpster
check_status "$BASE_URL:1239/dumpster/rubbish" 201 "-X POST -d 'garbage'"

echo -e "\n=== CGI (Port 1240) ==="
echo "Testing CGI GET (Success & Relative Path)..."
check_content "$BASE_URL:1240/tests/cgi/test.py?name=evaluator" "CGI Test Success!"
check_content "$BASE_URL:1240/tests/cgi/test.py" "Hello from data file!"

echo "Testing CGI POST..."
check_content "$BASE_URL:1240/tests/cgi/test.py" "Post Body: HelloWebserver" "-X POST -d HelloWebserver"

echo "Testing CGI Timeout (Infinite Loop)..."
check_status "$BASE_URL:1240/tests/cgi/infinite.py" 504

echo "Testing CGI Error (Script Crash/Exit)..."
check_status "$BASE_URL:1240/tests/cgi/error.py" 500

echo -e "\n=== Extra Features (Port 1241) ==="
echo "Testing Redirection (/google -> google.com)..."
check_status "$BASE_URL:1241/google" 301
# Check Location header
# Use -i to see headers, grep for Location
header=$(curl -s -i "$BASE_URL:1241/google" | grep "Location:")
if [[ "$header" == *"google.com"* ]]; then
    echo -e "${GREEN}PASS${NC} (Location: google.com)"
else
    echo -e "${RED}FAIL${NC} (Location mismatch: $header)"
fi

echo "Testing Autoindex (/tests/)..."
# The server should return "Index of /tests/" in the body
check_content "$BASE_URL:1241/tests/" "Index of /tests/"
