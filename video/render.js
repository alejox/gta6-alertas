#!/usr/bin/env node
/* =========================================================
   Renderiza las alertas a video con fondo transparente (alfa)
   y su sonido MP3.

   Uso:
     node video/render.js                  # las 9 alertas
     node video/render.js follow raid      # solo algunas
   Opciones (variables de entorno):
     NOMBRE="Usuario"   nombre que aparece en la placa
     SIN_NOMBRE=1       oculta la placa del nombre y el mensaje
     FPS=60  W=1920  H=1080  HOLD=6  (segundos en pantalla)
     FORMATOS=webm      (webm = VP9 alfa + Opus; añade ,mov para ProRes 4444 alfa + AAC,
                        para Premiere/Final Cut/DaVinci: pesa cientos de MB)
     SALIDA=video/salida

   Necesita: Node 18+, playwright (con Chromium) y ffmpeg con libvpx-vp9/prores_ks.
   ========================================================= */
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '..');
const FPS = +process.env.FPS || 60;
const W = +process.env.W || 1920;
const H = +process.env.H || 1080;
const HOLD = +process.env.HOLD || 6;
const NOMBRE = process.env.NOMBRE || 'Usuario';
const SIN_NOMBRE = !!process.env.SIN_NOMBRE;
const FORMATOS = (process.env.FORMATOS || 'webm').split(',').map(s => s.trim()).filter(Boolean);
const SALIDA = path.resolve(process.env.SALIDA || path.join(__dirname, 'salida'));

// Evento → archivo de salida, sonido y datos de ejemplo
const ALERTAS = {
  follow: { archivo: 'seguidor',   sonido: 'seguidor.mp3' },
  sub:    { archivo: 'suscriptor', sonido: 'suscriptor.mp3', amount: 1 },
  resub:  { archivo: 'resub',      sonido: 'resub.mp3',      amount: 12 },
  gift:   { archivo: 'regalos',    sonido: 'regalos.mp3',    amount: 5 },
  tip:    { archivo: 'donacion',   sonido: 'donacion.mp3',   amount: 10 },
  raid:   { archivo: 'raid',       sonido: 'raid.mp3',       amount: 50 },
  kicks:  { archivo: 'kicks',      sonido: 'kicks.mp3',      amount: 100 },
  cheer:  { archivo: 'bits',       sonido: 'bits.mp3',       amount: 500 },
  host:   { archivo: 'host',       sonido: 'host.mp3',       amount: 25 },
};

/* Reloj virtual: setTimeout, requestAnimationFrame, performance.now y
   las Web Animations avanzan solo cuando el script lo pide, así cada
   fotograma sale exacto aunque la captura tarde. */
const RELOJ = `(() => {
  const realTimeout = window.setTimeout.bind(window);
  let vt = 0, nid = 1;
  let timers = [];
  let rafs = new Map();
  const born = new WeakMap();
  window.setTimeout = (fn, ms, ...a) => { const id = nid++; timers.push({ id, t: vt + (+ms || 0), fn, a }); return id; };
  window.clearTimeout = id => { timers = timers.filter(x => x.id !== id); };
  window.setInterval = () => 0; window.clearInterval = () => {};
  window.requestAnimationFrame = fn => { const id = nid++; rafs.set(id, fn); return id; };
  window.cancelAnimationFrame = id => { rafs.delete(id); };
  performance.now = () => vt;
  const origAnimate = Element.prototype.animate;
  Element.prototype.animate = function (...args) { const a = origAnimate.apply(this, args); born.set(a, vt); a.pause(); a.currentTime = 0; return a; };
  function sync() {
    for (const a of document.getAnimations()) {
      if (!born.has(a)) born.set(a, vt);           // animaciones CSS (brillos, rayos)
      if (a.playState !== 'paused') a.pause();
      a.currentTime = vt - born.get(a);
    }
  }
  const flush = () => new Promise(r => realTimeout(r, 0));
  window.__vt = () => vt;
  window.__advance = async ms => {
    const target = vt + ms;
    for (;;) {
      timers.sort((x, y) => x.t - y.t || x.id - y.id);
      const tm = timers[0];
      if (!tm || tm.t > target) break;
      timers.shift();
      vt = Math.max(vt, tm.t);
      try { tm.fn(...tm.a); } catch (e) { console.error(e); }
      await flush(); sync();
    }
    vt = target;
    const list = [...rafs.values()]; rafs = new Map();
    for (const fn of list) { try { fn(vt); } catch (e) { console.error(e); } }
    sync();
    await flush();
  };
  window.__sync = sync;
})();`;

function paginaWidget() {
  const html = fs.readFileSync(path.join(ROOT, 'streamlabs/HTML.html'), 'utf8');
  const css = fs.readFileSync(path.join(ROOT, 'streamlabs/CSS.css'), 'utf8');
  const js = fs.readFileSync(path.join(ROOT, 'streamlabs/JS.js'), 'utf8');
  const extra = SIN_NOMBRE ? '.plate,.msg{display:none!important}' : '';
  return `<!doctype html><html><head><meta charset="utf-8"><style>${css}\n${extra}</style></head>
<body>${html}<script>${js}</script></body></html>`;
}

function ffmpeg(args) {
  return new Promise((res, rej) => {
    const p = spawn('ffmpeg', ['-hide_banner', '-loglevel', 'error', '-y', ...args], { stdio: ['ignore', 'inherit', 'inherit'] });
    p.on('exit', c => c === 0 ? res() : rej(new Error('ffmpeg salió con ' + c)));
  });
}

async function renderAlerta(browser, tipo) {
  const cfg = ALERTAS[tipo];
  const frames = fs.mkdtempSync(path.join(SALIDA, `.frames-${cfg.archivo}-`));
  const ctx = await browser.newContext({ viewport: { width: W, height: H }, deviceScaleFactor: 1 });
  const page = await ctx.newPage();
  page.on('pageerror', e => console.error(`[${tipo}]`, e.message));

  // Stickers y sonidos desde la copia local del repo en vez del CDN
  await page.route(/cdn\.jsdelivr\.net\/gh\/alejox\/gta6-alertas@[^/]+\/(.+)$/, (route, req) => {
    const rel = req.url().replace(/^.*gta6-alertas@[^/]+\//, '');
    const file = path.join(ROOT, rel);
    if (fs.existsSync(file)) route.fulfill({ path: file }); else route.continue();
  });
  // El widget usa <textarea> con {gtaHold}; aquí fijamos el tiempo en pantalla
  const widget = paginaWidget().replace('{gtaHold}', String(HOLD));
  await page.addInitScript(RELOJ);
  await page.route('https://local.alertas/', r => r.fulfill({ contentType: 'text/html', body: widget }));
  await page.goto('https://local.alertas/', { waitUntil: 'networkidle' });
  await page.evaluate(async () => {
    await Promise.all(['118px Bungee', '70px Pacifico', 'italic 900 54px Rubik', 'italic 800 30px Rubik'].map(f => document.fonts.load(f)));
    await document.fonts.ready;
    await Promise.all([...document.images].map(i => i.decode().catch(() => {})));
  });
  // Silenciar el audio del navegador (el sonido se añade luego con ffmpeg)
  await page.evaluate(() => { HTMLMediaElement.prototype.play = function () { return Promise.resolve(); }; });

  await page.evaluate(({ tipo, nombre, amount }) => {
    window.playAlert({ type: tipo, name: nombre, amount, message: '' });
  }, { tipo, nombre: NOMBRE, amount: cfg.amount || 0 });
  await page.waitForFunction(() => document.getElementById('alert').classList.contains('on'));
  const t0 = await page.evaluate(() => window.__vt());
  await page.evaluate(() => window.__sync());

  // Entrada + tiempo en pantalla + salida (1050 ms) + un respiro para las partículas
  const dur = Math.max(HOLD + 1.05 + 0.35, 0);
  const total = Math.ceil(dur * FPS);
  const paso = 1000 / FPS;
  for (let f = 0; f < total; f++) {
    if (f > 0) await page.evaluate(ms => window.__advance(ms), paso);
    await page.screenshot({ path: path.join(frames, String(f).padStart(5, '0') + '.png'), omitBackground: true });
  }
  const t1 = await page.evaluate(() => window.__vt());
  await ctx.close();

  // Duración final: la más larga entre la animación y el sonido
  const sonido = path.join(ROOT, 'sonidos', cfg.sonido);
  const base = path.join(SALIDA, cfg.archivo);
  const entrada = ['-framerate', String(FPS), '-i', path.join(frames, '%05d.png'), '-i', sonido];
  const pad = ['-filter_complex', '[0:v]format=rgba,tpad=stop_mode=add:stop=-1:color=0x00000000[v];[1:a]apad[a]', '-map', '[v]', '-map', '[a]'];
  const audioDur = await duracion(sonido);
  const largo = Math.max(total / FPS, audioDur).toFixed(3);
  if (FORMATOS.includes('webm')) {
    await ffmpeg([...entrada, ...pad, '-t', largo,
      '-c:v', 'libvpx-vp9', '-pix_fmt', 'yuva420p', '-b:v', '0', '-crf', '28', '-row-mt', '1', '-deadline', 'good', '-cpu-used', '2',
      '-auto-alt-ref', '0', '-metadata:s:v:0', 'alpha_mode=1',
      '-c:a', 'libopus', '-b:a', '160k', base + '.webm']);
  }
  if (FORMATOS.includes('mov')) {
    await ffmpeg([...entrada, ...pad, '-t', largo,
      '-c:v', 'prores_ks', '-profile:v', '4444', '-pix_fmt', 'yuva444p10le', '-alpha_bits', '16', '-vendor', 'apl0',
      '-c:a', 'aac', '-b:a', '256k', base + '.mov']);
  }
  fs.rmSync(frames, { recursive: true, force: true });
  console.log(`✓ ${cfg.archivo}  (${total} fotogramas, ${((t1 - t0) / 1000).toFixed(2)} s de animación, ${largo} s de video)`);
}

function duracion(file) {
  return new Promise((res, rej) => {
    const p = spawn('ffprobe', ['-v', 'error', '-show_entries', 'format=duration', '-of', 'csv=p=0', file]);
    let out = ''; p.stdout.on('data', d => out += d);
    p.on('exit', c => c === 0 ? res(parseFloat(out)) : rej(new Error('ffprobe falló')));
  });
}

(async () => {
  const pedidos = process.argv.slice(2);
  const tipos = pedidos.length ? pedidos : Object.keys(ALERTAS);
  for (const t of tipos) if (!ALERTAS[t]) { console.error('Alerta desconocida: ' + t + ' (usa: ' + Object.keys(ALERTAS).join(', ') + ')'); process.exit(1); }
  fs.mkdirSync(SALIDA, { recursive: true });
  const browser = await chromium.launch();
  const PAR = +process.env.PARALELO || 2;
  const cola = [...tipos];
  await Promise.all(Array.from({ length: Math.min(PAR, cola.length) }, async () => {
    while (cola.length) await renderAlerta(browser, cola.shift());
  }));
  await browser.close();
})().catch(e => { console.error(e); process.exit(1); });
