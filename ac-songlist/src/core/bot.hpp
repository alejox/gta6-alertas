// Interpreta los mensajes del chat (!sr, !lista, !cola...) y decide que responder.
#pragma once

#include "store.hpp"

#include <QElapsedTimer>
#include <QHash>
#include <functional>

namespace acsl {

struct ChatUser {
	QString login;   // nombre en minusculas
	QString display; // nombre visible
	bool moderator = false;
	bool broadcaster = false;
	QString name() const { return display.isEmpty() ? login : display; }
	bool admin() const { return moderator || broadcaster; }
};

class Bot {
public:
	using Translate = std::function<QString(const char *key)>;

	Bot(Store *store, Translate tr);

	// Devuelve el texto a responder en el chat, o vacio si no hay que decir nada.
	QString handle(const ChatUser &user, const QString &message);

	// Los comandos informativos (!cola, !cancion...) no se repiten mas seguido que esto.
	void setCooldownMs(int ms) { m_cooldownMs = ms; }

private:
	bool cooling(const QString &key);
	QString t(const char *key) const { return m_tr(key); }
	QString requestReply(const ChatUser &user, const ReqResult &res) const;

	Store *m_store;
	Translate m_tr;
	int m_cooldownMs = 5000;
	QElapsedTimer m_clock;
	QHash<QString, qint64> m_last;
};

} // namespace acsl
