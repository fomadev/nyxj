# Contributing to NyxJ Engine

First of all, thank you for considering contributing to NyxJ. As a project managed by FomaDev, we value professional and high-quality contributions to improve the efficiency and reliability of our JSON engine.

## Code of Conduct

By participating in this project, you are expected to maintain a professional demeanor and respect the technical decisions made by the core maintainers.

## How Can I Contribute?

### Reporting Bugs
* Ensure the bug was not already reported by searching on GitHub under Issues.
* If you're unable to find an open issue addressing the problem, open a new one.
* Include a clear title and a detailed description, including relevant information to reproduce the bug.

### Suggesting Enhancements
* Open a GitHub Issue to discuss the proposed enhancement.
* Provide a clear use case and explain why this enhancement would benefit the project.

### Pull Requests
1. Fork the repository and create your branch from `main`.
2. If you've added code that should be tested, add tests.
3. Ensure the code compiles on at least two of the supported platforms (Linux, Windows, macOS).
4. Follow the existing C coding style (consistent indentation, clear variable naming, and comprehensive comments).
5. Issue a Pull Request (PR) with a detailed description of your changes.

## Coding Standards

* **C Standard:** All code must adhere to the C99 or C11 standard for maximum portability.
* **Memory Management:** Every allocation (`malloc`, `calloc`) must be checked for NULL and properly freed.
* **Networking:** Any changes to the networking layer must maintain compatibility with the Mongoose library.
* **Documentation:** Update `README.md` and header files if you introduce new functions or modify existing APIs.

## Contact

For further inquiries, please contact the FomaDev team via the official GitHub repository.