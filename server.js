const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');

// --- Load .env configuration ---
function loadEnv() {
  const envPaths = [
    path.join(__dirname, '.env'),
    'C:/Users/niyaj/OneDrive/Documents/nebula_chatbot_backend/.env'
  ];

  const env = {
    PORT: 3000,
    GEMINI_MODEL: 'gemini-1.5-flash',
    DEEPGRAM_STT_MODEL: 'nova-3',
    DEEPGRAM_TTS_MODEL: 'aura-2-thalia-en',
    GEMINI_API_KEY: '',
    DEEPGRAM_API_KEY: ''
  };

  for (const p of envPaths) {
    if (fs.existsSync(p)) {
      try {
        const content = fs.readFileSync(p, 'utf8');
        content.split('\n').forEach(line => {
          const match = line.match(/^\s*([\w.-]+)\s*=\s*(.*)?\s*$/);
          if (match) {
            let key = match[1].trim();
            let val = (match[2] || '').trim();
            // Strip enclosing quotes and angle brackets <...>
            val = val.replace(/^<|>$/g, '').replace(/^['\"]|['\"]$/g, '').trim();
            if (val) env[key] = val;
          }
        });
        console.log(`[Config] Loaded environment variables from ${p}`);
        break;
      } catch (err) {
        console.warn(`[Config] Failed to read ${p}:`, err.message);
      }
    }
  }

  if (process.env.PORT) env.PORT = process.env.PORT;
  if (process.env.GEMINI_API_KEY) env.GEMINI_API_KEY = process.env.GEMINI_API_KEY;
  if (process.env.DEEPGRAM_API_KEY) env.DEEPGRAM_API_KEY = process.env.DEEPGRAM_API_KEY;

  return env;
}

const config = loadEnv();
const PORT = config.PORT || 3000;
const PUBLIC_DIR = path.join(__dirname, 'public');

// --- Global Simulated / Synced CyberDeck State ---
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
    uiState: "READY",
    statusText: "Ready",
    lastTranscript: "What is the weather today?",
    lastReply: "I don't have live weather, but you can check an app."
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
  '.woff': 'font/woff',
  '.wav': 'audio/wav'
};

function sendJson(res, statusCode, data) {
  res.writeHead(statusCode, {
    'Content-Type': 'application/json',
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type, Authorization'
  });
  res.end(JSON.stringify(data, null, 2));
}

// --- Deepgram Speech-To-Text Handler ---
function handleDeepgramSTT(audioBuffer) {
  return new Promise((resolve, reject) => {
    if (!config.DEEPGRAM_API_KEY) {
      return reject(new Error('DEEPGRAM_API_KEY missing in .env'));
    }

    const model = config.DEEPGRAM_STT_MODEL || 'nova-3';
    const req = https.request({
      hostname: 'api.deepgram.com',
      path: `/v1/listen?model=${encodeURIComponent(model)}&smart_format=true`,
      method: 'POST',
      headers: {
        'Authorization': `Token ${config.DEEPGRAM_API_KEY}`,
        'Content-Type': 'audio/wav',
        'Content-Length': audioBuffer.length
      },
      timeout: 30000
    }, (res) => {
      let data = '';
      res.on('data', chunk => { data += chunk; });
      res.on('end', () => {
        try {
          const parsed = JSON.parse(data);
          if (res.statusCode >= 200 && res.statusCode < 300) {
            const transcript = parsed.results?.channels?.[0]?.alternatives?.[0]?.transcript || '';
            resolve(transcript);
          } else {
            reject(new Error(parsed.err_msg || parsed.message || `Deepgram STT HTTP ${res.statusCode}`));
          }
        } catch (err) {
          reject(new Error(`Failed to parse Deepgram response: ${err.message}`));
        }
      });
    });

    req.on('error', reject);
    req.on('timeout', () => {
      req.destroy();
      reject(new Error('Deepgram STT timed out'));
    });

    req.write(audioBuffer);
    req.end();
  });
}

// --- Gemini AI Chatbot Handler ---
function handleGeminiAsk(question) {
  return new Promise((resolve, reject) => {
    if (!config.GEMINI_API_KEY) {
      return resolve(`Processed question: "${question}" (Add GEMINI_API_KEY to .env for live AI answers)`);
    }

    // Try configured model, fallback to gemini-1.5-flash if needed
    const model = config.GEMINI_MODEL || 'gemini-1.5-flash';
    const payload = JSON.stringify({
      contents: [
        {
          parts: [{ text: question }]
        }
      ],
      systemInstruction: {
        parts: [
          {
            text: "You are Nebula, a cute, witty, helpful cyberpunk desktop robot companion. Keep answers short, friendly, conversational, and under 2-3 sentences so they sound natural when spoken."
          }
        ]
      }
    });

    const req = https.request({
      hostname: 'generativelanguage.googleapis.com',
      path: `/v1beta/models/${encodeURIComponent(model)}:generateContent?key=${config.GEMINI_API_KEY}`,
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(payload)
      },
      timeout: 25000
    }, (res) => {
      let data = '';
      res.on('data', chunk => { data += chunk; });
      res.on('end', () => {
        try {
          const parsed = JSON.parse(data);
          if (res.statusCode >= 200 && res.statusCode < 300) {
            const reply = parsed.candidates?.[0]?.content?.parts?.[0]?.text?.trim();
            resolve(reply || "I'm thinking about that!");
          } else {
            console.warn(`[Gemini] HTTP ${res.statusCode}:`, data);
            resolve(`Hello from Nebula! You asked: "${question}"`);
          }
        } catch (err) {
          resolve(`Processed: "${question}"`);
        }
      });
    });

    req.on('error', (err) => {
      console.warn('[Gemini Error]', err.message);
      resolve(`Hello from Nebula! You asked: "${question}"`);
    });

    req.on('timeout', () => {
      req.destroy();
      resolve("I heard your question, but my cloud brain took a bit too long to respond.");
    });

    req.write(payload);
    req.end();
  });
}

// --- Deepgram Text-To-Speech Handler ---
function handleDeepgramTTS(text, clientRes) {
  if (!config.DEEPGRAM_API_KEY) {
    clientRes.writeHead(500, { 'Content-Type': 'text/plain' });
    return clientRes.end('DEEPGRAM_API_KEY missing in .env');
  }

  const model = config.DEEPGRAM_TTS_MODEL || 'aura-2-thalia-en';
  const postData = JSON.stringify({ text: text });

  const req = https.request({
    hostname: 'api.deepgram.com',
    path: `/v1/speak?model=${encodeURIComponent(model)}&encoding=linear16&container=wav&sample_rate=16000`,
    method: 'POST',
    headers: {
      'Authorization': `Token ${config.DEEPGRAM_API_KEY}`,
      'Content-Type': 'application/json',
      'Content-Length': Buffer.byteLength(postData)
    },
    timeout: 30000
  }, (deepgramRes) => {
    if (deepgramRes.statusCode >= 200 && deepgramRes.statusCode < 300) {
      clientRes.writeHead(200, {
        'Content-Type': 'audio/wav',
        'Connection': 'close',
        'Access-Control-Allow-Origin': '*'
      });
      deepgramRes.pipe(clientRes);
    } else {
      let errData = '';
      deepgramRes.on('data', c => { errData += c; });
      deepgramRes.on('end', () => {
        console.warn(`[Deepgram TTS Error HTTP ${deepgramRes.statusCode}]:`, errData);
        clientRes.writeHead(deepgramRes.statusCode, { 'Content-Type': 'text/plain' });
        clientRes.end(errData || 'TTS Generation Failed');
      });
    }
  });

  req.on('error', (err) => {
    console.warn('[Deepgram TTS Network Error]:', err.message);
    if (!clientRes.headersSent) {
      clientRes.writeHead(502, { 'Content-Type': 'text/plain' });
      clientRes.end('TTS Network Error');
    }
  });

  req.write(postData);
  req.end();
}

// --- HTTP Server ---
const server = http.createServer((req, res) => {
  // CORS Preflight
  if (req.method === 'OPTIONS') {
    res.writeHead(204, {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type, Authorization'
    });
    return res.end();
  }

  const url = new URL(req.url, `http://${req.headers.host}`);
  const pathname = url.pathname;

  // ============================================================
  // 1. ESP32 CHATBOT BACKEND ENDPOINTS (from nebula.ino)
  // ============================================================

  // --- POST /transcribe : Speech-To-Text ---
  if (pathname === '/transcribe' && req.method === 'POST') {
    const chunks = [];
    req.on('data', chunk => { chunks.push(chunk); });
    req.on('end', async () => {
      const audioBuffer = Buffer.concat(chunks);
      console.log(`[STT] Received WAV audio (${audioBuffer.length} bytes) from ESP32`);

      state.ui.uiState = 'VOICE_THINKING';
      state.ui.statusText = 'Fathoming...';

      try {
        const transcript = await handleDeepgramSTT(audioBuffer);
        console.log(`[STT Transcript]: "${transcript}"`);
        
        state.ui.lastTranscript = transcript || "No speech heard";
        sendJson(res, 200, {
          ok: true,
          transcript: transcript
        });
      } catch (err) {
        console.warn('[STT Error]:', err.message);
        state.ui.lastTranscript = `[Audio: ${audioBuffer.length}b]`;
        sendJson(res, 200, {
          ok: false,
          error: err.message,
          transcript: ""
        });
      }
    });
    return;
  }

  // --- POST /ask : Gemini AI Response ---
  if (pathname === '/ask' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', async () => {
      try {
        const json = JSON.parse(body || '{}');
        const question = json.question || 'Hello Nebula!';
        console.log(`[Chatbot Question]: "${question}"`);

        state.ui.lastTranscript = question;
        state.ui.uiState = 'VOICE_THINKING';
        state.ui.statusText = 'Fathoming...';

        const reply = await handleGeminiAsk(question);
        console.log(`[Chatbot Reply]: "${reply}"`);

        state.ui.lastReply = reply;
        state.ui.uiState = 'READY';
        state.ui.statusText = 'Ready';
        state.chatbot.lastReply = reply;

        sendJson(res, 200, {
          ok: true,
          reply: reply
        });
      } catch (err) {
        console.warn('[Ask Error]:', err.message);
        sendJson(res, 400, {
          ok: false,
          error: 'Invalid JSON request'
        });
      }
    });
    return;
  }

  // --- POST /tts : Text-To-Speech Audio Stream ---
  if (pathname === '/tts' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      try {
        const json = JSON.parse(body || '{}');
        const text = json.text || 'Hello from Nebula!';
        console.log(`[TTS Generating audio for]: "${text}"`);
        handleDeepgramTTS(text, res);
      } catch (err) {
        res.writeHead(400, { 'Content-Type': 'text/plain' });
        res.end('Invalid JSON request');
      }
    });
    return;
  }

  // ============================================================
  // 2. DASHBOARD API ENDPOINTS
  // ============================================================

  if (pathname === '/api/status' && req.method === 'GET') {
    const uptime = Math.floor((Date.now() - state.startTime) / 1000);
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

  // ============================================================
  // 3. STATIC FILES SERVING (Web Dashboard)
  // ============================================================
  let filePath = path.join(PUBLIC_DIR, pathname === '/' ? 'index.html' : pathname);
  const ext = path.extname(filePath).toLowerCase();

  fs.stat(filePath, (err, stats) => {
    if (err || !stats.isFile()) {
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
  console.log('====================================================');
  console.log(`✨ Nebula Combined Backend running at http://localhost:${PORT}`);
  console.log(`📡 ESP32 Chatbot Endpoints:`);
  console.log(`   - POST /transcribe (Deepgram STT: ${config.DEEPGRAM_STT_MODEL})`);
  console.log(`   - POST /ask        (Gemini AI:     ${config.GEMINI_MODEL})`);
  console.log(`   - POST /tts        (Deepgram TTS: ${config.DEEPGRAM_TTS_MODEL})`);
  console.log(`🖥️  Web Dashboard Endpoints:`);
  console.log(`   - GET  /api/status`);
  console.log(`   - GET  /api/sensors`);
  console.log(`   - GET  /api/ui`);
  console.log(`   - POST /api/command`);
  console.log('====================================================');
});
