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
  }
}

btnHome.addEventListener('click', () => switchPage('home'));
btnSettings.addEventListener('click', () => switchPage('settings'));


// --- 2. GESTIONE DINAMICA DEI CONTROLLI (Luci, Prese, Switch) ---
const containerLights = document.getElementById('light-container');
const containerOutlets = document.getElementById('outlets-container');
const generatedControls = new Set();

// Funzione per creare uno Slider 
function createDynamicSlider(container, type, idNumber, labelName, initialValue, minValue=0, maxValue=100) {
  const key = `${type}${idNumber}`;
  if (generatedControls.has(key)) return;
  
  if (generatedControls.size === 0 || container.querySelector('.loading-text')) container.innerHTML = '';
  generatedControls.add(key);

  const controlGroup = document.createElement('div');
  controlGroup.className = 'control-group';
  controlGroup.innerHTML = `
    <label>${labelName} ${idNumber}: <span id="val-${key}">${initialValue}</span>%</label>
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

// --- 3. LOGICA WEBSOCKET & SIMULATORE AGGIORNATO ---
const isLocalTest = window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1';
const wsStatus = document.getElementById('ws-status');
let ws;

function initWebSocket() {
  if (isLocalTest) {
    console.log("🛠️ Esecuzione in locale: Simulazione WebSocket attiva.");
    wsStatus.innerText = "WebSocket: Connesso (Simulazione Locale)";
    wsStatus.className = "status-bar online";
    
    // PRIMO MESSAGGIO SIMULATO: Configura la UI a run-time con un mix di Slider e Switch Booleani
    /*setTimeout(() => {
      handleIncomingData({
        light1: 30, light2: 65,
        light3: true,  // <-- Booleano: Diventerà uno Switch ON/OFF automaticamente!
        light4: false, // <-- Booleano: Diventerà uno Switch ON/OFF automaticamente!
        outlet1: 0, outlet2: 100,
        outlet3: false // <-- Anche le prese possono essere switch fisici puri
      });
    }, 500);*/

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
    return;
  }

  // AMBIENTE REALE (ESP32)
  ws = new WebSocket(`ws://${window.location.hostname}:81`);
  ws.onopen = () => { wsStatus.innerText = "Connesso"; wsStatus.className = "status-bar online"; };
  ws.onclose = () => { wsStatus.innerText = "Disconnesso..."; wsStatus.className = "status-bar offline"; setTimeout(initWebSocket, 2000); };
  ws.onmessage = (event) => { handleIncomingData(JSON.parse(event.data)); };
}

// Funzione centrale per applicare i dati o discriminare il tipo di controllo
function handleIncomingData(data) {
  console.log(data);
  if (data.type) {
    if (type == 'capabilities') {
        //const channels = data.payload.channels;
        const outlets = data.payload.outlets;
        if (data.payload.light)
            containerLights.classList.remove('hidden');

        if (data.payload.outlets == 0)
            containerOutlets.classList.add('hidden');
        else {
            for (var i = 0; i < outlets; i++) 
                createDynamicSlider(containerOutlets, 'outlet', i, 'Outlet ' + (i+1), 0);
        }
    }
  }

  if (data.event) {
    switch(data.event) {
        case 'battery_update':
            for (const el of ['voltage', 'current', 'soc', 'temperature']) {
                document.getElementById('batt-' + el).innerText = data[el];
            }
            document.getElementById('batt-sensors').innerText = (data.battery_sensor_ok ? 'OK' : 'FAIL' );
        break;

        case 'battery-autonomy-update':
            document.getElementById('batt-autonomy').innerText = data.hours;
        break;
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


