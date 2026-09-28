#pragma once
#include <stdint.h>

// Da chiamare una sola volta, il prima possibile in setup() (prima di ogni
// altra inizializzazione, cosi' il valore e' certamente quello del reset
// che ha preceduto QUESTO boot).
void diagSetup();

// Stringa breve, stabile, adatta ad essere inserita in un JSON
// (es. "task_wdt", "brownout", "poweron", "sw", ...).
const char* getResetReasonStr();

// Numero totale di boot da quando la NVS e' stata scritta la prima volta
// (persistito, sopravvive ai reboot). Utile per accorgersi di riavvii
// avvenuti senza che nessuno fosse collegato a osservarli: se il valore
// e' salito piu' di quanto ci si aspetti tra due controlli, ci sono stati
// riavvii nel frattempo, indipendentemente dall'uptime della sessione
// corrente.
uint32_t getRebootCount();