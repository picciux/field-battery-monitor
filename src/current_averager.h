// CurrentAverager.h
#pragma once
#include <Arduino.h>

// Ring buffer time-based per la media mobile della corrente di scarica.
// Non è a N campioni fissi: la finestra è espressa in millisecondi e la
// potatura (prune) rimuove i campioni più vecchi della finestra ad ogni
// inserimento/lettura, indipendentemente da quanti sample siano arrivati.
class CurrentAverager {
public:
    // capacity: dimensione massima del buffer. Va dimensionata sul caso
    // peggiore: sample_interval più fitto atteso entro la finestra scelta
    // (es. 1 campione/s su una finestra di 10 minuti => capacity >= 600).
    // windowMs: ampiezza della finestra temporale della media (es. 10*60*1000).
    CurrentAverager(size_t capacity, unsigned long windowMs)
        : _capacity(capacity), _windowMs(windowMs) {
        _timestamps = new unsigned long[capacity];
        _currents   = new float[capacity];
    }

    ~CurrentAverager() {
        delete[] _timestamps;
        delete[] _currents;
    }

    // Non copiabile: possiede memoria via new, evitiamo double-free/shallow copy.
    // Non serve nel progetto (l'oggetto vive per tutta la durata del programma
    // come membro di Battery), ma è corretto renderlo esplicito.
    CurrentAverager(const CurrentAverager&) = delete;
    CurrentAverager& operator=(const CurrentAverager&) = delete;

    void addSample(unsigned long now, float currentA) {
        _timestamps[_head] = now;
        _currents[_head] = currentA;
        _head = (_head + 1) % _capacity;
        if (_count < _capacity) {
            _count++;
        }
        prune(now);
    }

    // Ritorna false se non ci sono campioni nella finestra (es. subito dopo
    // il boot) - il chiamante deve gestire esplicitamente questo caso,
    // non assumere un valore di default silenzioso.
    bool getAverage(unsigned long now, float& outAvgA) {
        prune(now);
        if (_count == 0) {
            return false;
        }
        float sum = 0.0f;
        size_t idx = oldestIndex();
        for (size_t i = 0; i < _count; i++) {
            sum += _currents[(idx + i) % _capacity];
        }
        outAvgA = sum / static_cast<float>(_count);
        return true;
    }

    // Utili per diagnostica/debug (es. esporre "quanti campioni ha la media
    // attuale" in un endpoint di stato), non strettamente necessari al calcolo.
    size_t sampleCount() const { return _count; }
    bool isFull() const { return _count == _capacity; }

private:
    size_t oldestIndex() const {
        return (_head + _capacity - _count) % _capacity;
    }

    // now - _timestamps[...] con aritmetica unsigned gestisce correttamente
    // l'overflow di millis() (~49 giorni), stesso principio già applicato
    // alle transizioni di dimmerazione — nessuna gestione esplicita necessaria.
    void prune(unsigned long now) {
        while (_count > 0) {
            size_t oldest = oldestIndex();
            if (now - _timestamps[oldest] > _windowMs) {
                _count--;
            } else {
                break;
            }
        }
    }

    unsigned long* _timestamps;
    float* _currents;
    const size_t _capacity;
    const unsigned long _windowMs;
    size_t _head = 0;
    size_t _count = 0;
};