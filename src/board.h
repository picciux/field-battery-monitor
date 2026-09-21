#pragma once

#include "include_config.h"   // porta con se' CHANNELS_4 / DISABLE_LIGHT
#include "pins.h"

// Unico punto in cui le scelte di compilazione (CHANNELS_4, DISABLE_LIGHT)
// vengono tradotte in costanti. Il resto del codice usa queste costanti,
// senza #ifdef: il compilatore vede il codice di tutte le varianti.
#ifdef CHANNELS_4
constexpr int BOARD_CHANNELS = 4;
#else
constexpr int BOARD_CHANNELS = 2;
#endif

#ifdef DISABLE_LIGHT
constexpr bool HAS_LIGHT = false;
#else
constexpr bool HAS_LIGHT = true;
#endif

// Canali in ordine di scheda: heater (sempre), canale "luce", CH3, CH4.
// Senza luce, il suo canale diventa il primo outlet.
constexpr int CHANNEL_PINS[] = { PIN_MOSFET_HEATER, PIN_MOSFET_LIGHT,
                                 PIN_MOSFET_CH3, PIN_MOSFET_CH4 };

constexpr int MAX_OUTLETS          = 3;
constexpr int FIRST_OUTLET_CHANNEL = HAS_LIGHT ? 2 : 1;
constexpr int OUTLET_COUNT         = BOARD_CHANNELS - FIRST_OUTLET_CHANNEL;

static_assert(OUTLET_COUNT >= 0 && OUTLET_COUNT <= MAX_OUTLETS,
              "Numero di outlet incoerente con la variante hardware");