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
  var f=(ev.detail&&ev.detail.fieldData)||{}, c={stk:'%s'};
  for(var k in f) if(k.indexOf('esc')===0) c[k.slice(3).toLowerCase()]=f[k];   // escCanal -> canal, escVerCanal -> vercanal
  window.__ESC=c; escMain();
});
""" % CDN

G = '⚙️ Canal y elementos'
def txt(label, value='', g=G): return {'type': 'text', 'label': label, 'value': value, 'group': g}
def sino(label, value='si', g=G): return {'type': 'dropdown', 'label': label, 'value': value, 'options': {'si': 'Sí', 'no': 'No'}, 'group': g}
def hid(value): return {'type': 'hidden', 'label': '-', 'value': value}
def widget(name, box, fields, z=2):
    return {'name': name, 'css': {'z-index': z, 'left': box[0], 'top': box[1], 'width': '%dpx' % box[2], 'height': '%dpx' % box[3], 'opacity': 1},
            'fields': json.dumps(fields, ensure_ascii=False), 'fieldData': {k: v['value'] for k, v in fields.items()}}

# Piezas sueltas (tamaños medidos con escenas/index.html?escena=pieza&pieza=...)
def pz(k, **f): return {'escEscena': hid('pieza'), 'escPieza': hid(k), **f}
def nombre(x, y, w, estrellas):
    return widget('Nombre del canal', (x - 40, y - 40, w, 167), pz('nombre', escCanal=txt('Nombre del canal', '@TUCANAL', '⚙️ Canal'), escEstrellas=sino('Mostrar estrellas', estrellas, '⚙️ Canal')), 4)
def etiqueta(n, icono, titulo, x, y):
    g = '⚙️ Elemento'
    return widget('Placa · ' + titulo, (x - 40, y - 40, 480, 158), pz('etiqueta', escLt=txt('Título', titulo, g), escLv=txt('Texto (o deja — y pon encima una etiqueta de StreamElements)', '—', g),
        escIcono={'type': 'dropdown', 'label': 'Icono', 'value': icono, 'options': {'corazon': 'Corazón', 'estrella': 'Estrella', 'moneda': 'Moneda'}, 'group': g}), 3)
def envivo(x, y, estrellas):
    g = '⚙️ Elemento'
    return widget('En vivo', (x - 40, y - 40, 377, 136), pz('envivo', escTexto=txt('Texto', 'EN VIVO', g), escEstrellas=sino('Mostrar estrellas', estrellas, g)), 3)
LABS = [('corazon', 'Último seguidor'), ('estrella', 'Último sub'), ('moneda', 'Top donación')]

STREAMING = [
    widget('Marco de cámara', (1344, 642, 560, 366), pz('marco', escEstrellas=sino('Mostrar estrellas', 'si', '⚙️ Elemento')), 2),
    nombre(1418, 940, 540, 'no'),
    *[etiqueta(i, ic, t, 56 + i * 340, 48) for i, (ic, t) in enumerate(LABS)],
    envivo(1583, 44, 'si'),
]
HABLAR = [
    widget('Fondo', (0, 0, 1920, 1080), pz('fondo', escHueco=sino('Hueco transparente para la cámara (posición de fábrica)', 'si', '⚙️ Elemento')), 1),
    widget('Título', (32, 14, 760, 184), pz('titulo', escKicker=txt('Palabra en cursiva', 'solo', '⚙️ Elemento'), escTexto=txt('Título', 'CHARLANDO', '⚙️ Elemento'), escEstrellas=sino('Mostrar estrellas', 'si', '⚙️ Elemento')), 3),
    envivo(1640, 70, 'no'),
    widget('Marco de cámara', (24, 110, 1296, 780), pz('marcogrande'), 2),
    nombre(114, 818, 700, 'si'),
    widget('Caja del chat', (1296, 110, 600, 780), pz('chat', escTexto=txt('Título de la caja', 'CHAT', '⚙️ Elemento')), 2),
    *[etiqueta(i, ic, t, 534 + i * 400, 954) for i, (ic, t) in enumerate(LABS)],
]
# Pantallas completas, también por piezas. Anchos medidos del título y de la placa de cada tema.
MED = {'intro': (1618, 628), 'pausa': (1001, 603), 'vuelvo': (1469, 643), 'fin': (1123, 975)}
def full(tema, cuenta=False):
    g = '⚙️ Elemento'; hw, sw = MED[tema]; t = {'escTema': hid(tema)}
    sx = 960 - sw // 2 - (120 if cuenta else 0)
    w = [
        widget('Fondo', (0, 0, 1920, 1080), pz('collage', **t), 1),
        widget('Título', (960 - hw // 2, 198, hw, 545), pz('heroe', **t, escOver=txt('Texto pequeño de arriba (vacío = el de fábrica)', '', g),
               escKicker=txt('Palabra en cursiva (vacío = la de fábrica)', '', g), escTitulo=txt('Título grande (vacío = el de fábrica)', '', g)), 3),
        widget('Placa y estrellas', (sx, 610, sw + 240, 205), pz('subplaca', **t, escSub=txt('Texto de la placa (vacío = el de fábrica)', '', g), escEstrellas=sino('Mostrar estrellas', 'si', g)), 4),
        widget('Nombre del canal', (32, 924, 760, 136), pz('canal', **t, escCanal=txt('Nombre del canal', '@TUCANAL', '⚙️ Canal')), 4),
        widget('Vice City', (1535, 18, 385, 136), pz('ciudad', **t, escTexto=txt('Texto', 'VICE CITY', g), escEstrellas=sino('Mostrar estrellas', 'si', g)), 4),
    ]
    if cuenta:
        w.append(widget('Cuenta atrás', (sx + sw - 50, 628, 310, 172), pz('cuenta', **t, escMin={'type': 'number', 'label': 'Minutos', 'value': 5, 'min': 1, 'max': 120, 'step': 1, 'group': g}), 4))
    return w

SCENES = [
    ('intro', 'GTA 6 · Intro', 'Empezamos pronto, con cuenta atrás', full('intro', True)),
    ('juego', 'GTA 6 · Streaming', 'Cámara, nombre, placas y EN VIVO', STREAMING),
    ('hablar', 'GTA 6 · Hablar', 'Cámara grande + chat', HABLAR),
    ('pausa', 'GTA 6 · Pausa', 'Directo en pausa', full('pausa')),
    ('vuelvo', 'GTA 6 · Ya vuelvo', 'Descanso corto', full('vuelvo')),
    ('fin', 'GTA 6 · Salida', 'Fin del directo', full('fin')),
]
scenes = [{'key': k, 'name': n, 'desc': d, 'widgets': w} for k, n, d, w in SCENES]
import base64
cover = 'data:image/jpeg;base64,' + base64.b64encode((ROOT / 'portada.jpg').read_bytes()).decode()
PACK2 = {'html': '<div id="stage"></div>', 'css': css, 'js': js, 'scenes': scenes, 'transicion': RAW, 'cover': cover}

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
      <li>En el editor de cada overlay (enlace "Editar") haz clic en una pieza y abre <b>Settings</b>: ahí están las secciones <b>⚙️ Canal</b> (nombre del canal) y <b>⚙️ Elemento</b> (textos, estrellas, minutos…).</li>
      <li>En todas las escenas cada pieza (fondo, título, placa, marco de cámara, nombre, placas, EN VIVO…) es independiente: arrástrala, cámbiale el tamaño u ocúltala con el ojo de la lista de capas.</li>
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
      let packCover = '';
      try { const fd = new FormData(); fd.append('file', dataUrlToBlob(PACK2.cover), 'gta6-pack-portada.jpg'); packCover = (await call('/uploads/' + me._id, token, { method: 'POST', body: fd })).url || ''; } catch (e) {}
      for (let i = 0; i < PACK2.scenes.length; i++) {
        const s = PACK2.scenes[i];
        show('run', `<span class="spin"></span>Creando la escena <b>${s.name}</b>… (${i + 1}/${PACK2.scenes.length})`, 88 + 11 * (i + 1) / PACK2.scenes.length);
        const widgets = s.widgets.map((x, n) => ({
          id: n + 1, group: null, version: 2.2, type: 'se-widget-custom-event-list', name: x.name, visible: true, locked: false, listener: null, listeners: {},
          css: x.css, variables: { html: PACK2.html, css: PACK2.css, js: PACK2.js, fields: x.fields, fieldData: x.fieldData },
          provider: me.provider || me.providerId || undefined
        }));
        const body = { name: s.name, type: 'regular', settings: { width: 1920, height: 1080, name: '1080p' }, widgets };
        if (packCover) body.preview = packCover;
        const ov = await call('/overlays/' + me._id, token, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
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
h = h.replace("if (PACK.cover) { $('cover').src = PACK.cover;", "if (PACK.cover) { $('cover').src = PACK2.cover;")
(ROOT / 'instalar-pack.html').write_text(h)
print('instalar-pack.html', round(len(h) / 1e6, 2), 'MB')
