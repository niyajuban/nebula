const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');
const dgram = require('dgram');

// --- Event Sync UDP Broadcast Configuration ---
const EVENT_UDP_PORT = 4210;
let udpSocket = null;
try {
  udpSocket = dgram.createSocket({ type: 'udp4', reuseAddr: true });
  udpSocket.bind(0, () => {
    try {
      udpSocket.setBroadcast(true);
      console.log(`[Event Sync] UDP Broadcast socket active on port ${udpSocket.address().port} (targets port ${EVENT_UDP_PORT})`);
    } catch (e) {
      console.warn('[Event Sync] Failed to enable UDP broadcast mode:', e.message);
    }
  });
  udpSocket.on('error', (err) => {
    console.warn('[Event Sync UDP Error]:', err.message);
  });
} catch (err) {
  console.warn('[Event Sync] UDP socket creation failed:', err.message);
}

// --- Load .env configuration ---
function loadEnv() {
  const envPath = path.join(__dirname, '.env');

  const env = {
    PORT: 3000,
    GEMINI_MODEL: 'gemini-3.6-flash',
    DEEPGRAM_STT_MODEL: 'nova-3',
    DEEPGRAM_TTS_MODEL: 'aura-2-thalia-en',
    GEMINI_API_KEY: '',
    DEEPGRAM_API_KEY: '',
    WEATHER_CITY: 'Vellore',
    WEATHER_LAT: 12.9165,
    WEATHER_LON: 79.1325,
    WEATHER_UPDATE_MINS: 15
  };

  if (fs.existsSync(envPath)) {
    try {
      const content = fs.readFileSync(envPath, 'utf8');
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
      console.log(`[Config] Loaded environment variables from ${envPath}`);
    } catch (err) {
      console.warn(`[Config] Failed to read ${envPath}:`, err.message);
    }
  }

  if (process.env.PORT) env.PORT = process.env.PORT;
  if (process.env.GEMINI_API_KEY) env.GEMINI_API_KEY = process.env.GEMINI_API_KEY;
  if (process.env.DEEPGRAM_API_KEY) env.DEEPGRAM_API_KEY = process.env.DEEPGRAM_API_KEY;
  if (process.env.WEATHER_CITY) env.WEATHER_CITY = process.env.WEATHER_CITY;
  if (process.env.WEATHER_LAT) env.WEATHER_LAT = Number(process.env.WEATHER_LAT);
  if (process.env.WEATHER_LON) env.WEATHER_LON = Number(process.env.WEATHER_LON);
  if (process.env.WEATHER_UPDATE_MINS) env.WEATHER_UPDATE_MINS = Number(process.env.WEATHER_UPDATE_MINS);

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
    ssid: "ESP32 Wi-Fi",
    ip: "ESP32 IP",
    rssi: -62
  },
  screen: {
    current: "MAIN_MENU",
    idleSeconds: 12
  },
  sensors: {
    temperatureC: 25.2,
    pressureHpa: 985.3,
    humidity: 88,
    condition: "Partly cloudy",
    city: config.WEATHER_CITY || "Vellore",
    source: "Open-Meteo API",
    isLiveHardware: false,
    lastTelemetryTime: 0
  },
  weather: {
    city: config.WEATHER_CITY || "Vellore",
    lat: Number(config.WEATHER_LAT) || 12.9165,
    lon: Number(config.WEATHER_LON) || 79.1325,
    temperatureC: 25.2,
    pressureHpa: 985.3,
    humidity: 88,
    condition: "Partly cloudy",
    weatherCode: 2,
    lastUpdated: 0,
    source: "Open-Meteo API"
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
    lastReply: "It's 25.2°C and partly cloudy in Vellore with 88% humidity."
  },
  pomodoro: {
    state: "IDLE", // "IDLE", "RUNNING", "PAUSED", "BREAK"
    mode: "WORK",  // "WORK", "BREAK"
    workDuration: 25 * 60,
    breakDuration: 5 * 60,
    timeLeft: 25 * 60,
    sessionsCompleted: 0
  },
  todos: [
    { id: 1, text: "Build CyberDeck", completed: true },
    { id: 2, text: "Flash ESP32 Firmware", completed: true },
    { id: 3, text: "Voice Chatbot Test", completed: false }
  ],
  game: {
    active: false,
    score: 0
  },
  eventSync: {
    active: false,
    track: "event_track.wav",
    lastCommand: "STOP",
    timestamp: 0
  }
};

function broadcastEventSync(cmd) { // 'PLAY' or 'STOP'
  state.eventSync.active = (cmd === 'PLAY');
  state.eventSync.lastCommand = cmd;
  state.eventSync.timestamp = Date.now();

  if (udpSocket) {
    const msg = Buffer.from(cmd);
    udpSocket.send(msg, 0, msg.length, EVENT_UDP_PORT, '255.255.255.255', (err) => {
      if (err) {
        console.warn(`[Event Sync] UDP Broadcast error for "${cmd}":`, err.message);
      } else {
        console.log(`📡 [Event Sync] Broadcasted "${cmd}" to all CyberDecks on UDP port ${EVENT_UDP_PORT}`);
      }
    });
  }
}

// --- Server-side Authoritative Pomodoro Ticker (1 second) ---
setInterval(() => {
  if (state.pomodoro.state === 'RUNNING' || state.pomodoro.state === 'BREAK') {
    if (state.pomodoro.timeLeft > 0) {
      state.pomodoro.timeLeft--;
    } else {
      if (state.pomodoro.mode === 'WORK') {
        state.pomodoro.sessionsCompleted++;
        state.pomodoro.mode = 'BREAK';
        state.pomodoro.state = 'BREAK';
        state.pomodoro.timeLeft = state.pomodoro.breakDuration;
      } else {
        state.pomodoro.mode = 'WORK';
        state.pomodoro.state = 'IDLE';
        state.pomodoro.timeLeft = state.pomodoro.workDuration;
      }
    }
  }
}, 1000);

// --- Open-Meteo Weather Service ---
function mapWmoWeatherCode(code) {
  switch (code) {
    case 0: return 'Clear Sky';
    case 1: return 'Mainly Clear';
    case 2: return 'Partly Cloudy';
    case 3: return 'Overcast';
    case 45: return 'Fog';
    case 48: return 'Icy Fog';
    case 51: return 'Light Drizzle';
    case 53: return 'Moderate Drizzle';
    case 55: return 'Dense Drizzle';
    case 61: return 'Slight Rain';
    case 63: return 'Moderate Rain';
    case 65: return 'Heavy Rain';
    case 71: return 'Slight Snow';
    case 73: return 'Moderate Snow';
    case 75: return 'Heavy Snow';
    case 80: return 'Slight Showers';
    case 81: return 'Moderate Showers';
    case 82: return 'Violent Showers';
    case 95: return 'Thunderstorm';
    case 96:
    case 99: return 'Thunderstorm w/ Hail';
    default: return 'Fair';
  }
}

async function fetchLiveWeather() {
  const lat = config.WEATHER_LAT || 12.9165;
  const lon = config.WEATHER_LON || 79.1325;
  const city = config.WEATHER_CITY || 'Vellore';
  const url = `https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}&current=temperature_2m,relative_humidity_2m,surface_pressure,weather_code&timezone=auto`;

  try {
    const res = await fetch(url, { signal: AbortSignal.timeout(10000) });
    if (!res.ok) {
      console.warn(`[Weather] Open-Meteo responded with HTTP ${res.status}`);
      return;
    }
    const data = await res.json();
    if (data && data.current) {
      const cur = data.current;
      const condition = mapWmoWeatherCode(cur.weather_code);
      state.weather = {
        city: city,
        lat: Number(lat),
        lon: Number(lon),
        temperatureC: Number(cur.temperature_2m),
        pressureHpa: Number(cur.surface_pressure),
        humidity: Number(cur.relative_humidity_2m),
        condition: condition,
        weatherCode: cur.weather_code,
        lastUpdated: Date.now(),
        source: 'Open-Meteo API'
      };
      state.sensors.temperatureC = state.weather.temperatureC;
      state.sensors.pressureHpa = state.weather.pressureHpa;
      state.sensors.humidity = state.weather.humidity;
      state.sensors.condition = state.weather.condition;
      state.sensors.city = state.weather.city;
      state.sensors.source = state.weather.source;

      console.log(`[Weather] Synced live weather for ${city}: ${state.weather.temperatureC}°C, ${state.weather.humidity}% humidity, ${state.weather.pressureHpa} hPa, ${condition}`);
    }
  } catch (err) {
    console.warn(`[Weather] Error fetching weather for ${city}:`, err.message);
  }
}

// Initial fetch & recurring schedule
fetchLiveWeather();
const weatherIntervalMins = Math.max(1, Number(config.WEATHER_UPDATE_MINS) || 15);
setInterval(fetchLiveWeather, weatherIntervalMins * 60 * 1000);

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

    const model = config.GEMINI_MODEL || 'gemini-3.6-flash';
    const payload = JSON.stringify({
      contents: [
        {
          parts: [{ text: question }]
        }
      ],
      systemInstruction: {
        parts: [
          {
            text: `You are Nebula, a cute, witty, helpful cyberpunk desktop robot companion. Keep answers short, friendly, conversational, and under 2-3 sentences so they sound natural when spoken. You are located in ${state.weather.city}. Live environmental telemetry: ${state.weather.temperatureC}°C, ${state.weather.condition}, Humidity: ${state.weather.humidity}%, Barometric Pressure: ${state.weather.pressureHpa} hPa.`
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
    path: `/v1/speak?model=${encodeURIComponent(model)}&encoding=linear16&container=wav&sample_rate=24000`,
    method: 'POST',
    headers: {
      'Authorization': `Token ${config.DEEPGRAM_API_KEY}`,
      'Content-Type': 'application/json',
      'Content-Length': Buffer.byteLength(postData)
    },
    timeout: 30000
  }, (deepgramRes) => {
    if (deepgramRes.statusCode >= 200 && deepgramRes.statusCode < 300) {
      const chunks = [];
      deepgramRes.on('data', chunk => chunks.push(chunk));
      deepgramRes.on('end', () => {
        const fullWav = Buffer.concat(chunks);
        console.log(`[Deepgram TTS] Audio generated: ${fullWav.length} bytes (24kHz linear16 WAV)`);
        clientRes.writeHead(200, {
          'Content-Type': 'audio/wav',
          'Content-Length': fullWav.length,
          'Connection': 'close',
          'Access-Control-Allow-Origin': '*'
        });
        clientRes.end(fullWav);
      });
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
const server = http.createServer(async (req, res) => {
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
  // 2. DASHBOARD & TELEMETRY API ENDPOINTS
  // ============================================================

  // --- POST /api/telemetry (or /api/sensors) : Ingest Live Hardware Readings from ESP32 ---
  if ((pathname === '/api/telemetry' || pathname === '/api/sensors') && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      try {
        const json = JSON.parse(body || '{}');
        if (json.temperatureC !== undefined || json.temperature !== undefined) {
          state.sensors.temperatureC = Number(json.temperatureC !== undefined ? json.temperatureC : json.temperature);
        }
        if (json.pressureHpa !== undefined || json.pressure !== undefined) {
          state.sensors.pressureHpa = Number(json.pressureHpa !== undefined ? json.pressureHpa : json.pressure);
        }
        if (json.freeHeap !== undefined) {
          state.freeHeap = Number(json.freeHeap);
        }
        if (json.rssi !== undefined) {
          state.wifi.rssi = Number(json.rssi);
        }
        if (json.screen !== undefined) {
          state.screen.current = String(json.screen);
          const s = state.screen.current.toLowerCase();
          state.game.active = (s === 'game' || s === 'game_screen' || s === 'microracer');
        }

        // ESP32 quick action sync
        if (json.pomoAction) {
          if (json.pomoAction === 'start') {
            state.pomodoro.state = state.pomodoro.mode === 'BREAK' ? 'BREAK' : 'RUNNING';
          } else if (json.pomoAction === 'pause') {
            state.pomodoro.state = 'PAUSED';
          } else if (json.pomoAction === 'reset') {
            state.pomodoro.state = 'IDLE';
            state.pomodoro.mode = 'WORK';
            state.pomodoro.timeLeft = state.pomodoro.workDuration;
          } else if (json.pomoAction === 'start_break') {
            state.pomodoro.mode = 'BREAK';
            state.pomodoro.state = 'BREAK';
            state.pomodoro.timeLeft = state.pomodoro.breakDuration;
          } else if (json.pomoAction === 'start_work') {
            state.pomodoro.mode = 'WORK';
            state.pomodoro.state = 'RUNNING';
            state.pomodoro.timeLeft = state.pomodoro.workDuration;
          }
        }

        // ESP32 To-Do toggle sync
        if (json.todoToggleId !== undefined) {
          const toggleId = Number(json.todoToggleId);
          const item = state.todos.find(t => t.id === toggleId);
          if (item) {
            item.completed = !item.completed;
            console.log(`[Todo] Toggled task #${toggleId} to completed=${item.completed} from ESP32`);
          }
        }

        state.sensors.lastTelemetryTime = Date.now();
        state.sensors.isLiveHardware = true;

        sendJson(res, 200, {
          ok: true,
          message: 'Hardware telemetry recorded',
          sensors: {
            temperatureC: state.weather.temperatureC,
            pressureHpa: state.weather.pressureHpa,
            humidity: state.weather.humidity,
            condition: state.weather.condition,
            city: state.weather.city,
            source: state.weather.source
          },
          weather: {
            temp: state.weather.temperatureC,
            pressure: state.weather.pressureHpa,
            humidity: state.weather.humidity,
            condition: state.weather.condition,
            city: state.weather.city
          },
          pomodoro: {
            state: state.pomodoro.state,
            mode: state.pomodoro.mode,
            timeLeft: state.pomodoro.timeLeft,
            sessionsCompleted: state.pomodoro.sessionsCompleted
          },
          todos: state.todos,
          gameActive: state.game.active,
          eventSync: state.eventSync
        });
      } catch (err) {
        sendJson(res, 400, { ok: false, error: 'Invalid JSON payload' });
      }
    });
    return;
  }

  // --- Pomodoro API ---
  if (pathname === '/api/pomodoro' && req.method === 'GET') {
    return sendJson(res, 200, {
      state: state.pomodoro.state,
      mode: state.pomodoro.mode,
      timeLeft: state.pomodoro.timeLeft,
      workMinutes: Math.floor(state.pomodoro.workDuration / 60),
      breakMinutes: Math.floor(state.pomodoro.breakDuration / 60),
      sessionsCompleted: state.pomodoro.sessionsCompleted
    });
  }

  if (pathname === '/api/pomodoro' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      try {
        const json = JSON.parse(body || '{}');
        const action = json.action;

        if (action === 'start') {
          state.pomodoro.state = state.pomodoro.mode === 'BREAK' ? 'BREAK' : 'RUNNING';
        } else if (action === 'pause') {
          state.pomodoro.state = 'PAUSED';
        } else if (action === 'reset') {
          state.pomodoro.state = 'IDLE';
          state.pomodoro.timeLeft = state.pomodoro.mode === 'BREAK' ? state.pomodoro.breakDuration : state.pomodoro.workDuration;
        } else if (action === 'set') {
          if (json.mode) {
            const m = String(json.mode).toUpperCase();
            state.pomodoro.mode = (m === 'BREAK' || m === 'SHORTBREAK' || m === 'LONGBREAK') ? 'BREAK' : 'WORK';
            state.pomodoro.state = 'IDLE';
            state.pomodoro.timeLeft = state.pomodoro.mode === 'BREAK' ? state.pomodoro.breakDuration : state.pomodoro.workDuration;
          }
          if (json.workMinutes) {
            state.pomodoro.workDuration = Number(json.workMinutes) * 60;
            if (state.pomodoro.state === 'IDLE' && state.pomodoro.mode === 'WORK') {
              state.pomodoro.timeLeft = state.pomodoro.workDuration;
            }
          }
          if (json.breakMinutes) {
            state.pomodoro.breakDuration = Number(json.breakMinutes) * 60;
            if (state.pomodoro.state === 'IDLE' && state.pomodoro.mode === 'BREAK') {
              state.pomodoro.timeLeft = state.pomodoro.breakDuration;
            }
          }
        }

        return sendJson(res, 200, {
          ok: true,
          state: state.pomodoro.state,
          mode: state.pomodoro.mode,
          timeLeft: state.pomodoro.timeLeft,
          workMinutes: Math.floor(state.pomodoro.workDuration / 60),
          breakMinutes: Math.floor(state.pomodoro.breakDuration / 60),
          sessionsCompleted: state.pomodoro.sessionsCompleted
        });
      } catch (err) {
        return sendJson(res, 400, { ok: false, error: 'Invalid JSON' });
      }
    });
    return;
  }

  // --- To-Do API ---
  if (pathname === '/api/todos' && req.method === 'GET') {
    return sendJson(res, 200, state.todos);
  }

  if (pathname === '/api/todos' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      try {
        const json = JSON.parse(body || '{}');
        const text = (json.text || '').trim();
        if (text) {
          const nextId = state.todos.length > 0 ? Math.max(...state.todos.map(t => t.id || 0)) + 1 : 1;
          state.todos.push({
            id: nextId,
            text: text.slice(0, 24),
            completed: false
          });
        }
        return sendJson(res, 200, { ok: true, todos: state.todos });
      } catch (err) {
        return sendJson(res, 400, { ok: false, error: 'Invalid JSON' });
      }
    });
    return;
  }

  if (pathname === '/api/todos/toggle' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      try {
        const json = JSON.parse(body || '{}');
        const id = Number(json.id);
        const item = state.todos.find(t => t.id === id);
        if (item) {
          item.completed = !item.completed;
        }
        return sendJson(res, 200, { ok: true, todos: state.todos });
      } catch (err) {
        return sendJson(res, 400, { ok: false, error: 'Invalid JSON' });
      }
    });
    return;
  }

  if (pathname === '/api/todos/delete' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      try {
        const json = JSON.parse(body || '{}');
        const id = Number(json.id);
        state.todos = state.todos.filter(t => t.id !== id);
        return sendJson(res, 200, { ok: true, todos: state.todos });
      } catch (err) {
        return sendJson(res, 400, { ok: false, error: 'Invalid JSON' });
      }
    });
    return;
  }

  if (pathname === '/api/status' && req.method === 'GET') {
    const uptime = Math.floor((Date.now() - state.startTime) / 1000);
    const isLive = (Date.now() - state.sensors.lastTelemetryTime) < 45000;
    state.sensors.isLiveHardware = isLive;

    return sendJson(res, 200, {
      device: state.device,
      firmware: state.firmware,
      uptimeSeconds: uptime,
      freeHeap: state.freeHeap,
      wifi: state.wifi,
      screen: state.screen,
      sensors: {
        temperatureC: state.weather.temperatureC,
        pressureHpa: state.weather.pressureHpa,
        humidity: state.weather.humidity,
        condition: state.weather.condition,
        city: state.weather.city,
        source: state.weather.source,
        isLiveHardware: isLive,
        lastTelemetryTime: state.sensors.lastTelemetryTime
      },
      weather: state.weather,
      chatbot: state.chatbot,
      pomodoro: {
        state: state.pomodoro.state,
        mode: state.pomodoro.mode,
        timeLeft: state.pomodoro.timeLeft,
        workMinutes: Math.floor(state.pomodoro.workDuration / 60),
        breakMinutes: Math.floor(state.pomodoro.breakDuration / 60),
        sessionsCompleted: state.pomodoro.sessionsCompleted
      },
      todos: state.todos,
      gameActive: state.game.active,
      eventSync: state.eventSync
    });
  }

  if (pathname === '/api/sensors' && req.method === 'GET') {
    const isLive = (Date.now() - state.sensors.lastTelemetryTime) < 45000;
    state.sensors.isLiveHardware = isLive;

    return sendJson(res, 200, {
      timestamp: new Date().toISOString(),
      temperatureC: state.weather.temperatureC,
      pressureHpa: state.weather.pressureHpa,
      humidity: state.weather.humidity,
      condition: state.weather.condition,
      city: state.weather.city,
      source: state.weather.source,
      isLiveHardware: isLive,
      lastTelemetryTime: state.sensors.lastTelemetryTime
    });
  }

  if (pathname === '/api/weather' && req.method === 'GET') {
    if (url.searchParams.get('refresh') === '1' || url.searchParams.get('refresh') === 'true') {
      await fetchLiveWeather();
    }
    return sendJson(res, 200, {
      ok: true,
      weather: state.weather
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
  // EVENT SYNC BROADCAST API (Method 2)
  // ============================================================

  if (pathname === '/api/event/play' && req.method === 'POST') {
    broadcastEventSync('PLAY');
    return sendJson(res, 200, {
      ok: true,
      message: 'Event sync PLAY broadcasted to all CyberDecks',
      eventSync: state.eventSync
    });
  }

  if (pathname === '/api/event/stop' && req.method === 'POST') {
    broadcastEventSync('STOP');
    return sendJson(res, 200, {
      ok: true,
      message: 'Event sync STOP broadcasted to all CyberDecks',
      eventSync: state.eventSync
    });
  }

  if (pathname === '/api/event/status' && req.method === 'GET') {
    return sendJson(res, 200, {
      ok: true,
      eventSync: state.eventSync
    });
  }

  if (pathname === '/api/event/track' && req.method === 'GET') {
    const trackPath = path.join(PUBLIC_DIR, 'assets', 'event_track.wav');
    if (!fs.existsSync(trackPath)) {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      return res.end('Event track audio file not found');
    }

    const stat = fs.statSync(trackPath);
    res.writeHead(200, {
      'Content-Type': 'audio/wav',
      'Content-Length': stat.size,
      'Connection': 'close',
      'Access-Control-Allow-Origin': '*'
    });
    const stream = fs.createReadStream(trackPath);
    stream.pipe(res);
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
  console.log(`📡 ESP32 Chatbot & Telemetry Endpoints:`);
  console.log(`   - POST /transcribe (Deepgram STT: ${config.DEEPGRAM_STT_MODEL})`);
  console.log(`   - POST /ask        (Gemini AI:     ${config.GEMINI_MODEL})`);
  console.log(`   - POST /tts        (Deepgram TTS: ${config.DEEPGRAM_TTS_MODEL})`);
  console.log(`   - POST /api/telemetry (Syncs Pomodoro, To-Dos, and Live Weather)`);
  console.log(`🌤️  Live Weather API (Open-Meteo):`);
  console.log(`   - City: ${state.weather.city} (${state.weather.lat}, ${state.weather.lon})`);
  console.log(`   - GET  /api/weather   (Live Open-Meteo weather JSON)`);
  console.log(`🖥️  Web Dashboard Endpoints:`);
  console.log(`   - GET  /api/status`);
  console.log(`   - GET  /api/sensors`);
  console.log(`   - GET  /api/ui`);
  console.log(`   - POST /api/command`);
  console.log('====================================================');
});