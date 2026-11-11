#pragma once

// Stubs until Step 3 (music and rings). Keep signatures stable.

bool audioBegin();
void playRing(int n);       // 1 session start, 2 break start, 3 break end, 4 session end
void musicStart();
void musicPause();
void musicResume();
void musicStop();
