#!/usr/bin/env python3
"""Genera instalar-pack.html: el instalador con JWT de StreamElements que crea
el overlay de alertas (igual que index.html) y un overlay por cada escena.
Uso: python3 escenas/crear-instalador.py"""
import json, re, pathlib
ROOT = pathlib.Path(__file__).resolve().parent.parent
CDN = 'https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/stickers/'
RAW = 'https://github.com/alejox/gta6-alertas/raw/main/descargas/escenas/transicion.webm'

esc = (ROOT / 'escenas/index.html').read_text()
css = re.search(r'<style>(.*?)</style>', esc, re.S).group(1)
css = "@import url('https://fonts.googleapis.com/css2?family=Bungee&family=Pacifico&family=Rubik:ital,wght@0,700;1,800;1,900&display=block');\n" + css
js = re.search(r'<script>\n(function escMain\(\)\{.*?\n\})\nif\(!window', esc, re.S).group(1)
js = """window.__ESC_WIDGET=1;
""" + js + """
window.addEventListener('onWidgetLoad', function(ev){
  var f=(ev.detail&&ev.detail.fieldData)||{};
  window.__ESC={stk:'%s',escena:f.escEscena,canal:f.escCanal,titulo:f.escTitulo,kicker:f.escKicker,sub:f.escSub,over:f.escOver,
    min:f.escMin,cam:f.escCam,l1:f.escL1,l2:f.escL2,l3:f.escL3};
  escMain();
});
""" % CDN

G = 'Escena'
def txt(label, value=''): return {'type': 'text', 'label': label, 'value': value, 'group': G}
FULL = {
    'escCanal': txt('Nombre del canal', '@TUCANAL'),
    'escOver': txt('Texto pequeño de arriba (vacío = el de fábrica)'),
    'escKicker': txt('Palabra en cursiva (vacío = la de fábrica)'),
    'escTitulo': txt('Título grande (vacío = el de fábrica)'),
    'escSub': txt('Texto de la placa (vacío = el de fábrica)'),
}
LABS = {'escL1': txt('Placa 1 · Último seguidor', '—'), 'escL2': txt('Placa 2 · Último sub', '—'), 'escL3': txt('Placa 3 · Top donación', '—')}
SCENES = [
    ('intro', 'GTA 6 · Intro', 'Empezamos pronto', {**FULL, 'escMin': {'type': 'number', 'label': 'Cuenta atrás en minutos (0 = sin cuenta)', 'value': 0, 'min': 0, 'max': 120, 'step': 1, 'group': G}}),
    ('juego', 'GTA 6 · Streaming', 'Overlay transparente para jugar', {'escCanal': txt('Nombre del canal', '@TUCANAL'),
        'escCam': {'type': 'dropdown', 'label': 'Lado de la cámara', 'value': 'izq', 'options': {'izq': 'Izquierda', 'der': 'Derecha'}, 'group': G}, **LABS}),
    ('hablar', 'GTA 6 · Hablar', 'Cámara grande + chat', {'escCanal': txt('Nombre del canal', '@TUCANAL'), **LABS}),
    ('pausa', 'GTA 6 · Pausa', 'Directo en pausa', FULL),
    ('vuelvo', 'GTA 6 · Ya vuelvo', 'Descanso corto', FULL),
    ('fin', 'GTA 6 · Salida', 'Fin del directo', FULL),
]
scenes = []
for key, name, desc, fields in SCENES:
    fields = {'escEscena': {'type': 'hidden', 'label': 'Escena', 'value': key}, **fields}
    scenes.append({'key': key, 'name': name, 'desc': desc, 'fields': json.dumps(fields, ensure_ascii=False),
                   'fieldData': {k: v['value'] for k, v in fields.items()}})
PACK2 = {'html': '<div id="stage"></div>', 'css': css, 'js': js, 'scenes': scenes, 'transicion': RAW}

h = (ROOT / 'index.html').read_text()
def rep(a, b, n=1):
    global h
    assert h.count(a) >= 1, a[:60]
    h = h.replace(a, b, n)

rep('<title>Instalar · Alertas GTA 6 · Vice City</title>', '<title>Instalar · Pack GTA 6 · Vice City</title>')
rep('<h1>Alertas GTA 6 · Vice City</h1>', '<h1>Pack GTA 6 · Vice City</h1>')
rep('Este instalador crea el overlay en tu cuenta de <b>StreamElements</b>, sube los stickers y los sonidos a tu biblioteca y deja todo configurado.',
    'Pega tu JWT y este instalador crea en tu cuenta de <b>StreamElements</b> las <b>9 alertas</b> y las <b>6 escenas</b> (intro, streaming, hablar, pausa, ya vuelvo y salida), cada una como un overlay listo para usar en OBS o Streamlabs Desktop.')
rep('<button class="btn" id="install" disabled>⚡ Instalar en StreamElements</button>',
    '''<p style="margin-top:14px;font-size:14px;display:flex;gap:18px;flex-wrap:wrap"><label><input type="checkbox" id="optA" checked style="flex:none"> Alertas</label><label><input type="checkbox" id="optE" checked style="flex:none"> Escenas</label></p>
        <button class="btn" id="install" disabled>⚡ Instalar todo en StreamElements</button>''')
rep('.urlbox{display:flex;gap:8px;margin-top:14px}',
    '.urlbox{display:flex;gap:8px;margin-top:14px}\n.res{text-align:left;border-top:1px solid var(--line);padding:14px 0 4px;margin-top:14px}.res b{font-size:15px}.res small{color:var(--mut);margin-left:8px}.res .urlbox{margin-top:8px}.res a{color:var(--c);font-size:13px;margin-left:8px}\n.done ol{text-align:left;color:var(--mut);font-size:14px;line-height:1.6;margin:10px 0 0 20px}')
# tarjeta final
a = h.index('<div class="card done" id="done">'); b = h.index('<p class="foot">')
h = h[:a] + '''<div class="card done" id="done">
    <h2>¡Instalado!</h2>
    <p class="sub" style="margin:4px 0 0">Esto ya está en tu cuenta de StreamElements. Cada línea es una URL para una <b>Fuente de navegador de 1920×1080</b> en OBS o Streamlabs Desktop.</p>
    <div id="results"></div>
    <div class="res"><b>Transición entre escenas</b><small>video con sonido</small><br>
      <a class="btn ghost" id="trans" target="_blank" rel="noopener" style="margin-left:0">Descargar transicion.webm ↓</a>
      <p class="note">En OBS o Streamlabs Desktop: Transiciones de escena → añadir <b>Stinger</b> → elige el archivo → punto de transición <b>750 ms</b>.</p></div>
    <ol>
      <li>Crea una escena por cada línea y añade su URL como <b>Fuente de navegador</b> (1920×1080).</li>
      <li>En <b>Streaming</b> y <b>Hablar</b> añade también la URL de <b>Alertas</b> (encima) y tu cámara (debajo, ajustada al marco).</li>
      <li>El nombre del canal y los textos se cambian en el editor de cada overlay (enlace "Editar").</li>
    </ol>
    <p class="note">⚠️ Estas URLs son privadas: no las muestres en stream.</p>
  </div>
  ''' + h[b:]
rep('<script>\n(() => {\nconst PACK = JSON.parse', '<script id="pack2" type="application/json">' + json.dumps(PACK2, ensure_ascii=False).replace('</', '<\\/') + '</script>\n<script>\n(() => {\nconst PACK2 = JSON.parse(document.getElementById(\'pack2\').textContent);\nconst PACK = JSON.parse')
# lógica: alertas opcionales + escenas
rep("    const who = me.displayName || me.username || 'tu canal';\n",
    "    const who = me.displayName || me.username || 'tu canal';\n    const results = [];\n    const urlOf = id => me.apiToken ? `https://streamelements.com/overlay/${id}/${me.apiToken}` : '';\n    if ($('optA').checked) {\n")
a = h.index("    const id = created._id;"); b = h.index("  } catch (e) {\n    show('err'")
h = h[:a] + r'''    results.push({ name: created.name || PACK.title, desc: '9 alertas con sonido', id: created._id });
    }

    // 3) un overlay por escena
    if ($('optE').checked) {
      for (let i = 0; i < PACK2.scenes.length; i++) {
        const s = PACK2.scenes[i];
        show('run', `<span class="spin"></span>Creando la escena <b>${s.name}</b>… (${i + 1}/${PACK2.scenes.length})`, 88 + 11 * (i + 1) / PACK2.scenes.length);
        const w = {
          id: 1, group: null, version: 2.2, type: 'se-widget-custom-event-list', name: s.name, visible: true, locked: false, listener: null, listeners: {},
          css: { 'z-index': 1, width: '1920px', height: '1080px', opacity: 1, top: 0, left: 0 },
          variables: { html: PACK2.html, css: PACK2.css, js: PACK2.js, fields: s.fields, fieldData: s.fieldData },
          provider: me.provider || me.providerId || undefined
        };
        const ov = await call('/overlays/' + me._id, token, { method: 'POST', headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ name: s.name, type: 'regular', settings: { width: 1920, height: 1080, name: '1080p' }, widgets: [w] }) });
        results.push({ name: s.name, desc: s.desc, id: ov._id });
      }
    }
    if (!results.length) throw new Error('marca Alertas o Escenas');

    const box = $('results'); box.innerHTML = '';
    results.forEach(r => {
      const d = document.createElement('div'); d.className = 'res';
      d.innerHTML = `<b></b><small></small><a target="_blank" rel="noopener">Editar ↗</a><div class="urlbox"><input readonly><button class="btn ghost" style="margin:0">Copiar</button></div>`;
      d.querySelector('b').textContent = r.name; d.querySelector('small').textContent = r.desc;
      d.querySelector('a').href = `https://streamelements.com/overlay/${r.id}/editor`;
      const inp = d.querySelector('input'), bt = d.querySelector('button');
      inp.value = urlOf(r.id) || 'Cópiala desde el editor (ícono de enlace arriba a la derecha)';
      bt.onclick = async () => { try { await navigator.clipboard.writeText(inp.value); } catch (e) { inp.select(); document.execCommand('copy'); } bt.textContent = '¡Copiada!'; setTimeout(() => bt.textContent = 'Copiar', 1800); };
      box.appendChild(d);
    });
    $('trans').href = PACK2.transicion;
    $('steps').style.display = 'none'; $('done').classList.add('show'); jwt.value = '';
''' + h[b:]
# el botón "copiar" único ya no existe
a = h.index("$('copy').onclick"); b = h.index("})();", a)
h = h[:a] + h[b:]
rep("crear el overlay y subir los stickers y sonidos a tu biblioteca", "crear los overlays y subir los stickers y sonidos de las alertas a tu biblioteca")
(ROOT / 'instalar-pack.html').write_text(h)
print('instalar-pack.html', round(len(h) / 1e6, 2), 'MB')
