const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const script=fs.readFileSync(path.join(__dirname,'../firmware/XIAOS3Gatos/web_ui.h'),'utf8').match(/<script>([\s\S]*?)<\/script>/)[1];
const timers=new Map(),elements=new Map(),events={},windowEvents={};
let now=100000,nextTimer=0,draws=0,closed=0,checks=0,frame=80,apiFails=false,photoFails=false,noContent=false,badSize=false,bitmapFails=false,pending=false,decodeBlocked=false,releaseDecode=null,photoRequests=0,maxRequests=0,revoked=0;
const status={label:'gris',busy:false,frame:20,ageMs:20,foreground:.6,useful:.7,fps:8,videoFrame:80,streamPort:81,previewIntervalMs:33,analysisMs:4,decodeMs:60,freePsram:6000000,background:true,orangeSamples:8,graySamples:8,message:'Guardado',options:{x:20,y:20,width:60,height:60}};
function element(id){if(!elements.has(id))elements.set(id,{style:{},elements:{namedItem:()=>null},getContext:()=>({drawImage(){draws++},clearRect(){throw Error('La vista no debe borrarse')}}),removeAttribute(){throw Error('La vista no debe vaciarse')}});return elements.get(id)}
class Clock extends Date{static now(){return now}}
class TestURL extends URL{static createObjectURL(){return 'blob:test'}static revokeObjectURL(){revoked++}}
class TestImage{constructor(){this.naturalWidth=320;this.naturalHeight=240}set src(value){if(value)queueMicrotask(()=>{if(this.onload)this.onload()})}}
const document={hidden:false,getElementById:element,querySelectorAll:()=>[],addEventListener:(type,fn)=>events[type]=fn};
const context=vm.createContext({document,window:{addEventListener:(type,fn)=>windowEvents[type]=fn},location:{href:'http://192.168.4.1/'},Date:Clock,URL:TestURL,URLSearchParams,AbortController,Image:TestImage,
setTimeout:(fn,delay)=>{const id=++nextTimer;timers.set(id,{fn,delay});return id},clearTimeout:id=>timers.delete(id),
createImageBitmap:async()=>{if(decodeBlocked)await new Promise(resolve=>releaseDecode=resolve);if(bitmapFails)throw Error('Compatibilidad');return {width:badSize?640:320,height:240,close(){closed++}}},
fetch:async(url,options)=>{if(url==='/api/status'){if(apiFails)throw Error('Timeout de 10 segundos');return {ok:true,json:async()=>status}}photoRequests++;maxRequests=Math.max(maxRequests,photoRequests);try{if(pending)await new Promise((resolve,reject)=>options.signal.addEventListener('abort',()=>reject(Error('Abortado')),{once:true}));if(photoFails)throw Error('Corte Wi-Fi');return noContent?{ok:true,status:204}:{ok:true,status:200,headers:{get:()=>String(frame)},blob:async()=>({size:1024})}}finally{photoRequests--}}
});
const check=condition=>{assert.ok(condition);checks++};
async function tick(){vm.runInContext('clearTimeout(previewTimer);previewTimer=null',context);await vm.runInContext('previewTick()',context);await new Promise(resolve=>setImmediate(resolve))}
(async()=>{vm.runInContext(script,context);await new Promise(resolve=>setImmediate(resolve));check(vm.runInContext('previewUrl',context)==='http://192.168.4.1:81/frame');
await tick();check(draws===1&&closed===1);frame++;now+=67;await tick();check(draws===2);
apiFails=true;now+=10000;await vm.runInContext('poll()',context);frame++;await tick();check(draws===3);check(element('result').textContent.includes('Reintentando'));
photoFails=true;await tick();check(draws===3);check(element('previewState').textContent.includes('conservada'));photoFails=false;frame++;await tick();check(draws===4&&element('previewState').hidden);
badSize=true;frame++;await tick();check(draws===4);badSize=false;bitmapFails=true;frame++;await tick();check(draws===5&&revoked===1);bitmapFails=false;
await tick();check(draws===5);now+=4000;await tick();check(element('previewState').textContent.includes('nueva'));
pending=true;const request=vm.runInContext('clearTimeout(previewTimer);previewTimer=null;previewTick()',context);await new Promise(resolve=>setImmediate(resolve));await vm.runInContext('previewTick()',context);check(photoRequests===1&&maxRequests===1);document.hidden=true;events.visibilitychange();await request;check(photoRequests===0&&draws===5);pending=false;
document.hidden=false;events.visibilitychange();frame++;await tick();check(draws===6);windowEvents.pagehide();await tick();check(draws===6);windowEvents.pageshow();frame++;await tick();check(draws===7);
check(closed===7);noContent=true;now+=4000;await tick();check(draws===7&&element('previewState').textContent.includes('nueva'));noContent=false;
// La red avanza mientras el JPEG anterior se decodifica; la cola solo
// conserva la imagen pendiente más reciente.
decodeBlocked=true;frame++;await tick();check(vm.runInContext('previewPainting',context)&&draws===7);
frame++;await tick();const queued=frame;check(vm.runInContext('pendingPreview.frame',context)===queued);
frame++;await tick();check(vm.runInContext('pendingPreview.frame',context)===frame&&draws===7);check(maxRequests===1);
decodeBlocked=false;releaseDecode();await new Promise(resolve=>setImmediate(resolve));check(vm.runInContext('previewFrame',context)===frame&&draws===9);
check(vm.runInContext('pendingPreview===null&&!previewPainting',context));console.log('web_ui: '+checks+' comprobaciones de persistencia, errores y concurrencia correctas');})().catch(error=>{console.error(error);process.exitCode=1});
