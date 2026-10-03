# Qt Video Stream Server

A C++ Qt application that runs an HTTP server streaming live video (MJPEG) to any modern web browser and receives button-click events from the page back to the server.

## Features

- **MJPEG video stream** at `/video` (multipart/x-mixed-replace)
- Synthetic video source: bouncing ball + live timestamp (no camera required)
- Simple HTML UI with a button that POSTs a click event to `/click`
- Server logs every click with timestamp and client IP
- CORS-friendly headers
- Configurable port

## Requirements

- Qt 6.4 or later (Qt 6.5+ recommended)
- Modules: `Core`, `Network`, `Gui`, `HttpServer`
- CMake ≥ 3.16
- C++17 compiler

> **Note**: `Qt6::HttpServer` is part of the Qt Network / additional modules. On some distributions it may be in a separate package (`qt6-httpserver-dev` or similar).

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64   # adjust path
cmake --build .
```

Or with qmake (if you prefer a `.pro` file – see below).

## Run

```bash
./VideoStreamServer                 # listens on port 8080
./VideoStreamServer --port 9000     # custom port
```

Then open a browser at:

```
http://localhost:8080
```

You should see the live stream and a button. Clicking the button sends a POST to the server; the server prints a log line and returns a confirmation that appears under the button.

## Endpoints

| Path     | Method | Description                          |
|----------|--------|--------------------------------------|
| `/`      | GET    | HTML page with video + button        |
| `/video` | GET    | Continuous MJPEG stream              |
| `/click` | POST   | Receives button click events         |
| `/click` | GET    | Same as POST (for easy testing)      |
| `/info`  | GET    | Simple JSON status                   |

## How the streaming works

1. A background `VideoGenerator` produces JPEG frames at ~30 FPS and stores the latest one in a thread-safe buffer.
2. When a browser requests `/video`, an `MjpegStream` object takes ownership of the `QHttpServerResponder`.
3. The streamer writes the proper `multipart/x-mixed-replace` headers once, then periodically writes a new JPEG part.
4. If the client closes the connection, `write()` fails and the streamer deletes itself.

## Extending to a real camera

Replace (or augment) `VideoGenerator` with Qt Multimedia:

```cpp
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>

// In a class:
QCamera *camera = new QCamera;
QMediaCaptureSession session;
session.setCamera(camera);
QVideoSink *sink = new QVideoSink;
session.setVideoSink(sink);
connect(sink, &QVideoSink::videoFrameChanged, this, [](const QVideoFrame &frame) {
    QImage img = frame.toImage();
    // convert to JPEG and push into g_frameBuffer
});
camera->start();
```

Link against `Qt6::Multimedia` and add it to `CMakeLists.txt`.

## Alternative .pro file (qmake)

```pro
QT += core network gui httpserver
CONFIG += c++17 console
SOURCES += main.cpp
TARGET = VideoStreamServer
```

## License

This example is provided as-is for educational / mission-support purposes.
