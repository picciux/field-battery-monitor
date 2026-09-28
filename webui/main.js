
const VERSION = '0.9.6';

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

// --- 2. GESTIONE DINAMICA DEI CONTROLLI (Luci, Prese, Switch) ---
document.getElementById('version-ui').innerText = VERSION;

const containerLights = document.getElementById('light-container');
const containerOutlets = document.getElementById('outlets-container');
const generatedControls = new Set();

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

function createDynamicSlider(container, type, idNumber, labelName, initialValue, minValue=0, maxValue=100) {
  const key = `${type}${idNumber}`;
  if (generatedControls.has(key)) return;

  generatedControls.add(key);

  const controlGroup = document.createElement('div');
  controlGroup.className = 'control-group';
  controlGroup.innerHTML = `
    <label>${labelName}: <span id="val-${key}">${initialValue}</span>%</label>
    <input type="range" id="slider-${key}" min="${minValue}" max="${maxValue}" value="${initialValue}">
  `;
  container.appendChild(controlGroup);

  return controlGroup.querySelector('input[type="range"]');
}

function setSlider(field, value) {
    document.getElementById('val-' + field).innerText = value;
    document.getElementById('slider-' + field).value = value;
}

function setSwitch(field, value) {
    document.getElementById('txt-' + field).innerText = ( value ? 'ON' : 'OFF' );
    document.getElementById('switch-' + field).checked = value;
}

function createOutletSliders(n) {
  for (var i = 0; i < n; i++) {
    const label = `<svg class="icon"><use href="#i-bolt"/></svg> ${i+1}`
    const slider = createDynamicSlider(containerOutlets, 'outlet', i, label, 0);
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

// --- 3. LOGICA WEBSOCKET & SIMULATORE AGGIORNATO ---
const isLocalTest = window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1';
const wsStatus = document.getElementById('ws-status');
let ws;

function initWebSocket() {
  if (isLocalTest) {
    console.log("🛠️ Esecuzione in locale: Simulazione WebSocket attiva.");
    wsStatus.innerText = "Connesso (Simulazione Locale)";
    wsStatus.className = "status-bar online";
    
    var autonomy = 12.0;

    // PRIMO MESSAGGIO SIMULATO: capabilities e situazione iniziale.
    setTimeout(() => {
      handleIncomingData({
        type: 'capabilities',
        payload: {
            channels: 4,
            light: true,
            outlets: 2,
            fw_ver: "1.0-sim"
        }
      });

      //battery
      handleIncomingData({
        event: 'battery_update',
        temperature: (25 + Math.random() * 5).toFixed(1),
        voltage: (13.1 + Math.random() * 0.4).toFixed(2),
        current: (-0.8 + Math.random() * 0.3).toFixed(2),
        soc: (100 - Math.random() * 3.5).toFixed(0),
        battery_sensor_ok: true,
      });

      //autonomy 
      handleIncomingData({
        event: 'battery_autonomy_update',
        hours: autonomy.toFixed(2)
      });

      // cold protection
      handleIncomingData({
        event: 'cold_protection_update',
        lt: 5.0
      });

      //light
      handleIncomingData({
        event: 'light_update',
        brightness: Math.random(),
        auto: true,
        auto_br: Math.random(),
        auto_dr: (40 + Math.random() * 5).toFixed(0),
      });

      //outlets
      handleIncomingData({
        event: 'outlet_update',
        index: 0,
        power: Math.random(),
      });

      handleIncomingData({
        event: 'outlet_update',
        index: 1,
        power: Math.random(),
      });

    }, 500);

    // MESSAGGI SUCCESSIVI: Aggiornamento ciclico dei sensori fissi
    
    setInterval(() => {
      handleIncomingData({
        event: 'battery_update',
        temperature: (25 + Math.random() * 5).toFixed(1),
        voltage: (13.1 + Math.random() * 0.4).toFixed(2),
        current: (-0.8 + Math.random() * 0.3).toFixed(2),
        soc: (100 - Math.random() * 72).toFixed(0),
        battery_sensor_ok: true,
      });
    }, 2000);

    // Autonomia
    setInterval(() => {
      autonomy -= (10.0 / 3600.0);
      handleIncomingData({
        event: 'battery_autonomy_update',
        hours: autonomy.toFixed(2)
      });
    }, 10000);

    return;
  }

  // AMBIENTE REALE (ESP32)
  ws = new WebSocket(`ws://${window.location.hostname}:81`);
  ws.onopen = () => { wsStatus.innerText = "Connesso"; wsStatus.className = "status-bar online"; };
  ws.onclose = () => { wsStatus.innerText = "Disconnesso..."; wsStatus.className = "status-bar offline"; setTimeout(initWebSocket, 2000); };
  ws.onmessage = (event) => { handleIncomingData(JSON.parse(event.data)); };
}

// --- 4. GESTIONE SETTINGS VIA WEBSOCKET ---

const formSettings = document.getElementById('form-settings');

// A. Intercetta il click sul pulsante Salva
if (formSettings) {
  formSettings.addEventListener('submit', (e) => {
    e.preventDefault(); // Blocca l'invio HTTP classico della form

    // Sfrutta FormData per raccogliere automaticamente i dati della form
    const formData = new FormData(formSettings);
    const settingsData = {
      action: "update_settings",
      payload: {}
    };

    // Converte i campi della form in un oggetto chiave-valore JSON
    formData.forEach((value, key) => {
      // Se il valore è un numero, convertilo (opzionale ma consigliato per C++)
      settingsData.payload[key] = isNaN(value) || value === '' ? value : Number(value);
    });

    // Invia i dati tramite l'unica connessione WebSocket attiva
    sendWsMessage(settingsData);
    alert("Settings saved!"); 
  });
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
        //const channels = data.payload.channels;
        if (data.payload.light) {
          containerLights.classList.remove('hidden');
        }

        if (data.payload.outlets > 0) {
            createOutletSliders(data.payload.outlets);
            containerOutlets.classList.remove('hidden');
        }

        if (data.payload.cp_lt_max) {
            document.getElementById('slider-batt-lt').min = data.payload.cp_lt_min;
            document.getElementById('slider-batt-lt').max = data.payload.cp_lt_max;
        }

        if (data.payload.light_auto_br_min_pct)
            document.getElementById('slider-light-auto_br').min = data.payload.light_auto_br_min_pct;

        if (data.payload.light_auto_dr_max) {
            document.getElementById('slider-light-auto_dr').min = data.payload.light_auto_dr_min;
            document.getElementById('slider-light-auto_dr').max = data.payload.light_auto_dr_max;
        }

        if (data.payload.fw_ver)
          document.getElementById('version-fw').innerText = data.payload.fw_ver;

    } else if (data.type == 'result') {
        if (data.payload == false) {
            //TODO error
        }
    } else if (data.type == 'settings') {
        for (const [k,v] of Object.entries(data.payload)) {
            if (k == 'ap_no_def_gw')
                document.getElementById('stg-ap_no_def_gw').checked = v;
            else
                document.getElementById('stg-' + k).value = v;
        }
    }
  }

  if (data.event) {
    switch(data.event) {
        /* update battery state event.
        Pars:
            - float voltage (can be null if sensor desnt't work)
            - float current (can be null if sensor desnt't work)
            - float SoC
            - float temperature (can be null if sensor desnt't work)
            - bool battery_sensor_ok (false when INA226 not responding)
        */
        case 'battery_update':
            for (const el of ['voltage', 'current', 'soc', 'temperature']) {
                if (data[el] !== null)
                  document.getElementById('batt-' + el).innerText = data[el];
            }
            var battery_icon = 'empty';
            if (data.soc > 95)
              battery_icon = 'full';
            else if (data.soc > 60) 
              battery_icon = 'three-quarters';
            else if (data.soc > 49)
              battery_icon = 'half';
            else if (data.soc > 25)
              battery_icon = 'quarter';
            
            // dynamic icon and favicon
            document.querySelector("#icon-battery use").setAttribute("href", `#i-battery-${battery_icon}`);
            document.querySelector("link[rel*='icon']").href = `battery-${battery_icon}.svg`;

            // alarms
            let alarm = false;
            let alarmText = '';

            setControlAlarm(['icon-battery', 'batt-soc'], (data.soc <= 15));
            if (data.soc <= 15) {
              alarm = true;
              alarmText = 'Battery is low! ';
            }

            const temperature_valid = (data.temperature !== null);
            const battery_valid = (data.battery_sensor_ok && (data.voltage !== null));
            
            setControlAlarm(['batt-voltage', 'batt-current'], ! battery_valid);
            setControlAlarm('batt-temperature', ! temperature_valid);

            if (! battery_valid) {
              alarm = true;
              alarmText += "Battery voltage/current sensor is not working. ";
            }

            if (! temperature_valid) {
              alarm = true;
              alarmText += "Battery temperature sensor is not working. ";
            }

            const alarm_box = document.getElementById('batt-alarm');
            alarm_box.title = alarmText;
            if (alarm) 
              alarm_box.classList.remove('hidden'); 
            else 
              alarm_box.classList.add('hidden');
            break;

        /* update battery state event.
        Pars:
            - float hours
        */
       case 'battery_autonomy_update':
            const h = Math.trunc(data.hours);
            const m = Math.trunc((data.hours - h) * 60.0);
            document.getElementById('batt-autonomy').innerText = `${String(h).padStart(2, '0')}h ${String(m).padStart(2, '0')}m`;
            break;

        /* update safety state event.
        Pars:
            - bool is_safe
        */
        case 'safety_update':
            //TODO
            break;

        /* update cold protection state event.
        Pars:
            - float lt low threshold temperature 
        */
        case 'cold_protection_update':
            setSlider('batt-lt', data.lt);
            break

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
            break

        /* update power outlets state event.
        Pars:
            - int index
            - float power 
        */
        case 'outlet_update':
            var v = parseFloat(data.power) * 100.0;
            setSlider('outlet' + data.index, v.toFixed(0));
            break            
    }
  }
}

window.handleIncomingData = handleIncomingData;

function sendWsMessage(obj) {
  if (isLocalTest) { console.log("➡️ [WS SIMULATO] Invio:", obj); }
  else if (ws && ws.readyState === WebSocket.OPEN) { 
    //console.log("➡️ ", obj);
    ws.send(JSON.stringify(obj)); 
  }
}

initWebSocket();

// --- 5. GESTIONE CAMBIO TEMA (CHIARO / SCURO) ---
const btnTheme = document.getElementById('btn-theme');

// Controlla se l'utente aveva già salvato una preferenza, altrimenti usa il tema chiaro
const currentTheme = localStorage.getItem('theme') || 'light';

if (currentTheme === 'dark') {
  document.body.classList.add('dark');
  btnTheme.title = "Switch to light theme"
} else {
  btnTheme.title = "Switch to light theme"
}

btnTheme.addEventListener('click', () => {
  // Cambia la classe sul body
  document.body.classList.toggle('dark');
  
  // Determina il tema corrente e aggiorna localStorage e icona
  if (document.body.classList.contains('dark')) {
    localStorage.setItem('theme', 'dark');
    btnTheme.title = "Switch to light theme"

  } else {
    localStorage.setItem('theme', 'light');
    btnTheme.title = "Switch to dark theme"
  }
});

// --- 6. CONFIRM MODAL (sostituisce i confirm() nativi del browser) ---
// Un solo modal condiviso: ogni chiamata a showConfirm() sovrascrive i
// listener di ok/cancel invece di accumularli (niente doppie conferme se
// showConfirm viene richiamata più volte prima che l'utente risponda).
const confirmModal = document.getElementById('confirm-modal');
const confirmMessage = document.getElementById('confirm-message');
const confirmOkBtn = document.getElementById('confirm-ok');
const confirmCancelBtn = document.getElementById('confirm-cancel');

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

// Upload reale via XMLHttpRequest: serve xhr.upload.onprogress per il
// progress reale del caricamento, cosa che fetch() non offre in modo
// altrettanto diretto.
function uploadOtaFile(file, fieldName, title) {
  showProgressModal(title);

  if (isLocalTest) {
    // Simulazione locale, cosi' il modal si puo' provare senza hardware.
    let pct = 0;
    const interval = setInterval(() => {
      pct += 8 + Math.random() * 12;
      if (pct >= 100) {
        pct = 100;
        clearInterval(interval);
        setProgressDone('Upload done (simulation). Restart in progress...');
      } else {
        const p = Math.round(pct);
        setProgress(p, 'Upload in progress (simulation)...');
      }
    }, 250);
    return;
  }

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
    if (xhr.status === 200) {
      setProgressDone('Upload done. Device is restarting...');
      // Il device riavvia e riconnette WiFi/mDNS: attendiamo prima di
      // ricaricare la SPA, coerente col refresh lato server dopo un OTA.
      setTimeout(() => { window.location.href = '/'; }, 15000);
    } else {
      setProgressError(`Error (${xhr.status}): ${xhr.responseText || 'update failed'}`);
    }
  };

  xhr.onerror = () => {
    // Puo' capitare anche a upload riuscito, se il device si riavvia prima
    // di chiudere la risposta HTTP: non e' necessariamente un fallimento.
    setProgressError('Network error during upload (device could be already rebooting...)');
  };

  xhr.open('POST', '/update');
  xhr.send(formData);
}

function wireOtaForm(formId, inputId, fieldName, title) {
  const form = document.getElementById(formId);
  const input = document.getElementById(inputId);
  if (!form || !input) return;

  form.addEventListener('submit', (e) => {
    e.preventDefault();

    if (!input.files || input.files.length === 0) {
      alert('Select a file first');
      return;
    }

    showConfirm(
      `Are you sure you want to update the ${title.toLowerCase()}? The device will reboot.`,
      () => uploadOtaFile(input.files[0], fieldName, title)
    );
  });
}

wireOtaForm('form-ota-firmware', 'input-ota-firmware', 'firmware', 'Firmware update');
wireOtaForm('form-ota-filesystem', 'input-ota-filesystem', 'filesystem', 'Filesystem update');

// --- 8. Controls listeners
linkSliderLabel('batt-lt').addEventListener('change', (e) => {
  sendWsMessage({ action: 'cp_low_threshold', temperature: parseInt(e.target.value) });
});

linkSliderLabel('light-brightness').addEventListener('change', (e) => {
  sendWsMessage({ action: 'light_brightness', brightness: (parseFloat(e.target.value) / 100.0)});
});

linkSwitchLabel('light-auto').addEventListener('change', e => {
  sendWsMessage({ action: 'light_auto_enabled', enabled: e.target.checked });            
});

linkSliderLabel('light-auto_br').addEventListener('change', (e) => {
  sendWsMessage({ action: 'light_auto_brightness', brightness: (parseFloat(e.target.value) / 100.0) });
});

linkSliderLabel('light-auto_dr').addEventListener('change', (e) => {
  sendWsMessage({ action: 'light_auto_duration', seconds: parseInt(e.target.value) });
});

document.getElementById('btn-reset-soc').addEventListener('click', e => {
  showConfirm("Are you sure you want to reset battery charge to 100%?", () => {
    sendWsMessage({ action: 'battery_soc_reset' });
  });
});

document.getElementById('btn-restart').addEventListener('click', e => {
  showConfirm("Are you sure you want to restart the unit?", () => {
    sendWsMessage({ action: 'restart' });
  });
});

document.getElementById('btn-factory-reset').addEventListener('click', e => {
  // Invia i dati tramite l'unica connessione WebSocket attiva
  showConfirm(
    "Are you sure you want to factory reset the unit?",
    () => { 
      showConfirm("Are you REALLY shure you want to factory reset the unit? You'll loose all Wi-Fi settings.",
        () => {
          const data = {
            action: "update_settings",
            payload: { factory_reset: true, factory_reset_confirm: "CONFIRM FACTORY RESET" }
          };
          sendWsMessage(data);
        }
      );
     }
  );
});
