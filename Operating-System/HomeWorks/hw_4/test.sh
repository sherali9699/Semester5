#!/bin/bash

# Paths to test files
declare -a files=("testfile.txt" "largefile.txt" "image.jpg" "largeimage.jpg" "video.mp4" "mediumvideo.mp4" "archive.zip" "corruptedfile.bin")
declare -a threads=(1 5 10 20)

# Start the server in the background
./server & 
SERVER_PID=$!
sleep 1  # Give server time to start


# Function to run a test
run_test() {
    local file=$1
    local threads=$2
    echo "Testing $file with $threads threads..."

    # Run the client-server test 
    ./client "$file" "$threads"
}

# Running all tests
for file in "${files[@]}"; do
    for threads in "${threads[@]}"; do
        run_test "$file" "$threads"
    done
done

# Stop the server
kill $SERVER_PID