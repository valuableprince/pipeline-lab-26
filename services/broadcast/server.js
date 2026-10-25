'use strict';

// Browser microphone -> authenticated WebSocket -> ffmpeg -> Icecast MP3 mount.
// No source password is sent to the browser. Run behind HTTPS reverse proxy.
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { spawn } = require('node:child_process');
const { WebSocketServer } = require('ws');

const origin = process.env.BROADCAST_PUBLIC_ORIGIN;
const secret = process.env.BROADCAST_ADMIN_TOKEN;
const sourcePassword = process.env.ICECAST_SOURCE_PASSWORD;
const icecastHost = process.env.ICECAST_HOST || '127.0.0.1';
const icecastPort = Number(process.env.ICECAST_PORT || 8000);
const mount = '/live.mp3';
const listenHost = process.env.LISTEN_HOST || '127.0.0.1';
const listenPort = Number(process.env.PORT || 3471);
const maxMs = 120_000;

if (!origin || !/^https:\/\/[^/]+$/.test(origin) ||
    !secret || Buffer.byteLength(secret) < 32 ||
    !sourcePassword || !Number.isInteger(icecastPort) ||
    icecastPort < 1 || icecastPort > 65535) {
  throw new Error('Configure HTTPS BROADCAST_PUBLIC_ORIGIN, strong BROADCAST_ADMIN_TOKEN (32+ bytes), ICECAST_SOURCE_PASSWORD and ICECAST_PORT');
}
if (!Number.isInteger(listenPort) || listenPort < 1 || listenPort > 65535)
  throw new Error('Invalid PORT');
if (!/^[a-zA-Z0-9.-]+$/.test(icecastHost))
  throw new Error('Invalid ICECAST_HOST');

const sessions = new Map(); // session -> expiry
const failedLogins = new Map(); // remoteAddress -> { count, reset }
let publisher = null; // only one ingest session at a time
const web = new WebSocketServer({ noServer: true, maxPayload: 128 * 1024 });

function publicFile(res, relative, type) {
  fs.readFile(path.join(__dirname, 'public', relative), (error, data) => {
    if (error) return respond(res, 500, 'Cannot load studio assets');
    res.writeHead(200, { 'Content-Type': type, 'Cache-Control': 'no-store' });
    res.end(data);
  });
}
function respond(res, code, text, headers = {}) {
  res.writeHead(code, { 'Content-Type': 'text/plain; charset=utf-8',
    'Cache-Control': 'no-store', ...headers });
  res.end(text);
}
function validOrigin(req) {
  return req.headers.origin === origin;
}
function authToken(req) {
  const cookie = req.headers.cookie || '';
  const match = cookie.match(/(?:^|;\s*)broadcast_session=([0-9a-f]{64})(?:;|$)/);
  if (!match) return false;
  const expiry = sessions.get(match[1]);
  if (!expiry) return false;
  if (expiry <= Date.now()) {
    sessions.delete(match[1]);
    return false;
  }
  return true;
}
function secureCompare(value) {
  const a = crypto.createHash('sha256').update(value).digest();
  const b = crypto.createHash('sha256').update(secret).digest();
  return crypto.timingSafeEqual(a, b);
}
function limitLogin(req) {
  const key = req.socket.remoteAddress || 'unknown';
  const now = Date.now();
  let attempt = failedLogins.get(key);
  if (!attempt || now > attempt.reset) attempt = { count: 0, reset: now + 60_000 };
  attempt.count++;
  failedLogins.set(key, attempt);
  return attempt.count <= 5;
}
function readJson(req) {
  return new Promise((resolve, reject) => {
    let total = 0;
    const chunks = [];
    req.on('data', chunk => {
      total += chunk.length;
      if (total > 2048) { reject(new Error('Too large')); req.destroy(); return; }
      chunks.push(chunk);
    });
    req.on('end', () => {
      try { resolve(JSON.parse(Buffer.concat(chunks).toString('utf8'))); }
      catch { reject(new Error('Bad JSON')); }
    });
    req.on('error', reject);
  });
}
function shutdownPublisher() {
  const active = publisher;
  if (!active) return;
  publisher = null;
  clearTimeout(active.timer);
  if (active.ffmpeg) {
    active.ffmpeg.stdin.destroy();
    active.ffmpeg.kill('SIGTERM');
  }
  if (active.socket.readyState === active.socket.OPEN)
    active.socket.close(1000, 'Broadcast ended');
}
function startPublisher(socket) {
  if (publisher) { socket.close(1013, 'Busy'); return; }
  const target = 'icecast://source:' + encodeURIComponent(sourcePassword) +
    '@' + icecastHost + ':' + icecastPort + mount;
  const ffmpeg = spawn('ffmpeg', [
    '-hide_banner', '-loglevel', 'error', '-nostdin',
    '-f', 'webm', '-i', 'pipe:0', '-vn', '-ac', '1', '-ar', '44100',
    '-c:a', 'libmp3lame', '-b:a', '64k', '-content_type', 'audio/mpeg',
    '-f', 'mp3', target
  ], { stdio: ['pipe', 'ignore', 'pipe'], shell: false });
  const entry = { socket, ffmpeg, bytes: 0, timer: null };
  publisher = entry;
  entry.timer = setTimeout(() => shutdownPublisher(), maxMs);
  ffmpeg.on('error', error => {
    console.error('ffmpeg startup failed:', error.message);
    if (publisher === entry) shutdownPublisher();
  });
  ffmpeg.on('exit', code => {
    console.info('ffmpeg exited:', code);
    if (publisher === entry) shutdownPublisher();
  });
  ffmpeg.stderr.on('data', data => {
    // FFmpeg is unprivileged; never print its URL, which includes the Icecast password.
    const safe = String(data).replace(/icecast:\/\/\S+/ig, 'icecast://[REDACTED]');
    console.error(safe.slice(0, 400));
  });
  ffmpeg.stdin.on('error', () => {
    if (publisher === entry) shutdownPublisher();
  });
  socket.on('message', (data, isBinary) => {
    if (publisher !== entry || !isBinary) return;
    entry.bytes += data.length;
    if (entry.bytes > 48 * 1024 * 1024 || ffmpeg.stdin.destroyed) {
      shutdownPublisher();
      return;
    }
    // Bound the native pipe buffer; disconnect a publisher that cannot keep up.
    if (!ffmpeg.stdin.write(data) && ffmpeg.stdin.writableLength > 512 * 1024)
      shutdownPublisher();
  });
  socket.on('close', () => {
    if (publisher === entry) shutdownPublisher();
  });
  socket.on('error', () => {
    if (publisher === entry) shutdownPublisher();
  });
}

const server = http.createServer(async (req, res) => {
  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader('Referrer-Policy', 'no-referrer');
  res.setHeader('X-Frame-Options', 'DENY');
  res.setHeader('Content-Security-Policy',
    "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self' wss:; media-src 'self'; base-uri 'none'; form-action 'self'");
  const url = new URL(req.url, origin);
  if (req.method === 'GET' && url.pathname === '/')
    return publicFile(res, 'index.html', 'text/html; charset=utf-8');
  if (req.method === 'GET' && url.pathname === '/app.js')
    return publicFile(res, 'app.js', 'application/javascript; charset=utf-8');
  if (req.method === 'GET' && url.pathname === '/style.css')
    return publicFile(res, 'style.css', 'text/css; charset=utf-8');
  if (req.method === 'GET' && url.pathname === '/api/status') {
    if (!authToken(req)) return respond(res, 401, 'Unauthorized');
    return respond(res, 200, JSON.stringify({ live: !!publisher }),
      { 'Content-Type': 'application/json; charset=utf-8' });
  }
  if (req.method === 'POST' && !validOrigin(req))
    return respond(res, 403, 'Forbidden origin');
  if (req.method === 'POST' && url.pathname === '/api/login') {
    if (!limitLogin(req)) return respond(res, 429, 'Too many attempts');
    try {
      const data = await readJson(req);
      if (typeof data.token !== 'string' || !secureCompare(data.token))
        return respond(res, 401, 'Invalid token');
      const session = crypto.randomBytes(32).toString('hex');
      sessions.set(session, Date.now() + 15 * 60_000);
      return respond(res, 200, 'OK', {
        'Set-Cookie': 'broadcast_session=' + session +
          '; Path=/; Secure; HttpOnly; SameSite=Strict; Max-Age=900'
      });
    } catch {
      if (!res.headersSent) respond(res, 400, 'Invalid request');
    }
    return;
  }
  if (req.method === 'POST' && url.pathname === '/api/stop') {
    if (!authToken(req)) return respond(res, 401, 'Unauthorized');
    shutdownPublisher();
    return respond(res, 200, 'OK');
  }
  if (req.method === 'POST' && url.pathname === '/api/logout') {
    if (!authToken(req)) return respond(res, 401, 'Unauthorized');
    const match = (req.headers.cookie || '').match(/broadcast_session=([0-9a-f]{64})/);
    if (match) sessions.delete(match[1]);
    return respond(res, 200, 'OK', {
      'Set-Cookie': 'broadcast_session=; Path=/; Secure; HttpOnly; SameSite=Strict; Max-Age=0'
    });
  }
  return respond(res, 404, 'Not found');
});
server.on('upgrade', (req, socket, head) => {
  if (req.url !== '/ingest' || !validOrigin(req) || !authToken(req)) {
    socket.write('HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n');
    socket.destroy();
    return;
  }
  if (publisher) {
    socket.write('HTTP/1.1 409 Conflict\r\nConnection: close\r\n\r\n');
    socket.destroy();
    return;
  }
  web.handleUpgrade(req, socket, head, ws => startPublisher(ws));
});
server.listen(listenPort, listenHost, () =>
  console.log('Broadcast gateway listening on ' + listenHost + ':' + listenPort));
process.on('SIGTERM', () => {
  shutdownPublisher();
  server.close();
});
