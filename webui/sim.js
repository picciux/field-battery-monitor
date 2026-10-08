// Simulatore del firmware per lo sviluppo della web UI (solo DEV, escluso dalla build).
//
// Criterio: produrre i MESSAGGI che il firmware può produrre (websocket_proto.h),
// non riprodurne il comportamento interno. Un unico oggetto di stato `s` da cui
// derivare tutti gli eventi, così gli scenari sono coerenti (SoC che integra la
// corrente, safety con le stesse isteresi del firmware, valori congelati a
// sensore guasto).
//
// Console (window.sim):
//   sim.scenarios()            elenco scenari
//   sim.scenario('lowBattery') applica uno scenario
//   sim.speed(n)               accelera l'integrazione del SoC (1 = tempo reale)
//   sim.reject('outlet_power', 'power')  rifiuta un'azione con quel detail
//   sim.reject('outlet_power')           rimuove il rifiuto
//   sim.setIsSafe(false)       forza la safety; sim.setIsSafe(null) torna automatica
//   sim.reboot()               simula un riavvio (capabilities + stato iniziale)
//   sim.getState()             stato interno, per ispezione

// ---- Variante hardware simulata (come in capabilities). Il cambio richiede reload. ----
const VARIANT = { channels: 4, light: true, outlets: 2 };

const CAPACITY_AH = 50;
const FW_VER = '1.0-sim';
const LIMITS = { cpMin: -10, cpMax: 10, brMinPct: 1, drMin: 10, drMax: 60 };
const SAFETY = { socLow: 15, socRecover: 20, vLow: 11.0, vHyst: 0.3 };
const AUTONOMY_CAP_H = 24.0;
const MIN_DISCHARGE_A = 0.05;
const SOC_DEADBAND_A = 0.02;
const BOOT_KEY = 'sim_boot_count';

const FACTORY = {
  display_name: 'Simulator', hostname: 'simulator',
  main_ssid: 'Sim Main', alt_ssid: 'Sim Alt', ap_no_def_gw: true,
};

// ---------------------------------------------------------------------------
// Stato
// ---------------------------------------------------------------------------
const s = {
  // batteria
  soc: 95, voltage: 13.2, current: -0.8, temp: 27,
  baseCurrent: -0.8,      // corrente "di scenario" attorno a cui oscilla quella misurata
  voltOffset: 0,          // offset di scenario sulla tensione
  avgCurrent: -0.8,       // approssima la media a 10 minuti del firmware
  autonomy: AUTONOMY_CAP_H,
  sensorOk: true,         // INA226
  tempOk: true,           // DS18B20
  // safety
  isSafe: true, socUnsafe: false, voltUnsafe: false, safetyForced: null,
  // heater / luce / prese
  cpLt: 5.0,
  light: { brightness: 0, auto: true, auto_br: 0.25, auto_dr: 30 },
  outlets: [0, 0, 0],
  // diagnostica
  bootCount: 1, resetReason: 'poweron', bootTime: Date.now(),
  // settings
  settings: { ...FACTORY },
};

let emit = () => {};
let updates = true;
let timeScale = 1;
let actionResult = true;            // false: ogni azione rifiutata ('simulated_reject')
const rejections = new Map();       // azione -> detail

const clamp = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);
const round = (v, d) => Number(v.toFixed(d));

// ---------------------------------------------------------------------------
// Costruzione messaggi (stessi formati di wifi_comm.cpp / websocket_proto.h)
// ---------------------------------------------------------------------------
function caps() {
  return {
    type: 'capabilities',
    payload: {
      channels: VARIANT.channels,
      light: VARIANT.light,
      outlets: VARIANT.outlets,
      light_auto_br_min_pct: LIMITS.brMinPct,
      light_auto_dr_min: LIMITS.drMin,
      light_auto_dr_max: LIMITS.drMax,
      cp_lt_min: LIMITS.cpMin,
      cp_lt_max: LIMITS.cpMax,
      fw_ver: FW_VER,
      reset_reason: s.resetReason,
      boot_count: s.bootCount,
      uptime_s: Math.floor((Date.now() - s.bootTime) / 1000),
    },
  };
}

const batteryEvent = () => ({
  event: 'battery_update',
  voltage: s.sensorOk ? round(s.voltage, 2) : null,
  current: s.sensorOk ? round(s.current, 3) : null,
  soc: round(s.soc, 1),
  temperature: s.tempOk ? round(s.temp, 1) : null,
  battery_sensor_ok: s.sensorOk,
});

const autonomyEvent = () => ({ event: 'battery_autonomy_update', hours: round(s.autonomy, 1) });
const safetyEvent = () => ({ event: 'safety_update', is_safe: s.isSafe });
const cpEvent = () => ({ event: 'cold_protection_update', lt: s.cpLt });
const lightEvent = () => ({ event: 'light_update', ...s.light });
const outletEvent = (i) => ({ event: 'outlet_update', index: i, power: round(s.outlets[i], 2) });

function settingsMsg() {
  return { type: 'settings', payload: { ...s.settings, version: FW_VER } };
}

// Replica sendInitialState() del firmware.
function emitAll() {
  emit(batteryEvent());
  emit(autonomyEvent());
  emit(safetyEvent());
  emit(cpEvent());
  if (VARIANT.light) emit(lightEvent());
  for (let i = 0; i < VARIANT.outlets; i++) emit(outletEvent(i));
}

// ---------------------------------------------------------------------------
// Modello minimo della batteria
// ---------------------------------------------------------------------------
function targetVoltage() {
  let v = 11.8 + (s.soc / 100) * 1.4;      // 11.8 V vuota .. 13.2 V piena
  if (s.baseCurrent > 0.5) v += 0.9;       // in carica
  return clamp(v + s.voltOffset, 0, 20);
}

function setSafe(v) {
  if (v === s.isSafe) return;
  s.isSafe = v;
  if (updates) emit(safetyEvent());
}

// Stesse isteresi di Battery::updateSafety().
function updateSafety() {
  if (s.safetyForced !== null) return;
  if (s.soc <= SAFETY.socLow) s.socUnsafe = true;
  else if (s.soc >= SAFETY.socRecover) s.socUnsafe = false;

  if (s.voltage <= SAFETY.vLow) s.voltUnsafe = true;
  else if (s.voltage >= SAFETY.vLow + SAFETY.vHyst) s.voltUnsafe = false;

  setSafe(!(s.socUnsafe || s.voltUnsafe));
}

function computeAutonomy() {
  const discharge = -s.avgCurrent;
  if (discharge < MIN_DISCHARGE_A) return AUTONOMY_CAP_H;
  const remainingAh = CAPACITY_AH * s.soc / 100;
  return Math.min(AUTONOMY_CAP_H, remainingAh / discharge);
}

function autonomyTick() {
  if (!s.sensorOk) return;                 // il firmware non ricalcola a sensore guasto
  s.autonomy = computeAutonomy();
  if (updates) emit(autonomyEvent());
}

function tick() {
  const dtH = (1 / 3600) * timeScale;

  // La temperatura varia anche se il sensore è "guasto": è solo la lettura che manca.
  s.temp = clamp(s.temp + (Math.random() - 0.5) * 0.2, 5, 40);

  if (s.sensorOk) {
    s.current = s.baseCurrent + (Math.random() - 0.5) * 0.1;
    s.voltage = targetVoltage() + (Math.random() - 0.5) * 0.04;
    if (Math.abs(s.current) > SOC_DEADBAND_A)
      s.soc = clamp(s.soc + (s.current / CAPACITY_AH) * 100 * dtH, 0, 100);
    s.avgCurrent += (s.current - s.avgCurrent) * 0.05;
    updateSafety();
  }
  // Sensore guasto: tensione, corrente e SoC restano congelati, come nel firmware.

  // Emesso ogni secondo anche a sensore guasto.
  if (updates) emit(batteryEvent());
}

// ---------------------------------------------------------------------------
// Scenari
// ---------------------------------------------------------------------------
function baseReset() {
  s.sensorOk = true;
  s.tempOk = true;
  s.baseCurrent = -0.8;
  s.voltOffset = 0;
  s.safetyForced = null;
  timeScale = 1;
}

const SCENARIOS = {
  // tutto ok, batteria quasi piena, scarica leggera
  normal()      { baseReset(); s.soc = 90; },
  // INA226 non risponde: null su V/I, battery_sensor_ok:false, SoC congelato
  sensorFault() { baseReset(); s.sensorOk = false; },
  // DS18B20 non risponde: temperature null
  tempFault()   { baseReset(); s.tempOk = false; },
  allFaults()   { baseReset(); s.sensorOk = false; s.tempOk = false; },
  // scarica forte accelerata: il SoC scende sotto il 15% in pochi secondi
  lowBattery()  { baseReset(); s.soc = 22; s.baseCurrent = -6; timeScale = 600; },
  // tensione critica indipendentemente dal SoC (soglia 11.0 V, rientro 11.3 V)
  lowVoltage()  { baseReset(); s.soc = 90; s.voltOffset = -2.2; },
  // carica: corrente positiva, autonomia al tetto ("> 24h")
  charging()    { baseReset(); s.soc = 60; s.baseCurrent = 8; timeScale = 120; },
};

export function scenarios() { return Object.keys(SCENARIOS); }

export function scenario(name) {
  const fn = SCENARIOS[name];
  if (!fn) {
    console.error(`[sim] scenario sconosciuto '${name}'. Disponibili: ${scenarios().join(', ')}`);
    return;
  }
  fn();
  // Il firmware non cambia istantaneamente: qui si porta lo stato al nuovo regime
  // subito, per vedere l'effetto senza attendere.
  s.current = s.baseCurrent;
  s.avgCurrent = s.baseCurrent;
  if (s.sensorOk) s.voltage = targetVoltage();
  updateSafety();
  autonomyTick();
  if (updates) emit(batteryEvent());
  console.log(`[sim] scenario '${name}'`);
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
// Replica WifiComm::updateSettings(): salvataggio parziale, detail con i campi scartati.
function updateSettings(p) {
  if (!p || typeof p !== 'object' || Array.isArray(p))
    return { ok: false, detail: 'no_payload' };

  if (p.factory_reset === true) {
    if (p.factory_reset_confirm !== 'CONFIRM FACTORY RESET')
      return { ok: false, detail: 'factory_reset_confirm' };
    s.settings = { ...FACTORY };
    emit(settingsMsg());
    scheduleReboot();
    return { ok: true, detail: '' };
  }

  const rejected = [];
  const str = (key, min, max, emptyMeansUnchanged, apply) => {
    if (!(key in p) || p[key] === null) return;               // assente: non toccato
    const v = p[key];
    if (typeof v !== 'string') { rejected.push(key); return; }
    if (v.length === 0 && emptyMeansUnchanged) return;
    if (v.length < min || v.length > max) { rejected.push(key); return; }
    apply(v);
  };

  str('hostname', 1, 30, false, v => {
    if (/^[a-z0-9]([a-z0-9-]*[a-z0-9])?$/.test(v)) s.settings.hostname = v;
    else rejected.push('hostname');
  });
  str('display_name', 1, 30, false, v => { s.settings.display_name = v; });
  str('main_ssid', 1, 32, false, v => { s.settings.main_ssid = v; });
  str('alt_ssid', 0, 32, false, v => { s.settings.alt_ssid = v; });
  // PSK: validate soltanto, non vengono mai memorizzate né restituite
  str('ap_psk', 8, 63, true, () => {});
  str('main_psk', 8, 63, true, () => {});
  str('alt_psk', 8, 63, true, () => {});

  if (p.ap_no_def_gw !== undefined && p.ap_no_def_gw !== null) {
    if (typeof p.ap_no_def_gw === 'boolean' || typeof p.ap_no_def_gw === 'number')
      s.settings.ap_no_def_gw = !!p.ap_no_def_gw;
    else rejected.push('ap_no_def_gw');
  }

  emit(settingsMsg());   // il firmware risincronizza sempre la UI
  return { ok: rejected.length === 0, detail: rejected.join(',') };
}

// ---------------------------------------------------------------------------
// Azioni (client -> device)
// ---------------------------------------------------------------------------
const isNum = v => typeof v === 'number' && Number.isFinite(v);
const isInt = v => Number.isInteger(v);

function rejectionFor(action) {
  if (rejections.has(action)) return rejections.get(action);
  if (!actionResult) return 'simulated_reject';
  return null;
}

function scheduleReboot() {
  console.log('[sim] Riavvio tra 5 secondi.');
  setTimeout(() => reboot(), 5000);
}

function execute(obj) {
  const a = obj.action;
  const needLight = () => (VARIANT.light ? null : 'no_light');

  switch (a) {
    case 'battery_soc_reset':
      s.soc = 100;
      emit(batteryEvent());
      return { ok: true };

    case 'cp_low_threshold': {
      if (!isNum(obj.temperature)) return { ok: false, detail: 'temperature' };
      const v = clamp(obj.temperature, LIMITS.cpMin, LIMITS.cpMax);
      if (v !== s.cpLt) { s.cpLt = v; emit(cpEvent()); }
      return { ok: true };
    }

    case 'light_brightness': {
      const nl = needLight(); if (nl) return { ok: false, detail: nl };
      if (!isNum(obj.brightness)) return { ok: false, detail: 'brightness' };
      const v = clamp(obj.brightness, 0, 1);
      if (v !== s.light.brightness) { s.light.brightness = v; emit(lightEvent()); }
      return { ok: true };
    }

    case 'light_auto_enabled': {
      const nl = needLight(); if (nl) return { ok: false, detail: nl };
      if (typeof obj.enabled !== 'boolean') return { ok: false, detail: 'enabled' };
      if (obj.enabled !== s.light.auto) { s.light.auto = obj.enabled; emit(lightEvent()); }
      return { ok: true };
    }

    case 'light_auto_brightness': {
      const nl = needLight(); if (nl) return { ok: false, detail: nl };
      if (!isNum(obj.brightness)) return { ok: false, detail: 'brightness' };
      const v = clamp(obj.brightness, LIMITS.brMinPct / 100, 1);
      if (v !== s.light.auto_br) { s.light.auto_br = v; emit(lightEvent()); }
      return { ok: true };
    }

    case 'light_auto_duration': {
      const nl = needLight(); if (nl) return { ok: false, detail: nl };
      if (!isInt(obj.seconds)) return { ok: false, detail: 'seconds' };
      const v = clamp(obj.seconds, LIMITS.drMin, LIMITS.drMax);
      if (v !== s.light.auto_dr) { s.light.auto_dr = v; emit(lightEvent()); }
      return { ok: true };
    }

    case 'outlet_power': {
      if (!isInt(obj.index)) return { ok: false, detail: 'index' };
      if (obj.index < 0 || obj.index >= VARIANT.outlets) return { ok: false, detail: 'bad_index' };
      if (!isNum(obj.power)) return { ok: false, detail: 'power' };
      const v = Math.round(clamp(obj.power, 0, 1) * 255) / 255;   // quantizzazione PWM 8 bit
      if (v !== s.outlets[obj.index]) { s.outlets[obj.index] = v; emit(outletEvent(obj.index)); }
      return { ok: true };
    }

    case 'wifi_rescan':
      return { ok: true };   // per i rifiuti: sim.reject('wifi_rescan', 'already_on_main' | 'busy' | 'ota_running')

    case 'restart':
      scheduleReboot();
      return { ok: true };

    case 'get_settings':
      emit(settingsMsg());
      return { ok: true };

    case 'update_settings':
      return updateSettings(obj.payload);

    default:
      return { ok: false, detail: 'unknown_action' };
  }
}

export function send(obj, handle) {
  if (handle) emit = handle;

  const rej = rejectionFor(obj.action);
  const r = rej ? { ok: false, detail: rej } : execute(obj);

  const res = { type: 'result', payload: r.ok };
  if (obj.id) res.id = obj.id;
  if (r.detail) res.detail = r.detail;

  // Come il firmware: ogni azione riceve un result. Senza id lo si notifica via
  // handle(); con id il chiamante usa il valore di ritorno (vedi request() in main.js).
  if (!obj.id) emit(res);
  return res;
}

// ---------------------------------------------------------------------------
// Upload OTA finto: chiama i callback che gli passi
// ---------------------------------------------------------------------------
export function upload(cb) {
  let pct = 0;
  const t = setInterval(() => {
    pct += 8 + Math.random() * 12;
    if (pct >= 100) {
      clearInterval(t);
      cb.done();
      setTimeout(() => reboot(cb.handleData), 5000);
    } else {
      cb.progress(Math.round(pct));
    }
  }, 250);
}

// ---------------------------------------------------------------------------
// Boot / riavvio
// ---------------------------------------------------------------------------
function loadBootCount() {
  try {
    const v = parseInt(localStorage.getItem(BOOT_KEY), 10);
    if (Number.isFinite(v) && v >= 1) s.bootCount = v;
  } catch (_) {}
}

function saveBootCount() {
  try { localStorage.setItem(BOOT_KEY, String(s.bootCount)); } catch (_) {}
}

// Il boot_count sopravvive al reload della pagina (come la NVS del firmware):
// la UI lo usa per riconoscere che il riavvio è avvenuto.
export function reboot(handle) {
  if (handle) emit = handle;
  s.bootCount++;
  s.resetReason = 'sw';
  s.bootTime = Date.now();
  saveBootCount();
  emit(caps());
  emitAll();
}

export function start(handle, setConnected) {
  emit = handle;
  setConnected(true, 'Connected (local simulation)');
  loadBootCount();

  // Come alla connessione di un client: capabilities, poi lo stato completo.
  setTimeout(() => {
    emit(caps());
    emitAll();
  }, 500);

  setInterval(tick, 1000);              // battery_update ogni secondo, come il firmware
  setInterval(autonomyTick, 30000);     // autonomia ogni 30 s
}

// ---------------------------------------------------------------------------
// Controlli da console
// ---------------------------------------------------------------------------
export function enableUpdates()  { updates = true; }
export function disableUpdates() { updates = false; }   // simula il silenzio del device

export function setTemperatureValid(v) { s.tempOk = !!v; }
export function setBatteryValid(v)     { s.sensorOk = !!v; }

// Forza la safety (true/false); null torna al calcolo automatico.
export function setIsSafe(v, handle) {
  if (handle) emit = handle;
  if (v === null) { s.safetyForced = null; updateSafety(); return; }
  s.safetyForced = !!v;
  setSafe(!!v);
}

// false: ogni azione viene rifiutata con 'simulated_reject'.
export function setActionResult(v) { actionResult = !!v; }

// Rifiuto per singola azione con detail a scelta; senza detail rimuove il rifiuto.
export function reject(action, detail) {
  if (action) {
    if (detail) rejections.set(action, detail);
    else rejections.delete(action);
  } else {
    console.log('-------- ACTIVE REJECTIONS -------');
    rejections.forEach((value, key) => console.log(` - ${key} => ${value}` ))
    console.log('---- END OF ACTIVE REJECTIONS ----');
  }
}

// Moltiplicatore dell'integrazione del SoC (1 = tempo reale).
export function speed(n) { timeScale = Math.max(0, Number(n) || 1); }

export function getState() { return s; }