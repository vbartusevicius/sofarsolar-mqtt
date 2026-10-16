#ifndef SOFAR_WEBPAGE_H
#define SOFAR_WEBPAGE_H

#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sofar Solar Monitor</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0b1120;color:#e2e8f0;line-height:1.4}
.wrap{max-width:1120px;margin:0 auto;padding:14px}
.top{display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:12px;background:#111827;border:1px solid #1e293b;border-radius:12px;padding:12px 16px;margin-bottom:14px}
.brand{display:flex;align-items:baseline;gap:10px}
.brand h1{font-size:1.25rem;font-weight:700;color:#f8fafc;letter-spacing:-.02em}
.brand .sub{font-size:.75rem;color:#64748b;font-family:monospace}
.pills{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.pill{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;font-size:.75rem;font-weight:600;background:#1e293b;color:#cbd5e1;border:1px solid #334155}
.dot{width:8px;height:8px;border-radius:50%;display:inline-block}
.on{background:#22c55e;box-shadow:0 0 6px #22c55e88}.off{background:#ef4444;box-shadow:0 0 6px #ef444488}.warn{background:#eab308}
.tabs{display:flex;gap:6px;margin-bottom:14px;border-bottom:1px solid #1e293b;padding-bottom:10px}
.tab{padding:7px 16px;border-radius:8px;border:1px solid transparent;background:transparent;color:#94a3b8;font-size:.85rem;font-weight:600;cursor:pointer;transition:.15s}
.tab:hover{color:#e2e8f0;background:#1e293b66}
.tab.active{background:#1e293b;color:#38bdf8;border-color:#334155}
.pane{display:none}.pane.active{display:block}
.cbar{display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:12px;background:#111827;border:1px solid #1e293b;border-radius:12px;padding:12px 16px;margin-bottom:14px}
.cbar-l,.cbar-r{display:flex;flex-wrap:wrap;align-items:center;gap:10px}
.btn-bs{padding:8px 18px;border:none;border-radius:8px;font-size:.9rem;font-weight:700;cursor:pointer;color:#fff;transition:.15s}
.btn-on{background:#059669;box-shadow:0 0 12px #05966955}.btn-off{background:#991b1b}
.badge{padding:4px 10px;border-radius:6px;font-size:.78rem;font-weight:600;background:#0f172a;border:1px solid #334155;color:#22d3ee;font-family:monospace}
.ctrl-inline{display:flex;align-items:center;gap:6px;font-size:.8rem;color:#94a3b8}
.ctrl-inline select,.ctrl-inline input{padding:5px 8px;border:1px solid #334155;border-radius:6px;background:#0f172a;color:#f8fafc;font-size:.82rem;width:105px}
.ctrl-inline select{width:130px}
.btn-sm{padding:5px 11px;border:none;border-radius:6px;background:#2563eb;color:#fff;font-size:.8rem;font-weight:600;cursor:pointer}
.btn-sm:hover{background:#1d4ed8}
.flow-grid{display:grid;grid-template-columns:1fr 190px 1fr;gap:12px;align-items:stretch;margin-bottom:14px}
@media(max-width:760px){.flow-grid{grid-template-columns:1fr}}
.fcol{display:flex;flex-direction:column;gap:12px}
.fcard{background:#111827;border:1px solid #1e293b;border-top:3px solid #334155;border-radius:12px;padding:14px;display:flex;flex-direction:column;justify-content:space-between}
.fcard.solar{border-top-color:#eab308}.fcard.grid-exp{border-top-color:#22c55e}.fcard.grid-imp{border-top-color:#f87171}.fcard.home{border-top-color:#38bdf8}.fcard.batt{border-top-color:#06b6d4}
.fc-hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}
.fc-title{font-size:.75rem;font-weight:700;text-transform:uppercase;letter-spacing:.06em;color:#94a3b8}
.fc-tag{font-size:.72rem;font-weight:700;padding:2px 8px;border-radius:4px;background:#1e293b}
.fc-val{font-size:1.75rem;font-weight:800;letter-spacing:-.02em;margin:4px 0 8px}
.fc-sub{display:flex;justify-content:space-between;font-size:.8rem;color:#94a3b8;border-top:1px solid #1e293b;padding-top:7px;margin-top:4px}
.hub{background:#0f172a;border:1px solid #334155;border-radius:12px;padding:14px;display:flex;flex-direction:column;justify-content:center;align-items:center;text-align:center;gap:8px}
.hub-title{font-size:.72rem;font-weight:700;letter-spacing:.08em;color:#38bdf8;text-transform:uppercase}
.hub-self{font-size:1.35rem;font-weight:800;color:#22d3ee}
.hub-meta{font-size:.75rem;color:#94a3b8;line-height:1.5}
.bar-wrap{width:100%;height:10px;background:#0f172a;border:1px solid #334155;border-radius:999px;overflow:hidden;margin:6px 0 2px}
.bar-fill{height:100%;background:#22c55e;transition:width .3s}
.dgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:12px;margin-bottom:14px}
.panel{background:#111827;border:1px solid #1e293b;border-radius:12px;padding:14px}
.panel h2{font-size:.85rem;font-weight:700;text-transform:uppercase;letter-spacing:.05em;color:#94a3b8;border-bottom:1px solid #1e293b;padding-bottom:8px;margin-bottom:10px}
.tbl{width:100%;border-collapse:collapse;font-size:.83rem}
.tbl th,.tbl td{padding:6px 4px;text-align:left;border-bottom:1px solid #1e293b66}
.tbl th{color:#64748b;font-weight:600;font-size:.73rem;text-transform:uppercase}
.tbl td.num,.tbl th.num{text-align:right;font-variant-numeric:tabular-nums;color:#f8fafc}
.tbl th.num{color:#64748b}
.sgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px;margin-bottom:14px}
.scard{background:#111827;border:1px solid #1e293b;padding:10px 12px;border-radius:10px}
.scard .l{font-size:.68rem;color:#64748b;text-transform:uppercase;letter-spacing:.05em}
.scard .v{font-size:1.15rem;font-weight:700;margin-top:3px;font-variant-numeric:tabular-nums}
.fr{display:flex;gap:8px;align-items:center;margin:8px 0}
.fr label{width:120px;font-size:.82rem;color:#94a3b8}
.fr input,.fr select{flex:1;padding:6px 10px;border:1px solid #334155;border-radius:6px;background:#0f172a;color:#e2e8f0;font-size:.85rem}
.fr button{padding:7px 16px;border:none;border-radius:6px;background:#2563eb;color:#fff;cursor:pointer;font-size:.85rem;font-weight:600}
.fr button:hover{background:#1d4ed8}
.g{color:#22c55e}.r{color:#f87171}.y{color:#eab308}.c{color:#22d3ee}.o{color:#fb923c}.w{color:#f8fafc}
.logbox{background:#070b14;border:1px solid #1e293b;border-radius:8px;padding:10px;font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:.76rem;color:#cbd5e1;white-space:pre-wrap;max-height:420px;overflow-y:auto}
</style></head><body><div class="wrap">

<div class="top">
  <div class="brand">
    <h1>Sofar Solar HYD</h1>
    <span class="sub" id="hdr-meta">SN: -- &middot; FW: --</span>
  </div>
  <div class="pills">
    <span class="pill"><span class="dot" id="dw"></span>WiFi</span>
    <span class="pill"><span class="dot" id="dr"></span>RS485</span>
    <span class="pill"><span class="dot" id="dm"></span>MQTT</span>
    <span class="pill"><span class="dot" id="dp"></span><span id="dpt">Passive</span></span>
  </div>
</div>

<div class="tabs">
  <button class="tab active" onclick="sw(0)" id="tb0">Overview</button>
  <button class="tab" onclick="sw(1)" id="tb1">Settings</button>
  <button class="tab" onclick="sw(2)" id="tb2">System &amp; Log</button>
</div>

<div class="pane active" id="pn0">
  <div class="cbar">
    <div class="cbar-l">
      <button id="bsb" class="btn-bs btn-off" onclick="tb()">Battery Saver: --</button>
      <span class="badge" id="bst">Strategy: --</span>
    </div>
    <div class="cbar-r">
      <div class="ctrl-inline"><span>Mode</span>
        <select id="cm" onchange="sm(this.value)">
          <option value="auto">Auto</option>
          <option value="battery_saver">Battery Saver</option>
          <option value="charge">Charge / Dischg</option>
          <option value="standby">Standby</option>
        </select>
      </div>
      <div class="ctrl-inline"><span>Charge W</span>
        <input id="cp" type="number" min="-20000" max="20000" step="100" value="0">
        <button class="btn-sm" onclick="sc()">Set</button>
      </div>
      <div class="ctrl-inline"><span>Auto Limit W</span>
        <input id="al" type="number" min="100" max="20000" step="100" value="16384">
        <button class="btn-sm" onclick="sa()">Set</button>
      </div>
    </div>
  </div>

  <div class="flow-grid">
    <div class="fcol">
      <div class="fcard solar">
        <div class="fc-hdr"><span class="fc-title">Solar PV</span><span class="fc-tag y" id="f-pv-day">0.0 kWh today</span></div>
        <div class="fc-val y" id="f-pv-w">0 W</div>
        <div class="fc-sub"><span id="f-pv1">PV1: 0 W (0 V)</span><span id="f-pv2">PV2: 0 W (0 V)</span></div>
      </div>
      <div class="fcard home">
        <div class="fc-hdr"><span class="fc-title">Home Load</span><span class="fc-tag c" id="f-hm-self">Self 100%</span></div>
        <div class="fc-val w" id="f-hm-w">0 W</div>
        <div class="fc-sub"><span id="f-hm-day">Used today: 0.00 kWh</span><span id="f-hm-src">From grid: 0.00 kWh</span></div>
      </div>
    </div>

    <div class="hub">
      <div class="hub-title">Inverter Hub</div>
      <div class="hub-self" id="f-inv-self">0 W</div>
      <div style="font-size:.7rem;color:#64748b">Self-Consumption / Loss</div>
      <div class="hub-meta" id="f-inv-meta">AC Out: 0 W<br>Temp: -- &deg;C / HS: -- &deg;C<br>State: --</div>
    </div>

    <div class="fcol">
      <div class="fcard grid-exp" id="f-gr-card">
        <div class="fc-hdr"><span class="fc-title">Grid (3-Phase)</span><span class="fc-tag g" id="f-gr-dir">IDLE</span></div>
        <div class="fc-val g" id="f-gr-w">0 W</div>
        <div class="fc-sub"><span id="f-gr-v">-- / -- / -- V</span><span id="f-gr-io">In 0.0 &middot; Out 0.0 kWh</span></div>
      </div>
      <div class="fcard batt">
        <div class="fc-hdr"><span class="fc-title">Battery</span><span class="fc-tag c" id="f-bt-dir">IDLE</span></div>
        <div class="fc-val c" id="f-bt-w">0 W</div>
        <div class="bar-wrap"><div class="bar-fill" id="f-bt-bar" style="width:0%"></div></div>
        <div class="fc-sub"><span id="f-bt-soc">SOC: --% (&ndash; &deg;C)</span><span id="f-bt-io">In 0.0 &middot; Out 0.0 kWh</span></div>
      </div>
    </div>
  </div>

  <div class="dgrid">
    <div class="panel">
      <h2>Solar MPPT Strings</h2>
      <table class="tbl">
        <thead><tr><th>String</th><th class="num">Voltage</th><th class="num">Current</th><th class="num">Power</th></tr></thead>
        <tbody id="tb-pv"></tbody>
      </table>
    </div>
    <div class="panel">
      <h2>3-Phase Grid &amp; Inverter</h2>
      <table class="tbl">
        <thead><tr><th>Parameter</th><th class="num">Value</th></tr></thead>
        <tbody id="tb-gr"></tbody>
      </table>
    </div>
    <div class="panel">
      <h2>Battery Packs</h2>
      <table class="tbl">
        <thead><tr><th>Metric</th><th class="num">Pack 1</th><th class="num">Pack 2</th></tr></thead>
        <tbody id="tb-bt"></tbody>
      </table>
    </div>
  </div>
</div>

<div class="pane" id="pn1">
  <div class="dgrid">
    <div class="panel">
      <h2>Battery Saver Tuning</h2>
      <div class="fr"><label>Drift Hysteresis W</label><input id="td" type="number" min="20" max="2000" step="10" value="100"></div>
      <div class="fr"><label>Max Charge W</label><input id="tm" type="number" min="0" max="20000" step="100" value="20000"></div>
      <div class="fr"><label>Idle Lapse min</label><input id="tl" type="number" min="1" max="60" step="1" value="10"></div>
      <div class="fr"><label>Inverter Timeout</label><span id="tk" class="badge">-</span></div>
      <div class="fr"><button onclick="st()">Apply &amp; Save</button></div>
    </div>

    <div class="panel">
      <h2>MQTT &amp; Device Identity</h2>
      <form id="sf" onsubmit="return ss()">
        <div class="fr"><label>Device Name</label><input id="sd" name="deviceName"></div>
        <div class="fr"><label>MQTT Host</label><input id="sh" name="mqtthost"></div>
        <div class="fr"><label>MQTT Port</label><input id="sp" name="mqttport"></div>
        <div class="fr"><label>MQTT User</label><input id="su" name="mqttuser"></div>
        <div class="fr"><label>MQTT Password</label><input id="sw" name="mqttpass" type="password"></div>
        <div class="fr"><button type="submit">Save &amp; Reboot</button></div>
      </form>
    </div>

    <div class="panel">
      <h2>Firmware Update</h2>
      <div class="fr"><label>Running Version</label><input id="fvw" readonly style="border:none;font-family:monospace"></div>
      <div class="fr"><label>Firmware (.bin)</label><input id="fwf" type="file" accept=".bin"></div>
      <div class="fr"><label>Status</label><span id="fup" class="badge">idle</span><button onclick="uf()">Upload &amp; Flash</button></div>
    </div>

    <div class="panel">
      <h2>LCD Touchscreen</h2>
      <div class="fr"><label>Calibration</label><span id="tcs" class="badge">-</span></div>
      <div class="fr"><label>Wizard</label><button onclick="tc()">Calibrate Now</button></div>
      <p style="font-size:.76rem;color:#64748b;margin-top:6px">Tap the 3 crosshairs on the LCD, or hold the screen for 4 seconds at any time.</p>
    </div>
  </div>
</div>

<div class="pane" id="pn2">
  <div class="sgrid" id="sg"></div>
  <div class="panel">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px">
      <h2 style="border:none;margin:0;padding:0">System Log</h2>
      <button class="btn-sm" onclick="ul()">Refresh</button>
    </div>
    <div class="logbox" id="lb">Loading...</div>
  </div>
</div>

</div><script>
var WM={0:"Self Use",1:"Time of Use",2:"Timing",3:"Passive",4:"Peak Shaving",5:"Off-Grid",6:"Generator",7:"Export Priority"};
var RS={0:"Waiting",1:"Detection",2:"Grid-Connected",3:"EPS Off-Grid",4:"Recoverable Fault",5:"Permanent Fault",6:"Upgrading",7:"Self-Charging"};
function sw(i){for(var k=0;k<3;k++){document.getElementById("pn"+k).className="pane"+(k===i?" active":"");document.getElementById("tb"+k).className="tab"+(k===i?" active":"");}}
function el(id){return document.getElementById(id);}
function row(a,b,c,d){return"<tr><td>"+a+'</td><td class="num">'+b+(c!==undefined?'</td><td class="num">'+c:"")+(d!==undefined?'</td><td class="num">'+d:"")+"</td></tr>";}

function u(){fetch("/json").then(function(r){return r.json()}).then(function(d){
  el("dw").className="dot "+(d.wifi_ok?"on":"off");
  el("dr").className="dot "+(d.modbus_ok?"on":"off");
  el("dm").className="dot "+(d.mqtt_ok?"on":"off");
  var pok=d.passive_mode_ok;
  el("dp").className="dot "+(pok?"on":"off");
  el("dpt").textContent=pok?"Passive Mode":"Mode: "+(WM[d.working_mode]||d.working_mode);
  el("hdr-meta").textContent="SN: "+(d.serial_number||"--")+" \u00b7 FW: "+(d.firmware||"--");

  var b=el("bsb"),bs=d.battery_save,st=(d.bsave_strategy||"off").toUpperCase();
  b.textContent="Battery Saver: "+(bs?"ON":"OFF");
  b.className="btn-bs "+(bs?"btn-on":"btn-off");
  el("bst").textContent=bs?(st==="NATIVE"?"NATIVE [0..max]":"LOOP Target: "+d.battery_save_target+" W"):"Mode: "+(d.mode||"auto");
  if(d.mode&&document.activeElement!==el("cm"))el("cm").value=d.mode;
  if(d.charge_power!==undefined&&document.activeElement!==el("cp"))el("cp").value=d.charge_power;
  if(d.auto_limit!==undefined&&document.activeElement!==el("al"))el("al").value=d.auto_limit;

  el("f-pv-w").textContent=(d.pv_total||0)+" W";
  el("f-pv-day").textContent=(d.today_gen||0)+" kWh today";
  el("f-pv1").textContent="PV1: "+(d.pv1_power||0)+" W ("+(d.pv1_voltage||0)+" V)";
  el("f-pv2").textContent="PV2: "+(d.pv2_power||0)+" W ("+(d.pv2_voltage||0)+" V)";

  var gw=d.grid_power||0,gExp=gw>=0;
  el("f-gr-card").className="fcard "+(gExp?"grid-exp":"grid-imp");
  el("f-gr-dir").textContent=gw>0?"EXPORT":(gw<0?"IMPORT":"IDLE");
  el("f-gr-dir").className="fc-tag "+(gExp?"g":"r");
  el("f-gr-w").textContent=Math.abs(gw)+" W";
  el("f-gr-w").className="fc-val "+(gExp?"g":"r");
  el("f-gr-v").textContent=(d.grid_voltage||0)+" / "+(d.grid_voltage_l2||0)+" / "+(d.grid_voltage_l3||0)+" V";
  el("f-gr-io").textContent="In "+(d.today_imp||0)+" \u00b7 Out "+(d.today_exp||0)+" kWh";

  var tu=parseFloat(d.today_use)||0,ti=parseFloat(d.today_imp)||0;
  var sp=tu>0.05?Math.min(100,Math.max(0,Math.round((tu-ti)*100/tu))):100;
  el("f-hm-self").textContent="Self "+sp+"%";
  el("f-hm-w").textContent=(d.load_power||0)+" W";
  el("f-hm-day").textContent="Used today: "+(d.today_use||0)+" kWh";
  el("f-hm-src").textContent="From grid: "+(d.today_imp||0)+" kWh";

  var bw=d.batt_power||0,soc=d.batt_soc||0;
  el("f-bt-dir").textContent=bw>0?"CHARGING":(bw<0?"DISCHARGING":"IDLE");
  el("f-bt-dir").className="fc-tag "+(bw>=0?"c":"o");
  el("f-bt-w").textContent=Math.abs(bw)+" W";
  el("f-bt-w").className="fc-val "+(bw>=0?"c":"o");
  el("f-bt-bar").style.width=Math.min(100,soc)+"%";
  el("f-bt-bar").style.background=soc>=50?"#22c55e":(soc>=20?"#eab308":"#ef4444");
  el("f-bt-soc").textContent="SOC: "+soc+"% ("+(d.batt_temp||0)+" \u00b0C)";
  el("f-bt-io").textContent="In "+(d.today_chg||0)+" \u00b7 Out "+(d.today_dis||0)+" kWh";

  el("f-inv-self").textContent=(d.inverter_self_w||0)+" W";
  el("f-inv-meta").innerHTML="AC Out: <b>"+(d.inverter_power||0)+" W</b><br>Inv: "+(d.inverter_temp||0)+" &deg;C &middot; HS: "+(d.heatsink_temp||0)+" &deg;C<br>"+(RS[d.run_state]||("State "+d.run_state))+" &middot; "+(WM[d.working_mode]||("Mode "+d.working_mode));

  el("tb-pv").innerHTML=
    row("MPPT 1",(d.pv1_voltage||0)+" V",(d.pv1_current||0)+" A",(d.pv1_power||0)+" W")+
    row("MPPT 2",(d.pv2_voltage||0)+" V",(d.pv2_current||0)+" A",(d.pv2_power||0)+" W")+
    row("Combined PV","\u2014","\u2014",(d.pv_total||0)+" W")+
    row("Generated Today","\u2014","\u2014",(d.today_gen||0)+" kWh");

  el("tb-gr").innerHTML=
    row("Phase Voltages (L1/L2/L3)",(d.grid_voltage||0)+" / "+(d.grid_voltage_l2||0)+" / "+(d.grid_voltage_l3||0)+" V")+
    row("Grid Frequency",(d.grid_freq||0)+" Hz")+
    row("Inverter AC Output",(d.inverter_power||0)+" W")+
    row("Inverter Self Power",(d.inverter_self_w||0)+" W")+
    row("Inverter / Heatsink Temp",(d.inverter_temp||0)+" \u00b0C / "+(d.heatsink_temp||0)+" \u00b0C")+
    row("Today In / Out",(d.today_imp||0)+" / "+(d.today_exp||0)+" kWh");

  el("tb-bt").innerHTML=
    row("Power",(d.batt_power||0)+" W",(d.batt2_power||0)+" W")+
    row("Voltage",(d.batt_voltage||0)+" V",(d.batt2_voltage||0)+" V")+
    row("Current",(d.batt_current||0)+" A",(d.batt2_current||0)+" A")+
    row("SOC / SOH",(d.batt_soc||0)+"% / "+(d.batt_soh||0)+"%",(d.batt2_soc||0)+"% / "+(d.batt2_soh||0)+"%")+
    row("Temp / Cycles",(d.batt_temp||0)+" \u00b0C / "+(d.batt_cycles||0),(d.batt2_temp||0)+" \u00b0C / "+(d.batt2_cycles||0))+
    row("Today In / Out",(d.today_chg||0)+" kWh",(d.today_dis||0)+" kWh");

  var ut=Math.floor((d.uptime||0)/1000),uh=Math.floor(ut/3600),um=Math.floor((ut%3600)/60);
  var sh="";
  function sc(l,v,c){sh+='<div class="scard"><div class="l">'+l+'</div><div class="v '+c+'">'+v+"</div></div>";}
  sc("UPTIME",uh+"h "+um+"m","w");
  sc("PASSIVE MODE",pok?"ACTIVE":"OFF ("+d.working_mode+")",pok?"g":"r");
  sc("SAVER STRATEGY",st,st==="NATIVE"?"g":(st==="LOOP"?"y":"w"));
  sc("PASSIVE TIMEOUT",d.passive_timeout_s?d.passive_timeout_s+" s":"OFF",d.passive_timeout_s?"y":"g");
  sc("KEEP-ALIVE",d.keepalive_s?d.keepalive_s+" s":"NONE",d.keepalive_s?"y":"g");
  sc("FREE HEAP",(d.free_heap||0)+" B","w");
  sc("MAX BLOCK",(d.max_free_block||0)+" B","w");
  sc("HEAP FRAG",(d.heap_frag||0)+"%",(d.heap_frag||0)>40?"r":"g");
  el("sg").innerHTML=sh;
}).catch(function(){el("dw").className="dot off";});}

function tc(){fetch("/api/touch_cal",{method:"POST"}).then(function(r){return r.json()}).then(function(j){el("tcs").textContent=j.message;});}
function tb(){fetch("/api/battery_save").then(function(){u()});}
function sm(v){fetch("/api/mode?v="+v).then(function(){u()});}
function sc(){fetch("/api/charge?v="+el("cp").value).then(function(){u()});}
function sa(){fetch("/api/auto?v="+el("al").value).then(function(){u()});}
function st(){var p="delta="+el("td").value+"&maxpower="+el("tm").value+"&lapse="+el("tl").value;
fetch("/api/tuning?"+p).then(function(){alert("Tuning applied & saved.");});}
function uf(){var i=el("fwf"),s=el("fup");
if(!i.files.length){alert("Choose a firmware .bin first");return;}
var fd=new FormData();fd.append("firmware",i.files[0],i.files[0].name);
var x=new XMLHttpRequest();x.open("POST","/api/upload");
x.upload.onprogress=function(e){if(e.lengthComputable)s.textContent=Math.round(e.loaded/e.total*100)+"%";};
x.onload=function(){s.textContent=x.status==200?"flashed, rebooting...":"FAILED ("+x.status+")";};
x.onerror=function(){s.textContent="connection lost";};
s.textContent="uploading...";x.send(fd);}
function ss(){var f=el("sf");var p=new URLSearchParams(new FormData(f)).toString();
fetch("/command?"+p).then(function(){alert("Saved. Rebooting...");});return false;}
function ul(){fetch("/log").then(function(r){return r.text()}).then(function(t){
var lb=el("lb");lb.textContent=t||"(empty)";lb.scrollTop=lb.scrollHeight;});}
u();ul();setInterval(u,5000);setInterval(ul,5000);
fetch("/settings").then(function(r){return r.json()}).then(function(d){
  el("sh").value=d.mqtthost||"";
  el("sp").value=d.mqttport||"";
  el("su").value=d.mqttuser||"";
  el("sw").value=d.mqttpass||"";
  el("sd").value=d.deviceName||"";
  if(d.bsDelta!==undefined)el("td").value=d.bsDelta;
  if(d.bsMaxPower!==undefined)el("tm").value=d.bsMaxPower;
  if(d.passiveTimeoutS!==undefined){var t=d.passiveTimeoutS;
    var a=d.timeoutAction?"returns to previous mode":"forces standby";
    el("tk").textContent=t?(t+" s ("+a+"), keep-alive "+d.keepaliveS+" s"):"disabled - no keep-alive writes";}
  if(d.idleLapseMin!==undefined)el("tl").value=d.idleLapseMin;
  if(d.firmware!==undefined)el("fvw").value=d.firmware;
  if(d.touch_cal!==undefined)el("tcs").textContent=d.touch_cal?"calibrated":"not calibrated - gestures only";
});
</script></body></html>)rawliteral";

#endif
