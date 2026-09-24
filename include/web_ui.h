#pragma once

#include <Arduino.h>

static const char WEB_UI_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CRT Clock</title>
<style>
:root{color-scheme:dark;--bg:#0d0f12;--card:#171a1f;--line:#2a2f36;--text:#f0f2f4;--muted:#9ca3ad;--accent:#cfd5dc}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}main{max-width:760px;margin:auto;padding:20px 16px 48px}.top{display:flex;justify-content:space-between;gap:16px;align-items:flex-start;margin-bottom:20px}.top h1{font-size:24px;margin:0 0 5px}.sub{color:var(--muted);font-size:14px}.pill{border:1px solid var(--line);border-radius:999px;padding:7px 10px;color:var(--muted);font-size:13px;white-space:nowrap}.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin:0 0 14px}.card h2{font-size:15px;margin:0 0 14px;text-transform:uppercase;letter-spacing:.08em;color:var(--muted)}.field{margin:0 0 15px}.field:last-child{margin-bottom:0}label{display:flex;justify-content:space-between;gap:12px;align-items:center;font-size:14px;margin-bottom:7px}.value{color:var(--muted);font-variant-numeric:tabular-nums}input[type=text],input[type=url],select{width:100%;min-height:44px;border:1px solid var(--line);background:#0f1216;color:var(--text);border-radius:9px;padding:9px 11px;font-size:16px}input[type=range]{width:100%;accent-color:var(--accent)}.checks{display:grid;grid-template-columns:1fr 1fr;gap:10px}.check{display:flex;gap:9px;align-items:center;min-height:44px;border:1px solid var(--line);border-radius:9px;padding:9px 11px}.check input{width:18px;height:18px}.buttons{display:flex;flex-wrap:wrap;gap:9px;margin-top:16px}button{min-height:44px;border:1px solid var(--line);border-radius:9px;background:#22272e;color:var(--text);font-size:15px;padding:9px 14px;cursor:pointer}button.primary{background:var(--accent);color:#111;border-color:var(--accent);font-weight:700}button.danger{color:#ffb2b2}.status-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}.stat{border:1px solid var(--line);border-radius:9px;padding:10px;min-width:0}.stat b{display:block;font-size:18px;margin-top:3px;overflow-wrap:anywhere}.stat span{font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}.msg{min-height:22px;margin-top:10px;color:var(--muted);font-size:14px}.hint{font-size:13px;color:var(--muted);line-height:1.45;margin-top:8px}input[type=file]{width:100%;border:1px solid var(--line);background:#0f1216;color:var(--text);border-radius:9px;padding:10px;font-size:15px}progress{width:100%;height:12px;margin-top:12px;accent-color:var(--accent)}@media(max-width:540px){.status-grid{grid-template-columns:1fr 1fr}.checks{grid-template-columns:1fr}.top{display:block}.pill{display:inline-block;margin-top:8px}}
</style>
</head>
<body><main>
<div class="top"><div><h1>CRT Clock</h1><div class="sub" id="version">Loading settings…</div></div><div class="pill" id="ip">WiFi</div></div>
<form id="settingsForm">
<section class="card"><h2>Network & Time</h2>
<div class="field"><label for="bridge_url">Bridge URL</label><input id="bridge_url" name="bridge_url" type="url" autocomplete="off" spellcheck="false"></div>
<div class="field"><label for="timezone_rule">Timezone (POSIX rule)</label><input id="timezone_rule" name="timezone_rule" type="text" autocomplete="off" spellcheck="false"><div class="hint">Pacific example: PST8PDT,M3.2.0,M11.1.0</div></div>
</section>
<section class="card"><h2>Display</h2>
<div class="field"><label for="theme">Clock design</label><select id="theme" name="theme"><option value="0">Rich Cityscape</option><option value="1">Daylight</option><option value="2">Retro RPG</option></select><div class="hint">Theme changes can be previewed on the CRT for 60 seconds before saving.</div></div>
<div class="field"><label for="drift_pixels">Burn-in drift <span class="value"><span id="drift_pixels_v"></span> px</span></label><input id="drift_pixels" name="drift_pixels" type="range" min="0" max="5" step="1"></div>
<div class="field"><label for="drift_minutes">Drift interval <span class="value"><span id="drift_minutes_v"></span> min</span></label><input id="drift_minutes" name="drift_minutes" type="range" min="1" max="120" step="1"></div>
<div class="field"><label for="night_brightness">Night brightness <span class="value"><span id="night_brightness_v"></span>%</span></label><input id="night_brightness" name="night_brightness" type="range" min="10" max="100" step="1"></div>
<div class="field"><label for="clock_brightness">Clock brightness <span class="value"><span id="clock_brightness_v"></span>%</span></label><input id="clock_brightness" name="clock_brightness" type="range" min="25" max="100" step="1"></div>
<div class="field"><label for="background_brightness">Background brightness <span class="value"><span id="background_brightness_v"></span>%</span></label><input id="background_brightness" name="background_brightness" type="range" min="0" max="100" step="1"></div>
<div class="field"><label for="city_speed">City animation</label><select id="city_speed" name="city_speed"><option value="0">Off</option><option value="1">Slow</option><option value="2">Normal</option><option value="3">Fast</option></select></div>
<div class="field"><label for="time_format">Time format</label><select id="time_format" name="time_format"><option value="12">12-hour</option><option value="24">24-hour</option></select></div>
<div class="field"><label for="temperature_unit">Temperature</label><select id="temperature_unit" name="temperature_unit"><option value="C">Celsius</option><option value="F">Fahrenheit</option></select></div>
<div class="checks"><label class="check"><input id="weather_text" name="weather_text" type="checkbox"> Weather descriptor</label><label class="check"><input id="day_bar" name="day_bar" type="checkbox"> Day progress bar</label></div>
<div class="buttons"><button class="primary" type="submit">Save settings</button><button id="previewBtn" type="button">Preview for 60 sec</button><button id="revertBtn" type="button">Revert preview</button><button id="defaultsBtn" class="danger" type="button">Restore defaults</button></div>
<div class="msg" id="settingsMsg" aria-live="polite"></div>
</section>
</form>
<section class="card"><h2>Firmware Update</h2>
<div class="field"><label for="firmwareFile">Firmware file <span class="value" id="otaLimit">—</span></label><input id="firmwareFile" type="file" accept=".bin,application/octet-stream"></div>
<div class="buttons"><button id="firmwareBtn" class="primary" type="button">Upload firmware</button></div>
<progress id="firmwareProgress" max="100" value="0" hidden></progress>
<div class="msg" id="firmwareMsg" aria-live="polite"></div>
<div class="hint">Upload PlatformIO's <b>firmware.bin</b>. The CRT picture will intentionally freeze during the update. Settings and WiFi credentials are preserved. Do not remove power until the clock restarts.</div>
</section>
<section class="card"><h2>Status</h2><div class="status-grid">
<div class="stat"><span>WiFi</span><b id="wifiStatus">—</b></div><div class="stat"><span>Bridge</span><b id="bridgeStatus">—</b></div><div class="stat"><span>Time</span><b id="timeStatus">—</b></div>
<div class="stat"><span>RSSI</span><b id="rssi">—</b></div><div class="stat"><span>Free heap</span><b id="heap">—</b></div><div class="stat"><span>Largest block</span><b id="largest">—</b></div>
<div class="stat"><span>Uptime</span><b id="uptime">—</b></div><div class="stat"><span>Theme</span><b id="themeStatus">—</b></div><div class="stat"><span>Weather</span><b id="weather">—</b></div><div class="stat"><span>Next event</span><b id="event">—</b></div>
<div class="stat"><span>Video test</span><b id="videoTest">None</b></div>
</div><div class="buttons"><button id="refreshStatusBtn" type="button">Refresh status</button><button id="refreshDataBtn" type="button">Refresh data now</button><button id="diagBtn" type="button">Toggle diagnostics</button><button id="freezeBtn" type="button">Freeze 30 sec</button><button id="renderOnlyBtn" type="button">Render-only 30 sec</button><button id="swapOnlyBtn" type="button">Swap-only 30 sec</button><button id="wifiOffBtn" type="button">WiFi-off 30 sec</button><button id="restartBtn" class="danger" type="button">Restart clock</button></div><div class="hint">Video isolation tests pause normal rendering, bridge refreshes, and periodic diagnostic sampling. The WiFi-off test intentionally disconnects this page for about 30 seconds while composite scanout continues. Run one test at a time and watch the CRT during the ACTIVE period.</div><div class="msg" id="actionMsg" aria-live="polite"></div></section>
<div class="hint">WiFi provisioning remains on the physical GPIO4 reset flow. The web server never starts a captive portal while composite video is running.</div>
</main>
<script>
(()=>{if(window.__crtInit)return;window.__crtInit=true;
const $=id=>document.getElementById(id);const msg=(id,t)=>{$(id).textContent=t||''};
const ranges=['drift_pixels','drift_minutes','night_brightness','clock_brightness','background_brightness'];
for(const id of ranges){$(id).addEventListener('input',()=>$(id+'_v').textContent=$(id).value)}
function formBody(displayOnly=false){const p=new URLSearchParams();const ids=displayOnly?['theme','drift_pixels','drift_minutes','night_brightness','clock_brightness','background_brightness','city_speed','time_format','temperature_unit','weather_text','day_bar']:['bridge_url','timezone_rule','theme','drift_pixels','drift_minutes','night_brightness','clock_brightness','background_brightness','city_speed','time_format','temperature_unit','weather_text','day_bar'];for(const id of ids){const el=$(id);p.set(id,el.type==='checkbox'?(el.checked?'1':'0'):el.value)}return p}
async function post(url,body){const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});const j=await r.json().catch(()=>({ok:false,message:'Invalid response'}));if(!r.ok||j.ok===false)throw new Error(j.message||('HTTP '+r.status));return j}
async function loadSettings(){try{const r=await fetch('/api/settings',{cache:'no-store'});const s=await r.json();$('bridge_url').value=s.bridge_url;$('timezone_rule').value=s.timezone_rule;$('drift_pixels').value=s.drift_pixels;$('drift_minutes').value=s.drift_minutes;$('night_brightness').value=s.night_brightness;$('clock_brightness').value=s.clock_brightness;$('background_brightness').value=s.background_brightness;$('city_speed').value=s.city_speed;$('theme').value=String(s.theme||0);$('time_format').value=s.use_24h?'24':'12';$('temperature_unit').value=s.fahrenheit?'F':'C';$('weather_text').checked=s.weather_text;$('day_bar').checked=s.day_bar;for(const id of ranges)$(id+'_v').textContent=$(id).value;$('version').textContent='Firmware '+s.firmware+(s.preview_active?' • preview active':'');if(s.ota_slot_bytes)$('otaLimit').textContent='max '+(s.ota_slot_bytes/1048576).toFixed(2)+' MB'}catch(e){msg('settingsMsg','Could not load settings: '+e.message)}}
function fmtUptime(sec){sec=Math.max(0,Number(sec)||0);const d=Math.floor(sec/86400);const h=Math.floor((sec%86400)/3600);const m=Math.floor((sec%3600)/60);return (d?d+'d ':'')+h+'h '+m+'m'}
async function loadStatus(){try{const r=await fetch('/api/status',{cache:'no-store'});const s=await r.json();$('ip').textContent=s.ip||'No IP';$('wifiStatus').textContent=s.wifi?'OK':'Offline';$('bridgeStatus').textContent=s.bridge?'OK':'No';$('timeStatus').textContent=s.time_valid?(s.time_source||'OK'):'No time';$('rssi').textContent=s.wifi?(s.rssi+' dBm'):'—';$('heap').textContent=Math.round(s.free_heap/1024)+' KB';$('largest').textContent=Math.round(s.largest_block/1024)+' KB';$('uptime').textContent=fmtUptime(s.uptime_sec);$('themeStatus').textContent=s.theme_name||'—';$('weather').textContent=s.weather||'—';$('event').textContent=s.next_event||'—';$('videoTest').textContent=s.video_test_active?((s.video_test||'TEST')+' '+(s.video_test_remaining_sec||0)+'s'):'None';if(s.ota_slot_bytes)$('otaLimit').textContent='max '+(s.ota_slot_bytes/1048576).toFixed(2)+' MB'}catch(e){msg('actionMsg','Status failed: '+e.message)}}
$('settingsForm').addEventListener('submit',async e=>{e.preventDefault();try{const j=await post('/api/save',formBody(false));msg('settingsMsg',j.message||'Saved');await loadSettings();await loadStatus()}catch(e){msg('settingsMsg',e.message)}});
$('previewBtn').addEventListener('click',async()=>{try{const j=await post('/api/preview',formBody(true));msg('settingsMsg',j.message||'Preview active for 60 seconds');await loadSettings()}catch(e){msg('settingsMsg',e.message)}});
$('revertBtn').addEventListener('click',async()=>{try{const j=await post('/api/revert','');msg('settingsMsg',j.message||'Preview reverted');await loadSettings()}catch(e){msg('settingsMsg',e.message)}});
$('defaultsBtn').addEventListener('click',async()=>{if(!confirm('Restore all clock settings to firmware defaults? WiFi credentials will be kept.'))return;try{const j=await post('/api/defaults','');msg('settingsMsg',j.message||'Defaults restored');await loadSettings();await loadStatus()}catch(e){msg('settingsMsg',e.message)}});
$('refreshStatusBtn').addEventListener('click',loadStatus);
$('refreshDataBtn').addEventListener('click',async()=>{try{const j=await post('/api/refresh','');msg('actionMsg',j.message||'Refresh queued')}catch(e){msg('actionMsg',e.message)}});
$('diagBtn').addEventListener('click',async()=>{try{const j=await post('/api/diagnostics','');msg('actionMsg',j.message||'Diagnostics toggled')}catch(e){msg('actionMsg',e.message)}});
async function startVideoTest(mode){try{const j=await post('/api/video-test','mode='+encodeURIComponent(mode));msg('actionMsg',j.message||'Video test queued')}catch(e){msg('actionMsg',e.message)}}
$('freezeBtn').addEventListener('click',()=>startVideoTest('freeze'));
$('renderOnlyBtn').addEventListener('click',()=>startVideoTest('render'));
$('swapOnlyBtn').addEventListener('click',()=>startVideoTest('swap'));
$('wifiOffBtn').addEventListener('click',()=>{msg('actionMsg','Starting WiFi-off test. This page will disconnect for about 30 seconds.');startVideoTest('wifi')});
$('firmwareBtn').addEventListener('click',()=>{
 const file=$('firmwareFile').files&&$('firmwareFile').files[0];
 if(!file){msg('firmwareMsg','Choose firmware.bin first.');return}
 if(!file.name.toLowerCase().endsWith('.bin')){msg('firmwareMsg','Please choose a .bin firmware image.');return}
 if(!confirm('Install '+file.name+' and restart the clock?'))return;
 const fd=new FormData();fd.append('firmware',file,file.name);
 const xhr=new XMLHttpRequest();
 const btn=$('firmwareBtn'),bar=$('firmwareProgress');btn.disabled=true;bar.hidden=false;bar.value=0;msg('firmwareMsg','Uploading… keep power connected.');
 xhr.upload.addEventListener('progress',e=>{if(e.lengthComputable)bar.value=Math.round((e.loaded/e.total)*100)});
 xhr.addEventListener('load',()=>{let j={};try{j=JSON.parse(xhr.responseText||'{}')}catch(_){j={ok:false,message:'Invalid response from clock'}};if(xhr.status>=200&&xhr.status<300&&j.ok!==false){bar.value=100;msg('firmwareMsg',j.message||'Firmware accepted. Restarting…');setTimeout(()=>location.reload(),8000)}else{btn.disabled=false;msg('firmwareMsg',j.message||('Firmware update failed (HTTP '+xhr.status+')'))}});
 xhr.addEventListener('error',()=>{btn.disabled=false;msg('firmwareMsg','Upload connection failed. The existing firmware should remain active.')});
 xhr.open('POST','/api/firmware');xhr.send(fd);
});
$('restartBtn').addEventListener('click',async()=>{if(!confirm('Restart the CRT clock?'))return;try{const j=await post('/api/restart','');msg('actionMsg',j.message||'Restarting…')}catch(e){msg('actionMsg',e.message)}});
loadSettings();loadStatus();})();
</script></body></html>
)HTML";
