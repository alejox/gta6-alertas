#!/usr/bin/env node
/* Genera instalar-minimal.html: con el JWT de StreamElements crea un overlay por escena
   del Pack Minimal, con cada pieza como widget independiente.
   Uso: node minimal/crear-instalador.js   (antes: portada en minimal/portada.jpg) */
const fs = require('fs'), path = require('path');
const ROOT = path.resolve(__dirname, '..');
const src = fs.readFileSync(path.join(__dirname, 'index.html'), 'utf8');
const L = JSON.parse(/<script id="layout" type="application\/json">([\s\S]*?)<\/script>/.exec(src)[1]);
const css = "@import url('https://fonts.googleapis.com/css2?family=Rubik:ital,wght@0,500;0,700;1,800;1,900&display=block');\n" +
  /<style>([\s\S]*?)\/\* vista local \*\//.exec(src)[1];
const main = /(function minMain\(\)\{[\s\S]*?\n\})\n\n\/\* ---------- vista local/.exec(src)[1];
const piezasFn = /window\.minPiezas=(function\(L\)\{[\s\S]*?\n\});/.exec(src)[1];
const minPiezas = eval('(' + piezasFn + ')');
const js = `window.__MIN_WIDGET=1;
${main}
window.addEventListener('onWidgetLoad', function(ev){
  var d=ev.detail||{}, f=d.fieldData||{}, c={__sonidos:'${L.sonidos}', __logo:'${L.logo}', __editor:!!(d.overlay&&d.overlay.isEditorMode)};
  for(var k in f) if(/^m[A-Z]/.test(k)) c[k.slice(1).toLowerCase()]=f[k];   // mCanal -> canal
  window.__MIN=c; minMain();
});
`;

const GC = '⚙️ Canal', GE = '⚙️ Elemento';
const txt = (label, value, group = GE) => ({ type: 'text', label, value: value == null ? '' : value, group });
const num = (label, value, min, max) => ({ type: 'number', label, value, min, max, step: 1, group: GE });
const drop = (label, value, options) => ({ type: 'dropdown', label, value, options, group: GE });
const sino = (label, value) => drop(label, value, { si: 'Sí', no: 'No' });
const lado = v => drop('Alineación', v || 'izq', { izq: 'Izquierda', der: 'Derecha' });
function campos(k, c) {
  switch (k) {
    case 'fondo': return c.hueco ? { mHueco: sino('Hueco transparente para la cámara (posición de fábrica)', 'si') } : {};
    case 'titulo': return { mOver: txt('Texto pequeño de arriba', c.over), mTitulo: txt('Título', c.titulo), mSub: txt('Texto de abajo', c.sub), mLado: drop('Alineación', 'centro', { centro: 'Centrado', izq: 'Izquierda' }) };
    case 'logo': return { mLogo: txt('Imagen del logo (URL; vacío = logo de GTA VI del pack)', '') };
    case 'cuenta': return { mMin: num('Minutos', 5, 1, 120) };
    case 'canal': case 'nombre': return { mCanal: txt('Nombre del canal', '@TUCANAL', GC) };
    case 'texto': case 'envivo': return { mTexto: txt('Texto', c.texto || 'En vivo'), mLado: lado(c.lado) };
    case 'etiqueta': return { mLt: txt('Título', c.lt), mLv: txt('Texto (o deja — y pon encima una etiqueta de StreamElements)', c.lv) };
    case 'chat': return { mMax: num('Máximo de mensajes en pantalla', 8, 1, 30), mLado: drop('Los mensajes nuevos entran por', 'der', { der: 'La derecha', izq: 'La izquierda' }), mColor: drop('Color de los nombres', 'acento', { acento: 'Color del pack', usuario: 'Color de cada usuario' }) };
    case 'alerta': return { mSeg: num('Segundos en pantalla', 5, 2, 30), mSonido: sino('Sonido', 'si'), mVolumen: num('Volumen (0-100)', 50, 0, 100), mMoneda: txt('Símbolo de moneda', '$'),
      mMinTip: num('Donación mínima', 0, 0, 100000), mMinBits: num('Bits mínimos', 0, 0, 1000000), mMinKicks: num('KICKs mínimos', 0, 0, 1000000) };
    default: return {};
  }
}
const EVENTOS = { 'tip-latest': true, 'follower-latest': true, 'subscriber-latest': true, 'kicks-latest': true, 'raid-latest': true, 'host-latest': true, 'cheer-latest': true };
const piezas = minPiezas(L);
const scenes = Object.keys(L.escenas).map(key => {
  const s = L.escenas[key];
  return { name: 'Minimal · ' + s.nombre, desc: s.desc, widgets: piezas[key].map(([name, k, b, c], i) => {
    const fields = { mPieza: { type: 'hidden', label: '-', value: k }, mTema: { type: 'hidden', label: '-', value: s.tema }, mRetraso: { type: 'hidden', label: '-', value: c.retraso || '0' }, ...campos(k, c) };
    return { name, listeners: k === 'alerta' ? EVENTOS : {}, css: { 'z-index': i + 1, left: b[0], top: b[1], width: b[2] + 'px', height: b[3] + 'px', opacity: 1 },
      fields: JSON.stringify(fields), fieldData: Object.fromEntries(Object.entries(fields).map(([n, f]) => [n, f.value])) };
  }) };
});
const cover = 'data:image/jpeg;base64,' + fs.readFileSync(path.join(__dirname, 'portada.jpg')).toString('base64');
const PACK = { html: '<div id="p"></div>', css, js, scenes, cover };

/* La página reutiliza el estilo y el bloque de regalo del instalador de alertas */
const base = fs.readFileSync(path.join(ROOT, 'index.html'), 'utf8');
const style = /<style>([\s\S]*?)<\/style>/.exec(base)[1];
const gift = /(<div class="gift">[\s\S]*?)\n  <img class="cover"/.exec(base)[1];
const html = `<!DOCTYPE html>
<html lang="es"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Instalar · Pack Minimal · Vice City</title>
<link href="https://fonts.googleapis.com/css2?family=Bungee&family=Rubik:wght@400;600;800&display=swap" rel="stylesheet">
<style>${style}
.res{text-align:left;border-top:1px solid var(--line);padding:14px 0 4px;margin-top:14px}.res b{font-size:15px}.res small{color:var(--mut);margin-left:8px}.res .urlbox{margin-top:8px}.res a{color:var(--c);font-size:13px;margin-left:8px}
.done ol{text-align:left;color:var(--mut);font-size:14px;line-height:1.6;margin:14px 0 0 20px}
</style></head><body>
<div class="wrap">
  <div class="brand">ALEJOX GAMING · INSTALACIÓN EN UN CLIC</div>
  <h1>Pack Minimal · Vice City</h1>
  <p class="sub">Pega tu JWT y este instalador crea en tu cuenta de <b>StreamElements</b> las <b>6 escenas minimalistas</b> (intro, streaming, hablar, pausa, ya vuelvo y salida) con <b>chat horizontal inferior</b> y <b>alertas discretas</b>. Cada pieza se mueve, escala u oculta por separado.</p>
  ${gift}
  <img class="cover" id="cover" alt="" style="width:100%;border-radius:16px;margin:0 0 18px;box-shadow:0 16px 40px #0008">
  <div id="steps">
    <div class="card step"><div class="num">1</div><div>
      <h2>Copia tu JWT Token de StreamElements</h2>
      <ol><li>Inicia sesión en StreamElements con la cuenta donde quieres las escenas.</li>
      <li>Clic en tu avatar (arriba a la derecha) → <b>Account → Channels</b>.</li>
      <li>Clic en <b>Show secrets</b> y copia el <b>JWT Token</b> (empieza por <code>eyJ</code> y es muy largo).</li></ol>
      <a class="btn ghost" href="https://streamelements.com/dashboard/account/channels" target="_blank" rel="noopener">Abrir StreamElements ↗</a>
    </div></div>
    <div class="card step"><div class="num">2</div><div style="flex:1;min-width:0">
      <h2>Pega el token e instala</h2>
      <div class="field"><input id="jwt" type="password" placeholder="Pega aquí tu JWT Token" autocomplete="off" spellcheck="false"><button class="btn ghost" id="show" type="button" style="margin:0">👁</button></div>
      <button class="btn" id="install" disabled>⚡ Instalar en StreamElements</button>
      <p class="note">🔒 El token se usa solo para hablar con la API oficial de StreamElements (api.streamelements.com) y crear los overlays. No se guarda ni se envía a ningún otro sitio. Si quieres, regenéralo después en el mismo panel.</p>
      <div class="status" id="status"></div>
    </div></div>
  </div>
  <div class="card done" id="done">
    <h2>¡Instalado!</h2>
    <p class="sub" style="margin:4px 0 0">Cada línea es una URL para una <b>Fuente de navegador de 1920×1080</b> en OBS o Streamlabs Desktop.</p>
    <div id="results"></div>
    <ol>
      <li>Crea una escena por cada línea y añade su URL como <b>Fuente de navegador</b> (1920×1080, con "Controlar audio vía OBS" para oír las alertas).</li>
      <li>En <b>Streaming</b> y <b>Hablar</b> pon tu cámara debajo de la fuente y ajústala al marco.</li>
      <li>En el editor (enlace "Editar") cada pieza es independiente: arrástrala, cámbiale el tamaño u ocúltala con el ojo. En <b>Settings</b> están las secciones <b>⚙️ Canal</b> y <b>⚙️ Elemento</b>.</li>
      <li>El chat y el aviso de alerta muestran ejemplos solo dentro del editor; en el directo salen los reales.</li>
    </ol>
    <p class="note">⚠️ Estas URLs son privadas: no las muestres en stream.</p>
  </div>
  <p class="foot">Regalo de <a href="https://www.youtube.com/@alejoxcode" target="_blank" rel="noopener" style="color:var(--mut)">AlejoxCode</a> y <a href="https://www.youtube.com/@argegaming" target="_blank" rel="noopener" style="color:var(--mut)">ArgeGaming</a> para su comunidad · Instalador basado en la API pública de StreamElements</p>
</div>
<script id="pack" type="application/json">${JSON.stringify(PACK).replace(/<\//g, '<\\/')}</script>
<script>
(() => {
const PACK = JSON.parse(document.getElementById('pack').textContent);
const API = 'https://api.streamelements.com/kappa/v2';
const $ = id => document.getElementById(id);
const jwt = $('jwt'), btn = $('install'), st = $('status');
$('cover').src = PACK.cover;
const clean = s => s.replace(/\\s+/g, '').replace(/^Bearer/i, '').replace(/^["']|["']$/g, '');
jwt.addEventListener('input', () => {
  const v = clean(jwt.value), ok = /^eyJ[\\w-]+\\.[\\w-]+\\.[\\w-]+$/.test(v);
  btn.disabled = !ok;
  if (v && !ok) show('err', 'Eso no parece el <b>JWT Token</b>: debe empezar por <code>eyJ</code> y ser muy largo. No uses el "Account ID" ni el "Overlay token".'); else st.className = 'status';
});
$('show').onclick = () => jwt.type = jwt.type === 'password' ? 'text' : 'password';
function show(type, html, pct){ st.className = 'status show ' + type; st.innerHTML = html + (pct != null ? '<div class="bar"><i style="width:' + pct + '%"></i></div>' : ''); }
async function call(path, token, opts = {}) {
  const headers = { Accept: 'application/json', Authorization: 'Bearer ' + token, ...(opts.headers || {}) };
  const r = await fetch(API + path, { ...opts, headers });
  let data = null; try { data = await r.json(); } catch (e) {}
  if (!r.ok) { const e = new Error((data && (data.message || data.error)) || ('HTTP ' + r.status)); e.status = r.status; throw e; }
  return data;
}
function dataUrlToBlob(u){ const [h, b] = u.split(','); const mime = /data:([^;]+)/.exec(h)[1]; const bin = atob(b); const arr = new Uint8Array(bin.length); for (let i = 0; i < bin.length; i++) arr[i] = bin.charCodeAt(i); return new Blob([arr], { type: mime }); }
btn.onclick = async () => {
  const token = clean(jwt.value); btn.disabled = true;
  try {
    show('run', '<span class="spin"></span>Verificando tu cuenta…', 2);
    const me = await call('/channels/me', token);
    let cover = '';
    try { const fd = new FormData(); fd.append('file', dataUrlToBlob(PACK.cover), 'minimal-vicecity-portada.jpg'); cover = (await call('/uploads/' + me._id, token, { method: 'POST', body: fd })).url || ''; } catch (e) {}
    const results = [];
    for (let i = 0; i < PACK.scenes.length; i++) {
      const s = PACK.scenes[i];
      show('run', '<span class="spin"></span>Creando la escena <b>' + s.name + '</b>… (' + (i + 1) + '/' + PACK.scenes.length + ')', 10 + 90 * (i + 1) / PACK.scenes.length);
      const widgets = s.widgets.map((x, n) => ({
        id: n + 1, group: null, version: 2.2, type: 'se-widget-custom-event-list', name: x.name, visible: true, locked: false, listener: null, listeners: x.listeners,
        css: x.css, variables: { html: PACK.html, css: PACK.css, js: PACK.js, fields: x.fields, fieldData: x.fieldData },
        provider: me.provider || me.providerId || undefined
      }));
      const body = { name: s.name, type: 'regular', settings: { width: 1920, height: 1080, name: '1080p' }, widgets };
      if (cover) body.preview = cover;
      const ov = await call('/overlays/' + me._id, token, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
      results.push({ name: s.name, desc: s.desc, id: ov._id });
    }
    const box = $('results'); box.innerHTML = '';
    results.forEach(r => {
      const d = document.createElement('div'); d.className = 'res';
      d.innerHTML = '<b></b><small></small><a target="_blank" rel="noopener">Editar ↗</a><div class="urlbox"><input readonly><button class="btn ghost" style="margin:0">Copiar</button></div>';
      d.querySelector('b').textContent = r.name; d.querySelector('small').textContent = r.desc;
      d.querySelector('a').href = 'https://streamelements.com/overlay/' + r.id + '/editor';
      const inp = d.querySelector('input'), bt = d.querySelector('button');
      inp.value = me.apiToken ? 'https://streamelements.com/overlay/' + r.id + '/' + me.apiToken : 'Cópiala desde el editor (ícono de enlace arriba a la derecha)';
      bt.onclick = async () => { try { await navigator.clipboard.writeText(inp.value); } catch (e) { inp.select(); document.execCommand('copy'); } bt.textContent = '¡Copiada!'; setTimeout(() => bt.textContent = 'Copiar', 1800); };
      box.appendChild(d);
    });
    $('steps').style.display = 'none'; $('done').classList.add('show'); jwt.value = '';
  } catch (e) {
    show('err', (e.status === 401 || e.status === 403)
      ? 'StreamElements rechazó el token: puede estar vencido o ser de otra cuenta. Vuelve a copiar el <b>JWT Token</b> completo (Account → Channels → Show secrets) o regenéralo.'
      : 'No se pudo instalar: ' + String(e.message).replace(/[<>]/g, '') + '. Intenta de nuevo en un momento.');
    btn.disabled = false;
  }
};
})();
</script>
</body></html>
`;
fs.writeFileSync(path.join(ROOT, 'instalar-minimal.html'), html);
console.log('instalar-minimal.html', (html.length / 1e6).toFixed(2), 'MB ·', scenes.map(s => s.name + '(' + s.widgets.length + ')').join(' | '));
