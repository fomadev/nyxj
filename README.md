# NyxJ Engine

NyxJ is a high-performance, lightweight JSON processing engine and micro-service framework written in pure C. Designed for embedded systems, system-level tools, and high-concurrency micro-services, NyxJ provides a robust bridge between low-level system operations and high-level web interfaces.

## Core Features

* **Efficient JSON Parsing:** Specialized tree-based parsing for structured data extraction.
* **Zero-Dependency Core:** The core engine is written in standard C for maximum portability.
* **Multi-Platform Support:** Native compilation for Windows (x64), Linux (x86_64, ARM64), and macOS (Apple Silicon).
* **Integrated Micro-service Layer:** Powered by the Mongoose networking library to provide high-speed HTTP API capabilities.
* **Memory Safety:** Strict allocation management and error handling to prevent leaks and segmentation faults.

## Project Structure

* `src/`: Core engine implementation and server logic.
* `include/`: Public headers and API definitions.
* `vendor/`: Third-party dependencies (Mongoose).
* `examples/`: Implementation demonstrations including API services and client-side testing.
* `tests/`: Unit and integration testing suites.

## Getting Started

### Prerequisites

* GCC or Clang compiler.
* Make (optional, for automation).
* Windows users: MinGW-w64 (via w64devkit) is recommended for POSIX compatibility.

### Installation and Build

1. Clone the repository:
    ```bash
    git clone https://github.com/fomadev/nyxj.git
    cd nyxj
    ```

2. Compile the main server:
    ```bash
    make
    ```

3. Run the micro-service:
    ```bash
    ./nyxj_server
    ```

## API Documentation

### HTTP Interface

The NyxJ micro-service listens on port 8000 by default.

**Endpoint**: `POST /api/data`

**Content-Type**: `application/json`

**Example Request**:
```json
{
    "status": "request",
    "payload": "NyxJ Engine Test"
}
```

**Example Response**:
```json
{
    "status": "success",
    "engine": "NyxJ v1.0.0",
    "message": "Data processed successfully"
}
```

## Cross-Platform Compilation

NyxJ is designed to be compiled across various environments.

### Linux (x86_64 / ARM64)

```bash
gcc src/server.c src/core/parser.c vendor/mongoose/mongoose.c -Iinclude -Ivendor/mongoose -o nyxj_server -lpthread
```

### Windows (MinGW)

```bash
gcc src/server.c src/core/parser.c vendor/mongoose/mongoose.c -Iinclude -Ivendor/mongoose -o nyxj_server.exe -lws2_32
```

### macOS (Apple Silicon)

```bash
gcc src/server.c src/core/parser.c vendor/mongoose/mongoose.c -Iinclude -Ivendor/mongoose -o nyxj_server
```

## Development and Contributions

NyxJ is maintained by FomaDev. Contributions related to performance optimization, new JSON serialization methods, or additional networking protocols are welcome.

1. Fork the repository.

2. Create a feature branch (`git checkout -b feature/Optimization`).

3. Commit your changes (`git commit -m 'Add performance fix'`).

4. Push to the branch (`git push origin feature/Optimization`).

5. Open a Pull Request.

<a href="CONTRIBUTING.md">CONTRIBUTING.md</a>

## License

This project is licensed under the MIT <a href="LICENSE">License</a> - see the LICENSE file for details.

## Contributors

@fordimalanda