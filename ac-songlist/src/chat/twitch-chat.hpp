// Cliente minimo del chat de Twitch (IRC). Lee de forma anonima; si se configura
// una cuenta de bot con token, ademas puede responder.
#pragma once

#include "../core/bot.hpp"

#include <QObject>
#include <QStringList>
#include <QTimer>

class QTcpSocket;

namespace acsl {

class TwitchChat : public QObject {
	Q_OBJECT
public:
	enum class State { Off, Connecting, Connected, Retrying };

	explicit TwitchChat(QObject *parent = nullptr);
	~TwitchChat() override;

	// Acepta "canal", "#canal" o "https://twitch.tv/canal".
	static QString cleanChannel(const QString &input);
	static QString cleanToken(const QString &input);
	// Convierte una linea IRC en mensaje de chat. Devuelve false si no es un PRIVMSG.
	static bool parsePrivmsg(const QString &line, ChatUser *user, QString *text);

	void configure(const QString &channel, const QString &botUser, const QString &botToken);
	void stop();

	State state() const { return m_state; }
	QString channel() const { return m_channel; }
	bool canSend() const { return m_state == State::Connected && m_authenticated; }
	bool authFailed() const { return m_authFailed; }
	bool secure() const { return m_secure; }
	void send(const QString &text);

signals:
	void message(const acsl::ChatUser &user, const QString &text);
	void stateChanged();

private:
	void open();
	void close();
	void scheduleRetry();
	void setState(State s);
	void onConnected();
	void onReadyRead();
	void onClosed();
	void handleLine(const QString &line);
	void write(const QString &line);
	void flushOutbox();

	QTcpSocket *m_socket = nullptr;
	QTimer m_retryTimer, m_pingTimer, m_sendTimer;
	QByteArray m_buffer;
	QStringList m_outbox;
	QString m_channel, m_botUser, m_botToken;
	State m_state = State::Off;
	bool m_authenticated = false; // conectado con la cuenta del bot
	bool m_authFailed = false;    // el token no sirvio: seguimos en modo lectura
	bool m_secure = false;
	int m_attempt = 0;
	qint64 m_lastData = 0;
};

} // namespace acsl
