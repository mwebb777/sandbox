#include <QCoreApplication>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QTcpServer>
#include <QUrlQuery>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>

// Helper: common HTML head + navigation
QString htmlHeader(const QString &title)
{
    return QStringLiteral(R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>%1</title>
    <style>
        :root {
            --bg: #0f172a;
            --card: #1e293b;
            --accent: #38bdf8;
            --text: #e2e8f0;
            --muted: #94a3b8;
            --success: #22c55e;
            --border: #334155;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: system-ui, -apple-system, sans-serif;
            background: var(--bg);
            color: var(--text);
            line-height: 1.6;
            min-height: 100vh;
        }
        nav {
            background: var(--card);
            padding: 1rem 2rem;
            display: flex;
            gap: 1.5rem;
            border-bottom: 1px solid var(--border);
            position: sticky;
            top: 0;
            z-index: 10;
        }
        nav a {
            color: var(--muted);
            text-decoration: none;
            font-weight: 500;
            transition: color 0.2s;
        }
        nav a:hover, nav a.active { color: var(--accent); }
        .container {
            max-width: 800px;
            margin: 2rem auto;
            padding: 0 1.5rem;
        }
        h1 { font-size: 1.8rem; margin-bottom: 0.5rem; }
        p.lead { color: var(--muted); margin-bottom: 2rem; }
        .card {
            background: var(--card);
            border-radius: 12px;
            padding: 1.75rem;
            margin-bottom: 1.5rem;
            border: 1px solid var(--border);
        }
        label {
            display: block;
            margin-bottom: 0.4rem;
            font-weight: 500;
            color: var(--muted);
        }
        input[type="text"],
        input[type="number"],
        input[type="range"],
        select {
            width: 100%;
            padding: 0.6rem 0.8rem;
            border-radius: 8px;
            border: 1px solid var(--border);
            background: var(--bg);
            color: var(--text);
            font-size: 1rem;
            margin-bottom: 1.2rem;
        }
        input[type="range"] {
            padding: 0;
            height: 8px;
            cursor: pointer;
        }
        .range-value {
            display: inline-block;
            min-width: 40px;
            text-align: right;
            color: var(--accent);
            font-weight: 600;
        }
        .checkbox-group {
            display: flex;
            flex-wrap: wrap;
            gap: 1rem;
            margin-bottom: 1.2rem;
        }
        .checkbox-group label {
            display: flex;
            align-items: center;
            gap: 0.5rem;
            cursor: pointer;
            color: var(--text);
        }
        input[type="checkbox"] {
            width: 18px;
            height: 18px;
            accent-color: var(--accent);
        }
        button, .btn {
            background: var(--accent);
            color: #0f172a;
            border: none;
            padding: 0.7rem 1.4rem;
            border-radius: 8px;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: opacity 0.2s, transform 0.1s;
        }
        button:hover { opacity: 0.9; }
        button:active { transform: scale(0.98); }
        .btn-secondary {
            background: transparent;
            color: var(--accent);
            border: 1px solid var(--accent);
        }
        .result {
            background: #064e3b;
            border: 1px solid var(--success);
            border-radius: 8px;
            padding: 1rem 1.25rem;
            margin-top: 1.5rem;
            white-space: pre-wrap;
            font-family: ui-monospace, monospace;
            font-size: 0.9rem;
        }
        .row {
            display: flex;
            gap: 1rem;
            align-items: center;
            margin-bottom: 1rem;
        }
        .row label { margin: 0; }
    </style>
</head>
<body>
<nav>
    <a href="/">Home</a>
    <a href="/controls">Controls Demo</a>
    <a href="/form">Form Page</a>
    <a href="/about">About</a>
</nav>
<div class="container">
)").arg(title);
}

QString htmlFooter()
{
    return QStringLiteral(R"(
</div>
</body>
</html>
)");
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QHttpServer server;

    // ========== HOME PAGE ==========
    server.route("/", []() {
        QString body = htmlHeader("Home") + R"(
            <h1>Qt QHttpServer Demo</h1>
            <p class="lead">A multi-page web server built with Qt's QHttpServer featuring interactive controls.</p>

            <div class="card">
                <h2>Available Pages</h2>
                <ul style="margin-top:1rem; padding-left:1.5rem; color:var(--muted);">
                    <li><a href="/controls" style="color:var(--accent);">Controls Demo</a> – Buttons, sliders, checkboxes, spinners & edit boxes</li>
                    <li><a href="/form" style="color:var(--accent);">Form Page</a> – Submit data and see the result</li>
                    <li><a href="/about" style="color:var(--accent);">About</a> – Information about this demo</li>
                </ul>
            </div>

            <div class="card">
                <h2>Quick Test</h2>
                <p style="margin-bottom:1rem; color:var(--muted);">Click the button to send a simple request.</p>
                <form method="POST" action="/echo">
                    <input type="text" name="message" placeholder="Type something..." value="Hello from QHttpServer!">
                    <button type="submit">Send Message</button>
                </form>
            </div>
        )" + htmlFooter();

        return QHttpServerResponse("text/html", body.toUtf8());
    });

    // ========== POST /color – receive color change from circle button ==========
    server.route("/color", QHttpServerRequest::Method::Post,
        [](const QHttpServerRequest &request) {
         QUrlQuery query(QString::fromUtf8(request.body()));
         QString colorName = query.queryItemValue("color");
         QString colorHex  = query.queryItemValue("hex");

         qInfo() << "Circle button changed color to:" << colorName << colorHex;

         // Return a simple JSON response
         QJsonObject obj;
         obj["status"] = "ok";
         obj["received"] = colorName;
         obj["hex"] = colorHex;
         obj["message"] = QString("Server received color: %1").arg(colorName);

         return QHttpServerResponse(obj);
    });

    // ========== CONTROLS DEMO PAGE ==========
    server.route("/controls", []() {
        QString body = htmlHeader("Controls Demo") + R"(
            <h1>Interactive Controls</h1>
            <p class="lead">All the requested UI elements in one place.</p>

            <div class="card">
                <form method="POST" action="/submit" id="controlsForm">
                    <!-- Edit Box (text input) -->
                    <label for="name">Edit Box (Text Input)</label>
                    <input type="text" id="name" name="name" placeholder="Enter your name" value="Qt User">

                    <!-- Spinner (number input) -->
                    <label for="age">Spinner (Number Input)</label>
                    <input type="number" id="age" name="age" min="1" max="120" value="30" step="1">

                    <!-- Slider (range) -->
                    <label for="volume">
                        Slider
                        <span class="range-value" id="volumeValue">50</span>
                    </label>
                    <input type="range" id="volume" name="volume" min="0" max="100" value="50"
                           oninput="document.getElementById('volumeValue').textContent = this.value">

                    <!-- Another slider -->
                    <label for="brightness">
                        Brightness Slider
                        <span class="range-value" id="brightnessValue">75</span>
                    </label>
                    <input type="range" id="brightness" name="brightness" min="0" max="100" value="75"
                           oninput="document.getElementById('brightnessValue').textContent = this.value">

                    <!-- Checkboxes -->
                    <label>Checkboxes</label>
                    <div class="checkbox-group">
                        <label><input type="checkbox" name="options" value="wifi" checked> Wi-Fi</label>
                        <label><input type="checkbox" name="options" value="bluetooth"> Bluetooth</label>
                        <label><input type="checkbox" name="options" value="notifications" checked> Notifications</label>
                        <label><input type="checkbox" name="options" value="darkmode"> Dark Mode</label>
                    </div>

                    <!-- Select (another form of spinner / dropdown) -->
                    <label for="theme">Theme (Select / Spinner-like)</label>
                    <select id="theme" name="theme">
                        <option value="system">System</option>
                        <option value="light">Light</option>
                        <option value="dark" selected>Dark</option>
                        <option value="high-contrast">High Contrast</option>
                    </select>

                    <!-- Buttons -->
                    <div class="row" style="margin-top:1.5rem;">
                        <button type="submit">Submit All</button>
                        <button type="reset" class="btn-secondary">Reset</button>
                        <button type="button" onclick="alert('Button clicked!')dd">Plain Button</button>
                    </div>
                </form>
            </div>


        <!-- Circular Color-Changing Button -->
        <div class="card">
            <h2 style="margin-bottom: 1rem;">Circular Color Button</h2>
            <p style="color: var(--muted); margin-bottom: 1.5rem;">
                Click the circle to cycle through colors.
            </p>

            <div style="display: flex; align-items: center; gap: 2rem;">
                <button id="colorCircle" type="button"
                    style="
                        width: 100px;
                        height: 100px;
                        border-radius: 50%;
                        border: none;
                        background-color: #38bdf8;
                        color: #0f172a;
                        font-weight: 700;
                        font-size: 0.9rem;
                        cursor: pointer;
                        box-shadow: 0 4px 15px rgba(56, 189, 248, 0.4);
                        transition: background-color 0.3s ease, transform 0.15s ease, box-shadow 0.3s ease;
                    "
                    onmouseover="this.style.transform='scale(1.08)'"
                    onmouseout="this.style.transform='scale(1)'"
                    onmousedown="this.style.transform='scale(0.95)'"
                    onmouseup="this.style.transform='scale(1.08)'">
                    Click me
                </button>

                <div>
                    <div style="color: var(--muted); font-size: 0.9rem;">Current color:</div>
                    <div id="colorName" style="font-size: 1.2rem; font-weight: 600; color: #38bdf8;">
                        Sky Blue
                    </div>
                    <div id="serverStatus" style="margin-top: 0.6rem; font-size: 0.9rem; color: var(--muted);">
                        Waiting for click...
                    </div>
                </div>
            </div>
        </div>

        <script>
            const colors = [
                { hex: '#38bdf8', name: 'Sky Blue' },
                { hex: '#f472b6', name: 'Pink' },
                { hex: '#a78bfa', name: 'Purple' },
                { hex: '#34d399', name: 'Emerald' },
                { hex: '#fbbf24', name: 'Amber' },
                { hex: '#f87171', name: 'Red' },
                { hex: '#60a5fa', name: 'Blue' },
                { hex: '#c084fc', name: 'Violet' }
            ];

            let currentIndex = 0;
            const circle = document.getElementById('colorCircle');
            const colorName = document.getElementById('colorName');
            const serverStatus = document.getElementById('serverStatus');

            circle.addEventListener('click', async () => {
                currentIndex = (currentIndex + 1) % colors.length;
                const c = colors[currentIndex];

                // Update UI immediately
                circle.style.backgroundColor = c.hex;
                circle.style.boxShadow = `0 4px 15px ${c.hex}66`;
                colorName.textContent = c.name;
                colorName.style.color = c.hex;
                serverStatus.textContent = 'Sending to server...';
                serverStatus.style.color = 'var(--muted)';

                // Send POST to the server
                try {
                    const formData = new URLSearchParams();
                    formData.append('color', c.name);
                    formData.append('hex', c.hex);

                    const response = await fetch('/color', {
                        method: 'POST',
                        headers: {
                            'Content-Type': 'application/x-www-form-urlencoded'
                        },
                        body: formData
                    });

                    const data = await response.json();

                    if (data.status === 'ok') {
                        serverStatus.textContent = '✓ ' + data.message;
                        serverStatus.style.color = '#22c55e';
                    } else {
                        serverStatus.textContent = 'Server error';
                        serverStatus.style.color = '#f87171';
                    }
                } catch (err) {
                    serverStatus.textContent = 'Failed to reach server';
                    serverStatus.style.color = '#f87171';
                    console.error(err);
                }
            });
        </script>

        )" + htmlFooter();

        return QHttpServerResponse("text/html", body.toUtf8());
    });

    // ========== FORM PAGE ==========
    server.route("/form", []() {
        QString body = htmlHeader("Form Page") + R"(
            <h1>Simple Form</h1>
            <p class="lead">Fill out the form and submit it to the server.</p>

            <div class="card">
                <form method="POST" action="/submit">
                    <label for="username">Username (Edit Box)</label>
                    <input type="text" id="username" name="name" required placeholder="Your username">

                    <label for="level">Level (Spinner)</label>
                    <input type="number" id="level" name="age" min="1" max="99" value="1">

                    <label for="power">Power Level (Slider)</label>
                    <input type="range" id="power" name="volume" min="0" max="100" value="42"
                           oninput="this.nextElementSibling.textContent = this.value">
                    <span class="range-value">42</span>

                    <div class="checkbox-group" style="margin-top:1.2rem;">
                        <label><input type="checkbox" name="options" value="newsletter"> Subscribe to newsletter</label>
                        <label><input type="checkbox" name="options" value="terms" required> Accept terms</label>
                    </div>

                    <button type="submit" style="margin-top:1rem;">Submit Form</button>
                </form>
            </div>
        )" + htmlFooter();

        return QHttpServerResponse("text/html", body.toUtf8());
    });

    // ========== ABOUT PAGE ==========
    server.route("/about", []() {
        QString body = htmlHeader("About") + R"(
            <h1>About this Demo</h1>
            <div class="card">
                <p>This is a multi-page HTTP server written in C++ using <strong>Qt QHttpServer</strong>.</p>
                <br>
                <p>It demonstrates:</p>
                <ul style="margin:1rem 0 0 1.5rem; color:var(--muted);">
                    <li>Multiple routes / pages</li>
                    <li>HTML forms with various controls</li>
                    <li>POST request handling</li>
                    <li>Dynamic HTML responses</li>
                    <li>Modern dark UI styling</li>
                </ul>
                <br>
                <p>Built with Qt 6 HttpServer module.</p>
            </div>
        )" + htmlFooter();

        return QHttpServerResponse("text/html", body.toUtf8());
    });

    // ========== POST /submit – process form data ==========
    server.route("/submit", QHttpServerRequest::Method::Post,
        [](const QHttpServerRequest &request) {
            QUrlQuery query(QString::fromUtf8(request.body()));

            QString name = query.queryItemValue("name");
            QString age  = query.queryItemValue("age");
            QString volume = query.queryItemValue("volume");
            QString brightness = query.queryItemValue("brightness");
            QString theme = query.queryItemValue("theme");

            // Collect all checkbox values
            QStringList options;
            const auto items = query.queryItems();
            for (const auto &item : items) {
                if (item.first == "options")
                    options << item.second;
            }

            QString result = QStringLiteral(
                "Received form data:\n\n"
                "Name (Edit Box)     : %1\n"
                "Age/Level (Spinner) : %2\n"
                "Volume (Slider)     : %3\n"
                "Brightness (Slider) : %4\n"
                "Theme (Select)      : %5\n"
                "Checkboxes          : %6\n"
            ).arg(name.isEmpty() ? "(empty)" : name,
                  age.isEmpty() ? "(empty)" : age,
                  volume.isEmpty() ? "(empty)" : volume,
                  brightness.isEmpty() ? "(not set)" : brightness,
                  theme.isEmpty() ? "(not set)" : theme,
                  options.isEmpty() ? "(none)" : options.join(", "));

            QString body = htmlHeader("Form Result") + R"(
                <h1>Form Submitted</h1>
                <p class="lead">Here is what the server received:</p>
                <div class="card">
                    <div class="result">)" + result.toHtmlEscaped() + R"(</div>
                    <br>
                    <a href="/controls" class="btn" style="display:inline-block; text-decoration:none;">← Back to Controls</a>
                    <a href="/form" class="btn btn-secondary" style="display:inline-block; text-decoration:none; margin-left:0.5rem;">Back to Form</a>
                </div>
            )" + htmlFooter();

            return QHttpServerResponse("text/html", body.toUtf8());
        });

    // ========== POST /echo – simple echo ==========
    server.route("/echo", QHttpServerRequest::Method::Post,
        [](const QHttpServerRequest &request) {
            QUrlQuery query(QString::fromUtf8(request.body()));
            QString message = query.queryItemValue("message");

            QString body = htmlHeader("Echo") + R"(
                <h1>Message Received</h1>
                <div class="card">
                    <div class="result">Server received: ")" + message.toHtmlEscaped() + R"("</div>
                    <br>
                    <a href="/" class="btn" style="display:inline-block; text-decoration:none;">← Home</a>
                </div>
            )" + htmlFooter();

            return QHttpServerResponse("text/html", body.toUtf8());
        });

    // Start listening
    auto *tcpServer = new QTcpServer(&app);
    if (!tcpServer->listen(QHostAddress::Any, 8080) || !server.bind(tcpServer)) {
        qCritical() << "Failed to start server on port 8080";
        return 1;
    }

    qInfo().noquote() << QString("Server running at http://127.0.0.1:%1/").arg(tcpServer->serverPort());
    qInfo() << "Press Ctrl+C to quit.";

    return app.exec();
}