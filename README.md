# StrandServer

StrandServer is a simple HTTP proxy server with caching capabilities. It parses HTTP requests, forwards them to the appropriate remote server, and caches the responses to improve performance for subsequent requests.

## Features

- Parses HTTP requests and responses.
- Supports GET method.
- Caches responses to reduce load on remote servers.
- Handles multiple client requests concurrently using threads.
- Limits the number of concurrent clients and cache size.

## Files

- `proxy_parse.h`: Header file for the HTTP request parsing library.
- `proxy_parse.c`: Implementation of the HTTP request parsing library.
- `proxy_server_with_cache.c`: Implementation of the proxy server with caching.

## Usage

### Compilation

To compile the project, use the following command:

```sh
gcc -o proxy_server_with_cache proxy_server_with_cache.c proxy_parse.c -lpthread
```

### Running the Server

To run the server, use the following command:

```sh
./proxy_server_with_cache <port_number>
```

Replace `<port_number>` with the desired port number for the proxy server.

### Example

```sh
./proxy_server_with_cache 8080
```

## Code Structure

### `proxy_parse.h`

This file contains the declarations for the HTTP request parsing library. It includes the `ParsedRequest` and `ParsedHeader` structures and function declarations for creating, parsing, and destroying parsed requests.

### `proxy_parse.c`

This file contains the implementation of the HTTP request parsing library. It includes functions for creating, parsing, and destroying parsed requests, as well as functions for handling headers.

### `proxy_server_with_cache.c`

This file contains the implementation of the proxy server with caching. It includes functions for handling client requests, connecting to remote servers, and managing the cache.

## License

This project is licensed under the MIT License.