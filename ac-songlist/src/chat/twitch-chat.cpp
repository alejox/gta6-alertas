#include "twitch-chat.hpp"

#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpSocket>
#if QT_CONFIG(ssl)
#include <QSslSocket>
#endif

namespace acsl {

static const char *kHost = "irc.chat.twitch.tv";

TwitchChat::TwitchChat(QObject *parent) : QObject(parent)
{
	m_retryTimer.setSingleShot(true);
	connect(&m_retryTimer, &QTimer::timeout, this, &TwitchChat::open);

	// Twitch manda PING cada ~5 min; si pasa mucho sin nada, la conexion murio.
	m_pingTimer.setInterval(60 * 1000);
	connect(&m_pingTimer, &QTimer::timeout, this, [this]() {
		const qint64 idle = QDateTime::currentMSecsSinceEpoch() - m_lastData;
		if (idle > 7 * 60 * 1000) {
			close();
			scheduleRetry();
		} else if (idle > 4 * 60 * 1000) {
			write(QStringLiteral("PING :acsl"));
		}
	});

	// Limite de Twitch para cuentas normales: 20 mensajes cada 30 s.
	m_sendTimer.setInterval(1600);
	connect(&m_sendTimer, &QTimer::timeout, this, &TwitchChat::flushOutbox);
}

TwitchChat::~TwitchChat()
{
	close();
}

QString TwitchChat::cleanChannel(const QString &input)
{
	QString c = input.trimmed().toLower();
	const int slash = c.lastIndexOf(QLatin1Char('/'));
	if (slash >= 0)
		c = c.mid(slash + 1);
	const int query = c.indexOf(QLatin1Char('?'));
	if (query >= 0)
		c = c.left(query);
	c.remove(QLatin1Char('#'));
	c.remove(QLatin1Char('@'));
	static const QRegularExpression valid(QStringLiteral("^[a-z0-9_]{2,25}$"));
	return valid.match(c).hasMatch() ? c : QString();
}

QString TwitchChat::cleanToken(const QString &input)
{
	QString t = input.trimmed();
	if (t.startsWith(QLatin1String("oauth:"), Qt::CaseInsensitive))
		t = t.mid(6);
	static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9]{10,80}$"));
	return valid.match(t).hasMatch() ? t : QString();
}

void TwitchChat::configure(const QString &channel, const QString &botUser, const QString &botToken)
{
	const QString c = cleanChannel(channel);
	const QString u = cleanChannel(botUser);
	const QString t = cleanToken(botToken);
	if (c == m_channel && u == m_botUser && t == m_botToken && m_state != State::Off)
		return;
	m_channel = c;
	m_botUser = u;
	m_botToken = t;
	m_authFailed = false;
	m_attempt = 0;
	close();
	if (m_channel.isEmpty()) {
		setState(State::Off);
		return;
	}
	open();
}

void TwitchChat::stop()
{
	m_channel.clear();
	close();
	setState(State::Off);
}

void TwitchChat::setState(State s)
{
	if (m_state == s)
		return;
	m_state = s;
	emit stateChanged();
}

void TwitchChat::close()
{
	m_retryTimer.stop();
	m_pingTimer.stop();
	m_sendTimer.stop();
	m_outbox.clear();
	m_buffer.clear();
	m_authenticated = false;
	if (m_socket) {
		m_socket->disconnect(this);
		m_socket->abort();
		m_socket->deleteLater();
		m_socket = nullptr;
	}
}

void TwitchChat::open()
{
	if (m_channel.isEmpty())
		return;
	close();
	setState(State::Connecting);
	m_lastData = QDateTime::currentMSecsSinceEpoch();

	// Solo para pruebas: ACSL_IRC_TEST_PORT=6667 conecta a un servidor de chat simulado
	// en este mismo equipo en lugar de a Twitch.
	const int testPort = qEnvironmentVariableIntValue("ACSL_IRC_TEST_PORT");
	if (testPort > 0) {
		m_secure = true; // no sale del equipo
		m_socket = new QTcpSocket(this);
		connect(m_socket, &QAbstractSocket::connected, this, &TwitchChat::onConnected);
		connect(m_socket, &QAbstractSocket::readyRead, this, &TwitchChat::onReadyRead);
		connect(m_socket, &QAbstractSocket::disconnected, this, &TwitchChat::onClosed);
		connect(m_socket, &QAbstractSocket::errorOccurred, this, &TwitchChat::onClosed);
		m_socket->connectToHost(QStringLiteral("127.0.0.1"), quint16(testPort));
		return;
	}

#if QT_CONFIG(ssl)
	if (QSslSocket::supportsSsl()) {
		auto *ssl = new QSslSocket(this);
		m_socket = ssl;
		m_secure = true;
		connect(ssl, &QSslSocket::encrypted, this, &TwitchChat::onConnected);
		connect(ssl, &QAbstractSocket::readyRead, this, &TwitchChat::onReadyRead);
		connect(ssl, &QAbstractSocket::disconnected, this, &TwitchChat::onClosed);
		connect(ssl, &QAbstractSocket::errorOccurred, this, &TwitchChat::onClosed);
		ssl->connectToHostEncrypted(QString::fromLatin1(kHost), 6697);
		return;
	}
#endif
	// Sin TLS disponible: conexion sin cifrar, solo anonima (nunca mandamos el token asi).
	m_secure = false;
	m_socket = new QTcpSocket(this);
	connect(m_socket, &QAbstractSocket::connected, this, &TwitchChat::onConnected);
	connect(m_socket, &QAbstractSocket::readyRead, this, &TwitchChat::onReadyRead);
	connect(m_socket, &QAbstractSocket::disconnected, this, &TwitchChat::onClosed);
	connect(m_socket, &QAbstractSocket::errorOccurred, this, &TwitchChat::onClosed);
	m_socket->connectToHost(QString::fromLatin1(kHost), 6667);
}

void TwitchChat::onConnected()
{
	const bool useBot = m_secure && !m_authFailed && !m_botUser.isEmpty() && !m_botToken.isEmpty();
	m_authenticated = useBot;
	write(QStringLiteral("CAP REQ :twitch.tv/tags twitch.tv/commands"));
	if (useBot) {
		write(QStringLiteral("PASS oauth:") + m_botToken);
		write(QStringLiteral("NICK ") + m_botUser);
	} else {
		write(QStringLiteral("NICK justinfan%1").arg(QRandomGenerator::global()->bounded(10000, 99999)));
	}
}

void TwitchChat::onClosed()
{
	if (!m_socket)
		return;
	close();
	scheduleRetry();
}

void TwitchChat::scheduleRetry()
{
	if (m_channel.isEmpty()) {
		setState(State::Off);
		return;
	}
	static const int ladder[] = {2000, 5000, 10000, 30000, 60000};
	const int step = ladder[qMin(m_attempt, 4)];
	m_attempt++;
	setState(State::Retrying);
	m_retryTimer.start(step + QRandomGenerator::global()->bounded(step / 4 + 1));
}

void TwitchChat::write(const QString &line)
{
	if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
		return;
	m_socket->write(line.toUtf8() + "\r\n");
}

void TwitchChat::onReadyRead()
{
	if (!m_socket)
		return;
	m_lastData = QDateTime::currentMSecsSinceEpoch();
	m_buffer.append(m_socket->readAll());
	if (m_buffer.size() > 1024 * 1024) { // nunca deberia pasar
		m_buffer.clear();
		return;
	}
	int nl;
	while ((nl = m_buffer.indexOf('\n')) >= 0) {
		QByteArray raw = m_buffer.left(nl);
		m_buffer.remove(0, nl + 1);
		if (raw.endsWith('\r'))
			raw.chop(1);
		if (!raw.isEmpty())
			handleLine(QString::fromUtf8(raw));
		if (!m_socket)
			return; // handleLine cerro la conexion
	}
}

static QString unescapeTag(const QString &v)
{
	QString out;
	out.reserve(v.size());
	for (int i = 0; i < v.size(); i++) {
		if (v[i] != QLatin1Char('\\') || i + 1 >= v.size()) {
			out.append(v[i]);
			continue;
		}
		const QChar n = v[++i];
		if (n == QLatin1Char('s'))
			out.append(QLatin1Char(' '));
		else if (n == QLatin1Char(':'))
			out.append(QLatin1Char(';'));
		else if (n == QLatin1Char('\\'))
			out.append(QLatin1Char('\\'));
		else if (n != QLatin1Char('r') && n != QLatin1Char('n'))
			out.append(n);
	}
	return out;
}

bool TwitchChat::parsePrivmsg(const QString &line, ChatUser *user, QString *text)
{
	QString rest = line;
	QString tags;
	if (rest.startsWith(QLatin1Char('@'))) {
		const int sp = rest.indexOf(QLatin1Char(' '));
		if (sp < 0)
			return false;
		tags = rest.mid(1, sp - 1);
		rest = rest.mid(sp + 1);
	}
	if (!rest.startsWith(QLatin1Char(':')))
		return false;
	const int sp = rest.indexOf(QLatin1Char(' '));
	if (sp < 0)
		return false;
	const QString prefix = rest.mid(1, sp - 1);
	rest = rest.mid(sp + 1);
	if (!rest.startsWith(QLatin1String("PRIVMSG ")))
		return false;
	const int colon = rest.indexOf(QLatin1String(" :"));
	if (colon < 0)
		return false;

	QString body = rest.mid(colon + 2);
	if (body.startsWith(QChar(1))) { // "/me ..." llega como \x01ACTION ...\x01
		body.remove(QChar(1));
		if (body.startsWith(QLatin1String("ACTION ")))
			body = body.mid(7);
	}

	ChatUser u;
	u.login = prefix.section(QLatin1Char('!'), 0, 0).toLower();
	for (const QString &tag : tags.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
		const int eq = tag.indexOf(QLatin1Char('='));
		if (eq < 0)
			continue;
		const QString key = tag.left(eq);
		const QString value = tag.mid(eq + 1);
		if (key == QLatin1String("display-name"))
			u.display = unescapeTag(value).trimmed();
		else if (key == QLatin1String("mod"))
			u.moderator = u.moderator || value == QLatin1String("1");
		else if (key == QLatin1String("badges")) {
			u.broadcaster = value.contains(QLatin1String("broadcaster/"));
			u.moderator = u.moderator || value.contains(QLatin1String("moderator/"));
		}
	}
	if (u.login.isEmpty())
		return false;
	*user = u;
	*text = body;
	return true;
}

void TwitchChat::handleLine(const QString &line)
{
	if (line.startsWith(QLatin1String("PING"))) {
		write(QStringLiteral("PONG") + line.mid(4));
		return;
	}

	ChatUser user;
	QString text;
	if (parsePrivmsg(line, &user, &text)) {
		emit message(user, text);
		return;
	}

	// comando IRC sin etiquetas ni prefijo
	QString rest = line;
	if (rest.startsWith(QLatin1Char('@')))
		rest = rest.section(QLatin1Char(' '), 1);
	if (rest.startsWith(QLatin1Char(':')))
		rest = rest.section(QLatin1Char(' '), 1);
	const QString command = rest.section(QLatin1Char(' '), 0, 0);

	if (command == QLatin1String("001")) { // sesion aceptada
		write(QStringLiteral("JOIN #") + m_channel);
		m_attempt = 0;
		m_pingTimer.start();
		m_sendTimer.start();
		setState(State::Connected);
		emit stateChanged(); // puede cambiar "con bot / solo lectura" sin cambiar el estado
	} else if (command == QLatin1String("NOTICE") && m_authenticated &&
		   (rest.contains(QLatin1String("authentication failed")) ||
		    rest.contains(QLatin1String("Improperly formatted auth")))) {
		// Token invalido o caducado: seguimos leyendo el chat de forma anonima.
		m_authFailed = true;
		close();
		open();
	} else if (command == QLatin1String("RECONNECT")) {
		close();
		open();
	}
}

void TwitchChat::send(const QString &text)
{
	if (!canSend())
		return;
	QString t = text;
	t.replace(QLatin1Char('\r'), QLatin1Char(' '));
	t.replace(QLatin1Char('\n'), QLatin1Char(' '));
	t = t.simplified().left(450);
	if (t.isEmpty())
		return;
	// Que un titulo de cancion nunca pueda ejecutarse como comando de chat ("/ban ...").
	if (t.startsWith(QLatin1Char('/')) || t.startsWith(QLatin1Char('.')))
		t.prepend(QStringLiteral("• "));
	if (m_outbox.size() < 20)
		m_outbox << t;
}

void TwitchChat::flushOutbox()
{
	if (m_outbox.isEmpty() || !canSend())
		return;
	write(QStringLiteral("PRIVMSG #%1 :%2").arg(m_channel, m_outbox.takeFirst()));
}

} // namespace acsl
