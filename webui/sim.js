
export function start(handle, setConnected) {
  setConnected(true, 'Connected (local simulation)');
  let autonomy = 12.0;

  // initial events
  setTimeout(() => {
    handle({ type: 'capabilities', payload: { channels: 4, light: true, outlets: 2, fw_ver: '1.0-sim' } });
    handle({ event: 'battery_update', temperature: (25 + Math.random() * 5).toFixed(1), voltage: (13.1 + Math.random() * 0.4).toFixed(2), 
        current: (-0.8 + Math.random() * 0.3).toFixed(2), soc: (100 - Math.random() * 3.5).toFixed(0), battery_sensor_ok: true });
    handle({ event: 'battery_autonomy_update', hours: autonomy.toFixed(2) });
    handle({ event: 'cold_protection_update', lt: 5.0 });
    handle({ event: 'light_update', brightness: Math.random(), auto: true, auto_br: Math.random(), auto_dr: (40 + Math.random() * 5).toFixed(0)});
    handle({ event: 'outlet_update', index: 0, power: Math.random() });
    handle({ event: 'outlet_update', index: 1, power: Math.random() });    

  }, 500);

  // battery regular update
  setInterval(() => { handle({
        event: 'battery_update',
        temperature: (25 + Math.random() * 5).toFixed(1),
        voltage: (13.1 + Math.random() * 0.4).toFixed(2),
        current: (-0.8 + Math.random() * 0.3).toFixed(2),
        soc: (100 - Math.random() * 72).toFixed(0),
        battery_sensor_ok: true,
    }); 
  }, 2000);

  // autonomy regular update
  setInterval(() => { 
    autonomy -= (10.0 / 3600.0);
    handle({
        event: 'battery_autonomy_update',
        hours: autonomy.toFixed(2)
    }); 
  }, 10000);
}

// risposta simulata a una action; ritorna l'oggetto result
export function send(obj, handle) {
  if (obj.action === 'get_settings') {
    handle({ type: 'settings', payload: { display_name: 'Simulator', hostname: 'simulator',
                                          main_ssid: 'Sim Main', alt_ssid: 'Sim Alt' } });
  }
  return { type: 'result', id: obj.id, payload: true };
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
