const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = process.env.PORT || 3000;
const PUBLIC_DIR = path.join(__dirname, 'public');

// In-memory simulated/synced CyberDeck state
const state = {
  startTime: Date.now() - 1245000,
  device: "nebula-cyberdeck",
  firmware: "cyberdeck_nebula_v1",
  freeHeap: 152340,
  wifi: {
    connected: true,
    ssid: "IceApple",
    ip: "10.85.139.73",
    rssi: -62
  },
  screen: {
    current: "MAIN_MENU",
    idleSeconds: 12
  },
  sensors: {
    bmpAvailable: true,
    temperatureC: 24.3,
    pressureHpa: 1013.2
  },
  bluetooth: {
    audioStarted: true,
    phoneConnected: true,
    phonePlaying: false
  },
  chatbot: {
    mode: "voice",
    uiState: "READY",
    lastReply: "Tap to ask"
  },
  ui: {
    screen: "CHATBOT_SCREEN",
    mode: "voice",
    uiState: "VOICE_THINKING",
    statusText: "Fathoming...",
    lastTranscript: "What is the weather today?",
    lastReply: "I don’t have live weather, but you can check an app."
  },
  pomodoro: {
    active: false,
    workMinutes: 25,
    breakMinutes: 5,
    startedAt: null
  }
};

const mimeTypes = {
  '.html': 'text/html',
  '.css': 'text/css',
  '.js': 'text/javascript',
  '.json': 'application/json',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.woff2': 'font/woff2',
  '.woff': 'font/woff'
};

function sendJson(res, statusCode, data) {
  res.writeHead(statusCode, {
    'Content-Type': 'application/json',
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type'
  });
  res.end(JSON.stringify(data, null, 2));
}

const server = http.createServer((req, res) => {
  // CORS Preflight
  if (req.method === 'OPTIONS') {
    res.writeHead(204, {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type'
    });
    return res.end();
  }

  const url = new URL(req.url, `http://${req.headers.host}`);
  const pathname = url.pathname;

  // --- API Endpoints ---
  if (pathname === '/api/status' && req.method === 'GET') {
    const uptime = Math.floor((Date.now() - state.startTime) / 1000);
    // Micro jitter for live feel
    const tempJitter = Number((state.sensors.temperatureC + (Math.sin(uptime / 20) * 0.1)).toFixed(1));
    const pressJitter = Number((state.sensors.pressureHpa + (Math.cos(uptime / 35) * 0.2)).toFixed(1));

    return sendJson(res, 200, {
      device: state.device,
      firmware: state.firmware,
      uptimeSeconds: uptime,
      freeHeap: state.freeHeap,
      wifi: state.wifi,
      screen: state.screen,
      sensors: {
        bmpAvailable: state.sensors.bmpAvailable,
        temperatureC: tempJitter,
        pressureHpa: pressJitter
      },
      bluetooth: state.bluetooth,
      chatbot: state.chatbot
    });
  }

  if (pathname === '/api/sensors' && req.method === 'GET') {
    const uptime = Math.floor((Date.now() - state.startTime) / 1000);
    const tempJitter = Number((state.sensors.temperatureC + (Math.sin(uptime / 20) * 0.1)).toFixed(1));
    const pressJitter = Number((state.sensors.pressureHpa + (Math.cos(uptime / 35) * 0.2)).toFixed(1));

    return sendJson(res, 200, {
      timestamp: new Date().toISOString(),
      temperatureC: tempJitter,
      pressureHpa: pressJitter,
      bmpAvailable: state.sensors.bmpAvailable
    });
  }

  if (pathname === '/api/ui' && req.method === 'GET') {
    return sendJson(res, 200, {
      timestamp: new Date().toISOString(),
      screen: state.ui.screen,
      mode: state.ui.mode,
      uiState: state.ui.uiState,
      statusText: state.ui.statusText,
      lastTranscript: state.ui.lastTranscript,
      lastReply: state.ui.lastReply
    });
  }

  if (pathname === '/api/command' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      try {
        const payload = JSON.parse(body || '{}');
        const { command, params } = payload;

        if (command === 'startPomodoro') {
          state.pomodoro = {
            active: true,
            workMinutes: params?.workMinutes || 25,
            breakMinutes: params?.breakMinutes || 5,
            startedAt: Date.now()
          };
          state.screen.current = 'POMODORO';
          return sendJson(res, 200, {
            status: 'ok',
            message: 'Pomodoro timer started on Nebula CyberDeck',
            pomodoro: state.pomodoro
          });
        }

        if (command === 'stopPomodoro') {
          state.pomodoro.active = false;
          return sendJson(res, 200, {
            status: 'ok',
            message: 'Pomodoro timer stopped'
          });
        }

        if (command === 'sendTextQuestion') {
          const q = params?.question || 'What is the weather today?';
          state.ui.lastTranscript = q;
          state.ui.uiState = 'VOICE_THINKING';
          state.ui.statusText = 'Fathoming...';
          state.ui.lastReply = `Processed: "${q}"`;
          return sendJson(res, 200, {
            status: 'ok',
            transcript: q,
            reply: state.ui.lastReply
          });
        }

        if (command === 'toggleBluetoothPlayback') {
          state.bluetooth.phonePlaying = !state.bluetooth.phonePlaying;
          return sendJson(res, 200, {
            status: 'ok',
            phonePlaying: state.bluetooth.phonePlaying
          });
        }

        return sendJson(res, 200, {
          status: 'ok',
          received: payload
        });
      } catch (err) {
        return sendJson(res, 400, { error: 'Invalid JSON body' });
      }
    });
    return;
  }

  // --- Static Files Serving ---
  let filePath = path.join(PUBLIC_DIR, pathname === '/' ? 'index.html' : pathname);
  const ext = path.extname(filePath).toLowerCase();

  fs.stat(filePath, (err, stats) => {
    if (err || !stats.isFile()) {
      // Fallback to index.html for SPA if not found
      filePath = path.join(PUBLIC_DIR, 'index.html');
    }

    const contentType = mimeTypes[path.extname(filePath).toLowerCase()] || 'application/octet-stream';
    fs.readFile(filePath, (readErr, content) => {
      if (readErr) {
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        return res.end('404 Not Found');
      }
      res.writeHead(200, { 'Content-Type': contentType });
      res.end(content);
    });
  });
});

server.listen(PORT, () => {
  console.log(`✨ Nebula CyberDeck Dashboard running at http://localhost:${PORT}`);
});
