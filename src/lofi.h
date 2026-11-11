#pragma once

// Speaker test: plays a short procedurally generated lofi loop through the
// board's ES8311 codec + SC8002B amplifier (SPEAKER connector).
//
// Call audioBegin() once in setup() AFTER Wire.begin().

bool audioBegin();     // set up I2S + codec; returns true if the ES8311 answered
void lofiPlay();       // start playing (non-blocking, runs on its own task)
bool lofiIsPlaying();

// Microphone (board MEMS mic -> ES8311 ADC -> I2S in on GPIO6).
// Returns the peak level (0..1) heard since the last call.
float micLevel();
