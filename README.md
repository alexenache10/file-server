Implementation of a file server application in C. The server exposes access to a set of files through a socket interface, allowing clients to perform various operations such as listing available files, downloading, uploading, deleting, moving, updating files, and searching for specific words within files. The server supports multi-threaded concurrent connections, a configurable maximum number of simultaneous connections, and maintains a global log file to record all operations performed.

Features:
Multi-threaded file server handling concurrent client connections.
Support for operations including listing files, downloading, uploading, deleting, moving, updating files, and searching.
Implementation adheres to specified protocols for communication and error handling.

Implementation Details:
Utilizes POSIX sockets for network communication.
Implements multi-threaded server architecture for concurrent client handling.
Utilizes thread-safe mechanisms for file operations and logging.
Follows specified protocols for message encoding and error handling.

Clone the repository.
Compile the server application using provided instructions.
Run the server executable on the desired host and port.
Connect to the server using a client application.
Perform various file operations as per the specified protocol.
