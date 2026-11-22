// Importar y exportar la biblioteca en CSV (compatible con el export de StreamerSonglist).
#pragma once

#include "model.hpp"

namespace acsl {

// Acepta coma, punto y coma o tabulador; con o sin fila de cabeceras; y tambien
// texto plano con una cancion por linea ("Titulo - Artista").
QList<Song> parseSongsCsv(const QString &text, QStringList *warnings = nullptr);

QString songsToCsv(const QList<Song> &songs);

} // namespace acsl
