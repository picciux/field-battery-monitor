

const capabilities = { type: 'capabilities', payload: { channels: 4, light: true, outlets: 2, fw_ver: '1.0-sim', boot_count: 1 } };

let updates = true;
let temperatureValid = true;
let batteryValid = true;
let isSafe = true;
let actionResult = true;

const factorySettings = { type: 'settings', payload: { display_name: 'Simulator', hostname: 'simulator',
                                          main_ssid: 'Sim Main', alt_ssid: 'Sim Alt' } };

export function start(handle, setConnected) {
  setConnected(true, 'Connected (local simulation)');
  let autonomy = 24.0;


  // initial events
  setTimeout(() => {
    handle(capabilities);
    handle({ event: 'battery_update', temperature: (25 + Math.random() * 5).toFixed(1), voltage: (13.1 + Math.random() * 0.4).toFixed(2), 
        current: (-0.8 + Math.random() * 0.3).toFixed(2), soc: (100 - Math.random() * 3.5).toFixed(0), battery_sensor_ok: true });
    handle({ event: 'battery_autonomy_update', hours: autonomy.toFixed(2) });
    handle({ event: 'cold_protection_update', lt: 5.0 });
    handle({ event: 'light_update', brightness: Math.random(), auto: true, auto_br: Math.random(), auto_dr: (40 + Math.random() * 5).toFixed(0)});
    handle({ event: 'outlet_update', index: 0, power: Math.random() });
    handle({ event: 'outlet_update', index: 1, power: Math.random() });    

  }, 500);

  // battery regular update
  setInterval(() => { if (updates) handle({
        event: 'battery_update',
        temperature: temperatureValid ? (25 + Math.random() * 5).toFixed(1) : null,
        voltage: batteryValid ? (13.1 + Math.random() * 0.4).toFixed(2) : null,
        current: batteryValid ? (-0.8 + Math.random() * 0.3).toFixed(2) : null,
        soc: (100 - Math.random() * 81).toFixed(0),
        battery_sensor_ok: batteryValid,
    }); 
  }, 2000);

  // autonomy regular update
  setInterval(() => { 
    autonomy -= (10.0 / 3600.0);

    if (updates) handle({
        event: 'battery_autonomy_update',
        hours: autonomy.toFixed(2)
    }); 
  }, 10000);
}

export function send(obj, handle) {
  const ok = { type: 'result', id: obj.id, payload: actionResult };

  if (obj.action === 'get_settings') {
    handle(factorySettings);
    return { type: 'result', id: obj.id, payload: true };
  }

  if (obj.action === 'update_settings') {
    if (actionResult) {
      if (obj.payload.factory_reset == true) {
        handle(factorySettings);
      } else {
        const p = { ...obj.payload };
        delete p.main_psk; delete p.alt_psk; delete p.ap_psk;
        handle({ type: 'settings', payload: p });
      }
    } else {
      ok.detail = 'simulated_reject';
    }
    return ok;
  }

  if (obj.action === 'restart') {
    setTimeout(() => reboot(handle), 5000);
    return ok;
  }

  // fire-and-forget (senza id): notifica solo i rifiuti, come il firmware
  if (!obj.id) handle(ok);
  return ok;
}

// risposta simulata a una action; ritorna l'oggetto result
export function send(obj, handle) {
  if (obj.action === 'get_settings') {
    handle(factorySettings);
  } else if (obj.action == 'update_settings') {
    if (obj.payload.factory_reset == true) {
      handle(factorySettings);
    } else {
      delete obj.action;
      obj.payload.main_psk = '';
      obj.payload.alt_psk = '';
      obj.payload.ap_psk = '';
      obj.type='settings';
      handle(obj);
    }
  } else if (obj.action == 'restart') {
    setTimeout(() => {
        reboot(handle);
    }, 5000);
  } else 
    handle( { type: 'result', id: obj.id, payload: actionResult });
}

// upload finto: chiama i callback che gli passi
export function upload(cb) {
  let pct = 0;
  const t = setInterval(() => {
    pct += 8 + Math.random() * 12;
    if (pct >= 100) { clearInterval(t); cb.done(); }
    else cb.progress(Math.round(pct));
  }, 250);
}

export function reboot(handle) {
    capabilities.payload.boot_count++;
    handle(capabilities);
}

export function enableUpdates() {
    updates = true;
}

export function disableUpdates() {
    updates = false;
}

export function setTemperatureValid(v) {
    temperatureValid = v;
}

export function setBatteryValid(v) {
    batteryValid = v;
}

export function setIsSafe(v, handle) {
    if (! handle) {
        console.log("ERROR: Missing handling function argument.");
        return;
    }
    if (v != isSafe) {
        isSafe = v;
        handle({ event: 'safety_update', is_safe: v });
    }
}

export function setActionResult(v) {
    actionResult = v;
}