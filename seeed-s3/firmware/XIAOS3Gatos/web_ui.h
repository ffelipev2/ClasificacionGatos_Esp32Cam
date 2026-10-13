#pragma once
#include <Arduino.h>

static const char WEB_UI[] PROGMEM = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>XIAO ESP32S3 Sense · Gatos</title><style>
:root{color-scheme:dark;font:16px system-ui,sans-serif;background:#101720;color:#ebf0f6}
*{box-sizing:border-box}body{margin:0}main{max-width:1000px;margin:auto;padding:26px 18px}
h1{font-size:26px;margin:0 0 8px}h2{font-size:18px;margin:0 0 14px}p{line-height:1.5;color:#bbc8d6}
.layout{display:grid;grid-template-columns:1.3fr 1fr;gap:18px}.card{background:#1a2532;border:1px solid #324254;border-radius:16px;padding:20px}
.camera{position:relative;background:#070c12;aspect-ratio:4/3;border-radius:10px;overflow:hidden}
canvas{display:block;width:100%;height:100%;object-fit:contain}.roi{position:absolute;border:2px dashed #6fe1ad;pointer-events:none}
#previewState{position:absolute;bottom:8px;left:8px;padding:4px 8px;border-radius:5px;background:#101720dd;color:#ebf0f6;font-size:12px}
.status{font-size:26px;font-weight:700;margin:16px 0 6px}.orange{color:#ffb166}.gray{color:#d5dde5}.muted{color:#b7c6d6}
.meta{font-size:14px;color:#aebed0;line-height:1.6}.row{display:flex;gap:8px;flex-wrap:wrap}
button{font:inherit;border:1px solid #4a6077;color:#f0f5fa;background:#31465e;border-radius:9px;padding:11px 14px;cursor:pointer}
button:hover{background:#405b77}button:disabled{opacity:.5;cursor:wait}.orange-button{border-color:#b57841}.gray-button{border-color:#91a2b4}
.step{padding:12px 0;border-bottom:1px solid #324254}.step:first-of-type{padding-top:0}.step:last-of-type{border:0}
.step p{font-size:14px;margin:0 0 10px}.count{font-size:13px;margin-left:6px;color:#aebed0}
#notice{min-height:42px;margin-top:14px;color:#a9e9ca;white-space:pre-wrap}details{margin-top:18px}summary{cursor:pointer;font-weight:600}
.fields{display:grid;grid-template-columns:repeat(2,1fr);gap:12px;margin:18px 0}
label{font-size:14px;color:#bbc8d6}input{display:block;width:100%;padding:8px;margin-top:6px;background:#0e1722;color:#fff;border:1px solid #485e76;border-radius:7px;font:inherit}
.note{font-size:13px}footer{margin-top:20px;color:#8194aa;font-size:13px}
@media(max-width:740px){.layout{grid-template-columns:1fr}main{padding:18px 12px}}
</style></head><body><main>
<h1>XIAO ESP32S3 Sense · Naranja o gris</h1>
<p>Clasificación del pelaje en la zona marcada. Todo el análisis ocurre en la placa.</p>
<div class="layout"><section class="card" aria-label="Cámara y resultado">
<div class="camera"><canvas id="preview" width="320" height="240" role="img" aria-label="Vista de la cámara"></canvas><div class="roi" id="roi"></div><span id="previewState">Esperando imagen…</span></div>
<div class="status muted" id="result" aria-live="polite">Conectando…</div>
<div class="meta" id="metrics">Esperando imagen</div>
<div class="meta" id="performance"></div>
<p class="note">La cámara debe estar fija y un solo gato debe ocupar la zona marcada. El sistema compara colores; un objeto parecido también puede coincidir.</p>
</section><section class="card"><h2>Calibración</h2>
<div class="step"><p><b>1. Fondo vacío.</b> Retira los gatos de la zona y mantén la luz habitual. Registrar un nuevo fondo borra las muestras anteriores.</p>
<button data-learn="fondo">Registrar fondo</button><span class="count" id="bgCount"></span></div>
<div class="step"><p><b>2. Gato naranja.</b> Coloca su pelaje en la zona, sin manos ni ropa. Reuniremos ocho imágenes válidas; si se mueve, continúa hasta que termine.</p>
<button data-learn="naranja" class="orange-button">Aprender naranja</button><span class="count" id="orangeCount"></span></div>
<div class="step"><p><b>3. Gato gris.</b> Repite con el gato gris. Puedes añadir registros de ambos en distintas posiciones.</p>
<button data-learn="gris" class="gray-button">Aprender gris</button><span class="count" id="grayCount"></span></div>
<div id="notice" role="status" aria-live="polite"></div>
<button id="reset">Borrar calibración</button>
</section></div>
<details class="card"><summary>Zona y sensibilidad</summary>
<p class="note">Valores en porcentaje. Al cambiar la zona o la sensibilidad se borra la calibración: registra el fondo y ambos gatos otra vez.</p>
<form id="settings"><div class="fields">
<label>Inicio horizontal (%)<input name="x" type="number" min="0" max="90" required></label>
<label>Inicio vertical (%)<input name="y" type="number" min="0" max="90" required></label>
<label>Ancho (%)<input name="width" type="number" min="10" max="100" required></label>
<label>Alto (%)<input name="height" type="number" min="10" max="100" required></label>
<label>Diferencia con el fondo (5–80)<input name="difference" type="number" min="5" max="80" required></label>
<label>Área mínima ocupada (%)<input name="minForeground" type="number" min="5" max="80" required></label>
</div><button type="submit">Guardar ajustes</button></form></details>
<footer>Vista 3.4 · Sin servicios externos ni conexión a internet. Calibración guardada en la memoria de la ESP32.</footer>
</main><script>
const $=id=>document.getElementById(id);
const labels={sin_calibrar:'Falta calibración',sin_objeto:'Zona vacía',naranja:'Pelaje naranja',gris:'Pelaje gris',indeterminado:'Indeterminado',poca_luz:'Muy poca luz',error_camara:'Error de cámara'};
let initialized=false,offline=false,lastOptions='',previewUrl='',previewTimer=null,previewController=null,previewRunning=false,previewEnabled=false,previewEpoch=0,previewInterval=33,previewFrame=0,previewReceivedFrame=0,previewGoodAt=0,previewFps=0,previewRateAt=0,previewRateFrames=0,pageActive=true;
let pendingPreview=null,previewPainting=false,paintController=null,previewDecodeMs=0;
const previewCanvas=$('preview'),previewContext=previewCanvas.getContext('2d');
async function json(url,options={}){
  const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),10000);
  try{const response=await fetch(url,{cache:'no-store',...options,signal:controller.signal});
    const body=await response.json();if(!response.ok)throw Error(body.message||'No se pudo completar la acción');return body;
  }finally{clearTimeout(timeout)}
}
function setBusy(busy){document.querySelectorAll('button').forEach(b=>b.disabled=busy)}
function previewMessage(text){$('previewState').textContent=text;$('previewState').hidden=!text}
function schedulePreview(delay=0){
  if(!previewEnabled||!pageActive||document.hidden||previewRunning||previewTimer!==null)return;
  previewTimer=setTimeout(()=>{previewTimer=null;previewTick()},delay);
}
function startPreview(){schedulePreview()}
function stopPreview(){
  ++previewEpoch;clearTimeout(previewTimer);previewTimer=null;
  if(previewController)previewController.abort();
  if(paintController)paintController.abort();pendingPreview=null;
  previewMessage(previewGoodAt?'Vídeo en pausa':'Esperando imagen…');
}
async function decodePreview(blob,signal){
  if(typeof createImageBitmap==='function'){
    try{return await createImageBitmap(blob)}catch(error){/* Compatibilidad con tablets antiguas. */}
  }
  return await new Promise((resolve,reject)=>{
    const image=new Image(),url=URL.createObjectURL(blob);
    const cleanup=()=>{URL.revokeObjectURL(url);signal.removeEventListener('abort',cancel);image.onload=null;image.onerror=null};
    const cancel=()=>{cleanup();image.src='';reject(Error('Imagen cancelada'))};
    image.onload=()=>{cleanup();resolve(image)};
    image.onerror=()=>{cleanup();reject(Error('Imagen incompleta'))};
    if(signal.aborted){cancel();return}signal.addEventListener('abort',cancel,{once:true});image.src=url;
  });
}
async function paintPreview(){
  if(previewPainting||!pendingPreview||!pageActive||document.hidden)return;
  previewPainting=true;
  try{
    while(pendingPreview&&pageActive&&!document.hidden){
      const item=pendingPreview;pendingPreview=null;
      const controller=new AbortController();paintController=controller;
      const timeout=setTimeout(()=>controller.abort(),5000),started=Date.now();let image=null;
      try{
        image=await decodePreview(item.blob,controller.signal);
        if(controller.signal.aborted||item.epoch!==previewEpoch||!pageActive||document.hidden)continue;
        if((image.naturalWidth||image.width)!==320||(image.naturalHeight||image.height)!==240)throw Error('Dimensiones de imagen inválidas');
        // La recepción de la siguiente foto puede avanzar mientras se
        // decodifica esta. Solo se conserva una foto pendiente, la más nueva.
        previewContext.drawImage(image,0,0,320,240);
        const now=Date.now();
        if(!previewRateAt||now-previewGoodAt>1500){previewRateAt=now;previewRateFrames=0;previewFps=0}
        previewFrame=item.frame;previewGoodAt=now;previewMessage('');
        ++previewRateFrames;
        if(now-previewRateAt>=1000){previewFps=previewRateFrames*1000/(now-previewRateAt);previewRateAt=now;previewRateFrames=0}
        const elapsed=now-started;previewDecodeMs=previewDecodeMs?previewDecodeMs*.8+elapsed*.2:elapsed;
      }catch(error){
        if(item.epoch===previewEpoch&&pageActive&&!document.hidden)previewMessage(previewGoodAt?'Reconectando vídeo · última imagen conservada':'Conectando con la cámara…');
      }finally{
        if(image&&typeof image.close==='function')image.close();clearTimeout(timeout);paintController=null;
      }
    }
  }finally{previewPainting=false}
}
async function previewTick(){
  if(previewRunning||!previewEnabled||!pageActive||document.hidden)return;
  previewRunning=true;
  const epoch=previewEpoch,started=Date.now(),controller=new AbortController();previewController=controller;
  const timeout=setTimeout(()=>controller.abort(),5000);let delay=Math.max(previewInterval,Math.ceil(previewDecodeMs*.9));
  try{
    const response=await fetch(previewUrl+'?t='+started+'&after='+previewReceivedFrame,{cache:'no-store',signal:controller.signal});
    if(response.status===204){if(previewGoodAt&&Date.now()-previewGoodAt>3000)previewMessage('Esperando una imagen nueva…');return}
    if(!response.ok)throw Error('Esperando la cámara');
    const frame=Number(response.headers.get('X-Frame-Number'));
    const blob=await response.blob();
    if(!blob.size||blob.size>98304)throw Error('Imagen incompleta');
    if(frame&&frame===previewReceivedFrame){if(Date.now()-previewGoodAt>3000)previewMessage('Esperando una imagen nueva…');return}
    if(controller.signal.aborted||epoch!==previewEpoch||!pageActive||document.hidden)return;
    previewReceivedFrame=frame;pendingPreview={blob,frame,epoch};paintPreview();
  }catch(error){
    if(epoch===previewEpoch&&pageActive&&!document.hidden)previewMessage(previewGoodAt?'Reconectando vídeo · última imagen conservada':'Conectando con la cámara…');
    delay=250;
  }finally{
    clearTimeout(timeout);previewController=null;previewRunning=false;
    schedulePreview(Math.max(0,delay-(Date.now()-started)));
  }
}
document.addEventListener('visibilitychange',()=>{if(document.hidden)stopPreview();else startPreview()});
window.addEventListener('pagehide',()=>{pageActive=false;stopPreview()});
window.addEventListener('pageshow',()=>{pageActive=true;startPreview()});
async function poll(){
  try{
    const s=await json('/api/status');offline=false;
    $('result').textContent=s.busy?'Registrando '+s.job+'…':(labels[s.label]||s.label);
    $('result').className='status '+(s.label==='naranja'?'orange':s.label==='gris'?'gray':'muted');
    $('metrics').textContent='Zona ocupada: '+Math.round(s.foreground*100)+'% · Píxeles útiles: '+Math.round((s.useful||0)*100)+'% · Imagen '+s.frame+(s.frame?' · Hace '+(s.ageMs/1000).toFixed(1)+' s':'')+' · Reconocimiento '+s.fps.toFixed(1)+'/s · Vista '+(previewGoodAt&&Date.now()-previewGoodAt<3000?previewFps:0).toFixed(1)+' FPS';
    $('performance').textContent='Análisis '+s.analysisMs.toFixed(1)+' ms · Decodificación '+s.decodeMs.toFixed(1)+' ms · PSRAM libre '+(s.freePsram/1048576).toFixed(1)+' MB';
    $('bgCount').textContent=s.background?'Registrado':'Pendiente';
    $('orangeCount').textContent=s.orangeSamples+' imágenes';$('grayCount').textContent=s.graySamples+' imágenes';
    $('notice').textContent=s.busy?'Capturas válidas: '+s.progress+'/'+s.total+' · '+s.message:s.message;
    setBusy(s.busy);document.querySelectorAll('[data-learn]').forEach(b=>{if(b.dataset.learn!=='fondo')b.disabled=s.busy||!s.background});
    if(!initialized||lastOptions!==JSON.stringify(s.options)){lastOptions=JSON.stringify(s.options);for(const [key,value]of Object.entries(s.options)){const input=$('settings').elements.namedItem(key);if(input)input.value=value}initialized=true}
    const r=$('roi');r.style.left=s.options.x+'%';r.style.top=s.options.y+'%';r.style.width=s.options.width+'%';r.style.height=s.options.height+'%';
    if(s.streamPort&&s.videoFrame){
      const url=new URL(location.href);url.port=s.streamPort;url.pathname='/frame';url.search='';url.hash='';
      if(previewUrl!==url.href){stopPreview();previewUrl=url.href;previewFrame=0;previewReceivedFrame=0}
      previewInterval=Math.max(33,Number(s.previewIntervalMs)||33);previewEnabled=true;startPreview();
    }
  }catch(error){offline=true;$('result').textContent='Reintentando el estado de la placa';$('result').className='status muted';$('notice').textContent=error.message;setBusy(true)}
  setTimeout(poll,offline?2000:500);
}
document.querySelectorAll('[data-learn]').forEach(b=>b.onclick=async()=>{
  setBusy(true);try{const s=await json('/api/learn?label='+b.dataset.learn,{method:'POST'});$('notice').textContent=s.message}catch(e){$('notice').textContent=e.message;setBusy(false)}
});
$('reset').onclick=async()=>{try{const s=await json('/api/reset',{method:'POST'});$('notice').textContent=s.message}catch(e){$('notice').textContent=e.message}};
$('settings').onsubmit=async e=>{e.preventDefault();const form=e.currentTarget;const x=+form.elements.x.value,y=+form.elements.y.value,w=+form.elements.width.value,h=+form.elements.height.value;
  if(x+w>100||y+h>100){$('notice').textContent='La zona debe quedar dentro de la imagen';return}
  try{const s=await json('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(new FormData(form))});$('notice').textContent=s.message;initialized=false}catch(e){$('notice').textContent=e.message}
};
poll();
</script></body></html>)HTML";
