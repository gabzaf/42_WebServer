#!/usr/bin/env python3
import sys, os

print("Content-Type: text/html\r\n\r\n", end="")
print("<html><body>")
print("<h1>CGI Test Success!</h1>")
print("<p>Method: " + os.environ.get("REQUEST_METHOD", "Unknown") + "</p>")

if os.environ.get("REQUEST_METHOD") == "POST":
    body = sys.stdin.read()
    print("<p>Post Body: " + body + "</p>")
else:
    print("<p>Query String: " + os.environ.get("QUERY_STRING", "") + "</p>")

# Test relative path access
try:
    with open("data.txt", "r") as f:
        print("<p>Relative Data: " + f.read() + "</p>")
except Exception as e:
    print("<p>Relative Data Error: " + str(e) + "</p>")

print("</body></html>")
