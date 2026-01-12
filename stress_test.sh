#!/bin/bash

URL="http://localhost:8080"
CONCURRENCY=20
REQUESTS_PER_CLIENT=5

echo "Starting stress test on $URL..."
echo "Concurrency: $CONCURRENCY"
echo "Total expected requests: $((CONCURRENCY * REQUESTS_PER_CLIENT))"

success_count=0
fail_count=0

# Create a temporary file to store results
results_file=$(mktemp)

for ((i=1; i<=CONCURRENCY; i++)); do
    (
        for ((j=1; j<=REQUESTS_PER_CLIENT; j++)); do
            response=$(curl -s -o /dev/null -w "%{http_code}" "$URL")
            if [ "$response" == "200" ]; then
                echo "1" >> "$results_file"
            else
                echo "0" >> "$results_file"
            fi
        done
    ) &
done

# Wait for all background processes to finish
wait

success_count=$(grep -c "1" "$results_file")
fail_count=$(grep -c "0" "$results_file")

echo "--- Results ---"
echo "Successful requests: $success_count"
echo "Failed requests: $fail_count"

rm "$results_file"

if [ "$fail_count" -eq 0 ] && [ "$success_count" -gt 0 ]; then
    echo "STRESS TEST PASSED"
    exit 0
else
    echo "STRESS TEST FAILED"
    exit 1
fi
