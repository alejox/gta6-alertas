#include "bot.hpp"

namespace acsl {

namespace {
enum class Cmd { None, Request, List, Queue, Current, Next, Wrong, Mine, Open, Close, Done, Skip };

Cmd parseCommand(const QString &word)
{
	static const QHash<QString, Cmd> map{
		{"!sr", Cmd::Request},        {"!pedir", Cmd::Request},      {"!pido", Cmd::Request},
		{"!request", Cmd::Request},   {"!songrequest", Cmd::Request},
		{"!lista", Cmd::List},        {"!canciones", Cmd::List},     {"!list", Cmd::List},
		{"!sl", Cmd::List},           {"!songlist", Cmd::List},
		{"!cola", Cmd::Queue},        {"!queue", Cmd::Queue},        {"!sq", Cmd::Queue},
		{"!songqueue", Cmd::Queue},
		{"!cancion", Cmd::Current},   {"!song", Cmd::Current},       {"!current", Cmd::Current},
		{"!sonando", Cmd::Current},   {"!currentsong", Cmd::Current},
		{"!siguiente", Cmd::Next},    {"!next", Cmd::Next},          {"!nextsong", Cmd::Next},
		{"!quitar", Cmd::Wrong},      {"!wrongsong", Cmd::Wrong},    {"!ws", Cmd::Wrong},
		{"!mipedido", Cmd::Mine},     {"!posicion", Cmd::Mine},      {"!position", Cmd::Mine},
		{"!abrir", Cmd::Open},        {"!sropen", Cmd::Open},
		{"!cerrar", Cmd::Close},      {"!srclose", Cmd::Close},
		{"!tocada", Cmd::Done},       {"!srnext", Cmd::Done},
		{"!saltar", Cmd::Skip},       {"!srskip", Cmd::Skip},
	};
	return map.value(word.toLower(), Cmd::None);
}
} // namespace

Bot::Bot(Store *store, Translate tr) : m_store(store), m_tr(std::move(tr))
{
	m_clock.start();
}

bool Bot::cooling(const QString &key)
{
	const qint64 now = m_clock.elapsed();
	auto it = m_last.constFind(key);
	if (it != m_last.constEnd() && now - *it < m_cooldownMs)
		return true;
	m_last.insert(key, now);
	return false;
}

QString Bot::requestReply(const ChatUser &user, const ReqResult &res) const
{
	const QString who = user.name();
	switch (res.status) {
	case ReqStatus::Added:
		return t("Chat.Added").arg(who, res.song.display()).arg(res.position);
	case ReqStatus::Joined:
		return t("Chat.Joined").arg(who, res.song.display()).arg(res.position);
	case ReqStatus::Closed:
		return t("Chat.Closed").arg(who);
	case ReqStatus::Empty:
		return t("Chat.Usage").arg(who);
	case ReqStatus::NotFound:
		return t("Chat.NotFound").arg(who);
	case ReqStatus::QueueFull:
		return t("Chat.QueueFull").arg(who);
	case ReqStatus::UserLimit:
		return t("Chat.UserLimit").arg(who).arg(m_store->settings().perUser);
	case ReqStatus::Duplicate:
		return t("Chat.Duplicate").arg(who, res.song.display()).arg(res.position);
	case ReqStatus::AlreadyPlaying:
		return t("Chat.AlreadyPlaying").arg(who, res.song.display());
	}
	return {};
}

QString Bot::handle(const ChatUser &user, const QString &message)
{
	const QString text = message.simplified();
	if (!text.startsWith(QLatin1Char('!')))
		return {};
	const int space = text.indexOf(QLatin1Char(' '));
	const QString word = space < 0 ? text : text.left(space);
	const QString args = space < 0 ? QString() : text.mid(space + 1).trimmed();
	const QString who = user.name();

	switch (parseCommand(word)) {
	case Cmd::None:
		return {};

	case Cmd::Request:
		return requestReply(user, m_store->request(who, args, QStringLiteral("twitch")));

	case Cmd::List:
		m_store->showList(); // la lista aparece en pantalla aunque no se responda
		if (cooling("list"))
			return {};
		return t("Chat.List").arg(m_store->activeSongCount());

	case Cmd::Queue: {
		if (cooling("queue"))
			return {};
		const QList<QueueItem> &q = m_store->queue();
		if (q.isEmpty())
			return t("Chat.QueueEmpty");
		QStringList parts;
		for (int i = 0; i < q.size() && i < 5; i++)
			parts << QStringLiteral("%1) %2").arg(i + 1).arg(q[i].display());
		QString list = parts.join(QStringLiteral(" · "));
		if (q.size() > 5)
			list += QStringLiteral(" (+%1)").arg(q.size() - 5);
		return t("Chat.Queue").arg(q.size()).arg(list);
	}

	case Cmd::Current: {
		if (cooling("current"))
			return {};
		const QueueItem &p = m_store->playing();
		if (!p.valid())
			return t("Chat.NoCurrent");
		const QStringList by = p.requesters();
		return by.isEmpty() ? t("Chat.Current").arg(p.display())
				    : t("Chat.CurrentBy").arg(p.display(), by.join(QStringLiteral(", ")));
	}

	case Cmd::Next: {
		if (cooling("next"))
			return {};
		const QList<QueueItem> &q = m_store->queue();
		return q.isEmpty() ? t("Chat.QueueEmpty") : t("Chat.Next").arg(q.first().display());
	}

	case Cmd::Wrong: {
		QueueItem removed;
		if (!m_store->removeLastRequest(who, &removed))
			return t("Chat.NothingToRemove").arg(who);
		return t("Chat.Removed").arg(who, removed.display());
	}

	case Cmd::Mine: {
		const auto mine = m_store->requestsOf(who);
		if (mine.isEmpty())
			return t("Chat.NoRequests").arg(who);
		QStringList parts;
		for (const auto &p : mine)
			parts << QStringLiteral("%1) %2").arg(p.first).arg(p.second.display());
		return t("Chat.Mine").arg(who, parts.join(QStringLiteral(" · ")));
	}

	// --- solo streamer y moderadores
	case Cmd::Open:
		if (!user.admin())
			return {};
		m_store->setRequestsOpen(true);
		return t("Chat.Opened");
	case Cmd::Close:
		if (!user.admin())
			return {};
		m_store->setRequestsOpen(false);
		return t("Chat.ClosedNow");
	case Cmd::Done: {
		if (!user.admin())
			return {};
		m_store->next();
		const QueueItem &p = m_store->playing();
		return p.valid() ? t("Chat.Current").arg(p.display()) : t("Chat.QueueEmpty");
	}
	case Cmd::Skip:
		if (!user.admin())
			return {};
		m_store->skipPlaying();
		return {};
	}
	return {};
}

} // namespace acsl
