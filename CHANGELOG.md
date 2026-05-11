## [1.8.0] - 2026-05-11

### Added
- WebSocket RFC6455 compliance (v13)
- Fragmentation support
- Upload size limits

### Fixed
- WebSocket handshake (Sec-WebSocket-Accept)
- Memory leaks and double free issues
- Logger thread-safety issues
- HTTP header parsing bugs

### Improved
- Multipart handling (nginx-based parser)
- LocalRepository security and limits
- Logging robustness
- General C++ modernization
