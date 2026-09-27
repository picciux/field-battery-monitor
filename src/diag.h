#pragma once

// Da chiamare una sola volta, il prima possibile in setup() (prima di ogni
// altra inizializzazione, cosi' il valore e' certamente quello del reset
// che ha preceduto QUESTO boot).
void diagSetup();

// Stringa breve, stabile, adatta ad essere inserita in un JSON
// (es. "task_wdt", "brownout", "poweron", ...).
const char* getResetReasonStr();

