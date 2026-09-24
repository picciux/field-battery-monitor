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


// --- 2. AGGIORNAMENTO GRAFICO SLIDER IN TEMPO REALE ---
// Trova tutti gli input range e aggiorna il testo a fianco quando l'utente li muove
document.querySelectorAll('input[type="range"]').forEach(slider => {
  slider.addEventListener('input', (e) => {
    const id = e.target.id.replace('slider-', 'val-');
    const txtSpan = document.getElementById(id);
    if (txtSpan) txtSpan.innerText = e.target.value;
    
    // Invia il valore via WebSocket
    sendWsMessage({ action: 'set_' + e.target.id.replace('slider-', ''), value: parseInt(e.target.value) });
  });
});


// --- 3. LOGICA WEBSOCKET & SIMULATORE PER TESTING LOCALE ---
const isLocalTest = window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1';
const wsStatus = document.getElementById('ws-status');
let ws;

function initWebSocket() {
  if (isLocalTest) {
    // AMBIENTE DI TEST LOCALE (PC)
    console.log("🛠️ Esecuzione in locale: Simulazione WebSocket attiva.");
    wsStatus.innerText = "WebSocket: Connesso (Simulazione Locale)";
    wsStatus.className = "status-bar online";
    
    // Simula la ricezione di dati periodici dai sensori dell'ESP32 ogni 2 secondi
    setInterval(() => {
      handleIncomingData({
        temperature: (25 + Math.random() * 5).toFixed(1),
        s1: (3.7 + Math.random() * 0.4).toFixed(2),
        s2: (3.8 + Math.random() * 0.3).toFixed(2),
        s3: (3.6 + Math.random() * 0.5).toFixed(2),
        s4: (3.9 + Math.random() * 0.2).toFixed(2),
      });
    }, 2000);
    return;
  }

  // AMBIENTE REALE (SULL'ESP32)
  ws = new WebSocket(`ws://${window.location.hostname}/ws`);

  ws.onopen = () => {
    wsStatus.innerText = "WebSocket: Connesso";
    wsStatus.className = "status-bar online";
  };

  ws.onclose = () => {
    wsStatus.innerText = "WebSocket: Disconnesso. Riconnessione...";
    wsStatus.className = "status-bar offline";
    setTimeout(initWebSocket, 2000); // Tenta di riconnettersi
  };

  ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    handleIncomingData(data);
  };
}

// Funzione centrale per applicare i dati ricevuti alla UI
function handleIncomingData(data) {
  if (data.temperature) document.getElementById('batt-temp').innerText = data.temperature;
  if (data.s1) document.getElementById('batt-s1').innerText = data.s1;
  if (data.s2) document.getElementById('batt-s2').innerText = data.s2;
  if (data.s3) document.getElementById('batt-s3').innerText = data.s3;
  if (data.s4) document.getElementById('batt-s4').innerText = data.s4;
  
  // Aggiorna gli slider se l'evento arriva da fuori (es. cambio da altro smartphone)
  ['light1', 'light2', 'light3', 'light4', 'outlet1', 'outlet2', 'outlet3'].forEach(key => {
    if (data[key] !== undefined) {
      const slider = document.getElementById(`slider-${key}`);
      const txt = document.getElementById(`val-${key}`);
      if (slider) slider.value = data[key];
      if (txt) txt.innerText = data[key];
    }
  });
}

// Funzione per inviare i messaggi
function sendWsMessage(obj) {
  if (isLocalTest) {
    console.log("➡️ [WS SIMULATO] Invio:", obj);
  } else if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(obj));
  }
}

// Avvia il WebSocket al caricamento
initWebSocket();
