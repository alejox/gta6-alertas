#!/usr/bin/env node
/* Renderiza la transición de escenas (escenas/index.html?escena=transicion)
   a WebM VP9 con fondo transparente. Uso: node video/render-transicion.js
   El punto de corte (pantalla tapada) está en 750 ms. */
const fs = require('fs'), path = require('path');
const { spawnSync } = require('child_process');
const { chromium } = require('playwright');
const ROOT = path.resolve(__dirname, '..');
const FPS = 60, DUR = 1600;
(async () => {
  const out = path.join(ROOT, 'descargas/escenas'); fs.mkdirSync(out, { recursive: true });
  const frames = fs.mkdtempSync(path.join(out, '.frames-'));
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1920, height: 1080 } });
  await p.goto('file://' + path.join(ROOT, 'escenas/index.html') + '?escena=transicion&loop=0', { waitUntil: 'networkidle' });
  await p.evaluate(() => Promise.all([...document.images].map(i => i.decode().catch(() => {}))));
  const n = Math.ceil(DUR / 1000 * FPS) + 2;
  for (let f = 0; f < n; f++) {
    await p.evaluate(t => document.getAnimations().forEach(a => { a.pause(); a.currentTime = t; }), f * 1000 / FPS);
    await p.screenshot({ path: path.join(frames, String(f).padStart(4, '0') + '.png'), omitBackground: true });
  }
  await b.close();
  const r = spawnSync('ffmpeg', ['-hide_banner', '-loglevel', 'error', '-y', '-framerate', String(FPS), '-i', path.join(frames, '%04d.png'),
    '-c:v', 'libvpx-vp9', '-pix_fmt', 'yuva420p', '-b:v', '0', '-crf', '30', '-row-mt', '1', '-auto-alt-ref', '0', path.join(out, 'transicion.webm')], { stdio: 'inherit' });
  fs.rmSync(frames, { recursive: true, force: true });
  process.exit(r.status);
})();
