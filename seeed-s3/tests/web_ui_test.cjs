const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const html = fs.readFileSync(path.join(__dirname, '../firmware/XIAOS3Gatos/web_ui.h'), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const documentEvents = {}, windowEvents = {}, timers = new Map(), elements = new Map();
let now = 100000, nextTimer = 0, imageChanges = 0, offline = false, checks = 0;
const status = {
  label: 'gris', busy: false, frame: 20, ageMs: 20, foreground: .6, useful: .7,
  fps: 8, videoFps: 26, streamFps: 26, videoFrame: 80, streamFrame: 80,
  streamClients: 1, streamPort: 81, analysisMs: 4, decodeMs: 60, freePsram: 6000000,
  background: true, orangeSamples: 8, graySamples: 8, message: 'Calibración recuperada',
  options: {x: 20, y: 20, width: 60, height: 60, difference: 20, minForeground: 10}
};
function element(id) {
  if (!elements.has(id)) elements.set(id, {
    style: {}, dataset: {}, textContent: '', className: '',
    elements: {namedItem: () => null},
    removeAttribute(name) { if (name === 'src') this._src = ''; },
    set src(value) { this._src = value; imageChanges++; }, get src() { return this._src; }
  });
  return elements.get(id);
}
const document = {hidden: false, getElementById: element, querySelectorAll: () => [],
  addEventListener: (type, callback) => documentEvents[type] = callback};
class Clock extends Date { static now() { return now; } }
const context = vm.createContext({document, window: {addEventListener: (type, callback) => windowEvents[type] = callback},
  location: {href: 'http://192.168.4.1/'}, Date: Clock, URL, URLSearchParams, AbortSignal,
  setTimeout: (fn, delay) => { const id = ++nextTimer; timers.set(id, {fn, delay}); return id; },
  clearTimeout: id => timers.delete(id),
  fetch: async () => { if (offline) throw Error('Wi-Fi interrumpido'); return {ok: true, json: async () => ({...status})}; }
});
function check(condition) { assert.ok(condition); checks++; }
async function flush() { await new Promise(resolve => setImmediate(resolve)); }
async function poll() { timers.clear(); await vm.runInContext('poll()', context); }
async function retry() {
  const timer = [...timers].find(([, task]) => task.delay === 1000);
  assert.ok(timer, 'Debe existir un reintento de vídeo');
  timers.delete(timer[0]); await timer[1].fn();
}
(async () => {
  vm.runInContext(script, context);
  await flush();
  check(element('preview').src.startsWith('http://192.168.4.1:81/stream?t='));
  const firstUrl = element('preview').src;
  for (let i = 0; i < 5; i++) { now += 500; status.streamFrame++; await poll(); }
  check(element('preview').src === firstUrl && imageChanges === 1);
  // El socket sigue abierto y dice tener FPS, pero no envía imágenes nuevas.
  now += 3500;
  await poll();
  check(element('preview').src === '');
  await retry();
  check(element('preview').src !== firstUrl && imageChanges === 2);
  const secondUrl = element('preview').src;
  now += 500; status.streamFrame++; await poll();
  check(element('preview').src === secondUrl);
  // También renovar un socket sano por si solo se detuvo el renderizado.
  now += 30000; status.streamFrame++; await poll();
  check(element('preview').src === '');
  await retry(); check(imageChanges === 3);
  document.hidden = true; documentEvents.visibilitychange();
  check(element('preview').src === '');
  now += 10000; await poll(); element('preview').onerror();
  check(![...timers.values()].some(task => task.delay === 1000));
  document.hidden = false; documentEvents.visibilitychange(); check(imageChanges === 4);
  offline = true; await poll(); check(element('preview').src === '');
  offline = false; now += 2000; await poll(); check(imageChanges === 5);
  windowEvents.pagehide(); check(element('preview').src === '');
  now += 1000; windowEvents.pageshow(); check(imageChanges === 6);
  // Los múltiples avisos de error no crean múltiples conexiones simultáneas.
  element('preview').onerror(); element('preview').onerror();
  check([...timers.values()].filter(task => task.delay === 1000).length === 1);
  await retry(); check(imageChanges === 7);
  console.log(`web_ui: ${checks} comprobaciones de continuidad y recuperación correctas`);
})().catch(error => { console.error(error); process.exitCode = 1; });
