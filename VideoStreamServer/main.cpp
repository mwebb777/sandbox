/**
 * Qt Video Stream Server
 *
 * A simple HTTP server using Qt HttpServer that:
 *  - Serves an HTML page with a live MJPEG video stream
 *  - Streams synthetic video frames (bouncing ball + timestamp) as MJPEG
 *  - Provides a /click endpoint that receives button press events from the browser
 *
 * Build requirements (Qt 6.4+ recommended, Qt 6.5+ for best QHttpServerResponder support):
 *   - Qt6 Core, Network, Gui, HttpServer
 *
 * Usage:
 *   ./VideoStreamServer [--port 8080]
 *   Open http://localhost:8080 in a browser
 */

#include <QCoreApplication>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QHttpServerResponse>
#include <QTcpServer>
#include <QHostAddress>
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QByteArray>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QCommandLineParser>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>
#include <QPointer>
#include <atomic>
#include <memory>

// ---------------------------------------------------------------------------
// Shared latest JPEG frame (updated by the frame generator)
// ---------------------------------------------------------------------------
class FrameBuffer
{
public:
    void update(const QByteArray &jpeg)
    {
        QMutexLocker lock(&m_mutex);
        m_jpeg = jpeg;
        m_generation++;
    }

    QByteArray current() const
    {
        QMutexLocker lock(&m_mutex);
        return m_jpeg;
    }

    quint64 generation() const
    {
        QMutexLocker lock(&m_mutex);
        return m_generation;
    }

private:
    mutable QMutex m_mutex;
    QByteArray m_jpeg;
    quint64 m_generation = 0;
};

// Global frame buffer (simple for this demo)
static FrameBuffer g_frameBuffer;

// ---------------------------------------------------------------------------
// Stream connection: keeps a QHttpServerResponder alive and pushes frames
// ---------------------------------------------------------------------------
class MjpegStream : public QObject
{
    Q_OBJECT
public:
    explicit MjpegStream(QHttpServerResponder &&responder, QObject *parent = nullptr)
        : QObject(parent)
        , m_responder(std::move(responder))
    {
        // Send initial headers for multipart MJPEG stream
        m_responder.writeStatus(QHttpServerResponder::StatusCode::Ok);
        m_responder.writeHeader(QByteArrayLiteral("Content-Type"),
                                QByteArrayLiteral("multipart/x-mixed-replace; boundary=--frameboundary"));
        m_responder.writeHeader(QByteArrayLiteral("Cache-Control"),
                                QByteArrayLiteral("no-cache, no-store, must-revalidate"));
        m_responder.writeHeader(QByteArrayLiteral("Pragma"), QByteArrayLiteral("no-cache"));
        m_responder.writeHeader(QByteArrayLiteral("Connection"), QByteArrayLiteral("close"));
        m_responder.writeHeader(QByteArrayLiteral("Access-Control-Allow-Origin"), QByteArrayLiteral("*"));

        // Push first frame immediately
        sendFrame();

        // Then periodically
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &MjpegStream::sendFrame);
        m_timer->start(50); // ~20 FPS target
    }

    ~MjpegStream() override
    {
        if (m_timer)
            m_timer->stop();
    }

private slots:
    void sendFrame()
    {
        if (m_closed)
            return;

        const QByteArray jpeg = g_frameBuffer.current();
        if (jpeg.isEmpty())
            return;

        // Build one multipart part
        QByteArray part;
        part.reserve(jpeg.size() + 128);
        part.append("--frameboundary\r\n");
        part.append("Content-Type: image/jpeg\r\n");
        part.append("Content-Length: ");
        part.append(QByteArray::number(jpeg.size()));
        part.append("\r\n\r\n");
        part.append(jpeg);
        part.append("\r\n");

        // write() returns false when the client has disconnected
        if (!m_responder.write(part)) {
            qDebug() << "Client disconnected from MJPEG stream";
            m_closed = true;
            m_timer->stop();
            // Schedule self-deletion so the responder is destroyed cleanly
            deleteLater();
        }
    }

private:
    QHttpServerResponder m_responder;
    QTimer *m_timer = nullptr;
    bool m_closed = false;
};

// ---------------------------------------------------------------------------
// Synthetic video generator (bouncing ball + clock)
// ---------------------------------------------------------------------------
class VideoGenerator : public QObject
{
    Q_OBJECT
public:
    explicit VideoGenerator(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &VideoGenerator::generateFrame);
        m_timer->start(33); // ~30 FPS generation
    }

private slots:
    void generateFrame()
    {
        const int W = 640;
        const int H = 480;

        QImage img(W, H, QImage::Format_RGB32);
        img.fill(QColor(30, 30, 40));

        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);

        // Background grid
        p.setPen(QPen(QColor(50, 50, 70), 1));
        for (int x = 0; x < W; x += 40)
            p.drawLine(x, 0, x, H);
        for (int y = 0; y < H; y += 40)
            p.drawLine(0, y, W, y);

        // Bouncing ball
        m_x += m_dx;
        m_y += m_dy;
        if (m_x - m_radius < 0 || m_x + m_radius > W) m_dx = -m_dx;
        if (m_y - m_radius < 0 || m_y + m_radius > H) m_dy = -m_dy;

        QRadialGradient grad(m_x - 10, m_y - 10, m_radius);
        grad.setColorAt(0, QColor(255, 220, 100));
        grad.setColorAt(1, QColor(200, 80, 20));
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(m_x, m_y), m_radius, m_radius);

        // Timestamp and info
        p.setPen(Qt::white);
        p.setFont(QFont("Sans", 16, QFont::Bold));
        const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        p.drawText(20, 40, ts);

        p.setFont(QFont("Sans", 12));
        p.drawText(20, 70, QString("Frame gen: %1  |  Qt Video Stream Server").arg(g_frameBuffer.generation()));

        p.end();

        // Encode as JPEG
        QByteArray jpeg;
        QBuffer buffer(&jpeg);
        buffer.open(QIODevice::WriteOnly);
        img.save(&buffer, "JPEG", 80); // quality 80

        g_frameBuffer.update(jpeg);
    }

private:
    QTimer *m_timer = nullptr;
    double m_x = 320.0;
    double m_y = 240.0;
    double m_dx = 4.5;
    double m_dy = 3.2;
    const double m_radius = 35.0;
};

// ---------------------------------------------------------------------------
// HTML page served to the browser
// ---------------------------------------------------------------------------
static QByteArray htmlPage()
{
    return R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Qt Video Stream Server</title>
  <style>
    body {
      font-family: system-ui, -apple-system, sans-serif;
      background: #1a1a2e;
      color: #eee;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 2rem;
      margin: 0;
    }
    h1 { margin-bottom: 0.5rem; }
    .subtitle { color: #aaa; margin-bottom: 1.5rem; }
    #video {
      border: 3px solid #4a4a6a;
      border-radius: 8px;
      background: #000;
      max-width: 100%;
      box-shadow: 0 8px 32px rgba(0,0,0,0.5);
    }
    .controls {
      margin-top: 1.5rem;
      display: flex;
      gap: 1rem;
      align-items: center;
    }
    button {
      background: #4f46e5;
      color: white;
      border: none;
      padding: 0.75rem 1.5rem;
      font-size: 1rem;
      border-radius: 6px;
      cursor: pointer;
      transition: background 0.2s;
    }
    button:hover { background: #6366f1; }
    button:active { background: #4338ca; }
    #status {
      min-height: 1.5rem;
      color: #86efac;
      font-family: monospace;
    }
  </style>
</head>
<body>
  <h1>Qt Video Stream</h1>
  <p class="subtitle">MJPEG stream from a Qt C++ server &bull; Button events sent back via HTTP</p>

  <img id="video" src="/video" width="640" height="480" alt="Live stream">

  <div class="controls">
    <button id="btn" onclick="sendClick()">Send Click Event</button>
    <span id="status"></span>
  </div>

  <script>
    function sendClick() {
      const status = document.getElementById('status');
      status.textContent = 'Sending...';
      fetch('/click', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ event: 'button_click', timestamp: Date.now() })
      })
      .then(r => r.text())
      .then(text => {
        status.textContent = text + ' @ ' + new Date().toLocaleTimeString();
      })
      .catch(err => {
        status.textContent = 'Error: ' + err;
        status.style.color = '#f87171';
      });
    }
  </script>
</body>
</html>
)HTML";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("VideoStreamServer");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Qt HTTP video streaming server with click feedback");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(QStringList() << "p" << "port",
                                  "HTTP server port (default: 8080)",
                                  "port", "8080");
    parser.addOption(portOption);
    parser.process(app);

    const quint16 port = parser.value(portOption).toUShort();

    // Start synthetic video generator
    VideoGenerator generator;

    // HTTP server
    QHttpServer httpServer;

    // Root page
    httpServer.route("/", []() {
        return QHttpServerResponse("text/html", htmlPage());
    });

    // MJPEG stream endpoint
    // Note: we take ownership of the responder so the connection stays open
    httpServer.route("/video", [](const QHttpServerRequest &, QHttpServerResponder &&responder) {
        // The MjpegStream object will live until the client disconnects
        // (it calls deleteLater() on itself)
        new MjpegStream(std::move(responder));
        // We deliberately do not return a QHttpServerResponse here;
        // the responder is already being used by MjpegStream.
    });

    // Click / event endpoint (accepts POST and GET for convenience)
    httpServer.route("/click", [](const QHttpServerRequest &request) {
        const QByteArray body = request.body();
        qInfo().noquote() << "[CLICK]" << QDateTime::currentDateTime().toString(Qt::ISODate)
                          << "from" << request.remoteAddress().toString()
                          << "body:" << (body.isEmpty() ? "(empty)" : body.constData());

        return QHttpServerResponse("text/plain",
                                   QByteArray("Server received click event"));
    });

    // Also allow GET /click for simple testing
    httpServer.route("/click", QHttpServerRequest::Method::Get,
                     [](const QHttpServerRequest &request) {
        qInfo().noquote() << "[CLICK-GET]" << QDateTime::currentDateTime().toString(Qt::ISODate)
                          << "from" << request.remoteAddress().toString();
        return QHttpServerResponse("text/plain", QByteArray("Server received GET click"));
    });

    // Health / info
    httpServer.route("/info", []() {
        return QHttpServerResponse("application/json",
            QByteArrayLiteral(R"({"status":"ok","stream":"/video","click":"/click"})"));
    });

    // Bind
    auto tcpServer = std::make_unique<QTcpServer>();
    if (!tcpServer->listen(QHostAddress::Any, port) || !httpServer.bind(tcpServer.get())) {
        qCritical() << "Failed to bind HTTP server on port" << port;
        return 1;
    }
    // Transfer ownership of the TCP server to the HTTP server
    tcpServer.release();

    qInfo() << "Video stream server running at http://localhost:" << port;
    qInfo() << "Open the URL in a browser. Click the button to send events back.";
    qInfo() << "MJPEG endpoint: /video";

    return app.exec();
}

#include "main.moc"
