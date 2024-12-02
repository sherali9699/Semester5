Overview
This Homework implements a file transfer system with checksum verification using SHA-256. The server and client processes handle file transmission and integrity verification under various scenarios.

How to Run

1. Compilation
Use the provided Makefile to compile the source files.

1. Open a terminal and navigate to the project directory.
2. Run: make
   This will compile `client.c` and `server.c` into the executables `client` and `server`.

2. Running the Server
Start the server process to listen for client requests.

1. Run the server executable:
   ./server

2. The server will:
   - Wait for incoming connections on the default port.
   - Accept a filename and thread count as input from the client.
   - Handle file transfers, calculate file checksums, and send that checksum to Client for verification.


3. Running the Client
Connect the client process to the server and request file transfers.

1. Run the client executable with the following syntax:
   ./client <Filename> <No_of_threads>
   
   - Replace `<Filename>` with the file you want to get from the server.
   - Replace `<No_of_threads>` with the no threads you want the server to use for sending you file.

2. The client will:
   - Send a request to the server specifying the file to download and the thread count for parallel transfers.
   - Receive the file, calculate the checksum locally, and compare it with the server's checksum to verify file integrity.

Assumptions**
- Both the server and client are running on the same network, and the IP address of the server is known.
- The server has the requested file in its directory and has read permissions for the file.
- File integrity is validated using SHA-256 hashes on both the server and client sides.

Limitations
1. File Size:
   - The system is tested with files ranging from small text files (~1 KB) to large video files (~200 MB). Larger files may require additional testing for performance.


Additional Notes
- Ensure OpenSSL libraries (`libssl` and `libcrypto`) are installed, as they are required for checksum computation.
- Use the `clean` target in the Makefile to remove compiled binaries:
  make clean