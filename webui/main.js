
const VERSION = '0.9';

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

// Funzione per creare uno Slider 
function createDynamicSlider(container, type, idNumber, labelName, initialValue, minValue=0, maxValue=100) {
  const key = `${type}${idNumber}`;
  if (generatedControls.has(key)) return;
  
  //if (generatedControls.size === 0 || container.querySelector('.loading-text')) container.innerHTML = '';
  generatedControls.add(key);

  const controlGroup = document.createElement('div');
  controlGroup.className = 'control-group';
  controlGroup.innerHTML = `
    <label>${labelName}: <span id="val-${key}">${initialValue}</span>%</label>
    <input type="range" id="slider-${key}" min="${minValue}" max="${maxValue}" value="${initialValue}">
  `;
  container.appendChild(controlGroup);

  const slider = controlGroup.querySelector('input[type="range"]');
  slider.addEventListener('input', (e) => {
    const txtSpan = document.getElementById(`val-${key}`);
    if (txtSpan) txtSpan.innerText = e.target.value;
    sendWsMessage({ action: `set_${key}`, value: parseInt(e.target.value) });
  });
}

// Funzione per creare uno Switch ON/OFF (Valori Booleani true/false)
function createDynamicSwitch(container, type, idNumber, labelName, initialValue) {
  const key = `${type}${idNumber}`;
  if (generatedControls.has(key)) return;

  if (generatedControls.size === 0 || container.querySelector('.loading-text')) container.innerHTML = '';
  generatedControls.add(key);

  const switchGroup = document.createElement('div');
  switchGroup.className = 'switch-container';
  switchGroup.innerHTML = `
    <label>${labelName} ${idNumber}: <span id="txt-${key}">${initialValue ? 'ON' : 'OFF'}</span></label>
    <label class="switch">
      <input type="checkbox" id="switch-${key}" ${initialValue ? 'checked' : ''}>
      <span class="slider-toggle"></span>
    </label>
  `;
  container.appendChild(switchGroup);

  const toggle = switchGroup.querySelector('input[type="checkbox"]');
  toggle.addEventListener('change', (e) => {
    const txtSpan = document.getElementById(`txt-${key}`);
    if (txtSpan) txtSpan.innerText = e.target.checked ? 'ON' : 'OFF';
    sendWsMessage({ action: `set_${key}`, value: e.target.checked });
  });
}

function setSlider(field, value) {
    document.getElementById('val-' + field).innerText = value;
    document.getElementById('slider-' + field).value = value;
}

function setSwitch(field, value) {
    document.getElementById('txt-' + field).innerText = ( value ? 'ON' : 'OFF' );
    document.getElementById('switch-' + field).checked = value;
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
        soc: (100 - Math.random() * 3.5).toFixed(0),
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
  console.log(data);
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
        const outlets = data.payload.outlets;
        if (data.payload.light)
            containerLights.classList.remove('hidden');

        if (outlets == 0)
            containerOutlets.classList.add('hidden');
        else {
            for (var i = 0; i < outlets; i++) 
                createDynamicSlider(containerOutlets, 'outlet', i, 'Outlet ' + (i+1), 0);
        }

        if (data.payload.cp_lt_max) {
            document.getElementById('slider-batt-lt').min = data.payload.cp_lt_min;
            document.getElementById('slider-batt-lr').max = data.payload.cp_lt_max;
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
                document.getElementById('batt-' + el).innerText = data[el];
            }
            document.getElementById('batt-sensors').innerText = (data.battery_sensor_ok ? 'OK' : 'FAIL' );
            break;

        /* update battery state event.
        Pars:
            - float hours
        */
       case 'battery_autonomy_update':
            document.getElementById('batt-autonomy').innerText = data.hours;
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

function sendWsMessage(obj) {
  if (isLocalTest) { console.log("➡️ [WS SIMULATO] Invio:", obj); }
  else if (ws && ws.readyState === WebSocket.OPEN) { ws.send(JSON.stringify(obj)); }
}

initWebSocket();

// --- 4. GESTIONE CAMBIO TEMA (CHIARO / SCURO) ---
const btnTheme = document.getElementById('btn-theme');

// Controlla se l'utente aveva già salvato una preferenza, altrimenti usa il tema chiaro
const currentTheme = localStorage.getItem('theme') || 'light';

if (currentTheme === 'dark') {
  document.body.classList.add('dark');
  btnTheme.innerText = '☀️';
} else {
  btnTheme.innerText = '🌙';
}

btnTheme.addEventListener('click', () => {
  // Cambia la classe sul body
  document.body.classList.toggle('dark');
  
  // Determina il tema corrente e aggiorna localStorage e icona
  if (document.body.classList.contains('dark')) {
    localStorage.setItem('theme', 'dark');
    btnTheme.innerText = '☀️';
  } else {
    localStorage.setItem('theme', 'light');
    btnTheme.innerText = '🌙';
  }
});


