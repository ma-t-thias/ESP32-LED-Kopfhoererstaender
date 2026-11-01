#pragma once
#include <Arduino.h>

// Weboberflaeche (eine Seite, ohne externe Dateien, funktioniert auch offline im Heimnetz)
static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LED-Kopfhörer</title>
<style>
:root{--bg:#0f1214;--card:#181d21;--fg:#e6eaee;--muted:#8d98a3;--line:#2a3238;--acc:#3cc497}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.4 system-ui,sans-serif}
main{max-width:560px;margin:0 auto;padding:16px;display:grid;gap:14px}
h1{font-size:20px;margin:4px 0}h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted);margin:0 0 8px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px}
.row{display:flex;align-items:center;gap:12px}.row>label{flex:0 0 110px;color:var(--muted)}
input[type=range]{flex:1;accent-color:var(--acc)}output{width:40px;text-align:right;font-variant-numeric:tabular-nums}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:8px}
button{font:inherit;color:var(--fg);background:#222a30;border:1px solid var(--line);border-radius:8px;padding:10px;cursor:pointer}
button.sel{border-color:var(--acc);background:#16362c}button:focus-visible{outline:2px solid var(--acc)}
#power{width:100%;font-size:18px;font-weight:600}#power.on{background:var(--acc);color:#04130d;border-color:var(--acc)}
input[type=color]{width:56px;height:40px;border:none;background:none;padding:0}
select,input[type=number]{font:inherit;background:#222a30;color:var(--fg);border:1px solid var(--line);border-radius:8px;padding:8px;flex:1}
.stack{display:grid;gap:10px}small{color:var(--muted)}a{color:var(--acc)}
</style></head><body><main>
<h1>LED-Kopfhörerständer</h1>
<button id="power" type="button">Ein</button>
<div class="card stack">
 <div class="row"><label for="bri">Helligkeit</label><input id="bri" type="range" min="1" max="255"><output id="briO"></output></div>
 <div class="row"><label for="spd">Tempo</label><input id="spd" type="range" min="0" max="255"><output id="spdO"></output></div>
 <div class="row"><label for="int">Intensität</label><input id="int" type="range" min="0" max="255"><output id="intO"></output></div>
</div>
<div class="card stack"><h2>Farben</h2>
 <div class="row"><label for="c1">Farbe 1</label><input id="c1" type="color"><label for="c2" style="flex:0 0 auto">Farbe 2</label><input id="c2" type="color"></div>
 <div class="row"><label for="pal">Palette</label><select id="pal"></select></div>
</div>
<div class="card"><h2>Effekt</h2><div id="fx" class="grid"></div></div>
<div class="card stack"><h2>Einstellungen</h2>
 <div class="row"><label for="ma">Stromgrenze</label><input id="ma" type="number" min="200" step="100"><small>mA</small></div>
 <small>Etwa 85 % des Netzteilstroms, z. B. 3000 bei 4 A oder 8000 bei 10 A.</small>
 <small id="net"></small>
 <button id="wifiReset" type="button">WLAN neu einrichten</button>
</div>
</main>
<script>
const $=id=>document.getElementById(id);let st={},timer=null,pending={};
function send(obj){Object.assign(pending,obj);clearTimeout(timer);timer=setTimeout(()=>{
 const body=new URLSearchParams(pending);pending={};
 fetch('/api/state',{method:'POST',body}).then(r=>r.json()).then(show).catch(()=>{});},120);}
function show(s){st=s;$('power').textContent=s.on?'Ein':'Aus';$('power').classList.toggle('on',s.on);
 for(const k of['bri','spd','int']){if(document.activeElement!==$(k)){$(k).value=s[k];}$(k+'O').textContent=$(k).value;}
 if(document.activeElement!==$('c1'))$('c1').value=s.c1;if(document.activeElement!==$('c2'))$('c2').value=s.c2;
 $('pal').value=s.pal;if(document.activeElement!==$('ma'))$('ma').value=s.ma;
 document.querySelectorAll('#fx button').forEach((b,i)=>b.classList.toggle('sel',i===s.fx));}
$('power').onclick=()=>send({on:st.on?0:1});
for(const k of['bri','spd','int'])$(k).oninput=e=>{$(k+'O').textContent=e.target.value;send({[k]:e.target.value});};
$('c1').oninput=e=>send({c1:e.target.value});$('c2').oninput=e=>send({c2:e.target.value});
$('pal').onchange=e=>send({pal:e.target.value});$('ma').onchange=e=>send({ma:e.target.value});
$('wifiReset').onclick=()=>{if(confirm('Gespeicherte WLAN-Daten löschen und Einrichtungs-WLAN starten?'))
 fetch('/api/wifi/reset',{method:'POST'}).then(r=>r.text()).then(t=>alert(t));};
fetch('/api/info').then(r=>r.json()).then(info=>{
 info.effects.forEach((n,i)=>{const b=document.createElement('button');b.type='button';b.textContent=n;b.onclick=()=>send({fx:i});$('fx').appendChild(b);});
 info.palettes.forEach((n,i)=>{const o=document.createElement('option');o.value=i;o.textContent=n;$('pal').appendChild(o);});
 $('ma').max=info.maMax;$('net').textContent=`WLAN ${info.ssid} · ${info.ip} · ${info.rssi} dBm · ${info.leds} LEDs · http://${info.host}.local`;
 return fetch('/api/state');}).then(r=>r.json()).then(show);
setInterval(()=>{if(!timer||!Object.keys(pending).length)fetch('/api/state').then(r=>r.json()).then(show).catch(()=>{});},3000);
</script></body></html>)HTML";

static const char SETUP_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WLAN einrichten</title>
<style>
body{margin:0;background:#0f1214;color:#e6eaee;font:16px/1.4 system-ui,sans-serif}
main{max-width:460px;margin:0 auto;padding:16px;display:grid;gap:12px}
h1{font-size:20px;margin:4px 0}p{color:#8d98a3;margin:0}
button,input,select{font:inherit;padding:10px;border-radius:8px;border:1px solid #2a3238;background:#222a30;color:#e6eaee}
button{cursor:pointer}#save{background:#3cc497;color:#04130d;border-color:#3cc497;font-weight:600}
label{display:grid;gap:4px;color:#8d98a3}#msg{min-height:1.4em}
</style></head><body><main>
<h1>WLAN einrichten</h1>
<p>Wähle dein Heim-WLAN und gib das Passwort ein. Die Daten werden nur auf dem Controller gespeichert.</p>
<label for="ssid">WLAN<select id="ssid"><option value="">Suche läuft …</option></select></label>
<button id="rescan" type="button">Erneut suchen</button>
<label for="manual">oder Name von Hand<input id="manual" autocomplete="off"></label>
<label for="pass">Passwort<input id="pass" type="password" autocomplete="off"></label>
<button id="save" type="button">Speichern und verbinden</button>
<div id="msg"></div>
</main><script>
const $=id=>document.getElementById(id);
function scan(){$('ssid').innerHTML='<option value="">Suche läuft …</option>';
 fetch('/api/scan').then(r=>r.json()).then(list=>{list.sort((a,b)=>b.rssi-a.rssi);
  $('ssid').innerHTML=list.length?'':'<option value="">Kein WLAN gefunden</option>';
  const seen=new Set();list.forEach(n=>{if(seen.has(n.ssid))return;seen.add(n.ssid);
   const o=document.createElement('option');o.value=n.ssid;o.textContent=`${n.ssid} (${n.rssi} dBm${n.open?', offen':''})`;$('ssid').appendChild(o);});
 }).catch(()=>{$('ssid').innerHTML='<option value="">Suche fehlgeschlagen</option>';});}
$('rescan').onclick=scan;
$('save').onclick=()=>{const ssid=$('manual').value.trim()||$('ssid').value;
 if(!ssid){$('msg').textContent='Bitte ein WLAN auswählen.';return;}
 fetch('/api/wifi',{method:'POST',body:new URLSearchParams({ssid,pass:$('pass').value})})
  .then(r=>r.text()).then(t=>{$('msg').textContent=t+' Danach erreichst du ihn unter http://led-kopfhoerer.local';})
  .catch(()=>{$('msg').textContent='Speichern fehlgeschlagen, bitte erneut versuchen.';});};
scan();
</script></body></html>)HTML";
