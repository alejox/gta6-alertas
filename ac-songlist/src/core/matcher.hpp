// Busqueda tolerante de canciones: sin acentos, sin mayusculas, por palabras.
#pragma once

#include "model.hpp"

namespace acsl {

// minusculas, sin acentos ni puntuacion, espacios simples
QString normalizeText(const QString &text);

// Puntuacion de 0 (no coincide) a 1000 (titulo exacto).
int matchScore(const Song &song, const QString &normalizedQuery);

// Devuelve el indice de la mejor coincidencia dentro de `songs`, o -1.
// Solo considera canciones activas salvo que includeInactive sea true.
int findBestSong(const QList<Song> &songs, const QString &query, bool includeInactive = false);

// Para el buscador del panel: todas las que coinciden, mejor primero.
QList<int> searchSongs(const QList<Song> &songs, const QString &query);

} // namespace acsl
