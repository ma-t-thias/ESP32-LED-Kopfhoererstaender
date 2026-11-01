#pragma once
#include <Arduino.h>

// Weboberflaeche: Quelle in web/index.html, erzeugt mit tools/web2h.py
#include "webui_index.h"

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
