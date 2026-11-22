// AC Songlist - plugin de OBS Studio para pedidos de canciones.
// Biblioteca propia + pedidos por chat de Twitch + overlay minimalista.
#include "plugin.hpp"
#include "plugin-support.hpp"
#include "overlay/overlay-source.hpp"
#include "ui/dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("ac-songlist", "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
	return "AC Songlist";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Module.Description");
}

using acsl::Plugin;

namespace {

struct Hotkey {
	const char *name;        // identificador guardado en la coleccion de escenas
	const char *textKey;     // texto en Ajustes > Atajos
	void (*run)(Plugin *);   // se ejecuta en el hilo de la interfaz
	obs_hotkey_id id;
};

Hotkey g_hotkeys[] = {
	{"ac_songlist.next", "Hotkey.Next", [](Plugin *p) { p->store()->next(); }, OBS_INVALID_HOTKEY_ID},
	{"ac_songlist.finish", "Hotkey.Finish", [](Plugin *p) { p->store()->finishPlaying(); }, OBS_INVALID_HOTKEY_ID},
	{"ac_songlist.toggle_requests", "Hotkey.ToggleRequests",
	 [](Plugin *p) { p->store()->setRequestsOpen(!p->store()->settings().requestsOpen); }, OBS_INVALID_HOTKEY_ID},
	{"ac_songlist.show_list", "Hotkey.ShowList", [](Plugin *p) { p->store()->showList(); }, OBS_INVALID_HOTKEY_ID},
};

bool g_dockAdded = false;

void hotkeyPressed(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *hotkey = static_cast<Hotkey *>(data);
	Plugin::post(hotkey->run); // los atajos llegan desde otro hilo
}

void addDock()
{
	Plugin *plugin = Plugin::instance();
	auto *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());
	if (g_dockAdded || !plugin || !mainWindow)
		return;
	auto *dock = new acsl::Dock(plugin->store(), plugin->chat(), mainWindow);
	if (obs_frontend_add_dock_by_id("ac-songlist-dock", obs_module_text("Dock.Title"), dock))
		g_dockAdded = true;
	else
		delete dock;
}

// Los atajos se guardan junto con la coleccion de escenas.
void onFrontendSave(obs_data_t *saveData, bool saving, void *)
{
	for (Hotkey &hotkey : g_hotkeys) {
		if (hotkey.id == OBS_INVALID_HOTKEY_ID)
			continue;
		if (saving) {
			obs_data_array_t *bindings = obs_hotkey_save(hotkey.id);
			obs_data_set_array(saveData, hotkey.name, bindings);
			obs_data_array_release(bindings);
		} else {
			obs_data_array_t *bindings = obs_data_get_array(saveData, hotkey.name);
			if (bindings) {
				obs_hotkey_load(hotkey.id, bindings);
				obs_data_array_release(bindings);
			}
		}
	}
}

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		addDock(); // por si la ventana aun no existia al cargar el modulo
	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
		if (Plugin *plugin = Plugin::instance())
			plugin->saveNow();
		obs_frontend_remove_save_callback(onFrontendSave, nullptr);
		obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	}
}

} // namespace

bool obs_module_load(void)
{
	Plugin::create();
	acsl::registerOverlaySource();

	for (Hotkey &hotkey : g_hotkeys)
		hotkey.id = obs_hotkey_register_frontend(hotkey.name, obs_module_text(hotkey.textKey), hotkeyPressed,
							 &hotkey);
	obs_frontend_add_save_callback(onFrontendSave, nullptr);
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	addDock();

	blog(LOG_INFO, "[ac-songlist] plugin cargado (version %s)", ACSL_VERSION);
	return true;
}

void obs_module_unload(void)
{
	for (Hotkey &hotkey : g_hotkeys) {
		if (hotkey.id != OBS_INVALID_HOTKEY_ID)
			obs_hotkey_unregister(hotkey.id);
		hotkey.id = OBS_INVALID_HOTKEY_ID;
	}
	Plugin::destroy();
	blog(LOG_INFO, "[ac-songlist] plugin descargado");
}
