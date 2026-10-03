#include <QCoreApplication>
#include <QHttpServer>
#include <QHttpServerResponder>
#include <QHttpServerRequest>
#include <QTcpServer>
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QTimer>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QThread>
#include <memory>
#include <atomic>
#include <mutex>

#include <opencv2/opencv.hpp>

// ------------------------------------------------------------------
// Shared state
// ------------------------------------------------------------------
static std::atomic<int> g_frameCounter{0};
static std::atomic<int> g_selectCounter{0};
static std::atomic<int> g_clickCounter{0};

struct SelectionRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool valid = false;
};
static SelectionRect g_selection;
static std::mutex    g_selMutex;

// Latest webcam frame (JPEG already encoded) protected by mutex
static QByteArray    g_latestJpeg;
static std::mutex    g_frameMutex;
static bool          g_cameraOk = false;

// ------------------------------------------------------------------
// Background thread that continuously grabs from the webcam
// ------------------------------------------------------------------
class CameraThread : public QThread
{
    Q_OBJECT
public:
    explicit CameraThread(int cameraIndex = 0, QObject *parent = nullptr)
        : QThread(parent), m_index(cameraIndex) {}

protected:
    void run() override
    {
        cv::VideoCapture cap(m_index, cv::CAP_ANY);
        if (!cap.isOpened()) {
            qCritical() << "Cannot open camera" << m_index;
            g_cameraOk = false;
            return;
        }

        // Optional: force resolution
        cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 360);
        cap.set(cv::CAP_PROP_FPS, 30);

        g_cameraOk = true;
        qInfo() << "Camera opened successfully";

        cv::Mat frame, rgb;
        std::vector<uchar> buf;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 75};

        while (!isInterruptionRequested()) {
            if (!cap.read(frame) || frame.empty()) {
                QThread::msleep(30);
                continue;
            }

            // Convert BGR → RGB for Qt
            cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

            // Encode to JPEG
            buf.clear();
            cv::imencode(".jpg", rgb, buf, params);

            {
                std::lock_guard lock(g_frameMutex);
                g_latestJpeg = QByteArray(reinterpret_cast<const char*>(buf.data()),
                                          static_cast<int>(buf.size()));
            }

            g_frameCounter.fetch_add(1);
            QThread::msleep(5);          // small yield
        }

        cap.release();
        qInfo() << "Camera thread stopped";
    }

private:
    int m_index;
};

// ------------------------------------------------------------------
// Draw selection rectangle on top of a JPEG frame
// ------------------------------------------------------------------
static QByteArray overlaySelection(const QByteArray &jpegIn)
{
    if (jpegIn.isEmpty())
        return jpegIn;

    // Decode
    std::vector<uchar> data(jpegIn.begin(), jpegIn.end());
    cv::Mat mat = cv::imdecode(data, cv::IMREAD_COLOR);
    if (mat.empty())
        return jpegIn;

    // Draw selection
    {
        std::lock_guard lock(g_selMutex);
        if (g_selection.valid && g_selection.w > 2 && g_selection.h > 2) {
            //cv::Rect r(g_selection.x, g_selection.y, g_selection.w, g_selection.h);

            // Semi-transparent fill
            //cv::Mat overlay = mat.clone();
            //cv::rectangle(overlay, r, cv::Scalar(255, 120, 0), -1); // BGR
            //cv::addWeighted(overlay, 0.25, mat, 0.75, 0, mat);

            // Border
            //cv::rectangle(mat, r, cv::Scalar(255, 180, 0), 3);

            // Size text
            // std::string label = std::to_string(g_selection.w) + "x" +
            //                     std::to_string(g_selection.h);
            // cv::putText(mat, label,
            //             cv::Point(r.x + 6, std::max(20, r.y - 8)),
            //             cv::FONT_HERSHEY_SIMPLEX, 0.6,
            //             cv::Scalar(255, 255, 255), 2);

            // Crosshair
            int lineSize = 2;
            int radius = 20;
            int cy = g_selection.y + g_selection.h/2;
            int cx = g_selection.x + g_selection.w/2;
            int dx = radius/2;
            int dy = radius/2;

            cv::line(mat, cv::Point(cx-dx, cy), cv::Point(cx-3*dx, cy), cv::Scalar(255, 255, 255), lineSize);
            cv::line(mat, cv::Point(cx+dx, cy), cv::Point(cx+3*dx, cy), cv::Scalar(255, 255, 255), lineSize);

            cv::line(mat, cv::Point(cx, cy-dy), cv::Point(cx, cy-3*dy), cv::Scalar(255, 255, 255), lineSize);
            cv::line(mat, cv::Point(cx, cy+dy), cv::Point(cx, cy+3*dy), cv::Scalar(255, 255, 255), lineSize);

            // Circle
            cv::circle(mat, cv::Point(cx, cy), radius, cv::Scalar(255, 255, 255), lineSize);
        }
    }

    // Status text
    std::string status = "Webcam  •  Frame " + std::to_string(g_frameCounter.load()) +
                         "  •  Selections: " + std::to_string(g_selectCounter.load());
    cv::putText(mat, status, cv::Point(12, 28),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);

    // Re-encode
    std::vector<uchar> outBuf;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 75};
    cv::imencode(".jpg", mat, outBuf, params);

    return QByteArray(reinterpret_cast<const char*>(outBuf.data()),
                      static_cast<int>(outBuf.size()));
}

// ------------------------------------------------------------------
// Long-lived MJPEG streamer
// ------------------------------------------------------------------
class MjpegStreamer : public QObject
{
    Q_OBJECT
public:
    explicit MjpegStreamer(QHttpServerResponder &&responder, QObject *parent = nullptr)
        : QObject(parent)
        , m_responder(std::move(responder))
    {
        QHttpHeaders headers;
        headers.append(QHttpHeaders::WellKnownHeader::ContentType,
                       "multipart/x-mixed-replace; boundary=frame");
        headers.append(QHttpHeaders::WellKnownHeader::CacheControl, "no-cache, no-store");
        headers.append("Pragma", "no-cache");
        headers.append("Connection", "close");

        m_responder.writeBeginChunked(headers);
        sendFrame();

        m_timer.setInterval(40); // ~25 fps target
        connect(&m_timer, &QTimer::timeout, this, &MjpegStreamer::sendFrame);
        m_timer.start();
    }

private slots:
    void sendFrame()
    {
        // if (m_responder.isResponseCanceled()) {
        //     qDebug() << "Client disconnected – stopping streamer";
        //     m_timer.stop();
        //     deleteLater();
        //     return;
        // }

        QByteArray jpeg;
        {
            std::lock_guard lock(g_frameMutex);
            jpeg = g_latestJpeg;
        }

        if (jpeg.isEmpty()) {
            // Placeholder while camera is starting
             QImage placeholder(640, 360, QImage::Format_RGB32);
             placeholder.fill(Qt::darkGray);
            // QPainter p(&placeholder);
            // p.setPen(Qt::white);
            // p.drawText(placeholder.rect(), Qt::AlignCenter,
            //            g_cameraOk ? "Waiting for frame…" : "Camera not available");
            // p.end();

            QBuffer buf(&jpeg);
            buf.open(QIODevice::WriteOnly);
            placeholder.save(&buf, "JPEG", 70);
        } else {
            jpeg = overlaySelection(jpeg);   // add selection rectangle
        }

        QByteArray part;
        part += "--frame\r\n";
        part += "Content-Type: image/jpeg\r\n";
        part += "Content-Length: " + QByteArray::number(jpeg.size()) + "\r\n\r\n";
        part += jpeg;
        part += "\r\n";

        m_responder.writeChunk(part);
    }

private:
    QHttpServerResponder m_responder;
    QTimer m_timer;
};

// ------------------------------------------------------------------
// main
// ------------------------------------------------------------------
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Start camera capture thread
    CameraThread camera(0);          // change index if needed
    camera.start();

    QHttpServer server;

    // ---------- HTML page (drag-to-select) ----------
    server.route("/", []() {
        return QHttpServerResponse("text/html", R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <title>Qt6 + OpenCV Webcam Stream</title>
    <style>
        body {
            font-family: system-ui, sans-serif;
            background: #111; color: #eee;
            display: flex; flex-direction: column; align-items: center;
            padding: 2rem; user-select: none;
        }
        h1 { margin-bottom: 0.3rem; }
        .hint { color: #aaa; margin-bottom: 1rem; }
        #container {
            position: relative; display: inline-block;
            border: 3px solid #444; border-radius: 8px;
            overflow: hidden; cursor: crosshair;
        }
        #stream { display: block; max-width: 90vw; }
        #sel {
            position: absolute; border: 2px solid #00b4ff;
            background: rgba(0,120,255,0.25);
            pointer-events: none; display: none;
        }
        button { margin-top: 1.5rem; padding: 0.8rem 2rem; font-size: 1.2rem;
                 background: #0078d4; color: white; border: none; border-radius: 6px;
                 cursor: pointer; transition: background 0.2s; }
        button:hover { background: #106ebe; }
        button:active { background: #005a9e; }
        #status { margin-top: 1.2rem; font-size: 1.1rem; color: #8f8; min-height: 1.5em; }
    </style>
</head>
<body>
    <h1>Qt6 + OpenCV Webcam Stream</h1>
    <p class="hint">Click and drag on the video to select a rectangle</p>

    <div id="container">
        <img id="stream" src="/stream" width="640" height="360" alt="Webcam" draggable="false">
        <div id="sel"></div>
    </div>

    <div id="status">Drag to select a region…</div>

    <button id="btn">Click Me!</button>
    <div id="btnStatus">Clicks: 0</div>

    <script>
        const btn = document.getElementById('btn');
        const btnStatus = document.getElementById('btnStatus');

        btn.addEventListener('click', async () => {
            try {
                const resp = await fetch('/click', { method: 'POST' });
                const data = await resp.json();
                btnStatus.textContent = `Clicks: ${data.clicks}  (server time: ${data.time})`;
            } catch (e) {
                btnStatus.textContent = 'Error: ' + e;
            }
        });

        const img = document.getElementById('stream');
        const selDiv = document.getElementById('sel');
        const status = document.getElementById('status');

        let startX = 0, startY = 0, dragging = false;

        function getImageCoords(e) {
            const rect = img.getBoundingClientRect();
            const scaleX = img.naturalWidth  / rect.width;
            const scaleY = img.naturalHeight / rect.height;
            const x = Math.round((e.clientX - rect.left) * scaleX);
            const y = Math.round((e.clientY - rect.top)  * scaleY);
            return {
                x: Math.max(0, Math.min(img.naturalWidth  || 640, x)),
                y: Math.max(0, Math.min(img.naturalHeight || 360, y))
            };
        }

        function updateSelDiv(x1, y1, x2, y2) {
            const rect = img.getBoundingClientRect();
            const scaleX = rect.width  / (img.naturalWidth  || 640);
            const scaleY = rect.height / (img.naturalHeight || 360);
            const left   = Math.min(x1, x2) * scaleX;
            const top    = Math.min(y1, y2) * scaleY;
            const width  = Math.abs(x2 - x1) * scaleX;
            const height = Math.abs(y2 - y1) * scaleY;
            selDiv.style.left = left + 'px';
            selDiv.style.top  = top  + 'px';
            selDiv.style.width  = width  + 'px';
            selDiv.style.height = height + 'px';
            selDiv.style.display = 'block';
        }

        img.addEventListener('mousedown', e => {
            e.preventDefault();
            const pt = getImageCoords(e);
            startX = pt.x; startY = pt.y;
            dragging = true;
            updateSelDiv(startX, startY, startX, startY);
        });

        window.addEventListener('mousemove', e => {
            if (!dragging) return;
            const pt = getImageCoords(e);
            updateSelDiv(startX, startY, pt.x, pt.y);
        });

        window.addEventListener('mouseup', async e => {
            if (!dragging) return;
            dragging = false;
            const pt = getImageCoords(e);
            let x = Math.min(startX, pt.x);
            let y = Math.min(startY, pt.y);
            let w = Math.abs(pt.x - startX);
            let h = Math.abs(pt.y - startY);

            if (w < 5 || h < 5) {
                selDiv.style.display = 'none';
                status.textContent = 'Selection too small';
                return;
            }

            status.textContent = `Selected ${w}×${h} at (${x},${y}) – sending…`;

            try {
                const resp = await fetch('/select', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({x, y, w, h})
                });
                const data = await resp.json();
                status.textContent =
                    `Selection: ${data.w}×${data.h} at (${data.x},${data.y})  •  Total: ${data.count}`;
            } catch (err) {
                status.textContent = 'Error: ' + err;
            }
            setTimeout(() => selDiv.style.display = 'none', 250);
        });
    </script>
</body>
</html>
)HTML");
    });

    // ---------- MJPEG endpoint ----------
    server.route("/stream", [](QHttpServerResponder &responder) {
        new MjpegStreamer(std::move(responder));
    });

    //---------- 3. Button click handler ----------
    server.route("/click", QHttpServerRequest::Method::Post,
                 [](const QHttpServerRequest &) {
                     int clicks = ++g_clickCounter;
                     qDebug() << "Button clicked! Total clicks:" << clicks;

                     QJsonObject obj{
                         {"clicks", clicks},
                         {"time", QDateTime::currentDateTime().toString(Qt::ISODate)}
                     };
                     return QHttpServerResponse(obj);
                 });

    // Optional: also accept GET for simple testing
    server.route("/click", QHttpServerRequest::Method::Get,
                 []() {
                     int clicks = ++g_clickCounter;
                     return QHttpServerResponse(QJsonObject{{"clicks", clicks}});
                 });

    // ---------- Selection endpoint ----------
    server.route("/select", QHttpServerRequest::Method::Post,
                 [](const QHttpServerRequest &request) {
                     QJsonParseError err;
                     QJsonDocument doc = QJsonDocument::fromJson(request.body(), &err);

                     int x=0, y=0, w=0, h=0;
                     if (err.error == QJsonParseError::NoError && doc.isObject()) {
                         auto obj = doc.object();
                         x = obj.value("x").toInt();
                         y = obj.value("y").toInt();
                         w = obj.value("w").toInt();
                         h = obj.value("h").toInt();
                     }

                     // Clamp
                     x = qBound(0, x, 639);
                     y = qBound(0, y, 359);
                     w = qBound(1, w, 640 - x);
                     h = qBound(1, h, 360 - y);

                     int count = ++g_selectCounter;

                     {
                         std::lock_guard lock(g_selMutex);
                         g_selection = {x, y, w, h, true};
                     }

                     qDebug() << "Selection:" << x << y << w << "x" << h << "  count:" << count;

                     return QHttpServerResponse(QJsonObject{
                         {"x", x}, {"y", y}, {"w", w}, {"h", h},
                         {"count", count},
                         {"time", QDateTime::currentDateTime().toString(Qt::ISODate)}
                     });
                 });

    // ---------- Start HTTP server ----------
    auto tcp = std::make_unique<QTcpServer>();
    if (!tcp->listen(QHostAddress::Any, 8080) || !server.bind(tcp.get())) {
        qCritical() << "Failed to listen on port 8080";
        return -1;
    }
    tcp.release();

    qInfo() << "Server running at http://localhost:8080/";
    qInfo() << "Using OpenCV webcam. Drag on the image to select a rectangle.";

    int ret = app.exec();

    // Clean shutdown
    camera.requestInterruption();
    camera.wait(2000);

    return ret;
}

#include "main.moc"
