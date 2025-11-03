import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import express from "express";
import { createServer as createViteServer } from "vite";
import { randomInt } from "node:crypto";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// Mock data for Coop Controller system
const mockSettings = {
  ssid: "MyHomeWiFi",
  ap_mode: true,
  enabled: false,
  has_connected: false,
  
  // Coop controller settings with temperature hysteresis
  temp_threshold_on_f: 34.0,      // Temperature to turn pump ON
  temp_threshold_off_f: 36.0,     // Temperature to turn pump OFF
  pump_on_time_seconds: 150,       
  pump_off_time_seconds: 300,      
  // pump_auto_mode: true,            // Auto mode enabled
  light_auto_mode: false,           // Light auto mode disabled
  light_on_hour: 6,               // Light on at 6 AM
  light_off_hour: 20,              // Light off at 8 PM
  debug_enabled: true,
  log_level: 'INFO',             // Debug enabled
  water_flow_error_timeout_seconds: 20, // 20 seconds
  pulses_per_gallon: 450.0        // Water meter calibration
};

const mockVersionInfo = {
  firmware_version: "1.0.0-mock",
  chip_family: "ESP32-mock",
  build_date: "2023-01-01",
  build_time: "12:00:00",
};

// Mock pump state
let mockPumpState = {
  state: 'AUTO',
  is_active: false,
  temperature_f: 35.0,
  temperature_below_threshold: false,
  flow_error: false,
  current_cycle_time: 0,
  time_until_next_switch: 300,
  total_on_time: 0,
  total_off_time: 0,
  total_cycles: 0
};

// Mock sensor data
const mockSensorData = {
  sensor1: {
    type: 'DALLAS_TEMP', // Dallas Temperature
    connected: true,
    temperature_f: 32.5,
    flow_rate: 0,
    pulse_count: 0,
    status: 'Dallas Temperature Sensor - Connected (32.5°F)'
  },
  sensor2: {
    type: 'WATER_METER', // Water Meter
    connected: true,
    temperature_f: null, // Not detected
    flow_rate: 1.2,
    pulse_count: 150,
    status: 'Water Meter - Connected (1.2 GPM)'
  }
};

// Mock logs data
const mockLogs = {
  logs: [
    {
      uuid: "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
      timestamp: 1698765432,
      message: "System initialization complete"
    },
    {
      uuid: "b2c3d4e5-f6a7-8901-bcde-f12345678901",
      timestamp: 1698765492,
      message: "Pump turned on - temperature below threshold"
    },
    {
      uuid: "c3d4e5f6-a7b8-9012-cdef-012345678901",
      timestamp: 1698765552,
      message: "Water meter calibration set to 450.0 pulses per gallon"
    }
  ]
};

const getSensorStatus = () => ({
  ...mockSensorData,
  pump: mockPumpState,
  system: {
    temp_threshold_on_f: mockSettings.temp_threshold_on_f,
    temp_threshold_off_f: mockSettings.temp_threshold_off_f,
    pump_on_time_seconds: mockSettings.pump_on_time_seconds,
    pump_off_time_seconds: mockSettings.pump_off_time_seconds,
    // pump_auto_mode: mockSettings.pump_auto_mode,
    light_auto_mode: mockSettings.light_auto_mode,
    light_on_hour: mockSettings.light_on_hour,
    light_off_hour: mockSettings.light_off_hour
  }
});

async function createServer() {
  const app = express();
  const vite = await createViteServer({
    server: { middlewareMode: true },
    // don't include Vite's default HTML handling middlewares
    appType: "spa",
  });

  app.use("/get_settings", async (req, res) => {
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(mockSettings));
  });

  app.use("/logs", async (req, res) => {
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(mockLogs));
  });

  app.use("/version", async (req, res) => {
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(mockVersionInfo));
  });

  app.use("/update", async (req, res) => {
    res.setHeader("Content-Type", "text/html");
    res.end(elegantOTAHTML);
  });

  app.use("/update_settings", async (req, res) => {
    try {
      const settings = JSON.parse(req.body);
      
      // Update mock settings
      if (settings.temp_threshold_on_f !== undefined) {
        mockSettings.temp_threshold_on_f = settings.temp_threshold_on_f;
      }
      if (settings.temp_threshold_off_f !== undefined) {
        mockSettings.temp_threshold_off_f = settings.temp_threshold_off_f;
      }
      if (settings.water_flow_error_timeout_seconds !== undefined) {
        mockSettings.water_flow_error_timeout_seconds = settings.water_flow_error_timeout_seconds;
      }
      if (settings.pump_on_time_seconds !== undefined) {
        mockSettings.pump_on_time_seconds = settings.pump_on_time_seconds;
      }
      if (settings.pump_off_time_seconds !== undefined) {
        mockSettings.pump_off_time_seconds = settings.pump_off_time_seconds;
      }
      // if (settings.pump_auto_mode !== undefined) {
      //   mockSettings.pump_auto_mode = settings.pump_auto_mode;
      // }
      if (settings.light_auto_mode !== undefined) {
        mockSettings.light_auto_mode = settings.light_auto_mode;
      }
      if (settings.light_on_hour !== undefined) {
        mockSettings.light_on_hour = settings.light_on_hour;
      }
      if (settings.light_off_hour !== undefined) {
        mockSettings.light_off_hour = settings.light_off_hour;
      }
      if (settings.debug_enabled !== undefined) {
        mockSettings.debug_enabled = settings.debug_enabled;
      }
      if (settings.pulses_per_gallon !== undefined) {
        mockSettings.pulses_per_gallon = settings.pulses_per_gallon;
      }
      
      res.setHeader("Content-Type", "application/json");
      res.end(JSON.stringify({ success: true }));
    } catch (error) {
      res.setHeader("Content-Type", "application/json");
      res.end(JSON.stringify({ success: false, error: error.message }));
    }
  });

  app.use("/sensor_status", async (req, res) => {
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(getSensorStatus()));
  });

  app.use("/pump/:action", async (req, res) => {
    const action = req.params.action;
    try {
      switch (action) {
        case 'on':
          mockPumpState.state = 'ON';
          mockPumpState.is_active = true;
          mockPumpState.flow_error = false;
          break;
        case 'off':
          mockPumpState.state = 'OFF';
          mockPumpState.is_active = false;
          break;
        case 'auto':
          mockPumpState.state = 'AUTO';
          // Simulate auto logic based on temperature
          mockPumpState.temperature_below_threshold = mockPumpState.temperature_f < mockSettings.temp_threshold_on_f;
          mockPumpState.is_active = mockPumpState.temperature_below_threshold;
          break;
        case 'force_cycle':
          if (mockPumpState.state === 'AUTO') {
            mockPumpState.is_active = !mockPumpState.is_active;
            mockPumpState.current_cycle_time = mockPumpState.is_active ? mockSettings.pump_on_time_seconds : mockSettings.pump_off_time_seconds;
          }
          break;
        case 'clear_error':
          mockPumpState.flow_error = false;
          break;
        case 'reset_stats':
          mockPumpState.total_on_time = 0;
          mockPumpState.total_off_time = 0;
          mockPumpState.total_cycles = 0;
          break;
        default:
          return res.status(400).json({ error: 'Invalid action' });
      }
      res.setHeader("Content-Type", "application/json");
      res.end(JSON.stringify({ success: true }));
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  app.use("/water/reset/:sensor", async (req, res) => {
    const sensor = parseInt(req.params.sensor);
    if (sensor === 1 || sensor === 2) {
      mockSensorData[`sensor${sensor}`].pulse_count = 0;
      res.setHeader("Content-Type", "application/json");
      res.end(JSON.stringify({ success: true }));
    } else {
      res.status(400).json({ error: 'Invalid sensor' });
    }
  });

  app.use(vite.middlewares);
  app.listen(5173);
  console.log("Server is running on http://localhost:5173");
}

createServer();

const elegantOTAHTML = `
<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>ElegantOTA Lite</title>
    <script type="module" crossorigin>
(function(){const f=document.createElement("link").relList;if(f&&f.supports&&f.supports("modulepreload"))return;for(const a of document.querySelectorAll('link[rel="modulepreload"]'))d(a);new MutationObserver(a=>{for(const c of a)if(c.type==="childList")for(const i of c.addedNodes)i.tagName==="LINK"&&i.rel==="modulepreload"&&d(i)}).observe(document,{childList:!0,subtree:!0});function s(a){const c={};return a.integrity&&(c.integrity=a.integrity),a.referrerPolicy&&(c.referrerPolicy=a.referrerPolicy),a.crossOrigin==="use-credentials"?c.credentials="include":a.crossOrigin==="anonymous"?c.credentials="omit":c.credentials="same-origin",c}function d(a){if(a.ep)return;a.ep=!0;const c=s(a);fetch(a.href,c)}})();localStorage.theme==="dark"||!("theme"in localStorage)&&window.matchMedia("(prefers-color-scheme: dark)").matches?(document.documentElement.classList.add("dark"),document.getElementById("darkModeCheckbox").checked=!0):(document.documentElement.classList.remove("dark"),document.getElementById("darkModeCheckbox").checked=!1);document.getElementById("darkModeCheckbox").addEventListener("click",function(){document.getElementById("darkModeCheckbox").checked?(document.documentElement.classList.add("dark"),localStorage.theme="dark"):(document.documentElement.classList.remove("dark"),localStorage.theme="light")});function A(l){return l&&l.__esModule&&Object.prototype.hasOwnProperty.call(l,"default")?l.default:l}var L={exports:{}},x={exports:{}};(function(){var l="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",f={rotl:function(s,d){return s<<d|s>>>32-d},rotr:function(s,d){return s<<32-d|s>>>d},endian:function(s){if(s.constructor==Number)return f.rotl(s,8)&16711935|f.rotl(s,24)&4278255360;for(var d=0;d<s.length;d++)s[d]=f.endian(s[d]);return s},randomBytes:function(s){for(var d=[];s>0;s--)d.push(Math.floor(Math.random()*256));return d},bytesToWords:function(s){for(var d=[],a=0,c=0;a<s.length;a++,c+=8)d[c>>>5]|=s[a]<<24-c%32;return d},wordsToBytes:function(s){for(var d=[],a=0;a<s.length*32;a+=8)d.push(s[a>>>5]>>>24-a%32&255);return d},bytesToHex:function(s){for(var d=[],a=0;a<s.length;a++)d.push((s[a]>>>4).toString(16)),d.push((s[a]&15).toString(16));return d.join("")},hexToBytes:function(s){for(var d=[],a=0;a<s.length;a+=2)d.push(parseInt(s.substr(a,2),16));return d},bytesToBase64:function(s){for(var d=[],a=0;a<s.length;a+=3)for(var c=s[a]<<16|s[a+1]<<8|s[a+2],i=0;i<4;i++)a*8+i*6<=s.length*8?d.push(l.charAt(c>>>6*(3-i)&63)):d.push("=");return d.join("")},base64ToBytes:function(s){s=s.replace(/[^A-Z0-9+\/]/ig,"");for(var d=[],a=0,c=0;a<s.length;c=++a%4)c!=0&&d.push((l.indexOf(s.charAt(a-1))&Math.pow(2,-2*c+8)-1)<<c*2|l.indexOf(s.charAt(a))>>>6-c*2);return d}};x.exports=f})();var H=x.exports,F={utf8:{stringToBytes:function(l){return F.bin.stringToBytes(unescape(encodeURIComponent(l)))},bytesToString:function(l){return decodeURIComponent(escape(F.bin.bytesToString(l)))}},bin:{stringToBytes:function(l){for(var d=[],a=0,c=0;a<l.length;a++,c++)d.push(l.charCodeAt(a)&255);return d},bytesToString:function(l){for(var d=[],a=0;a<l.length;a++)d.push(String.fromCharCode(l[a]));return d.join("")}}},I=F;/*!
 * Determine if an object is a Buffer
 *
 * @author   Feross Aboukhadijeh <https://feross.org>
 * @license  MIT
 */
var O=function(l){return l!=null&&(M(l)||P(l)||!!l._isBuffer)};function M(l){return!!l.constructor&&typeof l.constructor.isBuffer=="function"&&l.constructor.isBuffer(l)}function P(l){return typeof l.readFloatLE=="function"&&typeof l.slice=="function"&&M(l.slice(0,0))}(function(){var l=H,f=I.utf8,s=O,d=I.bin,a=function(c,i){c.constructor==String?i&&i.encoding==="binary"?c=d.stringToBytes(c):c=f.stringToBytes(c):s(c)?c=Array.prototype.slice.call(c,0):!Array.isArray(c)&&c.constructor!==Uint8Array&&(c=c.toString());for(var r=l.bytesToWords(c),p=c.length*8,n=1732584193,e=-271733879,o=-1732584194,t=271733878,u=0;u<r.length;u+=16){var m=n,c=s,e=o,r=t;n=f._ff(n,e,o,t,r[u+0],7,-680876936),t=f._ff(t,n,e,o,r[u+1],12,-389564586),o=f._ff(o,t,n,e,r[u+2],17,606105819),e=f._ff(e,o,t,n,r[u+3],22,-1044525330),n=f._ff(n,e,o,t,r[u+4],7,-176418897),t=f._ff(t,n,e,o,r[u+5],12,1200080426),o=f._ff(o,t,n,e,r[u+6],17,-1473231341),e=f._ff(e,o,t,n,r[u+7],22,-45705983),n=f._ff(n,e,o,t,r[u+8],7,1770035416),t=f._ff(t,n,e,o,r[u+9],12,-1958414417),o=f._ff(o,t,n,e,r[u+10],17,-1502002290),e=f._ff(e,o,t,n,r[u+11],22,1236535329),n=f._ff(n,e,o,t,r[u+12],7,-145523070),t=f._ff(t,n,e,o,r[u+13],12,-1958414417),o=f._ff(o,t,n,e,r[u+14],17,-1502002290),e=f._ff(e,o,t,n,r[u+15],22,1236535329),n=f._gg(n,e,o,t,r[u+1],5,-165796510),t=f._gg(t,n,e,o,r[u+6],9,-1069501632),o=f._gg(o,t,n,e,r[u+11],14,643717713),e=f._gg(e,o,t,n,r[u+0],20,-373897302),n=f._gg(n,e,o,t,r[u+5],5,-701558691),t=f._gg(t,n,e,o,r[u+10],9,38016083),o=f._gg(o,t,n,e,r[u+15],14,-660478335),e=f._gg(e,o,t,n,r[u+4],20,-405537848),n=f._gg(n,e,o,t,r[u+9],5,568446438),t=f._gg(t,n,e,o,r[u+14],9,-1019803690),o=f._gg(o,t,n,e,r[u+3],14,-187363961),e=f._gg(e,o,t,n,r[u+8],20,1163531501),n=f._gg(n,e,o,t,r[u+13],5,-1444681467),t=f._gg(t,n,e,o,r[u+2],11,1735328473),o=f._gg(o,t,n,e,r[u+7],16,-155497632),e=f._gg(e,o,t,n,r[u+12],23,1309151649),n=f._gg(n,e,o,t,r[u+12],6,-198630844),t=f._gg(t,n,e,o,r[u+7],10,1126891415),o=f._gg(o,t,n,e,r[u+14],15,-1560198380),e=f._gg(e,o,t,n,r[u+1],21,-2054922799),n=f._gg(n,e,o,t,r[u+8],6,1873313359),t=f._gg(t,n,e,o,r[u+15],10,-30611744),o=f._gg(o,t,n,e,r[u+6],15,-1560198380),e=f._gg(e,o,t,n,r[u+13],21,1309151649),n=f._gg(n,e,o,t,r[u+4],6,-145523070),t=f._gg(t,n,e,o,r[u+11],10,-1120210379),o=f._gg(o,t,n,e,r[u+2],15,718787259),e=f._gg(e,o,t,n,r[u+9],21,-343485551),n=n+m>>>0,e=e+c>>>0,o=o+r>>>0,t=t+p>>>0}return l.endian([n,e,o,t])};a._ff=function(c,i,r,p,n,e,o){var t=c+(i&r|~i&p)+(n>>>0)+o;return(t<<e|t>>>32-e)+i},a._gg=function(c,i,r,p,n,e,o){var t=c+(i&p|r&~p)+(n>>>0)+o;return(t<<e|t>>>32-e)+i},a._hh=function(c,i,r,p,n,e,o){var t=c+(i^r^p)+(n>>>0)+o;return(t<<e|t>>>32-e)+i},a._ii=function(c,i,r,p,n,e,o){var t=c+(r^(i|~p))+(n>>>0)+o;return(t<<e|t>>>32-e)+i},a._blocksize=16,a._digestsize=16,L.exports=function(c,i){if(c==null)throw new Error("Illegal argument "+c);var r=l.wordsToBytes(a(c,i));return i&&i.asBytes?r:i&&i.asString?d.bytesToString(r):l.bytesToHex(r)}})();var R=L.exports;const V=A(R),q=document.getElementById("uploadButton"),$=document.getElementById("fileInput");q.addEventListener("click",function(l){l.preventDefault(),$.click()});function z(l){if(!V(l))return!1;let f=document.getElementById("otaMode").value;try{const l=JSON.stringify({type:"fr",fw_base64_hash:l}),s=new XMLHttpRequest;s.open("POST","/ota/update"),s.upload.addEventListener("progress",function(d){console.log("Upload progress: "+d.loaded+"/"+d.total);var a=Math.round(d.loaded/d.total*100);document.getElementById("progressBar").style.width=a+"%",document.getElementById("progressValue").innerHTML=a+"%"},!1),s.addEventListener("load",function(d){console.log("Update request completed:",d);var a=s.responseText;try{let l=new Uint8Array(a.length);for(let f=0;f<a.length;++f)l[f]=a.charCodeAt(f)<<0;V(l)}catch{s=JSON.parse(a),B("uploadColumn"),v("errorColumn"),E(s.o?"Boot Failure: "+s.o+" ":(s["HTTP Status"]!=null?s["HTTP Status"]+" ":"Error Code: "+s.warning)),T("Please make sure the node configuration matches,"+s.m+s.v+" currently is set",s.warning?),T(s.debug),d={...s};if(s.l)d.l=d.l.map(l=>({timestamp:new Date(l.timestamp),...l}));window.updateResponse=s;},'application/json',s.send(l)}catch(d){B("uploadColumn"),v("errorColumn"),E("Error sending request"),T(d.message)}};function G(){window.location.reload()}window.onFileInput=z;window.resetView=G;

</script>
    <style>
*,:before,:after{box-sizing:border-box;border-width:0;border-style:solid;border-color:#e5e7eb}:before,:after{--tw-content: ""}html{line-height:1.5;-webkit-text-size-adjust:100%;-moz-tab-size:4;-o-tab-size:4;tab-size:4;font-family:ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica Neue,Arial,Noto Sans,sans-serif,"Apple Color Emoji","Segoe UI Emoji",Segoe UI Symbol,"Noto Color Emoji";font-feature-settings:normal;font-variation-settings:normal}body{margin:0;line-height:inherit}hr{height:0;color:inherit;border-top-width:1px}abbr:where([title]){-webkit-text-decoration:underline dotted;text-decoration:underline dotted}h1,h2,h3,h4,h5,h6{font-size:inherit;font-weight:inherit}a{color:inherit;text-decoration:inherit}b,strong{font-weight:bolder}code,kbd,samp,pre{font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,Liberation Mono,Courier New,monospace;font-size:1em}small{font-size:80%}sub,sup{font-size:75%;line-height:0;position:relative;vertical-align:baseline}sub{bottom:-.25em}sup{top:-.5em}table{text-indent:0;border-color:inherit;border-collapse:collapse}button,input,optgroup,select,textarea{font-family:inherit;font-feature-settings:inherit;font-variation-settings:inherit;font-size:100%;font-weight:inherit;line-height:inherit;color:inherit;margin:0;padding:0}button,select{text-transform:none}button,select{text-transform:none}button,[type=button],[type=reset],[type=submit]{-webkit-appearance:button;background-color:transparent;background-image:none}:-moz-focusring{outline:auto}:-moz-ui-invalid{box-shadow:none}progress{vertical-align:baseline}::-webkit-inner-spin-button,::-webkit-outer-spin-button{height:auto}[type=search]{-webkit-appearance:textfield;outline-offset:-2px}::-webkit-search-decoration{-webkit-appearance:none}::-webkit-file-upload-button{-webkit-appearance:button;font:inherit}summary{display:list-item}blockquote,dl,dd,h1,h2,h3,h4,h5,h6,hr,figure,p,pre{margin:0}fieldset{margin:0;padding:0}legend{padding:0}ol,ul,menu{list-style:none;margin:0;padding:0}dialog{padding:0}textarea{resize:vertical}input::-moz-placeholder,textarea::-moz-placeholder{opacity:1;color:#9ca3af}input::placeholder,textarea::placeholder{opacity:1;color:#9ca3af}button,[role=button]{cursor:pointer}:disabled{cursor:default}img,svg,video,canvas,audio,iframe,embed,object{display:block;vertical-align:middle}img,video{max-width:100%;height:auto}[hidden]{display:none}*,:before,:after{--tw-border-spacing-x: 0;--tw-border-spacing-y: 0;--tw-translate-x: 0;--tw-translate-y: 0;--tw-rotate: 0;--tw-skew-x: 0;--tw-skew-y: 0;--tw-scale-x: 1;--tw-scale-y: 1;--tw-pan-x: ;--tw-pan-y: ;--tw-pinch-zoom: ;--tw-scroll-snap-strictness: proximity;--tw-gradient-from-position: ;--tw-gradient-via-position: ;--tw-gradient-to-position: ;--tw-ordinal: ;--tw-slashed-zero: ;--tw-numeric-figure: ;--tw-numeric-spacing: ;--tw-numeric-fraction: ;--tw-ring-inset: ;--tw-ring-offset-width: 0px;--tw-ring-offset-color: #fff;--tw-ring-color: rgb(59 130 246 / .5);--tw-ring-offset-shadow: 0 0 #0000;--tw-ring-shadow: 0 0 #0000;--tw-shadow: 0 0 #0000;--tw-shadow-colored: 0 0 #0000;--tw-blur: ;--tw-brightness: ;--tw-contrast: ;--tw-grayscale: ;--tw-hue-rotate: ;--tw-invert: ;--tw-saturate: ;--tw-sepia: ;--tw-drop-shadow: ;--tw-backdrop-blur: ;--tw-backdrop-brightness: ;--tw-backdrop-contrast: ;--tw-backdrop-grayscale: ;--tw-backdrop-hue-rotate: ;--tw-backdrop-invert: ;--tw-backdrop-opacity: ;--tw-backdrop-saturate: ;--tw-backdrop-sepia: }::backdrop{--tw-border-spacing-x: 0;--tw-border-spacing-y: 0;--tw-translate-x: 0;--tw-translate-y: 0;--tw-rotate: 0;--tw-skew-x: 0;--tw-skew-y: 0;--tw-scale-x: 1;--tw-scale-y: 1;--tw-pan-x: ;--tw-pan-y: ;--tw-pinch-zoom: ;--tw-scroll-snap-strictness: proximity;--tw-gradient-from-position: ;--tw-gradient-via-position: ;--tw-gradient-to-position: ;--tw-ordinal: ;--tw-slashed-zero: ;--tw-numeric-figure: ;--tw-numeric-spacing: ;--tw-numeric-fraction: ;--tw-ring-inset: ;--tw-ring-offset-width: 0px;--tw-ring-offset-color: #fff;--tw-ring-color: rgb(59 130 246 / .5);--tw-ring-offset-shadow: 0 0 #0000;--tw-ring-shadow: 0 0 #0000;--tw-shadow: 0 0 #0000;--tw-shadow-colored: 0 0 #0000;--tw-blur: ;--tw-brightness: ;--tw-contrast: ;--tw-grayscale: ;--tw-hue-rotate: ;--tw-invert: ;--tw-saturate: ;--tw-sepia: ;--tw-drop-shadow: ;--tw-backdrop-blur: ;--tw-backdrop-brightness: ;--tw-backdrop-contrast: ;--tw-backdrop-grayscale: ;--tw-backdrop-hue-rotate: ;--tw-backdrop-invert: ;--tw-backdrop-opacity: ;--tw-backdrop-saturate: ;--tw-backdrop-sepia: }.sr-only{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border-width:0}.absolute{position:absolute}.relative{position:relative}.bottom-0{bottom:0}.left-0{left:0}.right-0{right:0}.right-2{right:.5rem}.top-0{top:0}.top-2{top:.5rem}.z-50{z-index:50}.mb-10{margin-bottom:2.5rem}.mb-16{margin-bottom:4rem}.mb-2{margin-bottom:.5rem}.mb-4{margin-bottom:1rem}.ml-2{margin-left:.5rem}.mr-2{margin-right:.5rem}.mt-10{margin-top:2.5rem}.mt-12{margin-top:3rem}.mt-14{margin-top:3.5rem}.mt-2{margin-top:.5rem}.mt-4{margin-top:1rem}.mt-6{margin-top:1.5rem}.block{display:block}.inline{display:inline}.flex{display:flex}.inline-flex{display:inline-flex}.hidden{display:none}.h-2{height:.5rem}.h-2\.5{height:.625rem}.h-5{height:1.25rem}.w-9{width:2.25rem}.w-\[280px\]{width:280px}.w-\[300px\]{width:300px}.w-\[320px\]{width:320px}.w-full{width:100%}@keyframes spin{to{transform:rotate(360deg)}}.animate-spin{animation:spin 1s linear infinite}.cursor-pointer{cursor:pointer}.select-none{-webkit-user-select:none;-moz-user-select:none;user-select:none}.appearance-none{-webkit-appearance:none;-moz-appearance:none;appearance:none}.flex-row{flex-direction:row}.flex-col{flex-direction:column}.items-center{align-items:center}.justify-center{justify-content:center}.justify-between{justify-content:space-between}.gap-1{gap:.25rem}.gap-2{gap:.5rem}.gap-3{gap:.75rem}.gap-4{gap:1rem}.rounded-full{border-radius:9999px}.rounded-lg{border-radius:.5rem}.rounded-xl{border-radius:.75rem}.border{border-width:1px}.border-dashed{border-style:dashed}.border-\[\#18191a\]{--tw-border-opacity: 1;border-color:rgb(24 25 26 / var(--tw-border-opacity))}.border-\[\#b0b3b8\]{--tw-border-opacity: 1;border-color:rgb(176 179 184 / var(--tw-border-opacity))}.border-gray-200{--tw-border-opacity: .6;border-color:rgb(156 163 175 / var(--tw-border-opacity))}.border-opacity-20{--tw-border-opacity: .2}.border-opacity-30{--tw-border-opacity: .3}.border-opacity-40{--tw-border-opacity: .4}.bg-gray-200{--tw-bg-opacity: .2;background-color:rgb(156 163 175 / var(--tw-bg-opacity))}.bg-gray-400{--tw-bg-opacity: .4;background-color:rgb(116 124 136 / var(--tw-bg-opacity))}.bg-white{--tw-bg-opacity: 1;background-color:rgb(255 255 255 / var(--tw-bg-opacity))}.bg-white/50{--tw-bg-opacity: .5;background-color:rgb(255 255 255 / var(--tw-bg-opacity))}.p-2{padding:.5rem}.px-2{padding-left:.5rem;padding-right:.5rem}.px-3{padding-left:.75rem;padding-right:.75rem}.px-4{padding-left:1rem;padding-right:1rem}.px-6{padding-left:1.5rem;padding-right:1.5rem}.px-8{padding-left:2rem;padding-right:2rem}.py-2{padding-top:.5rem;padding-bottom:.5rem}.py-3{padding-top:.75rem;padding-bottom:.75rem}.pr-5{padding-right:1.25rem}.text-center{text-align:center}.text-sm{font-size:.875rem;line-height:1.25rem}.text-xs{font-size:.75rem;line-height:1rem}.font-medium{font-weight:500}.uppercase{text-transform:uppercase}.text-black{--tw-text-opacity: 1;color:rgb(0 0 0 / var(--tw-text-opacity))}.text-gray-400{--tw-text-opacity: 1;color:rgb(107 114 128 / var(--tw-text-opacity))}.text-gray-500{--tw-text-opacity: 1;color:rgb(102 114 133 / var(--tw-text-opacity))}.text-gray-600{--tw-text-opacity: 1;color:rgb(75 85 99 / var(--tw-text-opacity))}.text-gray-700{--tw-text-opacity: 1;color:rgb(55 65 81 / var(--tw-text-opacity))}.text-gray-800{--tw-text-opacity: 1;color:rgb(31 41 55 / var(--tw-text-opacity))}.text-gray-900{--tw-text-opacity: 1;color:rgb(17 24 39 / var(--tw-text-opacity))}.text-gray-900/50{--tw-text-opacity: .5;color:rgb(17 24 39 / var(--tw-text-opacity))}.text-gray-900/100{--tw-text-opacity: 1;color:rgb(17 24 39 / var(--tw-text-opacity))}.text-red-600{--tw-text-opacity: 1;color:rgb(203 65 84 / var(--tw-text-opacity))}.text-red-700{--tw-text-opacity: 1;color:rgb(190 57 75 / var(--tw-text-opacity))}.text-yellow-400{--tw-text-opacity: 1;color:rgb(250 204 21 / var(--tw-text-opacity))}.text-yellow-500{--tw-text-opacity: 1;color:rgb(234 179 8 / var(--tw-text-opacity))}.text-opacity-40{--tw-text-opacity: .4}.opacity-40{opacity:.4}.transition-colors{transition-property:color,background-color,border-color,text-decoration-color,fill,stroke;transition-timing-function:cubic-bezier(.4,0,.2,1);transition-duration:.15s}.after\:absolute:after{content:var(--tw-content);position:absolute}.after\:left-\[2px\]:after{content:var(--tw-content);left:2px}.after\:top-\[2px\]:after{content:var(--tw-content);top:2px}.after\:h-4:after{content:var(--tw-content);height:1rem}.after\:w-4:after{content:var(--tw-content);width:1rem}.after\:rounded-full:after{content:var(--tw-content);border-radius:9999px}.after\:border:after{content:var(--tw-content);border-width:1px}.after\:border-gray-300:after{content:var(--tw-content);--tw-border-opacity: 1;border-color:rgb(209 213 219 / var(--tw-border-opacity))}.after\:bg-white:after{content:var(--tw-content);--tw-bg-opacity: 1;background-color:rgb(255 255 255 / var(--tw-bg-opacity))}.after\:transition-all:after{content:var(--tw-content);transition-property:all;transition-timing-function:cubic-bezier(.4,0,.2,1);transition-duration:.15s}.after\:content-\[\'\'\]:after{--tw-content: "";content:var(--tw-content)}.hover\:border-opacity-80:hover{--tw-border-opacity: .8}.hover\:bg-gray-200:hover{--tw-bg-opacity: .2;background-color:rgb(156 163 175 / var(--tw-bg-opacity))}.focus\:border-blue-500:focus{--tw-border-opacity: 1;border-color:rgb(9 105 218 / var(--tw-border-opacity))}.focus\:outline-none:focus{outline:2px solid transparent;outline-offset:2px}.focus\:ring-4:focus{--tw-ring-offset-shadow: var(--tw-ring-inset) 0 0 0 var(--tw-ring-offset-width) var(--tw-ring-offset-color);--tw-ring-shadow: var(--tw-ring-inset) 0 0 0 calc(4px + var(--tw-ring-offset-width)) var(--tw-ring-color);box-shadow:var(--tw-ring-offset-shadow),var(--tw-ring-shadow),var(--tw-shadow, 0 0 #0000)}.focus\:ring-blue-500:focus{--tw-ring-opacity: 1;--tw-ring-color: rgb(9 105 218 / var(--tw-ring-opacity))}.focus\:ring-gray-200:focus{--tw-ring-opacity: 1;--tw-ring-color: rgb(156 163 175 / var(--tw-ring-opacity))}.peer:checked~.peer-checked\:bg-blue-600{--tw-bg-opacity: 1;background-color:rgb(9 105 218 / var(--tw-bg-opacity))}.peer:checked~.peer-checked\:bg-green-600{--tw-bg-opacity: 1;background-color:rgb(52 168 83 / var(--tw-bg-opacity))}.peer:checked~.peer-checked\:after\:translate-x-full:after{content:var(--tw-content);--tw-translate-x: 100%;transform:translate(var(--tw-translate-x),var(--tw-translate-y)) rotate(var(--tw-rotate)) skew(var(--tw-skew-x)) skewY(var(--tw-skew-y)) scaleX(var(--tw-scale-x)) scaleY(var(--tw-scale-y))}.peer:checked~.peer-checked\:after\:border-white:after{content:var(--tw-content);--tw-border-opacity: 1;border-color:rgb(255 255 255 / var(--tw-border-opacity))}.peer:focus~.peer-focus\:outline-none{outline:2px solid transparent;outline-offset:2px}.peer:focus~.peer-focus\:ring-4{--tw-ring-offset-shadow: var(--tw-ring-inset) 0 0 0 var(--tw-ring-offset-width) var(--tw-ring-offset-color);--tw-ring-shadow: var(--tw-ring-inset) 0 0 0 calc(4px + var(--tw-ring-offset-width)) var(--tw-ring-color);box-shadow:var(--tw-ring-offset-shadow),var(--tw-ring-shadow),var(--tw-shadow, 0 0 #0000)}.peer:focus~.peer-focus\:ring-blue-300{--tw-ring-opacity: 1;--tw-ring-color: rgb(134 197 243 / var(--tw-ring-opacity))}.peer:focus~.peer-focus\:ring-green-300{--tw-ring-opacity: 1;--tw-ring-color: rgb(128 222 219 / var(--tw-ring-opacity))}:is(.dark .dark\:border-\[\#242526\]){--tw-border-opacity: 1;border-color:rgb(36 37 38 / var(--tw-border-opacity))}:is(.dark .dark\:border-\[\#3a3b3c\]){--tw-border-opacity: 1;border-color:rgb(58 59 60 / var(--tw-border-opacity))}:is(.dark .dark\:border-\[\#e4e6eb\]){--tw-border-opacity: 1;border-color:rgb(228 230 235 / var(--tw-border-opacity))}:is(.dark .dark\:border-gray-600){--tw-border-opacity: 1;border-color:rgb(75 85 99 / var(--tw-border-opacity))}:is(.dark .dark\:border-opacity-20){--tw-border-opacity: .2}:is(.dark .dark\:bg-\[\#18191a\]){--tw-bg-opacity: 1;background-color:rgb(24 25 26 / var(--tw-bg-opacity))}:is(.dark .dark\:bg-\[\#242526\]){--tw-bg-opacity: 1;background-color:rgb(36 37 38 / var(--tw-bg-opacity))}:is(.dark .dark\:bg-gray-700){--tw-bg-opacity: 1;background-color:rgb(55 65 81 / var(--tw-bg-opacity))}:is(.dark .dark\:bg-transparent){background-color:transparent}:is(.dark .dark\:text-\[\#b0b3b8\]){--tw-text-opacity: 1;color:rgb(176 179 184 / var(--tw-text-opacity))}:is(.dark .dark\:text-\[\#e4e6eb\]){--tw-text-opacity: 1;color:rgb(228 230 235 / var(--tw-text-opacity))}:is(.dark .dark\:text-white){--tw-text-opacity: 1;color:rgb(255 255 255 / var(--tw-text-opacity))}:is(.dark .dark\:text-opacity-60){--tw-text-opacity: .6}:is(.dark .dark\:placeholder-gray-400)::-moz-placeholder{--tw-placeholder-opacity: 1;color:rgb(156 163 175 / var(--tw-placeholder-opacity))}:is(.dark .dark\:placeholder-gray-400)::placeholder{--tw-placeholder-opacity: 1;color:rgb(156 163 175 / var(--tw-placeholder-opacity))}:is(.dark .dark\:opacity-10){opacity:.1}:is(.dark .dark\:hover\:border-opacity-80:hover){--tw-border-opacity: .8}:is(.dark .dark\:hover\:bg-\[\#3a3b3c\]:hover){--tw-bg-opacity: 1;background-color:rgb(58 59 60 / var(--tw-bg-opacity))}:is(.dark .dark\:focus\:border-blue-500:focus){--tw-border-opacity: 1;border-color:rgb(9 105 218 / var(--tw-border-opacity))}:is(.dark .dark\:focus\:ring-\[\#3a3b3c\]):focus){--tw-ring-opacity: 1;--tw-ring-color: rgb(58 59 60 / var(--tw-ring-opacity))}:is(.dark .dark\:focus\:ring-blue-500:focus){--tw-ring-opacity: 1;--tw-ring-color: rgb(9 105 218 / var(--tw-ring-opacity))}:is(.dark .peer:focus~.dark\:peer-focus\:ring-blue-800){--tw-ring-opacity: 1;--tw-ring-color: rgb(23 78 132 / var(--tw-ring-opacity))}:is(.dark .peer:focus~.dark\:peer-focus\:ring-green-800){--tw-ring-opacity: 1;--tw-ring-color: rgb(23 102 82 / var(--tw-ring-opacity))}

</style>
  </head>
  <body class="dark:bg-[#18191a] bg-white dark:text-[#e4e6eb] px-6">
    <!-- <div class="absolute top-0 left-0 right-0 bottom-0 dark:bg-[#18191a] z-50 flex justify-center items-center">
      <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="animate-spin"><path d="M21 12a9 9 0 1 1-6.219-8.56"/></svg>
    </div> -->
    <div class="flex flex-col items-center mt-12 mb-16">
      <!-- Logo -->
      <svg version="1.2" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 840 197" width="220" height="60" class="dark:text-[#e4e6eb]">
        <path fill-rule="evenodd" fill="currentColor" d="m10.3 51.9l-10.3-5.9v76c0 1.7 0.9 3.4 2.4 4.2l7.9 4.6zm19.5 11l-12.2-6.9v79l12.2 7zm19.5 11.1l-12.2-6.9v79.1l12.2 7.1zm19.5 11.1l-12.2-6.9v79.4l12.2 7zm6.1-84.4c-1.5-0.9-3.3-0.9-4.8 0l-67 38.7 12.2 6.9 69.9-39.7zm-52.2 49.9l10.9 6.2 69.8-39.6-10.9-6.3zm18.4 10.4l10.9 6.2 69.6-39.5-10.8-6.3zm18.4 10.5l13 7.4 69.5-39.4-13-7.5zm16.8 13.8v79.4l66.4-38.3c1.5-0.9 2.4-2.5 2.4-4.2v-76z"/>
        <path fill="currentColor" d="m257.1 130v-17.1h-33.3v-15.5h30.1v-15.9h-30.1v-15.3h33.2v-17h-51.2v80.8zm31.3 0v-82.5h-17.3v82.5zm28.4-35.1c0.3-4.2 4-9.6 11.3-9.6 8.2 0 11.1 5.3 11.4 9.6zm23.9 14.7c-1.6 4.4-5.1 7.4-11.4 7.4-6.7 0-12.5-4.6-12.9-10.9h39.5c0.1-0.4 0.3-3 0.3-5.4 0-18.2-10.8-29.1-28.4-29.1-14.7 0-28.2 11.7-28.2 29.9 0 19 13.9 30.2 29.5 30.2 14.3 0 23.4-8.2 26.1-18zm22.7 23.6c1.6 10.1 11.9 20.2 28.2 20.2 7.5 0 13,3.2 15.9-8 0 3.9 0.5 5.9 0.6 6.4h15.6c-0.1-0.6-0.7-4.4-0.7-8.8v-27.7h-16.6v6.1c-1.6-3-6.5-7.1-15.7-7.1-15.5 0-26.3 12.8-26.3 27.7 0 15.8 11.3 27.6 26.3 27.6 8.2 0 13.1-3.3 15.2-6.4v2.6c0 10.3-5.4 14.5-14.1 14.5-6.4 0-11,4-12.1-9.4zm29.5-20.7c-7 0-12.2-4.8-12.2-12.5 0-7.8 5.7-12.5 12.2-12.5 6.5 0 12.1 4.7 12.1 12.5 0 7.7-5.1 12.5-12.1 12.5zm39.6 1.9c0 9 7.2 17.2 19.7 17.2 7.5 0 13,3.2 15.9-8 0 3.9 0.5 5.9 0.6 6.4h15.6c-0.1-0.6-0.7-4.4-0.7-8.8v-27.7c0-11.6-6.4-22.6-25.1-22.6-16.6 0-24.1 10.7-24.8 18.8l15 3.1c0.4-4.2 3.7-8.3 9.7-8.3 5.5 0 8.3 2.8 8.3 6.1 0 2-1 3.4-4.1 3.9l-13.3 2.1c-9.3 1.3-16.8 7-16.8 17.1zm23.9 4.6c-4.8 0-6.7-2.8-6.7-6.3 0-4 2.7-5.7 6.3-6.3l10.7-1.7v2.5c0 8.8-5.2 11.3-10.3 11.3zm59.1-21.5c0-5.7 3.7-9.9 9.4-9.9 6.3 0 9.2 4.2 9.2 9.8v32.6h17.4v-35.6c0-12.4-6.4-22.6-25.1-22.6-16.6 0-24.1 10.7-24.8 18.8l15 3.1c0.4-4.2 3.7-8.3 9.7-8.3 5.5 0 8.3 2.8 8.3 6.1 0 2-1 3.4-4.1 3.9l-13.3 2.1c-9.3 1.3-16.8 7-16.8 17.1zm59.1-21.5c0-5.7 3.7-9.9 9.4-9.9 6.3 0 9.2 4.2 9.2 9.8v32.6h17.4v-35.6c0-12.4-6.4-22.6-25.1-22.6-16.6 0-24.1 10.7-24.8 18.8l15 3.1c0.4-4.2 3.7-8.3 9.7-8.3 5.5 0 8.3 2.8 8.3 6.1 0 2-1 3.4-4.1 3.9l-13.3 2.1c-9.3 1.3-16.8 7-16.8 17.1zm71.3-40.7h-15.5v7.3c0 5.2-2.9 9.2-8.8 9.2h-2.8v15.2h10v24.2c0 11.3 7.2 18.3 18.9 18.3 5.5 0 8.3-1.3 9-1.6v-14.4c-1 0.3-2.7 0.6-4.5 0.6-3.8 0-6.3-1.3-6.3-5.9v-21.2h11v-15.2h-11zm38 32.7c0-16.8 12-24.5 23.3-24.5 11.2 0 23.3 7.7 23.3 24.5 0 16.9-12.1 24.4-23.3 24.4-11.3 0-23.3-7.5-23.3-24.4zm-18.7 0.1c0 25.6 19.2 42.1 42 42.1 22.7 0 42-16.5 42-42.1 0-25.6-19.3-42.1-42-42.1-22.8 0-42 16.5-42 42.1zm156.3-23.2v-17.2h-67.9v17.2h24.9v63.6h18v-63.6zm57.7 63.6h19.6l-30.1-80.8h-20.9l-30.4 80.8h18.9l5.8-16.4h31.3zm-21.2-60.8l9.6 27.9h-19.6zm-27.6 126.8v-8.3h-16.9v-30.7h-8.7v39zm13.5 0v-27.3h-8.4v27.3zm-9.1-35.7c0 2.7 2.2 4.9 4.9 4.9 2.7 0 4.9-2.2 4.9-4.9 0-2.7-2.2-4.9-4.9-4.9-2.7 0-4.9 2.2-4.9 4.9zm26.4 0.4h-7.5v3.5c0 2.5-1.3 4.5-4.2 4.5h-1.4v7.3h4.9v11.7c0 5.4 3.4 8.8 9.1 8.8 2.6 0 4-0.6 4.4-0.8v-6.9c-0.5 0.1-1.4 0.3-2.2 0.3-1.9 0-3.1-0.6-3.1-2.9v-10.2h5.4v-7.3h-5.4zm17.3 18.3c0.2-2 1.9-4.6 5.4-4.6 4 0 5.4 2.6 5.5 4.6zm11.6 7.1c-0.8 2.2-2.5 3.6-5.5 3.6-3.3 0-6.1-2.2-6.2-5.3h19c0.1-0.1 0.2-1.4 0.2-2.6 0-8.8-5.3-14,13.7-14-7.1 0-13.7 5.6-13.7 14.4 0 9.2 6.7 14.6 14.3 14.6 6.9 0 11.3-4 12.6-8.7z"/>
        <path d="M3.85 8.62a4 4 0 0 1 4.78-4.77 4 4 0 0 1 6.74 0 4 4 0 0 1 4.78 4.78 4 4 0 0 1 0 6.74 4 4 0 0 1-4.77 4.78 4 4 0 0 1-6.75 0 4 4 0 0 1-4.78-4.77 4 4 0 0 1 0-6.76Z"/><path d="M9 12v4M12 17h.01M16 17v4M19 12H5"/></svg>
      <!-- Upload Column -->
      <form>
        <button id="uploadButton" class="dark:text-white bg-gray-50 border border-[#b0b3b8] border-opacity-40 hover:bg-gray-200 text-gray-700 focus:outline-none focus:ring-4 focus:ring-gray-200 rounded-full px-5 py-3 mr-2 flex flex-row gap-3 items-center dark:bg-[#242526] dark:hover:bg-[#3a3b3c] dark:focus:ring-[#3a3b3c] dark:border-[#3a3b3c]">
          <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-file-up"><path d="M14.5 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 1.73-3Z"/><path d="M9 12v4"/><path d="M12 17h.01M16 17v4"/></svg>
          <span>
            Select File
          </span>
        </button>
        <input type="file" id="fileInput" class="hidden" accept=".bin,.bin.gz" onchange="onFileInput(files)">
      </form>
      <!-- Progress Column -->
      <div id="progressColumn" class="flex flex-col justify-center mt-14 mb-10 hidden">
        <p id="progressTitle" class="text-center mb-2 text-sm dark:text-[#b0b3b8]"></p>
        <div class="flex flex-row items-center gap-4 w-[280px]">
          <div class="w-full bg-gray-200 rounded-full h-2.5 dark:bg-[#242526]">
            <div id="progressBar" class="bg-blue-600 h-2.5 rounded-full" style="width: 0%"></div>
          </div>
          <p id="progressValue" class="text-sm">
            0%
          </p>
        </div>
      </div>
      <!-- Success Column -->
      <div id="successColumn" class="flex flex-col items-center gap-4 mt-12 hidden">
        <svg xmlns="http://www.w3.org/2000/svg" width="42" height="42" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="text-green-500"><path d="M3.85 8.62a4 4 0 0 1 4.78-4.77 4 4 0 0 1 6.74 0 4 4 0 0 1 4.78 4.78 4 4 0 0 1 0 6.74 4 4 0 0 1-4.77 4.78 4 4 0 0 1-6.75 0 4 4 0 0 1-4.78-4.77 4 4 0 0 1 0-6.76Z"/><path d="M9 12v4"/>
S<path d="M12 17h.01M16 17v4"/></svg>
        <p>
          Update Successful
        </p>
        <button onclick="resetView()" type="button" class="dark:text-white bg-gray-50 border border-[#b0b3b8] border-opacity-40 hover:bg-gray-200 text-gray-700 text-xs focus:outline-none focus:ring-4 focus:ring-gray-300 font-medium rounded-full px-4 py-2 mr-2 mt-2 flex flex-row gap-2 items-center dark:bg-[#242526] dark:hover:bg-[#3a3b3c] dark:focus:ring-[#3a3b3c] dark:border-[#3a3b3c]">
          <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-arrow-left"><path d="m12 19-7-7 7-7"/><path d="M19 12H5"/></svg>
          Go Back
        </button>
      </div>

      <!-- Error Column -->
      <div id="errorColumn" class="flex flex-col items-center gap-2 mt-12 hidden">
        <svg xmlns="http://www.w3.org/2000/svg" width="42" height="42" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="text-red-500"><path d="m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3Z"/><path d="M12 9v4"/>
<path d="M12 17h.01"/></svg>
        <p id="errorTitle" class="mt-2">
          Unexpected Error
        </p>
        <p id="errorReason" class="text-xs dark:text-[#b0b3b8]">
          Please try again
        </p>
        <button onclick="resetView()" type="button" class="dark:text-white bg-gray-50 border border-[#b0b3b8] border-opacity-40 hover:bg-gray-200 text-gray-700 text-xs focus:outline-none focus:ring-4 focus:ring-gray-300 font-medium rounded-full px-4 py-2 mr-2 mt-4 flex flex-row gap-2 items-center dark:bg-[#242526] dark:hover:bg-[#3a3b3c] dark:focus:ring-[#3a3b3c] dark:border-[#3a3b3c]">
          <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-arrow-left"><path d="m12 19-7-7 7-7"/><path d="M19 12H5"/></svg>
          Go Back
        </button>
      </div>

      <!-- Settings Column -->
      <div id="settingsColumn" class="flex flex-col justify-center mt-10">
        <div class="flex flex-row justify-center items-center gap-1 uppercase text-xs select-none">
          <p class="dark:text-[#b0b3b8] transition-colors">
            Operating System
          </p>
        </div>
        <div id="settingsContent" class="flex flex-col gap-4 mt-6 w-[300px]">
          <div class="flex flex-row justify-between items-center gap-4">
            <p>
              OTA Mode
            </p>
            <select id="otaMode" class="block appearance-none py-1 px-2 text-sm text-gray-900 border border-gray-200 rounded-lg bg-gray-50 focus:ring-blue-500 focus:border-blue-500 dark:bg-[#242526] dark:border-[#242526] dark:placeholder-gray-400 dark:text-white dark:focus:ring-blue-500 dark:focus:border-blue-500">
              <option value="fr" selected>Firmware</option>
              <option value="fs">LittleFS / SPIFFS</option>
            </select>
          </div>
          <div class="flex flex-row justify-between items-center gap-4">
            <svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="text-yellow-400"><path d="m12 3-1.912 5.813a2 2 0 0 1-1.275 1.275L3 12l5.813 1.912a2 2 0 0 1 1.275 1.275L12 21l1.912-5.813a2 2 0 0 1 1.275-1.275L21 12l-5.813-1.912a2 2 0 0 1-1.275-1.275L12 3Z"/><path d="M5 3v4"/><path d="M17 19v4"/><path d="M3 5h4"/><path d="M17 19h4"/></svg>
            <p class="text-sm ml-2 pr-5">
              Upgrade to ElegantOTA Pro - Get access to branding and more!
            </p>
            <svg xmlns="http://www.w3.org/2000/svg" width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="absolute right-2 top-2"><path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6"/><polyline points="15 3 21 3 21 9"/><line x1="10" x2="21" y1="14" y2="3"/></svg>
          </div>
        </div>
      </div>
      <!--
    </div>
  </body>
</html>
`;
