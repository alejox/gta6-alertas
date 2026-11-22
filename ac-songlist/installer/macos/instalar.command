#!/bin/bash
# Instala AC Songlist en OBS Studio (macOS).
cd "$(dirname "$0")" || exit 1
DESTINO="$HOME/Library/Application Support/obs-studio/plugins"

echo
echo " AC Songlist - plugin de pedidos de canciones para OBS Studio"
echo

if [ ! -d "ac-songlist.plugin" ]; then
  echo " No encuentro ac-songlist.plugin junto a este archivo."
  echo " Descomprime el paquete completo antes de ejecutar instalar.command."
  exit 1
fi

if pgrep -xq "OBS"; then
  echo " OBS esta abierto. Cierralo y vuelve a ejecutar este archivo."
  exit 1
fi

mkdir -p "$DESTINO" || exit 1
rm -rf "$DESTINO/ac-songlist.plugin"
cp -R "ac-songlist.plugin" "$DESTINO/" || { echo " No se pudo copiar."; exit 1; }
# macOS marca lo descargado de internet; sin esto OBS no podria cargar el plugin
xattr -dr com.apple.quarantine "$DESTINO/ac-songlist.plugin" 2>/dev/null

echo " Instalado en: $DESTINO/ac-songlist.plugin"
echo
echo " Abre OBS y activa el panel en el menu Paneles > AC Songlist."
echo " Tus canciones se guardan aparte: reinstalar o actualizar no las borra."
echo
