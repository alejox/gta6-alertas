#!/usr/bin/env node
/* =========================================================
   Crea descargas/GTA6-ViceCity.overlay para Streamlabs Desktop
   (Ajustes > Colección de escenas > Importar archivo de overlay).

   Un .overlay es un zip con config.json + archivos. El formato sale del
   código de Streamlabs Desktop (app/services/scene-collections/nodes/overlays).
   Como ese formato no admite fuentes de navegador, las pantallas completas
   van como video en bucle y los overlays como PNG; el nombre del canal y las
   etiquetas son fuentes de texto editables.

   Uso: node video/crear-overlay.js        (SOLO_CONFIG=1 reutiliza los videos)
   ========================================================= */
const fs = require('fs'), path = require('path'), crypto = require('crypto');
const { spawnSync } = require('child_process');
const { chromium } = require('playwright');
const ROOT = path.resolve(__dirname, '..');
const TMP = path.join(ROOT, 'video/salida/overlay');
const OUT = path.join(ROOT, 'descargas/GTA6-ViceCity.overlay');
const PAGE = 'file://' + path.join(ROOT, 'escenas/index.html');
const FPS = 30, LOOP = 10;

const FULL = [['intro', 'Intro'], ['pausa', 'Pausa'], ['vuelvo', 'Ya vuelvo'], ['fin', 'Salida']];

async function render() {
  fs.mkdirSync(TMP, { recursive: true });
  const b = await chromium.launch();
  for (const [esc] of FULL) {
    const frames = fs.mkdtempSync(path.join(TMP, '.frames-'));
    const p = await b.newPage({ viewport: { width: 1920, height: 1080 } });
    await p.goto(`${PAGE}?escena=${esc}&canal=&bucle=1`, { waitUntil: 'networkidle' });
    await p.evaluate(() => Promise.all([document.fonts.ready, ...[...document.images].map(i => i.decode().catch(() => {}))]));
    for (let f = 0; f < FPS * LOOP; f++) {
      // 20 s: las animaciones de entrada ya terminaron; todas las demás repiten cada 10 s
      await p.evaluate(t => document.getAnimations().forEach(a => { a.pause(); a.currentTime = t; }), 20000 + f * 1000 / FPS);
      await p.screenshot({ path: path.join(frames, String(f).padStart(4, '0') + '.jpg'), type: 'jpeg', quality: 96 });
    }
    await p.close();
    const r = spawnSync('ffmpeg', ['-hide_banner', '-loglevel', 'error', '-y', '-framerate', String(FPS), '-i', path.join(frames, '%04d.jpg'),
      '-c:v', 'libvpx-vp9', '-pix_fmt', 'yuv420p', '-b:v', '0', '-crf', '32', '-row-mt', '1', '-g', String(FPS * LOOP), path.join(TMP, esc + '.webm')], { stdio: 'inherit' });
    fs.rmSync(frames, { recursive: true, force: true });
    if (r.status) process.exit(r.status);
    console.log('✓ bucle', esc);
  }
  const p = await b.newPage({ viewport: { width: 1920, height: 1080 } });
  for (const esc of ['juego', 'hablar']) {
    await p.goto(`${PAGE}?escena=${esc}&canal=&l1=&l2=&l3=`, { waitUntil: 'networkidle' });
    await p.waitForTimeout(3000);
    await p.screenshot({ path: path.join(TMP, esc + '.png'), omitBackground: true });
    console.log('✓ imagen', esc);
  }
  await b.close();
}

/* ---------- config.json ---------- */
const W = 1920, H = 1080;
const id = () => crypto.randomUUID();
const node = (nodeType, schemaVersion, data) => ({ schemaVersion, nodeType, ...data });
const item = (name, x, y, content, extra = {}) => ({
  id: id(), name, sceneNodeType: 'item', x: x / W, y: y / H, scaleX: 1 / W, scaleY: 1 / H,
  crop: { top: 0, left: 0, right: 0, bottom: 0 }, rotation: 0, visible: true, locked: false, content, ...extra,
});
const WHITE = 0xFFFFFFFF, INK = 0xFF2A0516;   // enteros ABGR, como los guarda OBS
const textSettings = (text, size) => ({
  text, color: WHITE, opacity: 100, gradient: false, gradient_color: WHITE,
  font: { face: 'Arial', flags: 3, size, style: 'Bold Italic' },
  outline: true, outline_color: INK, outline_size: 3, outline_opacity: 100,
});
const text = (name, x, y, txt, size) => item(name, x, y, node('TextNode', 1, { settings: textSettings(txt, size) }));
const label = (name, x, y, stat) => item(name, x, y, node('StreamlabelNode', 1, { labelType: stat, textSource: node('TextNode', 1, { settings: textSettings('', 26) }) }));
const video = (name, file) => item(name, 0, 0, node('VideoNode', 1, { filename: file, settings: { is_local_file: true, looping: true, restart_on_activate: false, clear_on_media_end: false, close_when_inactive: false } }), { mixerHidden: true });
const image = (name, file) => item(name, 0, 0, node('ImageNode', 1, { filename: file }));
const webcam = (x, y, w, h) => item('Cámara', x, y, node('WebcamNode', 1, { width: w / W, height: h / H }));
const widget = (name, type, x, y, w, h) => item(name, x, y, node('WidgetNode', 1, { type, settings: { width: w, height: h } }));
const ALERTAS = () => widget('Alertas GTA 6 (Widget personalizado)', 24, 0, 0, W, H);   // WidgetType.CustomWidget
const LABELS = (xs, y) => [label('Último seguidor', xs[0] + 85, y + 27, 'most_recent_follower'), label('Último sub', xs[1] + 85, y + 27, 'most_recent_subscriber'), label('Top donación', xs[2] + 85, y + 27, 'all_time_top_donator')];
// Los elementos van de abajo hacia arriba (el último queda encima)
const scene = (name, items) => ({ name, sceneId: id(), slots: node('SlotsNode', 1, { items }) });
const full = (esc, name) => scene(name, [video('Fondo ' + name, esc + '.webm'), text('Nombre del canal', 114, 978, '@TUCANAL', 32)]);

function config() {
  const scenes = [
    full('intro', 'Intro'),
    scene('Streaming', [
      item('Juego', 0, 0, node('GameCaptureNode', 2, { width: W, height: H })),
      webcam(65, 691, 462, 252),
      image('Overlay streaming', 'juego.png'),
      text('Nombre del canal', 132, 952, '@TUCANAL', 34),
      ...LABELS([700, 1100, 1500], 984),
      ALERTAS(),
    ]),
    scene('Hablar', [
      webcam(75, 161, 1194, 662),
      image('Overlay hablar', 'hablar.png'),
      text('Nombre del canal', 158, 830, '@TUCANAL', 34),
      ...LABELS([534, 934, 1334], 954),
      widget('Chat', 6, 1356, 240, 480, 580),                                              // WidgetType.ChatBox
      ALERTAS(),
    ]),
    full('pausa', 'Pausa'), full('vuelvo', 'Ya vuelvo'), full('fin', 'Salida'),
  ];
  return node('RootNode', 2, {
    scenes: node('ScenesNode', 2, { items: scenes }),
    transition: node('TransitionNode', 1, { type: 'obs_stinger_transition', duration: 1600, settings: { path: 'transicion.webm', tp_type: 0, transition_point: 750, audio_monitoring: 0, audio_fade_style: 0 } }),
  });
}

(async () => {
  if (!process.env.SOLO_CONFIG) await render();
  const dir = path.join(TMP, 'pack'); fs.rmSync(dir, { recursive: true, force: true }); fs.mkdirSync(dir, { recursive: true });
  for (const f of ['intro.webm', 'pausa.webm', 'vuelvo.webm', 'fin.webm', 'juego.png', 'hablar.png']) fs.copyFileSync(path.join(TMP, f), path.join(dir, f));
  fs.copyFileSync(path.join(ROOT, 'descargas/escenas/transicion.webm'), path.join(dir, 'transicion.webm'));
  fs.writeFileSync(path.join(dir, 'config.json'), JSON.stringify(config(), null, 2));
  fs.rmSync(OUT, { force: true });
  const r = spawnSync('zip', ['-q', '-r', '-X', OUT, '.'], { cwd: dir, stdio: 'inherit' });
  console.log(r.status ? 'zip falló' : '✓ ' + path.relative(ROOT, OUT) + ' ' + (fs.statSync(OUT).size / 1e6).toFixed(1) + ' MB');
  process.exit(r.status);
})();
