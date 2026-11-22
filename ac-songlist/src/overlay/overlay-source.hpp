// Fuente de video de OBS ("AC Songlist") que muestra el overlay.
#pragma once

#include "renderer.hpp"

#include <atomic>
#include <memory>
#include <mutex>

namespace acsl {

// Estado compartido entre OBS (hilo grafico, hilos de carga) y el plugin (hilo de la interfaz).
struct SourceShared {
	std::mutex mutex;
	// --- protegido por mutex
	OverlayStyle style;
	bool listOnDemand = false;
	int pageSeconds = 8;
	int styleVersion = 0; // sube cada vez que el usuario cambia las propiedades
	QImage image;         // ultima imagen dibujada, pendiente de subir a la GPU
	bool imageDirty = false;
	// ---
	std::atomic<bool> alive{true};
	std::atomic<bool> showing{false}; // la fuente esta visible en alguna salida
};

void registerOverlaySource();

} // namespace acsl
