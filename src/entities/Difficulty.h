#pragma once

// "SecondaryHardMode" del original: desde el 5-3 en adelante algunos
// enemigos se ponen mas agresivos (Hammer Bros tiran mas seguido, el fuego
// de Bowser es mas rapido y frecuente, las Cheep Cheep rojas ondulan mas
// rapido). Lo fija Game al cargar cada nivel.
inline bool gSecondaryHardMode = false;

// "PrimaryHardMode" del original: la segunda vuelta, despues de ganar el
// 8-4 (dura hasta cerrar el juego, como en la consola). Goombas pasan a ser
// Buzzy Beetles, los enemigos que caminan van a 12 en vez de 8 (x1,5) y el
// caparazon pisado se recupera antes (11 intervalos en vez de 16); ademas
// vale SecondaryHardMode en todos los niveles.
inline bool gPrimaryHardMode = false;
inline float hardWalkFactor() { return gPrimaryHardMode ? 1.5f : 1.0f; }
inline float hardShellFactor() { return gPrimaryHardMode ? 11.0f / 16.0f : 1.0f; }
