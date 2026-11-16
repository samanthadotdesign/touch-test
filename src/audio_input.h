#pragma once

// The codec shares Wire with touch. Call after touchBegin() has started that bus.
// Begin probes the ES8311; capture remains disabled until explicitly requested.
bool audioInputBegin();

// Enabling starts a staged codec setup. A true return accepts the request;
// audioInputUpdate() finishes setup without sleeps and discards startup samples.
// Disabling stops DMA immediately, then powers down the codec in update().
bool audioInputSetEnabled(bool enabled);
bool audioInputAvailable();
bool audioInputEnabled();

// Unsmoothed RMS strength, normalized to 0..1 using the browser's noise floor.
// The atmosphere controller owns attack/release smoothing and blow thresholds.
float audioInputLevel();
void audioInputUpdate();
