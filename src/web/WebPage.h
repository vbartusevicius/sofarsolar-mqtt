#ifndef SOFAR_WEBPAGE_H
#define SOFAR_WEBPAGE_H

#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sofar Battery Saver</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,system-ui,sans-serif;background:#0f172a;color:#e2e8f0;padding:12px}
h1{font-size:1.3em;color:#38bdf8}
h2{font-size:1em;color:#94a3b8;border-bottom:1px solid #1e293b;padding:8px 0;margin:16px 0 8px}
.hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}
.dots{display:flex;gap:10px;font-size:.75em}
.dot{width:10px;height:10px;border-radius:50%;display:inline-block;vertical-align:middle;margin-right:3px}
.on{background:#22c55e}.off{background:#ef4444}
.bs-bar{display:flex;gap:8px;align-items:center;margin-bottom:12px}
.bs-bar button{padding:6px 16px;border:none;border-radius:6px;font-size:.9em;cursor:pointer;color:#fff}
.btn-on{background:#065f46}.btn-off{background:#7f1d1d}
.tgt{font-size:1.1em;color:#22d3ee;font-weight:bold}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(145px,1fr));gap:6px}
.card{background:#1e293b;padding:10px;border-radius:8px}
.card .l{font-size:.65em;color:#64748b;text-transform:uppercase;letter-spacing:.5px}
.card .v{font-size:1.3em;font-weight:700;margin-top:2px}
.section{margin-top:16px}
.fr{display:flex;gap:6px;align-items:center;margin:6px 0}
.fr label{width:90px;font-size:.8em;color:#94a3b8}
.fr input{flex:1;padding:5px 8px;border:1px solid #334155;border-radius:4px;background:#0f172a;color:#e2e8f0;font-size:.85em}
.fr button{padding:6px 14px;border:none;border-radius:4px;background:#1d4ed8;color:#fff;cursor:pointer;font-size:.85em}
.fr button:hover{background:#2563eb}
.fr select{flex:1;padding:5px 8px;border:1px solid #334155;border-radius:4px;background:#0f172a;color:#e2e8f0;font-size:.85em}
.ctrl{background:#1e293b;padding:12px;border-radius:8px}
.g{color:#22c55e}.r{color:#ef4444}.y{color:#eab308}.c{color:#22d3ee}.o{color:#f97316}.w{color:#e2e8f0}
.logbox{background:#0a0f1a;border:1px solid #1e293b;border-radius:6px;padding:8px;font-family:monospace;font-size:.75em;color:#94a3b8;white-space:pre-wrap;max-height:600px;overflow-y:auto}
</style></head><body>
<div class="hdr"><h1>Sofar Battery Saver</h1>
<div class="dots">
<span><span class="dot" id="dw"></span>WiFi</span>
<span><span class="dot" id="dr"></span>RS485</span>
<span><span class="dot" id="dm"></span>MQTT</span>
</div></div>
<div class="bs-bar">
<button id="bsb" onclick="tb()">Battery Saver: --</button>
<span class="tgt" id="bst"></span>
</div>

<h2>Live Metrics</h2>
<div class="grid" id="mg"></div>

<div class="section"><h2>Energy Today / Total</h2>
<div class="grid" id="eg"></div></div>

<div class="section"><h2>Inverter Control</h2>
<div class="ctrl">
<div class="fr"><label>Mode</label>
<select id="cm" onchange="sm(this.value)"><option value="auto">Auto</option><option value="charge">Charge</option><option value="standby">Standby</option><option value="battery_saver">Battery Saver</option></select></div>
<div class="fr"><label>Charge W</label><input id="cp" type="number" min="-20000" max="20000" step="100" value="0"><button onclick="sc()">Set</button></div>
<div class="fr"><label>Auto Limit W</label><input id="al" type="number" min="100" max="20000" step="100" value="16384"><button onclick="sa()">Set</button></div>
</div></div>

<div class="section"><h2>MQTT Settings</h2>
<form id="sf" onsubmit="return ss()">
<div class="fr"><label>Host</label><input id="sh" name="mqtthost"></div>
<div class="fr"><label>Port</label><input id="sp" name="mqttport"></div>
<div class="fr"><label>User</label><input id="su" name="mqttuser"></div>
<div class="fr"><label>Password</label><input id="sw" name="mqttpass" type="password"></div>
<div class="fr"><label>Device Name</label><input id="sd" name="deviceName"></div>
<div class="fr"><button type="submit">Save &amp; Reboot</button></div>
</form></div>

<div class="section"><h2>System Log</h2>
<div class="logbox" id="lb">Loading...</div></div>

<script>
var M={run_state:["Run State","w"],inverter_temp:["Inv Temp \u00b0C","w"],heatsink_temp:["HS Temp \u00b0C","w"],
grid_freq:["Grid Hz","w"],inverter_power:["Inv Power W","w"],
grid_power:["Grid W","g"],grid_voltage:["Grid V","w"],load_power:["Load W","w"],
pv1_voltage:["PV1 V","y"],pv1_current:["PV1 A","y"],pv1_power:["PV1 W","y"],
pv2_voltage:["PV2 V","y"],pv2_current:["PV2 A","y"],pv2_power:["PV2 W","y"],
pv_total:["Solar W","y"],
batt_voltage:["Batt V","c"],batt_current:["Batt A","c"],batt_power:["Batt W","c"],
batt_temp:["Batt \u00b0C","c"],batt_soc:["SOC %","c"],batt_soh:["SOH %","c"],batt_cycles:["Cycles","c"],
batt2_voltage:["Batt2 V","c"],batt2_current:["Batt2 A","c"],batt2_power:["Batt2 W","c"],
batt2_temp:["Batt2 \u00b0C","c"],batt2_soc:["SOC2 %","c"],batt2_soh:["SOH2 %","c"],batt2_cycles:["Cycles2","c"],
batt_total_power:["Batt Tot W","c"],batt_avg_soc:["Avg SOC %","c"],batt_avg_soh:["Avg SOH %","c"],
working_mode:["Mode","w"]};
var E={today_gen:["Gen Today kWh","y"],total_gen:["Gen Total kWh","y"],
today_use:["Use Today kWh","w"],total_use:["Use Total kWh","w"],
today_imp:["Import Today kWh","r"],total_imp:["Import Total kWh","r"],
today_exp:["Export Today kWh","g"],total_exp:["Export Total kWh","g"],
today_chg:["Charged Today kWh","c"],total_chg:["Charged Total kWh","c"],
today_dis:["Discharged Today kWh","o"],total_dis:["Discharged Total kWh","o"]};

function c(k,v,m){var d=m[k];if(!d)return"";
return'<div class="card"><div class="l">'+d[0]+'</div><div class="v '+d[1]+'">'+v+'</div></div>';}

function u(){fetch("/json").then(function(r){return r.json()}).then(function(d){
document.getElementById("dw").className="dot "+(d.wifi_ok?"on":"off");
document.getElementById("dr").className="dot "+(d.modbus_ok?"on":"off");
document.getElementById("dm").className="dot "+(d.mqtt_ok?"on":"off");
var b=document.getElementById("bsb");var bs=d.battery_save;
b.textContent="Battery Saver: "+(bs?"ON":"OFF");
b.className=bs?"btn-on":"btn-off";
document.getElementById("bst").textContent=bs?"Target: "+d.battery_save_target+" W":"";
if(d.mode)document.getElementById("cm").value=d.mode;
if(d.charge_power!==undefined)document.getElementById("cp").value=d.charge_power;
if(d.auto_limit!==undefined)document.getElementById("al").value=d.auto_limit;
var h="";for(var k in M){if(k in d)h+=c(k,d[k],M);}
document.getElementById("mg").innerHTML=h;
var eh="";for(var k in E){if(k in d)eh+=c(k,d[k],E);}
document.getElementById("eg").innerHTML=eh;
}).catch(function(){document.getElementById("dw").className="dot off";});}

function tb(){fetch("/api/battery_save").then(function(){u()});}
function sm(v){fetch("/api/mode?v="+v).then(function(){u()});}
function sc(){fetch("/api/charge?v="+document.getElementById("cp").value).then(function(){u()});}
function sa(){fetch("/api/auto?v="+document.getElementById("al").value).then(function(){u()});}
function ss(){
var f=document.getElementById("sf");
var p=new URLSearchParams(new FormData(f)).toString();
fetch("/command?"+p).then(function(){alert("Saved. Rebooting...");});return false;}

function ul(){fetch("/log").then(function(r){return r.text()}).then(function(t){
var lb=document.getElementById("lb");lb.textContent=t||"(empty)";
lb.scrollTop=lb.scrollHeight;});}
u();ul();setInterval(u,5000);setInterval(ul,5000);
fetch("/settings").then(function(r){return r.json()}).then(function(d){
document.getElementById("sh").value=d.mqtthost||"";
document.getElementById("sp").value=d.mqttport||"";
document.getElementById("su").value=d.mqttuser||"";
document.getElementById("sw").value=d.mqttpass||"";
document.getElementById("sd").value=d.deviceName||"";
});
</script></body></html>)rawliteral";

#endif // SOFAR_WEBPAGE_H
