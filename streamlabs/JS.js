(function(){
/* =========================================================
   ALERTAS GTA 6 · VICE CITY — Custom Widget StreamElements
   Eventos: seguidor, sub, resub, regalos, donación, raid,
            KICKs, bits y host. Cada uno con su animación.
   ========================================================= */
const TYPES = {
  follow:{sticker:'flamenco', pal:['#ffe3f4','#ff9ad6','#ff3fa4','#b02bd6'], accent:'#ff3fa4', accent2:'#b02bd6', ink:'#2a0630', enter:'pop', idle:'wobble', fx:'confetti', sfx:{root:349.23, extra:'sparkle'}},
  sub:{sticker:'caiman', pal:['#e3f0ff','#7cc0ff','#2f6bff','#4a1fd1'], accent:'#2f7bff', accent2:'#6a2cff', ink:'#071538', enter:'rise', idle:'chomp', fx:'confetti', sfx:{root:293.66, extra:'chomp'}},
  resub:{sticker:'sol', pal:['#fff6b0','#ffd23a','#ff9a1f','#c02bd6'], accent:'#ffae1f', accent2:'#c02bd6', ink:'#2c0a2e', enter:'spin', idle:'radiate', fx:'stars', sfx:{root:392.00, extra:'sparkle'}},
  gift:{sticker:'avioneta', pal:['#ffe6f2','#ff9ccc','#ff2f8e','#c4126b'], accent:'#ff2f8e', accent2:'#7b2cff', ink:'#300720', enter:'fly', idle:'bob', exit:'flyOut', fx:'rain', sfx:{root:329.63, extra:'prop'}, wide:true},
  tip:{sticker:'manati', pal:['#fff4c4','#ffd04a','#ff9f1c','#e0428f'], accent:'#ffa51c', accent2:'#e0428f', ink:'#2e1504', enter:'drop', idle:'dance', fx:'coins', sfx:{root:440.00, extra:'coins'}},
  raid:{sticker:'lowrider', pal:['#e6e0ff','#a98bff','#6a3dff','#1f4fd6'], accent:'#6a3dff', accent2:'#1f4fd6', ink:'#0d0a3a', enter:'drive', idle:'hop', exit:'driveOut', fx:'smoke', shake:2, sfx:{root:220.00, extra:'engine'}, wide:true},
  kicks:{sticker:'palmera', pal:['#f0ffd8','#9dff5a','#3fd85a','#ff9f1c'], accent:'#3fd85a', accent2:'#ff7a1c', ink:'#0a2410', enter:'sway', idle:'sway', fx:'coins', coin:'#53fc18', sfx:{root:369.99, extra:'coins'}},
  cheer:{sticker:'vi', pal:['#ffe2a8','#ff9a5a','#ff4fa8','#7b3bff'], accent:'#ff4fa8', accent2:'#7b3bff', ink:'#1d0726', enter:'slam', idle:'pulse', fx:'stars', shake:2, sfx:{root:415.30, extra:'sparkle'}},
  host:{sticker:'rstar', pal:['#ffe6ff','#ff7ad9','#c53bff','#5b2cff'], accent:'#d03bff', accent2:'#5b2cff', ink:'#16052a', enter:'flip', idle:'float', fx:'confetti', sfx:{root:311.13, extra:'sparkle'}},
};
const CFG = {volume:.55, sound:true, showMsg:true, currency:'$', hold:6200, palms:true, minTip:0, minCheer:0, minKicks:0};
let HOLD = 6200;
const $ = id => document.getElementById(id);
const root = document.documentElement;
const el = {alert:$('alert'),emb:$('emb'),ei:$('ei'),img:$('embImg'),kicker:$('kicker'),word:$('word'),plate:$('plate'),name:$('name'),msg:$('msg')};
const q = s => el.alert.querySelector(s);

function renderMsg(t, d, name){
  const amt = +d.amount || 0;
  let tpl = (d.type==='sub' && amt>1) ? t.tplMonths : t.tpl;
  const shown = d.type==='tip' ? (d.fmt || CFG.currency + (Math.round(amt*100)/100)) : amt;
  return esc(tpl||'').replace(/\{amount\}/g, `<b>${esc(shown)}</b>`).replace(/\{name\}/g, esc(name));
}
function fitStage(){ const s=Math.min(innerWidth/990, innerHeight/920, 1.6)*Math.min(1,window.__SIZE||1); root.style.setProperty('--scale', s.toFixed(3)); }
addEventListener('resize', fitStage); fitStage();
const sc = () => parseFloat(getComputedStyle(root).getPropertyValue('--scale'))||1;


function rgba(hex,a){ const n=parseInt(hex.slice(1),16); return `rgba(${n>>16&255},${n>>8&255},${n&255},${a})`; }
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}

async function build(d){
  const t = TYPES[d.type] || TYPES.follow;
  const vars = {'--c1':t.pal[0],'--c2':t.pal[1],'--c3':t.pal[2],'--c4':t.pal[3],'--accent':t.accent,'--accent2':t.accent2,'--ink':t.ink,
    '--glowA':rgba(t.accent,.42),'--glowB':rgba(t.accent2,.16),'--accentGlow':rgba(t.accent,.6)};
  for(const k in vars) root.style.setProperty(k, vars[k]);
  el.alert.classList.toggle('wide', !!t.wide);

  el.emb.style.display = t.img ? '' : 'none';
  if(t.img){ el.img.src = t.img; try{ await el.img.decode(); }catch(e){} }

  el.kicker.textContent = t.kicker;
  const w = esc(t.word);
  el.word.innerHTML = ['depth','rgb-a','rgb-b','stroke','fill'].map(c=>`<span class="${c}">${w}</span>`).join('');
  el.word.style.fontSize='118px';
  let ww = el.word.getBoundingClientRect().width/sc();
  if(ww>820){ el.word.style.fontSize=Math.floor(118*820/ww)+'px'; ww=820; }
  const kw = el.kicker.getBoundingClientRect().width/sc();
  root.style.setProperty('--kx', Math.round(-ww/2 + kw/2 - 40)+'px');

  const name=(d.name||'Usuario').slice(0,24);
  el.name.innerHTML=[...name].map(c=>`<span class="ch">${esc(c)}</span>`).join('');
  el.name.style.fontSize = name.length>16?'42px':'54px';
  el.msg.innerHTML = renderMsg(t, d, name) + (d.message && CFG.showMsg ? `<q>${esc(String(d.message).slice(0,120))}</q>` : '');
  return t;
}

/* =========================================================
   ANIMACIONES
   ========================================================= */
const E={back:'cubic-bezier(.34,1.56,.64,1)',out:'cubic-bezier(.16,1,.3,1)',in:'cubic-bezier(.55,0,1,.45)'};
let running=[];
const A=(n,kf,o)=>{const a=n.animate(kf,{fill:'both',...o});running.push(a);return a;};

/* Entradas del sticker (cada evento distinto) */
const ENTER = {
  pop:  ()=>A(el.emb,[{opacity:0,transform:'scale(0) rotate(-25deg)'},{opacity:1,transform:'scale(1.12) rotate(4deg)',offset:.7},{opacity:1,transform:'scale(1) rotate(0)'}],{duration:750,delay:60,easing:E.out}),
  rise: ()=>A(el.emb,[{opacity:0,transform:'translateY(320px) scale(.7)'},{opacity:1,transform:'translateY(-26px) scale(1.05)',offset:.65},{opacity:1,transform:'translateY(0) scale(1)'}],{duration:800,delay:60,easing:E.out}),
  spin: ()=>A(el.emb,[{opacity:0,transform:'scale(0) rotate(-540deg)'},{opacity:1,transform:'scale(1.1) rotate(10deg)',offset:.75},{opacity:1,transform:'scale(1) rotate(0)'}],{duration:1000,delay:60,easing:E.out}),
  fly:  ()=>A(el.emb,[{opacity:1,transform:'translate(1300px,-160px) rotate(-14deg) scale(.7)'},{opacity:1,transform:'translate(-50px,14px) rotate(5deg) scale(1.02)',offset:.72},{opacity:1,transform:'translate(0,0) rotate(0) scale(1)'}],{duration:1150,delay:0,easing:E.out}),
  drop: ()=>A(el.emb,[{opacity:1,transform:'translateY(-760px) rotate(-8deg)'},{transform:'translateY(0) rotate(0) scaleY(.86) scaleX(1.08)',offset:.5},{transform:'translateY(-70px) scale(1)',offset:.68},{transform:'translateY(0) scaleY(.95) scaleX(1.03)',offset:.84},{opacity:1,transform:'translateY(0) scale(1)'}],{duration:1000,delay:40,easing:'ease-in-out'}),
  drive:()=>A(el.emb,[{opacity:1,transform:'translateX(-1500px) skewX(16deg)'},{transform:'translateX(60px) skewX(-8deg)',offset:.62},{transform:'translateX(-14px) skewX(3deg)',offset:.82},{opacity:1,transform:'translateX(0) skewX(0)'}],{duration:950,delay:0,easing:E.out}),
  sway: ()=>A(el.emb,[{opacity:0,transform:'translateY(160px) rotate(-38deg)'},{opacity:1,transform:'translateY(0) rotate(9deg)',offset:.6},{transform:'rotate(-4deg)',offset:.82},{opacity:1,transform:'translateY(0) rotate(0)'}],{duration:1050,delay:60,easing:E.out}),
  slam: ()=>A(el.emb,[{opacity:0,transform:'scale(4)',filter:'blur(16px)'},{opacity:1,transform:'scale(.9)',filter:'blur(0)',offset:.6},{opacity:1,transform:'scale(1)',filter:'blur(0)'}],{duration:600,delay:80,easing:E.in}),
  flip: ()=>A(el.emb,[{opacity:0,transform:'perspective(900px) rotateY(-540deg) scale(.3)'},{opacity:1,transform:'perspective(900px) rotateY(15deg) scale(1.05)',offset:.75},{opacity:1,transform:'perspective(900px) rotateY(0) scale(1)'}],{duration:1050,delay:60,easing:E.out}),
};
/* Movimiento en reposo */
const IDLE = {
  wobble:[[{transform:'rotate(-4deg)'},{transform:'rotate(4deg)'}],{duration:1100,direction:'alternate',easing:'ease-in-out'}],
  chomp:[[{transform:'scale(1) rotate(0)'},{transform:'scale(1.06) rotate(-3deg)',offset:.08},{transform:'scale(.97) rotate(2deg)',offset:.16},{transform:'scale(1.04) rotate(-2deg)',offset:.24},{transform:'scale(1) rotate(0)',offset:.34},{transform:'scale(1) rotate(0)'}],{duration:2000}],
  radiate:[[{transform:'rotate(-6deg) scale(1)'},{transform:'rotate(6deg) scale(1.05)'}],{duration:1600,direction:'alternate',easing:'ease-in-out'}],
  bob:[[{transform:'translateY(0) rotate(-2deg)'},{transform:'translateY(-16px) rotate(2deg)'}],{duration:1200,direction:'alternate',easing:'ease-in-out'}],
  dance:[[{transform:'rotate(-6deg) translateX(-6px)'},{transform:'rotate(6deg) translateX(6px)'}],{duration:520,direction:'alternate',easing:'ease-in-out'}],
  hop:[[{transform:'translateY(0) rotate(0)'},{transform:'translateY(-34px) rotate(-3deg)',offset:.18},{transform:'translateY(0) rotate(0)',offset:.36},{transform:'translateY(-14px) rotate(2deg)',offset:.5},{transform:'translateY(0) rotate(0)',offset:.64},{transform:'translateY(0) rotate(0)'}],{duration:1300}],
  sway:[[{transform:'rotate(-4deg)'},{transform:'rotate(4deg)'}],{duration:1500,direction:'alternate',easing:'ease-in-out'}],
  pulse:[[{transform:'scale(1)'},{transform:'scale(1.07)'}],{duration:700,direction:'alternate',easing:'ease-in-out'}],
  float:[[{transform:'translateY(0) rotate(-3deg)'},{transform:'translateY(-14px) rotate(3deg)'}],{duration:1500,direction:'alternate',easing:'ease-in-out'}],
};
const EXIT = {
  flyOut:  [{opacity:1,transform:'translate(0,0) rotate(0)'},{opacity:1,transform:'translate(-1500px,-220px) rotate(-10deg)'}],
  driveOut:[{opacity:1,transform:'translateX(0) skewX(0)'},{transform:'translateX(-40px) skewX(6deg)',offset:.2},{opacity:1,transform:'translateX(1600px) skewX(-14deg)'}],
  def:     [{opacity:1,transform:'scale(1)'},{opacity:0,transform:'scale(.3) rotate(-12deg)'}],
};

function shake(level, delay){
  const k=level===2?16:9;
  A(el.alert,[{translate:'0 0'},{translate:`${-k}px ${k*.6}px`},{translate:`${k*.8}px ${-k*.5}px`},{translate:`${-k*.6}px ${-k*.3}px`},{translate:`${k*.4}px ${k*.4}px`},{translate:`${-k*.2}px 0`},{translate:'0 0'}],{duration:level===2?460:380,delay,easing:'linear'});
}

function enter(t){
  running.forEach(a=>a.cancel()); running=[];
  el.alert.classList.add('on');
  q('.word .fill').classList.remove('shine'); el.plate.classList.remove('lit');

  A(q('.flash'),[{opacity:0,transform:'scale(.3)'},{opacity:.9,offset:.25},{opacity:0,transform:'scale(1.4)'}],{duration:700,easing:'ease-out'});
  A(q('.glow'),[{opacity:0,transform:'scale(.2)'},{opacity:1,transform:'scale(1)'}],{duration:800,easing:E.out});
  A(q('.rays'),[{opacity:0,transform:'scale(.4)'},{opacity:1,transform:'scale(1)'}],{duration:900,easing:E.out});
  if(!t.wide){
    A(q('.palm.l'),[{opacity:0,transform:'translate(-120px,80px) rotate(-18deg)'},{opacity:1,transform:'translate(0,0) rotate(0)'}],{duration:800,delay:180,easing:E.back});
    A(q('.palm.r'),[{opacity:0,transform:'translate(120px,80px) rotate(18deg)'},{opacity:1,transform:'translate(0,0) rotate(0)'}],{duration:800,delay:180,easing:E.back});
    A(q('.palm.l .pi'),[{transform:'rotate(0)'},{transform:'rotate(-2.5deg)'}],{duration:1700,delay:1000,iterations:Infinity,direction:'alternate',easing:'ease-in-out'});
    A(q('.palm.r .pi'),[{transform:'rotate(0)'},{transform:'rotate(2.5deg)'}],{duration:1900,delay:1000,iterations:Infinity,direction:'alternate',easing:'ease-in-out'});
  }

  ENTER[t.enter]();
  const [ik,io]=IDLE[t.idle]; A(el.ei,ik,{...io,delay:1150,iterations:Infinity});

  const wd = (t.enter==='fly'||t.enter==='drive')?620:340;
  A(el.word,[{opacity:0,transform:'scale(2.6)',filter:'blur(14px)'},{opacity:1,transform:'scale(.92)',filter:'blur(0)',offset:.7},{opacity:1,transform:'scale(1)',filter:'blur(0)'}],{duration:520,delay:wd,easing:E.in});
  A(q('.word .rgb-a'),[{opacity:.9,transform:'translate(-22px,-6px)'},{opacity:0,transform:'translate(0,0)'}],{duration:520,delay:wd+300,easing:E.out});
  A(q('.word .rgb-b'),[{opacity:.9,transform:'translate(22px,6px)'},{opacity:0,transform:'translate(0,0)'}],{duration:520,delay:wd+300,easing:E.out});
  shake(t.shake||1, wd+220);

  A(el.kicker,[{opacity:0,transform:'rotate(-30deg) scale(0)'},{opacity:1,transform:'rotate(-8deg) scale(1)'}],{duration:520,delay:wd+380,easing:E.back});
  A(el.plate,[{opacity:0,transform:'skewX(-12deg) translateX(-140px) scaleX(.2)'},{opacity:1,transform:'skewX(-12deg) translateX(0) scaleX(1)'}],{duration:560,delay:wd+560,easing:E.back});
  el.name.querySelectorAll('.ch').forEach((c,i)=>A(c,[{opacity:0,transform:'translateY(26px) scale(.6)'},{opacity:1,transform:'translateY(0) scale(1)'}],{duration:380,delay:wd+730+i*32,easing:E.back}));
  A(el.msg,[{opacity:0,transform:'translateY(16px)'},{opacity:1,transform:'translateY(0)'}],{duration:500,delay:wd+1030,easing:E.out});

  setTimeout(()=>{q('.word .fill').classList.add('shine');el.plate.classList.add('lit');},wd+930);
  setTimeout(()=>FX.play(t),wd+260);
  setTimeout(()=>FX.sparkle(true),wd+600);
}

function leave(t){
  return new Promise(res=>{
    FX.sparkle(false);
    const o={duration:520,easing:E.in};
    A(el.msg,[{opacity:1},{opacity:0,transform:'translateY(10px)'}],{duration:300,easing:E.in});
    A(el.plate,[{opacity:1},{opacity:0,transform:'skewX(-12deg) translateX(160px) scaleX(.3)'}],{...o,delay:60});
    A(el.kicker,[{opacity:1},{opacity:0,transform:'rotate(-8deg) scale(0)'}],{...o,delay:80});
    A(el.word,[{opacity:1,transform:'translate(0,0) skewX(0)'},{transform:'translate(14px,0) skewX(-14deg)',offset:.2},{transform:'translate(-10px,0) skewX(10deg)',offset:.4},{opacity:1,transform:'scale(1.05)',offset:.55},{opacity:0,transform:'scale(.2)',filter:'blur(8px)'}],{duration:620,delay:120,easing:'linear'});
    const ex=EXIT[t.exit||'def'];
    A(el.emb,ex,{duration:t.exit?800:520,delay:200,easing:E.in});
    if(t.exit==='driveOut') FX.smoke();
    ['.palm.l','.palm.r','.rays','.glow'].forEach((s,i)=>A(q(s),[{opacity:1},{opacity:0,transform:'scale(.3)'}],{...o,delay:260+i*40}));
    setTimeout(()=>{el.alert.classList.remove('on');running.forEach(a=>a.cancel());running=[];res();},1050);
  });
}

/* =========================================================
   PARTÍCULAS: confeti, lluvia, monedas, humo, estrellas
   ========================================================= */
const FX=(()=>{
  const cv=$('fx'),cx=cv.getContext('2d');
  let W,H,parts=[],emitters=[],sparkOn=false,raf=0,last=0,acc=0,coinCol=null;
  function size(){const d=Math.min(devicePixelRatio||1,2);W=innerWidth;H=innerHeight;cv.width=W*d;cv.height=H*d;cx.setTransform(d,0,0,d,0,0);}
  addEventListener('resize',size);size();
  const pal=()=>['--c1','--c2','--c3','--c4','--accent'].map(v=>getComputedStyle(root).getPropertyValue(v).trim()).concat('#ffffff');
  const ctr=n=>{const r=n.getBoundingClientRect();return{x:r.left+r.width/2,y:r.top+r.height/2,r};};
  const R=Math.random;
  const ef=(x,y)=>{const m=Math.min(x,y,W-x,H-y),f=Math.min(W,H)*0.06;return m<=0?0:Math.min(1,m/f);};

  function confetti(x,y,n,spread){const c=pal(),s=sc();for(let i=0;i<n;i++){const a=R()*Math.PI*2,v=(6+R()*13)*s*(spread||1);
    parts.push({k:'c',x,y,vx:Math.cos(a)*v,vy:Math.sin(a)*v-5*s,w:(6+R()*9)*s,h:(3+R()*5)*s,r:R()*6,vr:(R()-.5)*.4,col:c[i%c.length],life:1,decay:.006+R()*.008,f:R()*6});}}
  function rainOne(){const c=pal(),s=sc();parts.push({k:'c',x:R()*W,y:-20,vx:(R()-.5)*2*s,vy:(2+R()*3)*s,w:(7+R()*9)*s,h:(4+R()*5)*s,r:R()*6,vr:(R()-.5)*.3,col:c[Math.floor(R()*c.length)],life:1.6,decay:.004,f:R()*6,g:.08});}
  function coinOne(){const s=sc();parts.push({k:'coin',x:W*.2+R()*W*.6,y:-30,vx:(R()-.5)*2.5*s,vy:(1+R()*3)*s,rad:(9+R()*7)*s,f:R()*6,vf:.12+R()*.12,life:1.6,decay:.003,col:coinCol||'#ffc83a'});}
  function puff(x,y,s0){const s=sc();parts.push({k:'p',x,y,vx:(-2-R()*3)*s,vy:(-.6-R()*1.2)*s,rad:(14+R()*16)*s*(s0||1),grow:.6*s,life:.9,decay:.012+R()*.01});}
  function starBurst(x,y,n){const c=pal(),s=sc();for(let i=0;i<n;i++){const a=R()*Math.PI*2,v=(4+R()*10)*s;parts.push({k:'s',x,y,vx:Math.cos(a)*v,vy:Math.sin(a)*v,sz:(6+R()*12)*s,col:c[i%c.length],life:1,decay:.012+R()*.01,t:R()*6,drag:.94});}}
  function sparkOne(){const r=el.alert.getBoundingClientRect(),c=pal(),s=sc();parts.push({k:'s',x:r.left+R()*r.width,y:r.top+r.height*(.25+R()*.7),vx:(R()-.5)*.4*s,vy:-(.4+R()*1.1)*s,sz:(4+R()*9)*s,col:c[Math.floor(R()*c.length)],life:1,decay:.008+R()*.01,t:R()*6,drag:1});}

  function star(x,y,s){cx.beginPath();cx.moveTo(x,y-s);cx.quadraticCurveTo(x,y,x+s,y);cx.quadraticCurveTo(x,y,x,y+s);cx.quadraticCurveTo(x,y,x-s,y);cx.quadraticCurveTo(x,y,x,y-s);cx.fill();}
  function frame(t){
    const dt=Math.min(2,(t-(last||t))/16.67);last=t;const now=performance.now();
    cx.clearRect(0,0,W,H);
    emitters=emitters.filter(e=>{ if(now>e.until) return false; e.acc+=dt*e.rate; while(e.acc>=1){e.fn();e.acc--;} return true; });
    if(sparkOn){acc+=dt;while(acc>3){sparkOne();acc-=3;}}
    for(let i=parts.length-1;i>=0;i--){const p=parts[i];
      if(p.k==='c'){p.vx*=Math.pow(.96,dt);p.vy=p.vy*Math.pow(p.g?.99:.96,dt)+(p.g||.32)*dt*sc();p.x+=p.vx*dt;p.y+=p.vy*dt;p.r+=p.vr*dt;p.f+=.15*dt;p.life-=p.decay*dt;
        cx.save();cx.globalAlpha=Math.max(0,Math.min(1,p.life*1.6))*ef(p.x,p.y);cx.translate(p.x,p.y);cx.rotate(p.r);cx.scale(1,Math.cos(p.f));cx.fillStyle=p.col;cx.fillRect(-p.w/2,-p.h/2,p.w,p.h);cx.restore();}
      else if(p.k==='coin'){p.vy+=.12*dt*sc();p.x+=p.vx*dt;p.y+=p.vy*dt;p.f+=p.vf*dt;p.life-=p.decay*dt;const sx=Math.abs(Math.cos(p.f))+.08;
        cx.save();cx.globalAlpha=Math.max(0,Math.min(1,p.life))*ef(p.x,p.y);cx.translate(p.x,p.y);cx.scale(sx,1);
        cx.fillStyle=p.col;cx.beginPath();cx.arc(0,0,p.rad,0,7);cx.fill();cx.lineWidth=p.rad*.22;cx.strokeStyle='rgba(0,0,0,.28)';cx.stroke();
        cx.fillStyle='rgba(255,255,255,.65)';cx.beginPath();cx.arc(-p.rad*.3,-p.rad*.3,p.rad*.28,0,7);cx.fill();cx.restore();}
      else if(p.k==='p'){p.x+=p.vx*dt;p.y+=p.vy*dt;p.rad+=p.grow*dt;p.life-=p.decay*dt;
        cx.globalAlpha=Math.max(0,p.life)*.35*ef(p.x,p.y);cx.fillStyle='#d9f7f2';cx.beginPath();cx.arc(p.x,p.y,p.rad,0,7);cx.fill();}
      else{p.vx*=p.drag;p.vy*=p.drag;p.x+=p.vx*dt;p.y+=p.vy*dt;p.t+=.12*dt;p.life-=p.decay*dt;const tw=.55+.45*Math.sin(p.t*2.2);
        cx.globalAlpha=Math.max(0,p.life)*tw*ef(p.x,p.y);cx.fillStyle=p.col;cx.shadowColor=p.col;cx.shadowBlur=12;star(p.x,p.y,p.sz*tw);cx.shadowBlur=0;}
      if(p.life<=0||p.y>H+60) parts.splice(i,1);}
    cx.globalAlpha=1;
    if(parts.length||sparkOn||emitters.length) raf=requestAnimationFrame(frame); else {raf=0;last=0;cx.clearRect(0,0,W,H);}
  }
  const loop=()=>{if(!raf){last=0;raf=requestAnimationFrame(frame);}};
  const emit=(fn,rate,ms)=>{emitters.push({fn,rate,until:performance.now()+ms,acc:0});loop();};

  function play(t){
    coinCol=t.coin||null;
    const w=ctr(el.word), e=ctr(el.emb);
    if(t.fx==='confetti'){confetti(w.x,w.y,110);}
    if(t.fx==='rain'){confetti(w.x,w.y,60);emit(rainOne,1.4,2600);}
    if(t.fx==='coins'){confetti(w.x,w.y,40,.8);emit(coinOne,.9,2400);}
    if(t.fx==='stars'){starBurst(w.x,w.y,60);starBurst(e.x,e.y,30);}
    if(t.fx==='smoke'){confetti(w.x,w.y,50,.8);smoke();}
    loop();
  }
  function smoke(){const e=ctr(el.emb);emit(()=>puff(e.r.left+e.r.width*.15+R()*30*sc(),e.r.top+e.r.height*.8),.5,1400);}
  return{play,smoke,sparkle(on){sparkOn=on;if(on)loop();}};
})();

/* =========================================================
   SONIDO SINTETIZADO por evento (sin archivos ni licencias)
   ========================================================= */
const SFX=(()=>{
  let ctx,master,muted=false;
  function init(){if(ctx)return;ctx=new(window.AudioContext||window.webkitAudioContext)();master=ctx.createGain();master.gain.value=CFG.volume;master.connect(ctx.destination);}
  function noise(d){const b=ctx.createBuffer(1,ctx.sampleRate*d,ctx.sampleRate),x=b.getChannelData(0);for(let i=0;i<x.length;i++)x[i]=Math.random()*2-1;const s=ctx.createBufferSource();s.buffer=b;return s;}
  function tone(type,f,t0,dur,vol,f2){const o=ctx.createOscillator(),g=ctx.createGain();o.type=type;o.frequency.setValueAtTime(f,t0);if(f2)o.frequency.exponentialRampToValueAtTime(f2,t0+dur);g.gain.setValueAtTime(vol,t0);g.gain.exponentialRampToValueAtTime(.001,t0+dur);o.connect(g).connect(master);o.start(t0);o.stop(t0+dur+.05);}
  // En el editor de StreamElements el navegador bloquea el audio hasta que el usuario hace clic
  // dentro del widget; en OBS no hace falta. Esto lo desbloquea al primer clic/tecla.
  const unlock=()=>{ try{ init(); ctx.resume(); }catch(e){} };
  ['pointerdown','keydown','touchstart'].forEach(ev=>addEventListener(ev,unlock,{passive:true}));
  function playFile(url,delay){
    const a=new Audio(url); a.volume=Math.min(1,CFG.volume*1.4); a.preload='auto';
    setTimeout(()=>{ a.play().catch(()=>{}); }, Math.max(0,delay-500));
  }
  function play(t,delay){
    if(muted||!CFG.sound)return;
    if(t.sound){ playFile(t.sound,delay); return; }
    init(); if(ctx.state==='suspended') ctx.resume().catch(()=>{});
    const s=t.sfx, t0=ctx.currentTime, ti=t0+delay/1000;
    // whoosh
    const n=noise(.7),f=ctx.createBiquadFilter(),g=ctx.createGain();f.type='bandpass';f.Q.value=1.2;f.frequency.setValueAtTime(300,ti-.4);f.frequency.exponentialRampToValueAtTime(5000,ti);
    g.gain.setValueAtTime(0,ti-.45);g.gain.linearRampToValueAtTime(.3,ti-.1);g.gain.exponentialRampToValueAtTime(.001,ti+.15);n.connect(f).connect(g).connect(master);n.start(Math.max(t0,ti-.45));
    // golpe
    tone('sine',150,ti,.5,.9,42);
    const sn=noise(.2),sg=ctx.createGain(),sf=ctx.createBiquadFilter();sf.type='highpass';sf.frequency.value=1800;sg.gain.setValueAtTime(.35,ti);sg.gain.exponentialRampToValueAtTime(.001,ti+.18);sn.connect(sf).connect(sg).connect(master);sn.start(ti);
    // acorde synthwave según el evento
    [1,1.26,1.5,1.89,2.52].forEach((m,i)=>{const st=ti+.06+i*.07,o1=ctx.createOscillator(),o2=ctx.createOscillator(),lp=ctx.createBiquadFilter(),ag=ctx.createGain();
      o1.type=o2.type='sawtooth';o1.frequency.value=s.root*m;o2.frequency.value=s.root*m*1.006;lp.type='lowpass';lp.frequency.setValueAtTime(3200,st);lp.frequency.exponentialRampToValueAtTime(600,st+1.4);
      ag.gain.setValueAtTime(0,st);ag.gain.linearRampToValueAtTime(.065,st+.02);ag.gain.exponentialRampToValueAtTime(.001,st+1.6);o1.connect(lp);o2.connect(lp);lp.connect(ag).connect(master);o1.start(st);o2.start(st);o1.stop(st+1.7);o2.stop(st+1.7);});
    // extra característico
    if(s.extra==='coins'){for(let i=0;i<7;i++){const c=ti+.15+i*.11;tone('sine',1900+Math.random()*700,c,.25,.12);tone('sine',3800+Math.random()*900,c,.12,.05);}}
    if(s.extra==='sparkle'){for(let i=0;i<5;i++)tone('triangle',1400+i*260,ti+.25+i*.06,.3,.06);}
    if(s.extra==='chomp'){tone('square',90,ti-.25,.12,.25,50);tone('square',80,ti-.08,.12,.25,45);}
    if(s.extra==='engine'){const o=ctx.createOscillator(),lfo=ctx.createOscillator(),lg=ctx.createGain(),eg=ctx.createGain(),lp=ctx.createBiquadFilter();
      o.type='sawtooth';o.frequency.setValueAtTime(48,t0);o.frequency.linearRampToValueAtTime(95,ti);o.frequency.linearRampToValueAtTime(55,ti+.8);
      lfo.frequency.value=18;lg.gain.value=.12;lfo.connect(lg).connect(eg.gain);lp.type='lowpass';lp.frequency.value=700;
      eg.gain.setValueAtTime(.18,t0);eg.gain.linearRampToValueAtTime(.001,ti+1);o.connect(lp).connect(eg).connect(master);o.start(t0);lfo.start(t0);o.stop(ti+1.05);lfo.stop(ti+1.05);
      tone('sine',60,ti+.5,.2,.5,40);tone('sine',60,ti+.85,.2,.4,40);}
    if(s.extra==='prop'){const nz=noise(1.6),bp=ctx.createBiquadFilter(),am=ctx.createGain(),lfo=ctx.createOscillator(),lg=ctx.createGain(),out=ctx.createGain();
      bp.type='bandpass';bp.frequency.value=420;bp.Q.value=2;lfo.frequency.value=32;lg.gain.value=.5;am.gain.value=.5;lfo.connect(lg).connect(am.gain);
      out.gain.setValueAtTime(.001,t0);out.gain.linearRampToValueAtTime(.5,ti-.2);out.gain.exponentialRampToValueAtTime(.001,ti+1.2);nz.connect(bp).connect(am).connect(out).connect(master);nz.start(t0);lfo.start(t0);lfo.stop(ti+1.3);}
  }
  return{play,init,toggle(){muted=!muted;return muted;}};
})();



/* =========================================================
   CONFIGURACION (campos personalizados de Streamlabs)
   ========================================================= */
const RAW = {};
document.querySelectorAll('#gtacfg textarea').forEach(t => {
  const v = t.value; RAW[t.dataset.k] = /^\{gta[\w]*\}$/.test(v.trim()) ? undefined : v;   // sin reemplazar = valor por defecto
});
const val = (k, def) => (RAW[k] === undefined || RAW[k] === null || RAW[k] === 'null') ? def : RAW[k];
const num = (k, def) => { const n = parseFloat(String(val(k, def)).replace(',', '.')); return isNaN(n) ? def : n; };
window.__SIZE = num('gtaSize', 100) / 100; fitStage();
HOLD = num('gtaHold', 6) * 1000;
CFG.volume = num('gtaVolume', 60) / 100;
const SOUND_MODE = val('gtaSound', 'file');
CFG.sound = SOUND_MODE !== 'off';
CFG.showMsg = val('gtaShowMsg', 'si') !== 'no';
CFG.currency = val('gtaCurrency', '$');
CFG.minTip = num('gtaMinTip', 0); CFG.minCheer = num('gtaMinCheer', 0); CFG.minKicks = num('gtaMinKicks', 0);
if (val('gtaPalms', 'si') === 'no') document.body.classList.add('no-palms');
const DEF = {"follow": {"img": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/stickers/flamenco.png", "snd": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/sonidos/seguidor.mp3", "kicker": "nuevo", "word": "SEGUIDOR", "msg": "¡Bienvenido a Vice City!"}, "sub": {"img": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/stickers/caiman.png", "snd": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/sonidos/suscriptor.mp3", "kicker": "nuevo", "word": "SUSCRIPTOR", "msg": "¡Ya es parte de la banda!"}, "resub": {"img": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/stickers/sol.png", "snd": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/sonidos/resub.mp3", "kicker": "de vuelta", "word": "RESUB", "msg": "{amount} meses en Vice City"}, "gift": {"img": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/stickers/avioneta.png", "snd": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/sonidos/regalos.mp3", "kicker": "llueven", "word": "REGALOS", "msg": "regaló {amount} subs a la banda"}, "tip": {"img": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/stickers/manati.png", "snd": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/sonidos/donacion.mp3", "kicker": "nueva", "word": "DONACIÓN", "msg": "donó {amount} ¡gracias!"}, "raid": {"img": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/stickers/lowrider.png", "snd": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/sonidos/raid.mp3", "kicker": "llegó un", "word": "RAID", "msg": "llega con {amount} espectadores"}, "kicks": {"img": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/stickers/palmera.png", "snd": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/sonidos/kicks.mp3", "kicker": "llueven", "word": "KICKS", "msg": "envió {amount} KICKs"}, "cheer": {"img": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/stickers/vi.png", "snd": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/sonidos/bits.mp3", "kicker": "llueven", "word": "BITS", "msg": "lanzó {amount} bits"}, "host": {"img": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/stickers/rstar.png", "snd": "https://cdn.jsdelivr.net/gh/alejox/gta6-alertas@d926553af1227572d2e1112b55b146e9cb55c1c1/sonidos/host.mp3", "kicker": "nuevo", "word": "HOST", "msg": "nos hostea con {amount} espectadores"}};
for (const k in TYPES) {
  const t = TYPES[k], p = 'gta_' + k + '_', d = DEF[k];
  t.on = val(p + 'on', 'si') !== 'no';
  t.img = val(p + 'img', '') || d.img;
  t.sound = SOUND_MODE === 'file' ? (val(p + 'snd', '') || d.snd) : '';
  t.kicker = val(p + 'kicker', d.kicker);
  t.word = val(p + 'word', '') || d.word;
  t.tpl = val(p + 'msg', d.msg);
  t.tplMonths = val(p + 'msgm', t.tpl);
  const i = new Image(); i.src = t.img;                       // precarga
  if (t.sound) { const a = new Audio(); a.preload = 'auto'; a.src = t.sound; }
}

/* =========================================================
   COLA PROPIA (Streamlabs no trae cola para widgets personalizados)
   ========================================================= */
const queue = []; let busy = false;
async function next() {
  if (busy || !queue.length) return; busy = true;
  const d = queue.shift();
  try { await Promise.race([Promise.all(['118px Bungee','70px Pacifico','italic 900 54px Rubik','italic 800 30px Rubik'].map(x => document.fonts.load(x))), new Promise(r => setTimeout(r, 2500))]); } catch (e) {}
  const t = await build(d);
  enter(t); SFX.play(t, (t.enter === 'fly' || t.enter === 'drive') ? 840 : 560);
  await new Promise(r => setTimeout(r, HOLD));
  await leave(t); await new Promise(r => setTimeout(r, 400));
  busy = false; next();
}
function play(d) { const t = TYPES[d.type]; if (!t || t.on === false) return; queue.push(d); next(); }
window.playAlert = play;

/* =========================================================
   EVENTOS DE STREAMLABS (Twitch, YouTube, Kick, Facebook...)
   ========================================================= */
const seenGift = {};
function nameOf(d) { return d.display_name || d.from_display_name || d.name || d.from || 'Anónimo'; }
function handle(d) {
  if (!d || typeof d !== 'object') return;
  if (Array.isArray(d.message) && d.message.length && typeof d.message[0] === 'object') {   // formato alterno {type, message:[...]}
    d.message.forEach(m => handle(Object.assign({ type: d.type, platform: d.for || d.platform }, m))); return;
  }
  const t = String(d.type || '').toLowerCase();
  const name = nameOf(d);
  const msg = typeof d.message === 'string' ? d.message : '';
  const amt = +d.amount || 0;
  if (/kick/.test(t)) {                                       // KICKs de Kick
    const n = +d.amount || +d.kicks || +d.count || 0;
    if (n >= CFG.minKicks) play({ type: 'kicks', name, amount: n, message: msg }); return;
  }
  if (t === 'follow' || t === 'follower') { play({ type: 'follow', name }); return; }
  if (t === 'donation' || t === 'superchat' || /donation|tip/.test(t)) {
    if (amt >= CFG.minTip) play({ type: 'tip', name, amount: amt, fmt: d.formattedAmount || d.formatted_amount || '', message: msg }); return;
  }
  if (t === 'bits' || t === 'cheer' || t === 'stars') { if (amt >= CFG.minCheer) play({ type: 'cheer', name, amount: amt, message: msg }); return; }
  if (t === 'raid') { play({ type: 'raid', name, amount: +d.raiders || +d.viewers || amt }); return; }
  if (t === 'host') { play({ type: 'host', name, amount: +d.viewers || amt }); return; }
  const isSub = /sub|member|sponsor/.test(t) || (!t && (d.months != null || d.sub_plan != null));
  if (isSub) {
    const st = String(d.sub_type || '').toLowerCase();
    const gifter = d.gifter_display_name || d.gifter;
    const cid = d.community_gift_id;
    const count = +d.count || +d.giftCount || 1;
    if (/mystery|community|mass/.test(st + t) || (cid && count > 1)) {   // regalo masivo
      if (cid) { if (seenGift[cid]) return; seenGift[cid] = 1; }
      play({ type: 'gift', name: gifter || name, amount: count }); return;
    }
    if (gifter || /gift/.test(st + t)) {                     // regalo individual
      if (cid && seenGift[cid]) return;                       // hijo de un regalo masivo ya mostrado
      play({ type: 'gift', name: gifter || name, amount: 1 }); return;
    }
    const months = +d.months || +d.streak_months || 1;
    if (t === 'resub' || st === 'resub' || months > 1) play({ type: 'resub', name, amount: months, message: msg });
    else play({ type: 'sub', name, amount: 1, message: msg });
  }
}
document.addEventListener('onEventReceived', obj => { try { handle(obj && obj.detail); } catch (e) { console.error(e); } });

})();
