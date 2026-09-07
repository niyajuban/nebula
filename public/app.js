/**
 * Nebula CyberDeck Companion Web Dashboard
 * Console-Style Interface with Deep Space & Stars Background
 */

// Ordered Modes list
const modesOrder = ['music', 'pomodoro', 'todo', 'game', 'weather', 'chatbot'];

const modeMeta = {
  music: { num: 'MODE 1/6', title: 'Music Control' },
  pomodoro: { num: 'MODE 2/6', title: 'Pomodoro Timer' },
  todo: { num: 'MODE 3/6', title: 'To do List' },
  game: { num: 'MODE 4/6', title: 'Game Mode' },
  weather: { num: 'MODE 5/6', title: 'Weather' },
  chatbot: { num: 'MODE 6/6', title: 'AI chatbot' }
};

// Global State
const state = {
  currentModeIndex: 2, // Default to To do List (Index 2)
  pomo: {
    mode: 'pomodoro',
    durations: {
      pomodoro: 25 * 60,
      shortBreak: 5 * 60,
      longBreak: 15 * 60
    },
    secondsLeft: 25 * 60,
    isRunning: false,
    intervalId: null,
    round: 1
  },
  music: {
    isPlaying: true
  },
  todos: []
};

/* ====================================================
   0. DEEP SPACE & STARS BACKGROUND CANVAS
==================================================== */
function initSpaceCanvas() {
  const canvas = document.getElementById('space-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');

  let width = 0;
  let height = 0;
  let dpr = window.devicePixelRatio || 1;

  function resize() {
    width = window.innerWidth;
    height = window.innerHeight;
    canvas.width = width * dpr;
    canvas.height = height * dpr;
    ctx.scale(dpr, dpr);
  }
  window.addEventListener('resize', resize);
  resize();

  // Create stars
  const STAR_COUNT = 150;
  const stars = [];

  const starColors = [
    'rgba(255, 255, 255, ',
    'rgba(216, 180, 254, ', // Lilac
    'rgba(196, 181, 253, ', // Light purple
    'rgba(233, 213, 255, ', // Pastel lilac
    'rgba(244, 244, 245, '  // Pure white/silver
  ];

  for (let i = 0; i < STAR_COUNT; i++) {
    stars.push({
      x: Math.random() * width,
      y: Math.random() * height,
      size: Math.random() * 1.6 + 0.5,
      colorPrefix: starColors[Math.floor(Math.random() * starColors.length)],
      baseAlpha: Math.random() * 0.65 + 0.25,
      twinkleSpeed: Math.random() * 0.04 + 0.01,
      phase: Math.random() * Math.PI * 2,
      vx: (Math.random() - 0.5) * 0.08,
      vy: (Math.random() - 0.5) * 0.08
    });
  }

  // Shooting stars (meteors)
  let shootingStar = null;
  let nextShootingStarTime = Date.now() + Math.random() * 4000 + 3000;

  function spawnShootingStar() {
    const angle = Math.PI / 4 + (Math.random() - 0.5) * 0.3; // ~45 degrees diagonal
    const speed = Math.random() * 7 + 9;
    shootingStar = {
      x: Math.random() * (width * 0.7),
      y: Math.random() * (height * 0.4),
      vx: Math.cos(angle) * speed,
      vy: Math.sin(angle) * speed,
      length: Math.random() * 70 + 50,
      opacity: 1,
      fadeRate: Math.random() * 0.015 + 0.018
    };
    nextShootingStarTime = Date.now() + Math.random() * 7000 + 5000;
  }

  let lastTime = performance.now();

  function animate(now) {
    if (document.hidden) {
      requestAnimationFrame(animate);
      return;
    }

    ctx.clearRect(0, 0, width, height);

    // Draw twinkling stars
    for (let i = 0; i < stars.length; i++) {
      const s = stars[i];

      // Twinkle calculation
      s.phase += s.twinkleSpeed;
      const alpha = s.baseAlpha * (0.55 + 0.45 * Math.sin(s.phase));

      // Slow drift
      s.x += s.vx;
      s.y += s.vy;
      if (s.x < 0) s.x = width;
      if (s.x > width) s.x = 0;
      if (s.y < 0) s.y = height;
      if (s.y > height) s.y = 0;

      ctx.beginPath();
      ctx.arc(s.x, s.y, s.size, 0, Math.PI * 2);
      ctx.fillStyle = s.colorPrefix + alpha.toFixed(3) + ')';
      ctx.fill();

      // Soft glow halo on bigger stars
      if (s.size > 1.3) {
        ctx.beginPath();
        ctx.arc(s.x, s.y, s.size * 2.4, 0, Math.PI * 2);
        ctx.fillStyle = s.colorPrefix + (alpha * 0.25).toFixed(3) + ')';
        ctx.fill();
      }
    }

    // Shooting star update & render
    if (Date.now() > nextShootingStarTime && !shootingStar) {
      spawnShootingStar();
    }

    if (shootingStar) {
      const sx = shootingStar.x;
      const sy = shootingStar.y;
      const tailX = sx - (shootingStar.vx / 10) * shootingStar.length;
      const tailY = sy - (shootingStar.vy / 10) * shootingStar.length;

      const grad = ctx.createLinearGradient(tailX, tailY, sx, sy);
      grad.addColorStop(0, 'rgba(216, 180, 254, 0)');
      grad.addColorStop(0.7, `rgba(216, 180, 254, ${(shootingStar.opacity * 0.6).toFixed(3)})`);
      grad.addColorStop(1, `rgba(255, 255, 255, ${shootingStar.opacity.toFixed(3)})`);

      ctx.beginPath();
      ctx.moveTo(tailX, tailY);
      ctx.lineTo(sx, sy);
      ctx.strokeStyle = grad;
      ctx.lineWidth = 1.8;
      ctx.stroke();

      shootingStar.x += shootingStar.vx;
      shootingStar.y += shootingStar.vy;
      shootingStar.opacity -= shootingStar.fadeRate;

      if (shootingStar.opacity <= 0 || shootingStar.x > width + 100 || shootingStar.y > height + 100) {
        shootingStar = null;
      }
    }

    requestAnimationFrame(animate);
  }

  requestAnimationFrame(animate);
}

// --- Fun Audio Feedback (Web Audio API) ---
function playBlip(freq = 440, type = 'sine', duration = 0.08) {
  try {
    const ctx = new (window.AudioContext || window.webkitAudioContext)();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = type;
    osc.frequency.setValueAtTime(freq, ctx.currentTime);
    gain.gain.setValueAtTime(0.12, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + duration);
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start();
    osc.stop(ctx.currentTime + duration);
  } catch (e) {}
}

/* ====================================================
   1. NINTENDO-STYLE MODE SWAPPING
==================================================== */
function initNavigation() {
  const prevBtn = document.getElementById('prev-mode-btn');
  const nextBtn = document.getElementById('next-mode-btn');

  if (prevBtn) {
    prevBtn.addEventListener('click', () => {
      playBlip(520, 'triangle', 0.07);
      swapMode(-1);
    });
  }

  if (nextBtn) {
    nextBtn.addEventListener('click', () => {
      playBlip(620, 'triangle', 0.07);
      swapMode(1);
    });
  }

  // Keyboard navigation: Left & Right arrow keys
  window.addEventListener('keydown', (e) => {
    if (document.activeElement && document.activeElement.tagName === 'INPUT') return;

    if (e.key === 'ArrowLeft') {
      playBlip(520, 'triangle', 0.07);
      swapMode(-1);
    } else if (e.key === 'ArrowRight') {
      playBlip(620, 'triangle', 0.07);
      swapMode(1);
    }
  });

  // Clicking dots to jump directly
  const dots = document.querySelectorAll('.mode-dots .dot');
  dots.forEach(dot => {
    dot.addEventListener('click', () => {
      const idx = parseInt(dot.dataset.idx, 10);
      if (!isNaN(idx)) {
        playBlip(580, 'sine', 0.06);
        goToMode(idx);
      }
    });
  });

  // Initial render of active mode
  goToMode(state.currentModeIndex);
}

function swapMode(direction) {
  const total = modesOrder.length;
  state.currentModeIndex = (state.currentModeIndex + direction + total) % total;
  goToMode(state.currentModeIndex);
}

function goToMode(index) {
  state.currentModeIndex = index;
  const modeKey = modesOrder[index];
  const meta = modeMeta[modeKey];
  if (!meta) return;

  document.getElementById('screen-mode-num').textContent = meta.num;
  document.getElementById('screen-mode-title').textContent = meta.title;

  document.querySelectorAll('.mode-dots .dot').forEach((dot, idx) => {
    dot.classList.toggle('active', idx === index);
  });

  document.querySelectorAll('.mode-view').forEach(view => {
    view.classList.remove('active');
  });

  const activeView = document.getElementById(`view-${modeKey}`);
  if (activeView) {
    activeView.classList.add('active');
  }
}

/* ====================================================
   2. TO DO LIST (Two-Way Server Synced)
==================================================== */
async function fetchTodos() {
  try {
    const res = await fetch('/api/todos');
    if (!res.ok) return;
    state.todos = await res.json();
    renderTodos();
  } catch (err) {}
}

function initTodoList() {
  fetchTodos();

  const form = document.getElementById('todo-form');
  const input = document.getElementById('todo-input');

  form.addEventListener('submit', async (e) => {
    e.preventDefault();
    const text = input.value.trim();
    if (!text) return;

    playBlip(780, 'sine', 0.08);

    try {
      const res = await fetch('/api/todos', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ text })
      });
      if (res.ok) {
        const data = await res.json();
        state.todos = data.todos;
        renderTodos();
      }
    } catch (err) {}

    input.value = '';
  });
}

function renderTodos() {
  const list = document.getElementById('todo-list');
  if (!list) return;
  list.innerHTML = '';

  let remaining = 0;

  state.todos.forEach(item => {
    if (!item.completed) remaining++;

    const row = document.createElement('div');
    row.className = `todo-item ${item.completed ? 'completed' : ''}`;

    const left = document.createElement('div');
    left.className = 'todo-item-left';
    left.addEventListener('click', () => {
      playBlip(item.completed ? 420 : 660, 'sine', 0.06);
      toggleTodo(item.id);
    });

    const check = document.createElement('div');
    check.className = 'todo-checkbox';
    check.innerHTML = `
      <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="#09090e" stroke-width="3.5" stroke-linecap="round" stroke-linejoin="round">
        <polyline points="20 6 9 17 4 12"></polyline>
      </svg>
    `;

    const textSpan = document.createElement('span');
    textSpan.className = 'todo-text';
    textSpan.textContent = item.text;

    left.appendChild(check);
    left.appendChild(textSpan);

    const delBtn = document.createElement('button');
    delBtn.className = 'todo-del-btn';
    delBtn.innerHTML = '&times;';
    delBtn.title = 'Remove task';
    delBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      playBlip(320, 'sine', 0.05);
      deleteTodo(item.id);
    });

    row.appendChild(left);
    row.appendChild(delBtn);
    list.appendChild(row);
  });

  const countEl = document.getElementById('todo-count');
  if (countEl) countEl.textContent = remaining;
}

async function toggleTodo(id) {
  try {
    const res = await fetch('/api/todos/toggle', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id })
    });
    if (res.ok) {
      const data = await res.json();
      state.todos = data.todos;
      renderTodos();
    }
  } catch (err) {}
}

async function deleteTodo(id) {
  try {
    const res = await fetch('/api/todos/delete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id })
    });
    if (res.ok) {
      const data = await res.json();
      state.todos = data.todos;
      renderTodos();
    }
  } catch (err) {}
}

/* ====================================================
   3. POMODORO TIMER (Two-Way Server Synced)
==================================================== */
function initPomodoro() {
  const startBtn = document.getElementById('pomo-start-btn');
  const resetBtn = document.getElementById('pomo-reset-btn');
  const tabs = document.querySelectorAll('.pomo-tab');

  tabs.forEach(tab => {
    tab.addEventListener('click', async () => {
      playBlip(540, 'sine', 0.06);
      tabs.forEach(t => t.classList.remove('active'));
      tab.classList.add('active');

      const mode = tab.dataset.mode;
      const minutes = parseInt(tab.dataset.time, 10);
      
      if (mode === 'pomodoro') {
        await sendPomoAction('set', { workMinutes: minutes });
      } else {
        await sendPomoAction('set', { breakMinutes: minutes });
      }
    });
  });

  startBtn.addEventListener('click', async () => {
    playBlip(700, 'triangle', 0.09);
    const isRunning = state.pomo.serverState === 'RUNNING' || state.pomo.serverState === 'BREAK';
    if (isRunning) {
      await sendPomoAction('pause');
    } else {
      await sendPomoAction('start');
    }
  });

  resetBtn.addEventListener('click', async () => {
    playBlip(380, 'sine', 0.06);
    await sendPomoAction('reset');
  });

  fetchPomodoro();
}

async function sendPomoAction(action, extra = {}) {
  try {
    const res = await fetch('/api/pomodoro', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action, ...extra })
    });
    if (res.ok) {
      const data = await res.json();
      applyPomodoroState(data);
    }
  } catch (err) {}
}

async function fetchPomodoro() {
  try {
    const res = await fetch('/api/pomodoro');
    if (!res.ok) return;
    const data = await res.json();
    applyPomodoroState(data);
  } catch (err) {}
}

function applyPomodoroState(data) {
  if (!data) return;
  state.pomo.serverState = data.state; // 'IDLE', 'RUNNING', 'PAUSED', 'BREAK'
  state.pomo.mode = data.mode;
  state.pomo.secondsLeft = data.timeLeft !== undefined ? data.timeLeft : 1500;
  state.pomo.round = (data.sessionsCompleted || 0) + 1;

  // Update clock display
  const total = state.pomo.secondsLeft;
  const mins = Math.floor(total / 60);
  const secs = total % 60;
  const formatted = `${String(mins).padStart(2, '0')}:${String(secs).padStart(2, '0')}`;
  const display = document.getElementById('pomo-timer-display');
  if (display) display.textContent = formatted;

  // Update Start/Pause button
  const startBtn = document.getElementById('pomo-start-btn');
  if (startBtn) {
    if (data.state === 'RUNNING' || data.state === 'BREAK') {
      startBtn.textContent = 'PAUSE';
    } else if (data.state === 'PAUSED') {
      startBtn.textContent = 'RESUME';
    } else {
      startBtn.textContent = 'START';
    }
  }

  // Update label & active tab
  const label = document.getElementById('pomo-status-label');
  const tabs = document.querySelectorAll('.pomo-tab');
  if (data.mode === 'BREAK' || data.state === 'BREAK') {
    if (label) label.textContent = '☕ Break in progress!';
    tabs.forEach(t => t.classList.toggle('active', t.dataset.mode === 'shortBreak'));
  } else {
    if (label) {
      label.textContent = data.state === 'PAUSED' ? `#${state.pomo.round} Paused` : `#${state.pomo.round} Time to focus!`;
    }
    tabs.forEach(t => t.classList.toggle('active', t.dataset.mode === 'pomodoro'));
  }
}

function playChime() {
  try {
    const ctx = new (window.AudioContext || window.webkitAudioContext)();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(587.33, ctx.currentTime);
    osc.frequency.exponentialRampToValueAtTime(880, ctx.currentTime + 0.3);
    gain.gain.setValueAtTime(0.3, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 0.8);
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start();
    osc.stop(ctx.currentTime + 0.8);
  } catch (e) {}
}

/* ====================================================
   4. MUSIC CONTROL (Static)
==================================================== */
function initMusic() {
  const playBtn = document.getElementById('music-play-btn');
  const iconPlay = document.getElementById('icon-play');
  const iconPause = document.getElementById('icon-pause');
  const visualizer = document.getElementById('music-visualizer');
  const playingSub = document.getElementById('music-playing-sub');

  state.music.isPlaying = true;
  visualizer.classList.remove('paused');
  iconPlay.style.display = 'none';
  iconPause.style.display = 'block';

  playBtn.addEventListener('click', () => {
    playBlip(600, 'sine', 0.08);
    state.music.isPlaying = !state.music.isPlaying;

    if (state.music.isPlaying) {
      iconPlay.style.display = 'none';
      iconPause.style.display = 'block';
      visualizer.classList.remove('paused');
      playingSub.textContent = 'Music Playing';
    } else {
      iconPlay.style.display = 'block';
      iconPause.style.display = 'none';
      visualizer.classList.add('paused');
      playingSub.textContent = 'Audio Standby / Paused';
    }

    sendCommand('toggleBluetoothPlayback', {});
  });
}

/* ====================================================
   5. BACKEND TELEMETRY
==================================================== */
async function sendCommand(command, params) {
  try {
    const res = await fetch('/api/command', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command, params })
    });
    if (res.ok) {
      const data = await res.json();
      console.log('Command response:', data);
    }
  } catch (err) {}
}

async function fetchSensors() {
  try {
    const res = await fetch('/api/sensors');
    if (!res.ok) return;
    const data = await res.json();
    if (data.temperatureC !== undefined) {
      const tempEl = document.getElementById('weather-temp');
      if (tempEl) tempEl.textContent = Number(data.temperatureC).toFixed(1);
    }
    if (data.pressureHpa !== undefined) {
      const pressEl = document.getElementById('weather-pressure');
      if (pressEl) pressEl.textContent = Number(data.pressureHpa).toFixed(1);
    }
    const dotEl = document.getElementById('weather-sensor-dot');
    const textEl = document.getElementById('weather-sensor-status-text');
    if (dotEl && textEl) {
      if (data.isLiveHardware) {
        dotEl.className = 'status-dot green';
        textEl.textContent = 'BMP180 Sensor: Live Hardware Sync (ESP32 Connected)';
      } else {
        dotEl.className = 'status-dot yellow';
        textEl.textContent = 'BMP180 Sensor: Standby (Awaiting ESP32 Telemetry)';
      }
    }
  } catch (err) {}
}

async function fetchUIState() {
  try {
    const res = await fetch('/api/ui');
    if (!res.ok) return;
    const data = await res.json();

    if (data.uiState) {
      document.getElementById('chat-state-badge').textContent = data.uiState;
    }
    if (data.lastTranscript) {
      document.getElementById('chat-transcript').textContent = `"${data.lastTranscript}"`;
    }
    if (data.lastReply) {
      document.getElementById('chat-reply').textContent = `"${data.lastReply}"`;
    }
  } catch (err) {}
}

async function fetchStatus() {
  try {
    const res = await fetch('/api/status');
    if (!res.ok) return;
    const data = await res.json();

    // Game Mode Active Detection
    const gameHeadline = document.getElementById('game-headline');
    const gameSubtext = document.getElementById('game-subtext');
    const gameDot = document.getElementById('game-status-dot');
    const gameStatusText = document.getElementById('game-status-text');

    if (gameHeadline && gameDot) {
      if (data.gameActive) {
        gameHeadline.textContent = 'Game Mode Active';
        if (gameSubtext) gameSubtext.textContent = 'Micro Racer running on CyberDeck OLED • Controls engaged';
        gameDot.className = 'status-dot green';
        if (gameStatusText) gameStatusText.textContent = '🟢 Active • Direct hardware link engaged';
      } else {
        gameHeadline.textContent = 'Game Mode Inactive';
        const scr = data.screen && data.screen.current ? data.screen.current : 'Standby';
        if (gameSubtext) gameSubtext.textContent = `Hardware controller is on ${scr} • Open "Game" on CyberDeck OLED`;
        gameDot.className = 'status-dot orange';
        if (gameStatusText) gameStatusText.textContent = '⚪ Standby • Waiting for CyberDeck console';
      }
    }

    // Pomodoro Sync
    if (data.pomodoro) {
      applyPomodoroState(data.pomodoro);
    }
  } catch (err) {}
}

/* ====================================================
   6. INITIALIZATION
==================================================== */
document.addEventListener('DOMContentLoaded', () => {
  initSpaceCanvas();
  initNavigation();
  initTodoList();
  initPomodoro();
  initMusic();

  fetchSensors();
  fetchUIState();
  fetchStatus();

  setInterval(fetchSensors, 4000);
  setInterval(fetchUIState, 3000);
  setInterval(fetchStatus, 1500); // Fast 1.5s sync for Pomodoro timer & Game state
  setInterval(fetchTodos, 3500);  // 3.5s sync for To-Do list
});
