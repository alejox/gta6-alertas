// Biblioteca de canciones, cola de pedidos, "sonando ahora" e historial.
// Vive en el hilo de la interfaz. Solo QtCore.
#pragma once

#include "model.hpp"

#include <QJsonObject>
#include <QObject>

namespace acsl {

class Store : public QObject {
	Q_OBJECT
public:
	explicit Store(QObject *parent = nullptr);

	const QList<Song> &songs() const { return m_songs; }
	const QList<QueueItem> &queue() const { return m_queue; }
	const QueueItem &playing() const { return m_playing; }
	const QList<HistoryItem> &history() const { return m_history; }
	const Settings &settings() const { return m_settings; }
	const Song *song(int id) const;
	QStringList categories() const;
	int activeSongCount() const;

	// --- biblioteca
	int addSong(const QString &title, const QString &artist, const QStringList &categories = {},
		    const QString &note = {});
	bool updateSong(const Song &song);
	void removeSongs(const QList<int> &ids);
	// Devuelve cuantas se agregaron; las que ya existian (mismo titulo y artista) se actualizan.
	int importSongs(const QList<Song> &incoming, int *updated = nullptr);

	// --- pedidos
	// force = pedido hecho por el streamer desde el panel: se salta cierres y limites.
	ReqResult request(const QString &user, const QString &query, const QString &source, bool force = false);
	ReqResult requestSong(const QString &user, int songId, const QString &source, bool force = false);
	bool removeLastRequest(const QString &user, QueueItem *removed = nullptr);
	// Puestos (1 = siguiente) de los pedidos de un usuario.
	QList<QPair<int, QueueItem>> requestsOf(const QString &user) const;

	// --- cola y "sonando ahora"
	void playQueueItem(int queueId); // pasa un pedido a "sonando ahora"
	void playSong(int songId);       // tocar una cancion de la biblioteca sin pedido
	bool finishPlaying();            // la que suena pasa al historial
	bool next();                     // termina la actual y empieza la siguiente de la cola
	void skipPlaying();              // la quita sin contarla como tocada
	void removeQueueItem(int queueId);
	void moveQueueItem(int queueId, int delta);
	void clearQueue();
	void clearHistory();

	void setSettings(const Settings &settings);
	void setRequestsOpen(bool open);
	void showList(); // alguien pidio ver la lista en pantalla

	// --- persistencia
	QJsonObject toJson() const;
	void fromJson(const QJsonObject &root);
	bool load(const QString &path);
	bool save(const QString &path) const;

signals:
	void changed();
	void settingsChanged();
	void listRequested();

private:
	Song *songPtr(int id);
	void returnPlayingToQueue();
	void registerPlay(const QueueItem &item);

	QList<Song> m_songs;
	QList<QueueItem> m_queue;
	QueueItem m_playing;
	QList<HistoryItem> m_history;
	Settings m_settings;
	int m_nextSongId = 1;
	int m_nextQueueId = 1;
	int m_nextHistoryId = 1;
};

} // namespace acsl
