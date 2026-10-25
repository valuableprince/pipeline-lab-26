'use strict';

const loginPanel = document.getElementById('login');
const studioPanel = document.getElementById('studio');
const loginForm = document.getElementById('login-form');
const status = document.getElementById('status');
const startButton = document.getElementById('start');
const stopButton = document.getElementById('stop');
const logoutButton = document.getElementById('logout');
let ws = null;
let recorder = null;
let microphone = null;
let timer = null;

function showStatus(message) {
  status.textContent = message;
}
function setActive(active) {
  startButton.disabled = active;
  stopButton.disabled = !active;
}
async function api(route, options) {
  const response = await fetch(route, {
    credentials: 'same-origin', cache: 'no-store', ...options
  });
  if (!response.ok) throw new Error(await response.text());
  return response;
}
async function refresh() {
  try {
    const response = await api('/api/status');
    const data = await response.json();
    loginPanel.hidden = true;
    studioPanel.hidden = false;
    showStatus(data.live ? 'Идёт передача звука другим оператором.' :
      'Студия готова. Микрофон выключен.');
  } catch {
    loginPanel.hidden = false;
    studioPanel.hidden = true;
    showStatus('Для работы требуется авторизация.');
  }
}
loginForm.addEventListener('submit', async event => {
  event.preventDefault();
  const token = document.getElementById('token').value;
  try {
    await api('/api/login', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ token })
    });
    document.getElementById('token').value = '';
    await refresh();
  } catch (error) {
    showStatus('Ошибка доступа: ' + error.message);
  }
});
async function stop() {
  if (timer) clearTimeout(timer);
  timer = null;
  if (recorder && recorder.state !== 'inactive') recorder.stop();
  recorder = null;
  if (ws) ws.close();
  ws = null;
  if (microphone) microphone.getTracks().forEach(track => track.stop());
  microphone = null;
  setActive(false);
}
startButton.addEventListener('click', async () => {
  if (recorder || ws) return;
  if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia ||
    typeof MediaRecorder === 'undefined' ||
    !MediaRecorder.isTypeSupported('audio/webm;codecs=opus')) {
    showStatus('Нужен HTTPS и браузер Chrome/Edge с поддержкой WebM/Opus.');
    return;
  }
  try {
    microphone = await navigator.mediaDevices.getUserMedia({
      audio: { echoCancellation: true, noiseSuppression: true, autoGainControl: true }
    });
    ws = new WebSocket((location.protocol === 'https:' ? 'wss://' : 'ws://') +
      location.host + '/ingest');
    ws.onopen = () => {
      recorder = new MediaRecorder(microphone, {
        mimeType: 'audio/webm;codecs=opus', audioBitsPerSecond: 64000
      });
      recorder.addEventListener('dataavailable', event => {
        if (event.data.size && ws && ws.readyState === WebSocket.OPEN) {
          // Source stream is assembled in chronological order by WebSocket.
          ws.send(event.data);
          if (ws.bufferedAmount > 1024 * 1024) {
            showStatus('Канал перегружен: передача остановлена.');
            stop();
          }
        }
      });
      recorder.start(400);
      setActive(true);
      showStatus('Микрофон подключён. Дайте потоку запуститься, затем /broadcast start в GTA.');
      timer = setTimeout(() => {
        stop();
        showStatus('Лимит 115 секунд. Завершите эфир командой /broadcast stop.');
      }, 115000);
    };
    ws.onerror = () => {
      showStatus('Не удалось подключиться к серверу вещания.');
      stop();
    };
    ws.onclose = () => {
      if (recorder) {
        stop();
        showStatus('Соединение с вещателем закрыто.');
      }
    };
  } catch (error) {
    await stop();
    showStatus('Доступ к микрофону отклонён или устройство недоступно: ' + error.message);
  }
});
stopButton.addEventListener('click', async () => {
  await stop();
  showStatus('Микрофон отключён. Завершите эфир в игре: /broadcast stop.');
});
logoutButton.addEventListener('click', async () => {
  await stop();
  try { await api('/api/logout', { method: 'POST' }); } catch { /* no active login */ }
  await refresh();
});
window.addEventListener('pagehide', () => { stop(); });
refresh();
