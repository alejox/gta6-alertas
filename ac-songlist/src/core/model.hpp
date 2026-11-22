// AC Songlist - modelo de datos (solo QtCore, sin dependencias de OBS)
#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace acsl {

struct Song {
	int id = 0; // numero que ve el publico: "!sr 12"
	QString title;
	QString artist;
	QStringList categories;
	bool active = true;
	int timesPlayed = 0;
	QDateTime lastPlayed;
	QDateTime createdAt;
	QString note;

	QString display() const
	{
		return artist.isEmpty() ? title : title + QStringLiteral(" — ") + artist;
	}
};

struct Request {
	QString user;
	QString source; // "twitch" | "manual"
	QDateTime at;
};

struct QueueItem {
	int id = 0;
	int songId = 0; // 0 = cancion fuera de la lista
	QString title;
	QString artist;
	QList<Request> requests;
	QDateTime at;

	bool valid() const { return id > 0; }
	QStringList requesters() const
	{
		QStringList out;
		for (const Request &r : requests)
			if (!r.user.isEmpty() && !out.contains(r.user, Qt::CaseInsensitive))
				out << r.user;
		return out;
	}
	QString display() const
	{
		return artist.isEmpty() ? title : title + QStringLiteral(" — ") + artist;
	}
};

struct HistoryItem {
	int id = 0;
	int songId = 0;
	QString title;
	QString artist;
	QStringList requesters;
	QDateTime playedAt;
};

struct Settings {
	bool requestsOpen = true;
	int maxQueue = 0;         // 0 = sin limite
	int perUser = 2;          // pedidos simultaneos por usuario, 0 = sin limite
	bool allowOffList = false; // aceptar canciones que no estan en la biblioteca
	bool replies = true;      // responder en el chat (requiere bot)
	int listSeconds = 30;     // tiempo que se muestra la lista al pedirla
	QString channel;
	QString botUser;
	QString botToken;
};

enum class ReqStatus {
	Added,          // entro a la cola
	Joined,         // ya estaba en cola: se suma el usuario al pedido
	Closed,         // pedidos cerrados
	Empty,          // no escribio nada
	NotFound,       // no existe en la biblioteca
	QueueFull,
	UserLimit,
	Duplicate,      // ese usuario ya la habia pedido
	AlreadyPlaying, // es la que esta sonando
};

struct ReqResult {
	ReqStatus status = ReqStatus::NotFound;
	Song song;
	int position = 0; // 1 = la siguiente
	bool ok() const { return status == ReqStatus::Added || status == ReqStatus::Joined; }
};

} // namespace acsl
