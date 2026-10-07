
const VERSION = '1.0.6';

// --- 1. GESTIONE ROUTER (Cambio Pagine) ---
const btnHome = document.getElementById('btn-home');
const btnSettings = document.getElementById('btn-settings');
const pageHome = document.getElementById('page-home');
const pageSettings = document.getElementById('page-settings');

function switchPage(page) {
  if (page === 'home') {
    btnHome.classList.add('active');
    btnSettings.classList.remove('active');
    pageHome.classList.remove('hidden');
    pageSettings.classList.add('hidden');
  } else {
    btnHome.classList.remove('active');
    btnSettings.classList.add('active');
    pageHome.classList.add('hidden');
    pageSettings.classList.remove('hidden');
    
    sendWsMessage({ action: "get_settings" });
  }
}

btnHome.addEventListener('click', () => switchPage('home'));
btnSettings.addEventListener('click', () => switchPage('settings'));

// --- 2. GESTIONE DEI CONTROLLI (Luci, Prese, Switch) ---
document.getElementById('version-ui').innerText = VERSION;

const mainEl = document.querySelector('main');
const systemName = document.getElementById('txt-system-name');
const containerLights = document.getElementById('light-container');
const containerOutlets = document.getElementById('outlets-container');
const generatedControls = new Set();
const alarmSections = new Map()

// Solo aggiornamento visivo, ad ogni tick del trascinamento: leggero, nessun invio.
function linkSliderLabel(sliderKey) {
  const txtSpan = document.getElementById(`val-${sliderKey}`);
  if (txtSpan) {
    const slider = document.getElementById(`slider-${sliderKey}`);
    if (slider)
      slider.addEventListener('input', (e) => {
        txtSpan.innerText = e.target.value;
      });
    return slider;
  }
}

function linkSwitchLabel(switchKey) {
  const txtSpan = document.getElementById(`txt-${switchKey}`);
  const sw = document.getElementById(`switch-${switchKey}`);
  if (sw && txtSpan) {
    sw.addEventListener('change', (e) => {
      if (txtSpan) txtSpan.innerText = e.target.checked ? 'ON' : 'OFF';
    });
    return sw;
  }
}

function createDynamicSlider(container, type, idNumber, labelName, iconId, initialValue, minValue=0, maxValue=100) {
  const key = `${type}${idNumber}`;
  if (generatedControls.has(key)) return;
  generatedControls.add(key);

  const ctl = document.createElement('div');
  ctl.className = 'ctl';
  ctl.innerHTML = `
    <div class="ctl-head">
      <span class="lbl"><svg class="icon"><use href="#${iconId}"/></svg> ${labelName}</span>
      <span class="ctl-value"><span id="val-${key}">${initialValue}</span>%</span>
    </div>
    <input type="range" id="slider-${key}" min="${minValue}" max="${maxValue}" value="${initialValue}" aria-label="${labelName}">
  `;
  container.appendChild(ctl);
  return ctl.querySelector('input[type="range"]');
}

function setSlider(field, value) {
    document.getElementById('val-' + field).innerText = value;
    document.getElementById('slider-' + field).value = value;
}

function setSwitch(field, value) {
    document.getElementById('txt-' + field).innerText = ( value ? 'ON' : 'OFF' );
    document.getElementById('switch-' + field).checked = value;

    // automation details reaction to enable/disable signalled by device
    if (field === 'light-auto') dimAutoSettings(value); 
}

function createOutletSliders(n) {
  if (containerOutlets.children.length > 1) return; // do nothing if we already have sliders
  for (let i = 0; i < n; i++) {
    const slider = createDynamicSlider(containerOutlets, 'outlet', i, `Outlet ${i + 1}`, 'i-plug', 0);
    linkSliderLabel(`outlet${i}`);
    const index = i;
    slider.addEventListener('change', (e) => {
      sendWsMessage({ action: 'outlet_power', index: index, power: (parseFloat(e.target.value) / 100.0) });
    });
  }
}

function setControlAlarm(controls, al=true) {
  const lst = Array.isArray(controls) ? controls : [controls];
  lst.forEach(cid => {
    const c = document.getElementById(cid);
    if (al)
      c.classList.add('alarm');
    else
      c.classList.remove('alarm');
  });
}

function updateAlarm(which, alarm, text='') {
  if (alarm) {
    if (! alarmSections.has(which)) {
      alarmSections.set(which, text);
    }
  } else {
    if (alarmSections.has(which)) {
      alarmSections.delete(which);
    }
  }
    
  const alarm_box = document.getElementById('batt-alarm');
  if (alarmSections.size > 0) {
    let at = '';

    alarmSections.forEach( (value, key) => {
      at += value + '. ';
    });
    
    alarm_box.title = at;
    alarm_box.classList.remove('hidden');
  } else 
    alarm_box.classList.add('hidden');
}

function dimAutoSettings(on) {
  document.getElementById('light-auto-details').classList.toggle('off', !on);
}

linkSliderLabel('batt-lt').addEventListener('change', (e) => {
  sendWsMessage({ action: 'cp_low_threshold', temperature: parseInt(e.target.value) });
});

linkSliderLabel('light-brightness').addEventListener('change', (e) => {
  sendWsMessage({ action: 'light_brightness', brightness: (parseFloat(e.target.value) / 100.0)});
});

linkSwitchLabel('light-auto').addEventListener('change', e => {
  dimAutoSettings(e.target.checked);
  sendWsMessage({ action: 'light_auto_enabled', enabled: e.target.checked });            
});

linkSliderLabel('light-auto_br').addEventListener('change', (e) => {
  sendWsMessage({ action: 'light_auto_brightness', brightness: (parseFloat(e.target.value) / 100.0) });
});

linkSliderLabel('light-auto_dr').addEventListener('change', (e) => {
  sendWsMessage({ action: 'light_auto_duration', seconds: parseInt(e.target.value) });
});

document.getElementById('batt-alarm').addEventListener('click', (e) => {
  showAlert(document.getElementById('batt-alarm').title);
});

document.getElementById('btn-restore-settings').addEventListener('click', e => {
  settingsChanged = false;
  sendWsMessage({ action: 'get_settings' });
});

document.getElementById('btn-reset-soc').addEventListener('click', e => {
  showConfirm("Are you sure you want to reset battery charge to 100%?", async () => {
    try {
      const r = await request({ action: 'battery_soc_reset' });
      if (! r.payload) { showAlert(`SoC reset failed: ${r.detail || 'unknown'}`); return; }
      // no need to confirm success: SoC is now synced to 100%.
    } catch(err) {
      showToast('No response from the device. ', {error:true});
    }
  });
});

document.getElementById('btn-restart').addEventListener('click', e => {
  showConfirm("Are you sure you want to restart the unit?", () => {
    restartDevice();
  });
});

document.getElementById('btn-factory-reset').addEventListener('click', e => {
  // Invia i dati tramite l'unica connessione WebSocket attiva
  showConfirm(
    "Are you sure you want to factory reset the unit?",
    () => { 
      showConfirm("Are you REALLY sure you want to factory reset the unit? You'll lose all Wi-Fi settings.",
        async () => {
          const data = {
            action: "update_settings",
            payload: { factory_reset: true, factory_reset_confirm: "CONFIRM FACTORY RESET" }
          };

          try {
            settingsChanged = false;
            const r = await request(data);
            if (! r.payload) { showAlert(`Factory reset failed: ${r.detail || 'unknown'}`); return; }
            const hostname = document.getElementById('stg-hostname').value;
            const main_ssid = document.getElementById('stg-main_ssid').value;
            const alt_ssid = document.getElementById('stg-alt_ssid').value;
            showAlert(`Device reset to factory-state and now restarting: it'll become reachable as '${hostname}' on 
              default main or alt networks ('${main_ssid}' and '${alt_ssid}'), or connecting to self-hotspot '${hostname}' 
              network with default secret.`
            );
          } catch(err) {
            showAlert('No response from the device. Factory reset may have failed.');
          }
        }
      );
     }
  );
});

document.getElementById('btn-wifi-rescan').addEventListener('click', () => {
  showConfirm(
    "Scan now for the configured Wi-Fi networks and switch if one is available? The connection to the device may drop.",
    async () => {
      try {
        const r = await request({ action: 'wifi_rescan' });
        if (!r.payload) {
          const msg = r.detail === 'already_on_main' ? 'Already connected to the main network.'
                    : `Rescan not started: ${r.detail || 'unknown'}`;
          showAlert(msg, () => {}, { danger: false });
          return;
        }
        showToast('Scanning for networks...');
      } catch (err) {
        showToast('No response from the device.', { error: true });
      }
    },
    { confirmLabel: 'Scan', danger: false }
  );
});

/* Dynamic Battery icon */
// Soglie icona batteria: la prima con soc >= min vince; sotto l'ultima, 'empty'.
const BATTERY_ICON_LEVELS = [
  { min: 95, icon: 'full' },
  { min: 60, icon: 'three-quarters' },
  { min: 50, icon: 'half' },
  { min: 15, icon: 'quarter' },
];

const batteryIconUse = document.querySelector('#icon-battery use');
const faviconLink = document.querySelector("link[rel*='icon']");
let currentBatteryIcon = null;

function batteryIconFor(soc) {
  for (const l of BATTERY_ICON_LEVELS)
    if (soc >= l.min) return l.icon;
  return 'empty';
}

// Tocca il DOM (e la favicon, che costringe il browser a rifare la richiesta)
// solo quando l'icona cambia davvero.
function updateBatteryIcon(soc) {
  const icon = batteryIconFor(soc);
  if (icon === currentBatteryIcon) return;
  currentBatteryIcon = icon;
  batteryIconUse.setAttribute('href', `#i-battery-${icon}`);
  faviconLink.href = `battery-${icon}.svg`;
}

// --- 3. LOGICA WEBSOCKET & SIMULATORE ---
// websocket globals
let ws = null, lastMsg = null, reconnectTimer = null;

// DEV only: ?host=192.168.x.x punta la UI a un device reale (implica niente simulatore).
// La condizione resta scritta con import.meta.env.DEV direttamente, così in build
// viene foldata a `location.hostname`.
const urlParams = new URLSearchParams(location.search);
const WS_HOST = (import.meta.env.DEV && urlParams.get('host')) || location.hostname;

// globals to manage page reload on device restart
let restarting = false, bootCount = null, waitingRestartTimeout = null, oldHostname = null, restartHostname = null;
let uploading = false;

// simulation module
let sim = null; 

// request -> result globals
let nextReqId = 1;
const pending = new Map();

const wsStatus = document.getElementById('ws-status');

function waitForRestart() {
  if (waitingRestartTimeout)
    clearTimeout(waitingRestartTimeout);
  restarting = true;
  waitingRestartTimeout = setTimeout(() => {
    restarting = false;
    waitingRestartTimeout = null;
    if (!progressModal.classList.contains('hidden'))
      progressModal.classList.add('hidden');
    showAlert("Timeout waiting for device to restart.");
  }, 60000);
}

function setConnected(on, label) {
  const text = label || (on ? 'Connected' : 'Disconnected');
  wsStatus.className = 'conn ' + (on ? 'online' : 'offline');
  wsStatus.title = text;
  wsStatus.setAttribute('aria-label', text);
  document.body.classList.toggle('is-offline', !on);
  mainEl.inert = !on;
}

function connect() {
  clearTimeout(reconnectTimer);
  lastMsg = Date.now();                      // vale anche per lo stato CONNECTING
  const sock = new WebSocket(`ws://${WS_HOST}:81`);
  ws = sock;
  sock.onopen = () => { lastMsg = Date.now(); setConnected(true); };
  sock.onmessage = (e) => {
    lastMsg = Date.now();
    try { handleIncomingData(JSON.parse(e.data)); } catch (err) { console.warn('bad message', err); }
  };
  sock.onclose = () => { if (sock === ws) dropConnection(); };   // ignora socket già abbandonati
}

function dropConnection() {
  if (ws) {
    const old = ws; ws = null;
    old.onopen = old.onmessage = old.onclose = null;   // niente callback tardive
    try { old.close(); } catch (_) {}
  }
  setConnected(false);
  failPending();                                       
  clearTimeout(reconnectTimer);
  if (!uploading) 
    reconnectTimer = setTimeout(connect, 2000);
}

// watchdog
setInterval(() => {
  if (ws && ws.readyState <= WebSocket.OPEN && Date.now() - lastMsg > 6000) dropConnection();
}, 2000);

// disconnect on pagehide and reconnect on visibilitychange if we're on stage
window.addEventListener('pagehide', () => { if (ws) ws.close(); });
document.addEventListener('visibilitychange', () => {
  if (!document.hidden && ws && Date.now() - lastMsg > 3000) dropConnection();
});

function initWebSocket() {
  if (import.meta.env.DEV && !urlParams.has('real') && !urlParams.has('host')) {
    import('./sim.js').then(
      m => {
        sim = m; window.sim = sim; m.start(handleIncomingData, setConnected);
      });
    return;
  }

  // PRODUCTION
  setConnected(false);
  connect();
}

function sendWsMessage(obj) {
  if (import.meta.env.DEV && sim) {
    console.log("➡️ [WS SIMULATO] Invio:", obj); 
    sim.send(obj, handleIncomingData); 
    return; 
  }

  if (ws && ws.readyState === WebSocket.OPEN) { 
    //console.log("➡️ ", obj);
    ws.send(JSON.stringify(obj)); 
  } else {
    showToast('Not connected', {error:true});
  }
}

/** sends a request adding it to the response-waiting queue */
function request(obj, timeoutMs = 5000) {
  if (import.meta.env.DEV && sim) {
    console.log("➡️ [WS SIMULATO] Request:", obj);
    return Promise.resolve(sim.send(obj, handleIncomingData));
  }

  return new Promise((resolve, reject) => {
    if (!ws || ws.readyState !== WebSocket.OPEN) { 
      reject(new Error('offline'));
      return; 
    }
    const id = nextReqId++;
    const timer = setTimeout(() => { 
      pending.delete(id); 
      reject(new Error('timeout')); 
    }, timeoutMs);
    pending.set(id, { resolve, timer });
    ws.send(JSON.stringify({ ...obj, id }));
  });
}

// chiamata da dropConnection(): risolve in errore tutte le richieste in volo
function failPending() {
  for (const [, p] of pending) {
    clearTimeout(p.timer);
    p.resolve({ type: 'result', payload: false, detail: 'disconnected' });
  }
  pending.clear();
}

/** Asks the device to restart and schedules a page refresh */
async function restartDevice() {
  try {
    const r = await request({ action: 'restart' });
    if (! r.payload) { showAlert(`Restart failed: ${r.detail || 'unknown'}`); return; }
    waitForRestart();
    showAlert("Device is restarting...", () => {}, { danger: false });
  } catch(err) {
    showAlert('No response from the device. Device may not have restarted.');
  }
}

// Funzione centrale per applicare i dati o discriminare il tipo di controllo
function handleIncomingData(data) {
  //console.log(data);
  if (data.type) {
    /*
        - channels
        - light
        - outlets
        - light_auto_br_min_pct
        - light_auto_dr_min
        - light_auto_dr_max
        - cp_lt_min
        - cp_lt_max
    */
    if (data.type == 'capabilities') {
        const p = data.payload;

        //const channels = p.channels;
        if (restarting && bootCount !== null && p.boot_count > bootCount) {
          if (restartHostname) {
            const newHref = location.href.replace(oldHostname, restartHostname);
            if (import.meta.env.DEV) {
              console.log(`Hostname '${oldHostname}' => '${restartHostname}'. Switching to '${newHref}' in 5s.`);
              setTimeout(() => {
                location.replace(newHref);
              }, 5000);
            } else {
              location.replace(newHref);
            }
          } else {
            location.reload();
          }
          return;
        }
        bootCount = p.boot_count;

        if (p.light) {
          containerLights.classList.remove('hidden');
        }

        if (p.outlets > 0) {
          createOutletSliders(p.outlets);
          containerOutlets.classList.remove('hidden');
        }

        if (p.cp_lt_max !== undefined) {
          document.getElementById('slider-batt-lt').min = p.cp_lt_min;
          document.getElementById('slider-batt-lt').max = p.cp_lt_max;
        }

        if (p.light_auto_br_min_pct)
          document.getElementById('slider-light-auto_br').min = p.light_auto_br_min_pct;

        if (p.light_auto_dr_max) {
          document.getElementById('slider-light-auto_dr').min = p.light_auto_dr_min;
          document.getElementById('slider-light-auto_dr').max = p.light_auto_dr_max;
        }

        if (p.fw_ver)
          document.getElementById('version-fw').innerText = p.fw_ver;

        sendWsMessage({ action: 'get_settings'});

    } else if (data.type == 'result') {
      const p = pending.get(data.id);
      if (p) {
        clearTimeout(p.timer);
        pending.delete(data.id);
        p.resolve(data);
      } else if (! data.payload) {
        showToast(`Command rejected${data.detail ? ': ' + data.detail : ''}`, { error: true });
      }
    } else if (data.type == 'settings') {
      // don't reset currently edited form fields        
      if (settingsChanged) { return; }

      for (const [k,v] of Object.entries(data.payload)) {
        if (k == 'ap_no_def_gw')
            document.getElementById('stg-ap_no_def_gw').checked = v;
        else {
          const el = document.getElementById('stg-' + k)
          if (el) el.value = v;
        }

        if (k == 'hostname') {
          currentHostname = v;
        }

        if (k == 'display_name') {
          systemName.innerText = v;
          document.title = v;
        }
      }  
      
      document.getElementById('btn-save-settings').disabled = true;
      document.getElementById('btn-restore-settings').disabled = true;
    }
  }

  if (data.event) {
    const BATT_DECIMALS = { voltage: 2, current: 3, soc: 0, temperature: 1 };

    switch(data.event) {
        /* update battery state event.
        Pars:
            - float voltage (can be null if sensor desnt't work)
            - float current (can be null if sensor desnt't work)
            - float SoC
            - float temperature (can be null if sensor desnt't work)
            - bool battery_sensor_ok (false when INA226 not responding)
        */
        case 'battery_update': {
            for (const el of ['voltage', 'current', 'soc', 'temperature']) {
              const v = data[el];
              document.getElementById('batt-' + el).innerText = 
                (v !== null && v !== undefined) ? Number(v).toFixed(BATT_DECIMALS[el]) : '--';
            }

            updateBatteryIcon(Number(data.soc));
            
            const temperature_valid = (data.temperature !== null);
            const battery_valid = (data.battery_sensor_ok && (data.voltage !== null));
            
            setControlAlarm(['batt-voltage', 'batt-current'], ! battery_valid);
            setControlAlarm('batt-temperature', ! temperature_valid);

            updateAlarm('battery', ! battery_valid, "Battery voltage/current sensor is not working");
            updateAlarm('temperature', ! temperature_valid, "Battery temperature sensor is not working");
            break; }

        /* update battery state event.
        Pars:
            - float hours
        */
       case 'battery_autonomy_update': 
            if (data.hours >= 24.0) {
              document.getElementById('batt-autonomy').innerText = '> 24h';
            } else {
              const h = Math.trunc(data.hours);
              const m = Math.trunc((data.hours - h) * 60.0);
              document.getElementById('batt-autonomy').innerText = `${String(h).padStart(2, '0')}h ${String(m).padStart(2, '0')}m`;
            }
            break;

        /* update safety state event.
        Pars:
            - bool is_safe
        */
        case 'safety_update':
            setControlAlarm(['icon-battery', 'batt-soc'], !data.is_safe);
            updateAlarm('safety', !data.is_safe, "Battery is low");
            break;

        /* update cold protection state event.
        Pars:
            - float lt low threshold temperature 
        */
        case 'cold_protection_update':
            setSlider('batt-lt', data.lt);
            break;

        /* update light state event.
        Pars: 
            - float brightness
            - bool auto enabled/disabled
            - float auto_br brightness
            - int auto_dr duration 
        */
        case 'light_update':
            //set sliders
            [ 'brightness', 'auto_br', 'auto_dr' ].forEach(function(k, i) {
                var v = parseFloat(data[k]);
                if (k == 'auto_dr')
                    v = v.toFixed(0);
                else
                    v = (v * 100.0).toFixed(0);
                setSlider('light-' + k, v);
            });

            //set switch
            setSwitch('light-auto', data.auto);
            break;

        /* update power outlets state event.
        Pars:
            - int index
            - float power 
        */
        case 'outlet_update': {
            let v = parseFloat(data.power) * 100.0;
            setSlider('outlet' + data.index, v.toFixed(0));
            break;
        }          

        case 'wifi_scan_result':
          showToast('No usable network found (or connection failed).', { error: true });
          break;

        case 'debug':
          console.log('[device]', data.msg);
          break;
    }
  }
}

if (import.meta.env.DEV) {
  // for debugging purposes
  window.handleIncomingData = handleIncomingData;
}

initWebSocket();

// --- 4. GESTIONE SETTINGS VIA WEBSOCKET ---
let settingsChanged = false; // if something is changed in settings form.
let currentHostname = null;

const formSettings = document.getElementById('form-settings');

function formToSettings() {
  const result = {};
  for (const [k, v] of new FormData(formSettings)) 
    if (k !== 'ap_no_def_gw') result[k] = v;

  result.ap_no_def_gw = document.getElementById('stg-ap_no_def_gw').checked;
  return result;
}

if (formSettings) {
  formSettings.addEventListener('input', (e) => {
    settingsChanged = true;
    document.getElementById('btn-save-settings').disabled = false;
    document.getElementById('btn-restore-settings').disabled = false;
  });

  // A. Intercetta il click sul pulsante Salva
  formSettings.addEventListener('submit', async (e) => {
    e.preventDefault(); // Blocca l'invio HTTP classico della form

    // Sfrutta FormData per raccogliere automaticamente i dati della form
    const settingsData = {
      action: "update_settings",
    };
   
    settingsData.payload = formToSettings();

    let newHostname = '';
    oldHostname = currentHostname;
    if (settingsData.payload.hostname != currentHostname) {
      newHostname = settingsData.payload.hostname;
    }

    try {
      const r = await request(settingsData);
      if (! r.payload) { 
        settingsChanged = true;
        showAlert(`Not saved. Rejected: ${r.detail || 'unknown'}`); 
        return; 
      }
      settingsChanged = false;
      sendWsMessage({ action: 'get_settings' });
      
      let hnMessage = '';
      if (newHostname) {
        hnMessage = ` After restart the device will be reachable at '${newHostname}'`;
        restartHostname = newHostname;
      }

      showConfirm(`Settings saved. Restart now to apply them?${ hnMessage }`,
        () => { restartDevice(); },
        { confirmLabel: "Restart", danger: true }
      );
    } catch(err) {
      showAlert('No response from the device. Settings may not have been saved.');
    }
  });
}

// --- 5. GESTIONE CAMBIO TEMA (CHIARO / SCURO) ---
const btnTheme = document.getElementById('btn-theme');

function saveTheme(name) {
  try { localStorage.setItem('theme', name); } catch (_) {}
}

// Lo stato iniziale lo decide lo script inline nel <head>: qui ci si limita
// ad allineare titolo del bottone e a gestire il toggle.
function applyTheme(name, persist = false) {
  const dark = (name === 'dark');
  document.documentElement.classList.toggle('dark', dark);
  btnTheme.title = dark ? "Switch to light theme" : "Switch to dark theme";
  if (persist) saveTheme(name);
}

applyTheme(document.documentElement.classList.contains('dark') ? 'dark' : 'light');

btnTheme.addEventListener('click', () => {
  const dark = document.documentElement.classList.contains('dark');
  applyTheme(dark ? 'light' : 'dark', true);
});

// --- 6. ALERT & CONFIRM MODAL (sostituisce alert() e confirm() nativi del browser) + TOAST ---
// Un solo modal condiviso (uno per alert e uno per confirm): ogni chiamata a 
// showAlert/Confirm() sovrascrive i // listener di ok/cancel invece di accumularli 
// (niente doppie conferme se showConfirm viene richiamata più volte prima che 
// l'utente risponda).
const alertModal = document.getElementById('alert-modal');
const alertMessage = document.getElementById('alert-message');
const alertCloseBtn = document.getElementById('alert-close');

/**  
 * Shows an alert box with a message, an optional onClose listener and options.
 * Options object supports following variables:
 *  - danger: bool, default: true. Renders close button in danger or normal color.
 *  - closeLabel: string, default 'Close'. The text label for close button.
*/
function showAlert(message, onClose = undefined, options = {}) {
  alertMessage.innerText = message;
  alertCloseBtn.innerText = options.closeLabel || 'Close';
  alertCloseBtn.className = (options.danger === false) ? 'btn-submit' : 'btn-submit btn-danger';
  alertCloseBtn.onclick = () => {
    alertModal.classList.add('hidden');
    if (onClose) onClose();
  }

  alertModal.classList.remove('hidden');
}

const confirmModal = document.getElementById('confirm-modal');
const confirmMessage = document.getElementById('confirm-message');
const confirmOkBtn = document.getElementById('confirm-ok');
const confirmCancelBtn = document.getElementById('confirm-cancel');

/**  
 * Shows a confirm box with a message, an onConfirm listener and options.
 * Options object supports following variables:
 *  - danger: bool, default: true. Renders confirm button in danger or normal color.
 *  - confirmLabel: string, default 'Confirm'. The text label for confirm button.
*/
function showConfirm(message, onConfirm, options = {}) {
  confirmMessage.innerText = message;
  confirmOkBtn.innerText = options.confirmLabel || 'Confirm';
  confirmOkBtn.className = (options.danger === false) ? 'btn-submit' : 'btn-submit btn-danger';

  confirmOkBtn.onclick = () => {
    confirmModal.classList.add('hidden');
    onConfirm();
  };
  confirmCancelBtn.onclick = () => {
    confirmModal.classList.add('hidden');
  };

  confirmModal.classList.remove('hidden');
}

// --- TOAST ---
const toastContainer = document.getElementById('toast-container');

/**
 * Shows a non-blocking toast.
 *  - message: text (inserted with textContent, safe against injection)
 *  - options.error: bool, default false. Uses the danger accent color.
 *  - options.duration: ms, default 4000 (6000 for errors).
 */
function showToast(message, options = {}) {
  const duration = options.duration || (options.error ? 6000 : 4000);

  const t = document.createElement('div');
  t.className = 'toast' + (options.error ? ' error' : '');
  t.textContent = message;
  toastContainer.appendChild(t);

  // doppio rAF: il browser deve registrare lo stato iniziale prima della classe .show
  requestAnimationFrame(() => requestAnimationFrame(() => t.classList.add('show')));

  const remove = () => {
    t.classList.remove('show');
    setTimeout(() => t.remove(), 300);
  };
  const timer = setTimeout(remove, duration);
  t.addEventListener('click', () => { clearTimeout(timer); remove(); });

  // evita pile infinite: massimo 3 toast contemporanei
  while (toastContainer.children.length > 3)
    toastContainer.firstElementChild.remove();
}

// --- 7. PROGRESS MODAL + UPLOAD OTA (firmware/filesystem) ---
const progressModal = document.getElementById('progress-modal');
const progressTitle = document.getElementById('progress-title');
const progressFill = document.getElementById('progress-bar-fill');
const progressPercent = document.getElementById('progress-percent');
const progressStatus = document.getElementById('progress-status');
const progressCloseBtn = document.getElementById('progress-close');

function showProgressModal(title) {
  progressTitle.innerText = title;
  progressFill.classList.remove('error');
  progressFill.style.width = '0%';
  progressPercent.innerText = '0%';
  progressStatus.innerText = '';
  progressCloseBtn.classList.add('hidden');
  progressModal.classList.remove('hidden');
}

function setProgress(pct, statusText) {
  progressFill.style.width = pct + '%';
  progressPercent.innerText = pct + '%';
  if (statusText !== undefined) progressStatus.innerText = statusText;
}

function setProgressError(statusText) {
  progressFill.classList.add('error');
  progressStatus.innerText = statusText;
  progressCloseBtn.classList.remove('hidden');
}

function setProgressDone(statusText) {
  setProgress(100, statusText);
  progressCloseBtn.classList.remove('hidden');
}

progressCloseBtn.addEventListener('click', () => {
  progressModal.classList.add('hidden');
});

function endUpload() {
  uploading = false;
  clearTimeout(reconnectTimer);
  reconnectTimer = setTimeout(connect, 3000);
}

// Upload reale via XMLHttpRequest: serve xhr.upload.onprogress per il
// progress reale del caricamento, cosa che fetch() non offre in modo
// altrettanto diretto.
function uploadOtaFile(file, fieldName, title) {
  showProgressModal(title);

  if (import.meta.env.DEV && sim) { 
    sim.upload({
      progress: p => setProgress(p, 'Upload in progress (simulation)...'),
      done: () => { setProgressDone('Upload done (simulation). Restart in progress...');  waitForRestart(); },
      handleData: handleIncomingData
    });
    return;
  }

  uploading = true;
  dropConnection();      // libera il socket sull'ESP durante la scrittura

  const xhr = new XMLHttpRequest();
  const formData = new FormData();
  formData.append(fieldName, file);

  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      const pct = Math.round((e.loaded / e.total) * 100);
      setProgress(pct, 'Upload in progress...');
    }
  };

  xhr.onload = () => {
    endUpload();
    if (xhr.status === 200) {
      setProgressDone('Upload done. Device is restarting...');
      // Il device riavvia e riconnette WiFi/mDNS: attendiamo prima di
      // ricaricare la SPA, coerente col refresh lato server dopo un OTA.
      waitForRestart();
    } else {
      setProgressError(`Error (${xhr.status}): ${xhr.responseText || 'update failed'}`);
    }
  };

  xhr.onerror = () => {
    endUpload();
    // Puo' capitare anche a upload riuscito, se il device si riavvia prima
    // di chiudere la risposta HTTP: non e' necessariamente un fallimento.
    waitForRestart();
    setProgressError('Connection lost. Waiting for the device to come back...');
  };

  xhr.onabort = () => { endUpload(); setProgressError('Upload aborted'); };

  xhr.open('POST', '/update');
  xhr.send(formData);
}

// Legge i primi 36 byte e applica gli stessi controlli del firmware.
async function checkOtaFile(file, fieldName) {
  const b = new Uint8Array(await file.slice(0, 36).arrayBuffer());
  const looksLikeApp = b.length > 0 && b[0] === 0xE9;

  if (fieldName === 'filesystem')
    return looksLikeApp ? 'This is a firmware image, not a filesystem image.' : null;

  if (b.length < 36 || !looksLikeApp) return 'Not a valid ESP32 firmware image.';
  if ((b[12] | (b[13] << 8)) !== 0)  return 'Firmware built for a different chip.';
  if (file.size < 100 * 1024)        return 'File too small to be a firmware image.';
  return null;
}

function wireOtaForm(formId, inputId, fieldName, title) {
  const form = document.getElementById(formId);
  const input = document.getElementById(inputId);
  if (!form || !input) return;

  form.addEventListener('submit', async (e) => {
    e.preventDefault();

    if (!input.files || input.files.length === 0) {
      showAlert('Select a file first', () => {}, { danger: false });
      return;
    }

    const file = input.files[0];
    const problem = await checkOtaFile(file, fieldName);
    if (problem) { showAlert(problem); return; }

    showConfirm(
      `Are you sure you want to update the ${title.toLowerCase()}? The device will reboot.`,
      () => uploadOtaFile(file, fieldName, title)
    );
  });
}

wireOtaForm('form-ota-firmware', 'input-ota-firmware', 'firmware', 'Firmware update');
wireOtaForm('form-ota-filesystem', 'input-ota-filesystem', 'filesystem', 'Filesystem update');


