const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
let playwright;
try { playwright = require('playwright'); }
catch { playwright = require('../../.build/browser-tests/node_modules/playwright'); }

const root = path.resolve(__dirname, '../..');
const output = path.join(root, '.build/seeed-s3/tests');
const html = fs.readFileSync(path.join(__dirname, '../firmware/XIAOS3Gatos/web_ui.h'), 'utf8')
  .match(/R"HTML\(([\s\S]*?)\)HTML"/)[1];
const pending = new Set();
let apiFault = false, photoFault = '', photoActive = 0, maxPhotoActive = 0, frame = 0;
const listen = server => new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const close = server => new Promise(resolve => { server.close(resolve); server.closeAllConnections(); });

(async () => {
  fs.mkdirSync(output, {recursive: true});
  const edge = 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
  const executablePath = process.env.PREVIEW_BROWSER || (fs.existsSync(edge) ? edge : undefined);
  const browser = await playwright.chromium.launch({headless: true, executablePath});
  let pageServer, photoServer;
  try {
    const fixture = await browser.newPage();
    await fixture.setContent('<canvas width="320" height="240"></canvas>');
    const data = await fixture.evaluate(() => {
      const canvas = document.querySelector('canvas'), ctx = canvas.getContext('2d');
      ctx.fillStyle = '#299d70'; ctx.fillRect(0, 0, 320, 240);
      ctx.fillStyle = '#ed9545'; ctx.fillRect(20, 20, 80, 180);
      return canvas.toDataURL('image/jpeg', .8).split(',')[1];
    });
    const jpeg = Buffer.from(data, 'base64');
    await fixture.close();
    photoServer = http.createServer((req, res) => {
      if (!req.url.startsWith('/frame')) { res.writeHead(404); res.end(); return; }
      ++photoActive; maxPhotoActive = Math.max(maxPhotoActive, photoActive);
      let finished = false;
      const done = () => { if (!finished) { finished = true; --photoActive; pending.delete(res); } };
      res.once('finish', done); res.once('close', done);
      const headers = {'Access-Control-Allow-Origin': '*', 'Access-Control-Expose-Headers': 'X-Frame-Number',
        'Cache-Control': 'no-store', 'Content-Type': 'image/jpeg', 'X-Frame-Number': String(++frame)};
      if (photoFault === '503') { res.writeHead(503, headers); res.end('No disponible'); return; }
      if (photoFault === 'corrupt') { res.writeHead(200, headers); res.end('JPEG incompleto'); return; }
      res.writeHead(200, {...headers, 'Content-Length': jpeg.length});
      if (photoFault === 'partial') { res.write(jpeg.subarray(0, 100)); pending.add(res); return; }
      res.end(jpeg);
    });
    await listen(photoServer);
    const status = {label: 'gris', busy: false, frame: 20, ageMs: 20, foreground: .6, useful: .7,
      fps: 8, videoFrame: 1, streamPort: photoServer.address().port, previewIntervalMs: 67,
      analysisMs: 4, decodeMs: 60, freePsram: 6000000, background: true, orangeSamples: 8,
      graySamples: 8, message: 'Guardado', options: {x: 20, y: 20, width: 60, height: 60, difference: 20, minForeground: 10}};
    pageServer = http.createServer((req, res) => {
      if (req.url.startsWith('/api/status')) {
        if (apiFault) { pending.add(res); res.once('close', () => pending.delete(res)); return; }
        res.writeHead(200, {'Content-Type': 'application/json'}); res.end(JSON.stringify(status)); return;
      }
      res.writeHead(200, {'Content-Type': 'text/html; charset=utf-8'}); res.end(html);
    });
    await listen(pageServer);
    const context = await browser.newContext({viewport: {width: 768, height: 1024}, isMobile: true, hasTouch: true});
    await context.addInitScript(() => {
      window.previewDraws = 0; window.previewClears = 0;
      const draw = CanvasRenderingContext2D.prototype.drawImage;
      const clear = CanvasRenderingContext2D.prototype.clearRect;
      CanvasRenderingContext2D.prototype.drawImage = function (...args) {
        if (this.canvas.id === 'preview') ++window.previewDraws;
        return draw.apply(this, args);
      };
      CanvasRenderingContext2D.prototype.clearRect = function (...args) {
        if (this.canvas.id === 'preview') ++window.previewClears;
        return clear.apply(this, args);
      };
    });
    const page = await context.newPage();
    const pageErrors = []; page.on('pageerror', error => pageErrors.push(error.message));
    const sample = () => page.evaluate(() => ({draws: window.previewDraws, clears: window.previewClears,
      pixel: [...document.getElementById('preview').getContext('2d').getImageData(160, 120, 1, 1).data]}));
    const opaque = value => { assert.equal(value.pixel[3], 255); assert.ok(value.pixel[1] > 50); assert.equal(value.clears, 0); };
    await page.goto(`http://127.0.0.1:${pageServer.address().port}/`);
    await page.waitForFunction(() => window.previewDraws >= 5);
    const normal = await sample(); opaque(normal);
    apiFault = true;
    await page.waitForTimeout(11200);
    const statusTimeout = await sample(); opaque(statusTimeout);
    assert.ok(statusTimeout.draws > normal.draws + 50, 'El timeout de estado no debe cortar el vídeo');
    apiFault = false;
    photoFault = '503'; await page.waitForTimeout(400);
    const beforeFault = await sample();
    await page.waitForTimeout(1600);
    const unavailable = await sample(); opaque(unavailable);
    assert.deepEqual(unavailable.pixel, beforeFault.pixel);
    assert.equal(unavailable.draws, beforeFault.draws);
    photoFault = 'corrupt'; await page.waitForTimeout(700);
    const corrupt = await sample(); opaque(corrupt);
    assert.equal(corrupt.draws, unavailable.draws);
    photoFault = 'partial'; await page.waitForTimeout(5600);
    const partial = await sample(); opaque(partial);
    assert.equal(partial.draws, corrupt.draws);
    photoFault = ''; for (const response of pending) response.destroy(); pending.clear();
    await page.waitForFunction(count => window.previewDraws > count + 3, partial.draws);
    const recovered = await sample(); opaque(recovered);
    await page.evaluate(() => window.dispatchEvent(new PageTransitionEvent('pagehide', {persisted: true})));
    const hidden = await sample(); await page.waitForTimeout(200);
    assert.equal((await sample()).draws, hidden.draws);
    await page.evaluate(() => window.dispatchEvent(new PageTransitionEvent('pageshow', {persisted: true})));
    await page.waitForFunction(count => window.previewDraws > count, hidden.draws);
    opaque(await sample()); assert.deepEqual(pageErrors, []);
    assert.equal(maxPhotoActive, 1, 'Solo debe haber una foto en recepción');
    await page.screenshot({path: path.join(output, 'vista-estable.png'), fullPage: true});
    const result = {viewport: '768x1024', statusTimeoutMs: 11200, photoUnavailable: true,
      corruptJpeg: true, partialJpegTimeout: true, recovered: true, blackCanvasSamples: 0,
      canvasClears: recovered.clears, maxPhotoActive, pageErrors};
    fs.writeFileSync(path.join(output, 'vista-estable-browser.json'), JSON.stringify(result, null, 2) + '\n');
    console.log(JSON.stringify(result));
    await context.close();
  } finally {
    for (const response of pending) response.destroy();
    if (pageServer) await close(pageServer);
    if (photoServer) await close(photoServer);
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
