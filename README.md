# StrandServer

StrandServer is a high-performance, multi-threaded HTTP proxy server with caching, designed for learning and experimentation. It efficiently parses HTTP requests, forwards them to remote servers, and caches responses to accelerate repeated requests. Built in C, it demonstrates core networking, concurrency, and caching concepts.

---

## ✨ Features

- **HTTP Request Parsing:** Robust parsing of HTTP/1.0 and HTTP/1.1 GET requests.
- **Caching:** In-memory LRU cache for fast repeated responses.
- **Concurrency:** Handles hundreds of clients using POSIX threads and semaphores.
- **Error Handling:** Graceful responses for common HTTP errors (400, 403, 404, 500, 501, 505).
- **Customizable:** Easily adjust cache size, element size, and client limits.

---

## 📁 Project Structure

- `proxy_parse.h` & `proxy_parse.c`  
  HTTP request parsing library (structs, parsing, header management).
- `proxy_server_with_cache.c`  
  Main proxy server logic, client handling, caching, and networking.
- `Makefile`  
  (Optional) For easy compilation.

---

## 🚀 Getting Started

### 1. Clone the Repository

```sh
git clone https://github.com/myselfaryan/StrandServer.git
cd StrandServer
```

### 2. Build

```sh
gcc -o proxy_server_with_cache proxy_server_with_cache.c proxy_parse.c -lpthread
```

Or use the provided Makefile:

```sh
make
```

### 3. Run the Proxy Server

```sh
./proxy_server_with_cache <port_number>
```

Example:

```sh
./proxy_server_with_cache 8080
```

---

## 🛠️ Usage

- Configure your browser or HTTP client to use `localhost:<port_number>` as the HTTP proxy.
- The server will cache responses for repeated GET requests, improving speed for subsequent requests.
- Logs client connections and cache hits to the console.

---

## 🧩 How It Works

1. **Client connects** to the proxy and sends an HTTP GET request.
2. **Request is parsed** using the custom parsing library.
3. **Cache is checked** for a matching response (LRU eviction policy).
4. If **cache miss**, the proxy connects to the remote server, forwards the request, and caches the response.
5. **Response is sent** back to the client.

---

## 📚 Example Code Snippet

```c
// Create and parse a request
ParsedRequest *req = ParsedRequest_create();
if (ParsedRequest_parse(req, buffer, len) < 0) {
    printf("parse failed\n");
}
// Access fields
printf("Method: %s\nHost: %s\n", req->method, req->host);
ParsedRequest_destroy(req);
```

---

## 📝 License

MIT License. See [LICENSE](LICENSE) for details.

---

## 🤝 Contributing

Pull requests and suggestions are welcome! For major changes, please open an issue first to discuss what you would like to change.

---




