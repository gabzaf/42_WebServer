#!/bin/bash

URL="http://localhost:8080/uploads"
CONCURRENCY=10
REQUESTS_PER_CLIENT=3

echo "Starting POST/DELETE stress test on $URL..."

success_count=0
fail_count=0
results_file=$(mktemp)

for ((i=1; i<=CONCURRENCY; i++)); do
    (
        for ((j=1; j<=REQUESTS_PER_CLIENT; j++)); do
            fname="test_${i}_${j}.txt"
            # 1. Upload
            res_post=$(curl -s -o /dev/null -w "%{http_code}" -X POST -d "Stress test data $i $j" "$URL/$fname")
            
            # 2. Delete
            res_del=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "$URL/$fname")
            
            if [ "$res_post" == "201" ] && [ "$res_del" == "204" ]; then
                echo "1" >> "$results_file"
            else
                echo "0" >> "$results_file"
                echo "Fail: POST=$res_post, DELETE=$res_del"
            fi
        done
    ) &
done

wait

success_count=$(grep -c "1" "$results_file")
fail_count=$(grep -c "0" "$results_file")

echo "--- Results ---"
echo "Successful cycles: $success_count"
echo "Failed cycles: $fail_count"

rm "$results_file"

if [ "$fail_count" -eq 0 ] && [ "$success_count" -gt 0 ]; then
    echo "POST/DELETE STRESS TEST PASSED"
    exit 0
else
    echo "POST/DELETE STRESS TEST FAILED"
    exit 1
fi
