#include "store.hpp"
#include "matcher.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>

namespace acsl {

static const int kMaxHistory = 500;

Store::Store(QObject *parent) : QObject(parent) {}

const Song *Store::song(int id) const
{
	for (const Song &s : m_songs)
		if (s.id == id)
			return &s;
	return nullptr;
}

Song *Store::songPtr(int id)
{
	for (Song &s : m_songs)
		if (s.id == id)
			return &s;
	return nullptr;
}

QStringList Store::categories() const
{
	QStringList out;
	for (const Song &s : m_songs)
		for (const QString &c : s.categories)
			if (!out.contains(c, Qt::CaseInsensitive))
				out << c;
	out.sort(Qt::CaseInsensitive);
	return out;
}

int Store::activeSongCount() const
{
	int n = 0;
	for (const Song &s : m_songs)
		if (s.active)
			n++;
	return n;
}

// ---------------------------------------------------------------- biblioteca

int Store::addSong(const QString &title, const QString &artist, const QStringList &categories, const QString &note)
{
	const QString t = title.simplified();
	if (t.isEmpty())
		return 0;
	Song s;
	s.id = m_nextSongId++;
	s.title = t;
	s.artist = artist.simplified();
	s.categories = categories;
	s.note = note;
	s.createdAt = QDateTime::currentDateTimeUtc();
	m_songs.append(s);
	emit changed();
	return s.id;
}

bool Store::updateSong(const Song &song)
{
	Song *s = songPtr(song.id);
	if (!s || song.title.simplified().isEmpty())
		return false;
	const QDateTime created = s->createdAt;
	*s = song;
	s->title = song.title.simplified();
	s->artist = song.artist.simplified();
	s->createdAt = created;
	// los pedidos en cola muestran el titulo nuevo
	for (QueueItem &q : m_queue)
		if (q.songId == s->id) {
			q.title = s->title;
			q.artist = s->artist;
		}
	if (m_playing.valid() && m_playing.songId == s->id) {
		m_playing.title = s->title;
		m_playing.artist = s->artist;
	}
	emit changed();
	return true;
}

void Store::removeSongs(const QList<int> &ids)
{
	bool any = false;
	for (int i = m_songs.size() - 1; i >= 0; i--)
		if (ids.contains(m_songs[i].id)) {
			m_songs.removeAt(i);
			any = true;
		}
	if (any)
		emit changed();
}

static QString songKey(const QString &title, const QString &artist)
{
	return normalizeText(title) + QChar(0x1f) + normalizeText(artist);
}

int Store::importSongs(const QList<Song> &incoming, int *updated)
{
	QHash<QString, int> index;
	for (int i = 0; i < m_songs.size(); i++)
		index.insert(songKey(m_songs[i].title, m_songs[i].artist), i);

	int added = 0, changedCount = 0;
	for (const Song &in : incoming) {
		const QString title = in.title.simplified();
		if (title.isEmpty())
			continue;
		const QString key = songKey(title, in.artist);
		auto it = index.constFind(key);
		if (it != index.constEnd()) {
			Song &s = m_songs[*it];
			for (const QString &c : in.categories)
				if (!s.categories.contains(c, Qt::CaseInsensitive))
					s.categories << c;
			if (in.timesPlayed > s.timesPlayed)
				s.timesPlayed = in.timesPlayed;
			if (in.lastPlayed.isValid() && (!s.lastPlayed.isValid() || in.lastPlayed > s.lastPlayed))
				s.lastPlayed = in.lastPlayed;
			if (s.note.isEmpty())
				s.note = in.note;
			changedCount++;
			continue;
		}
		Song s = in;
		s.id = m_nextSongId++;
		s.title = title;
		s.artist = in.artist.simplified();
		if (!s.createdAt.isValid())
			s.createdAt = QDateTime::currentDateTimeUtc();
		index.insert(key, m_songs.size());
		m_songs.append(s);
		added++;
	}
	if (updated)
		*updated = changedCount;
	if (added || changedCount)
		emit changed();
	return added;
}

// ------------------------------------------------------------------- pedidos

static bool hasUser(const QueueItem &item, const QString &user)
{
	for (const Request &r : item.requests)
		if (r.user.compare(user, Qt::CaseInsensitive) == 0)
			return true;
	return false;
}

ReqResult Store::request(const QString &user, const QString &query, const QString &source, bool force)
{
	ReqResult res;
	if (!force && !m_settings.requestsOpen) {
		res.status = ReqStatus::Closed;
		return res;
	}
	const QString q = query.simplified();
	if (q.isEmpty()) {
		res.status = ReqStatus::Empty;
		return res;
	}

	// "!sr 12" o "!sr #12": por numero
	static const QRegularExpression numberRe(QStringLiteral("^#?\\s*(\\d{1,6})$"));
	const QRegularExpressionMatch m = numberRe.match(q);
	if (m.hasMatch()) {
		const Song *s = song(m.captured(1).toInt());
		if (s && (s->active || force))
			return requestSong(user, s->id, source, force);
	}

	const int idx = findBestSong(m_songs, q, force);
	if (idx >= 0)
		return requestSong(user, m_songs[idx].id, source, force);

	if (!force && !m_settings.allowOffList) {
		res.status = ReqStatus::NotFound;
		return res;
	}

	// cancion fuera de la biblioteca
	if (!force && m_settings.perUser > 0 && requestsOf(user).size() >= m_settings.perUser) {
		res.status = ReqStatus::UserLimit;
		return res;
	}
	if (!force && m_settings.maxQueue > 0 && m_queue.size() >= m_settings.maxQueue) {
		res.status = ReqStatus::QueueFull;
		return res;
	}
	QueueItem item;
	item.id = m_nextQueueId++;
	item.title = q;
	item.at = QDateTime::currentDateTimeUtc();
	if (!user.isEmpty())
		item.requests.append({user, source, item.at});
	m_queue.append(item);
	res.status = ReqStatus::Added;
	res.song.title = q;
	res.position = m_queue.size();
	emit changed();
	return res;
}

ReqResult Store::requestSong(const QString &user, int songId, const QString &source, bool force)
{
	ReqResult res;
	const Song *s = song(songId);
	if (!s) {
		res.status = ReqStatus::NotFound;
		return res;
	}
	res.song = *s;
	if (!force && !m_settings.requestsOpen) {
		res.status = ReqStatus::Closed;
		return res;
	}
	if (m_playing.valid() && m_playing.songId == songId) {
		res.status = ReqStatus::AlreadyPlaying;
		return res;
	}
	const bool limited = !force && m_settings.perUser > 0 && requestsOf(user).size() >= m_settings.perUser;
	const QDateTime now = QDateTime::currentDateTimeUtc();

	for (int i = 0; i < m_queue.size(); i++) {
		QueueItem &item = m_queue[i];
		if (item.songId != songId)
			continue;
		res.position = i + 1;
		if (user.isEmpty() || hasUser(item, user)) {
			res.status = ReqStatus::Duplicate;
			return res;
		}
		if (limited) {
			res.status = ReqStatus::UserLimit;
			return res;
		}
		item.requests.append({user, source, now});
		res.status = ReqStatus::Joined;
		emit changed();
		return res;
	}

	if (limited) {
		res.status = ReqStatus::UserLimit;
		return res;
	}
	if (!force && m_settings.maxQueue > 0 && m_queue.size() >= m_settings.maxQueue) {
		res.status = ReqStatus::QueueFull;
		return res;
	}
	QueueItem item;
	item.id = m_nextQueueId++;
	item.songId = s->id;
	item.title = s->title;
	item.artist = s->artist;
	item.at = now;
	if (!user.isEmpty())
		item.requests.append({user, source, now});
	m_queue.append(item);
	res.status = ReqStatus::Added;
	res.position = m_queue.size();
	emit changed();
	return res;
}

QList<QPair<int, QueueItem>> Store::requestsOf(const QString &user) const
{
	QList<QPair<int, QueueItem>> out;
	if (user.isEmpty())
		return out;
	for (int i = 0; i < m_queue.size(); i++)
		if (hasUser(m_queue[i], user))
			out.append({i + 1, m_queue[i]});
	return out;
}

bool Store::removeLastRequest(const QString &user, QueueItem *removed)
{
	int best = -1;
	QDateTime bestAt;
	for (int i = 0; i < m_queue.size(); i++)
		for (const Request &r : m_queue[i].requests)
			if (r.user.compare(user, Qt::CaseInsensitive) == 0 && (best < 0 || r.at >= bestAt)) {
				best = i;
				bestAt = r.at;
			}
	if (best < 0)
		return false;
	if (removed)
		*removed = m_queue[best];
	QueueItem &item = m_queue[best];
	for (int i = item.requests.size() - 1; i >= 0; i--)
		if (item.requests[i].user.compare(user, Qt::CaseInsensitive) == 0)
			item.requests.removeAt(i);
	if (item.requests.isEmpty())
		m_queue.removeAt(best); // nadie mas la habia pedido
	emit changed();
	return true;
}

// ------------------------------------------------------ cola / sonando ahora

void Store::returnPlayingToQueue()
{
	if (!m_playing.valid())
		return;
	// Si la habia pedido alguien vuelve al principio de la cola; si la eligio
	// el streamer sin pedido, simplemente deja de sonar.
	if (!m_playing.requests.isEmpty())
		m_queue.prepend(m_playing);
	m_playing = QueueItem();
}

void Store::playQueueItem(int queueId)
{
	for (int i = 0; i < m_queue.size(); i++) {
		if (m_queue[i].id != queueId)
			continue;
		const QueueItem item = m_queue.takeAt(i);
		returnPlayingToQueue();
		m_playing = item;
		emit changed();
		return;
	}
}

void Store::playSong(int songId)
{
	const Song *s = song(songId);
	if (!s)
		return;
	if (m_playing.valid() && m_playing.songId == songId)
		return;
	// si ya estaba pedida, se toca ese pedido (conserva quien la pidio)
	for (const QueueItem &q : m_queue)
		if (q.songId == songId) {
			playQueueItem(q.id);
			return;
		}
	returnPlayingToQueue();
	QueueItem item;
	item.id = m_nextQueueId++;
	item.songId = s->id;
	item.title = s->title;
	item.artist = s->artist;
	item.at = QDateTime::currentDateTimeUtc();
	m_playing = item;
	emit changed();
}

void Store::registerPlay(const QueueItem &item)
{
	const QDateTime now = QDateTime::currentDateTimeUtc();
	if (Song *s = songPtr(item.songId)) {
		s->timesPlayed++;
		s->lastPlayed = now;
	}
	HistoryItem h;
	h.id = m_nextHistoryId++;
	h.songId = item.songId;
	h.title = item.title;
	h.artist = item.artist;
	h.requesters = item.requesters();
	h.playedAt = now;
	m_history.prepend(h);
	while (m_history.size() > kMaxHistory)
		m_history.removeLast();
}

bool Store::finishPlaying()
{
	if (!m_playing.valid())
		return false;
	registerPlay(m_playing);
	m_playing = QueueItem();
	emit changed();
	return true;
}

bool Store::next()
{
	const bool hadPlaying = m_playing.valid();
	if (hadPlaying) {
		registerPlay(m_playing);
		m_playing = QueueItem();
	}
	const bool hasNext = !m_queue.isEmpty();
	if (hasNext)
		m_playing = m_queue.takeFirst();
	if (hadPlaying || hasNext)
		emit changed();
	return hasNext;
}

void Store::skipPlaying()
{
	if (!m_playing.valid())
		return;
	m_playing = QueueItem();
	emit changed();
}

void Store::removeQueueItem(int queueId)
{
	for (int i = 0; i < m_queue.size(); i++)
		if (m_queue[i].id == queueId) {
			m_queue.removeAt(i);
			emit changed();
			return;
		}
}

void Store::moveQueueItem(int queueId, int delta)
{
	for (int i = 0; i < m_queue.size(); i++) {
		if (m_queue[i].id != queueId)
			continue;
		const int to = qBound(0, i + delta, int(m_queue.size()) - 1);
		if (to != i) {
			m_queue.move(i, to);
			emit changed();
		}
		return;
	}
}

void Store::clearQueue()
{
	if (m_queue.isEmpty())
		return;
	m_queue.clear();
	emit changed();
}

void Store::clearHistory()
{
	if (m_history.isEmpty())
		return;
	m_history.clear();
	emit changed();
}

void Store::setSettings(const Settings &settings)
{
	m_settings = settings;
	emit settingsChanged();
	emit changed();
}

void Store::setRequestsOpen(bool open)
{
	if (m_settings.requestsOpen == open)
		return;
	m_settings.requestsOpen = open;
	emit settingsChanged();
	emit changed();
}

void Store::showList()
{
	emit listRequested();
}

// -------------------------------------------------------------- persistencia

static QString iso(const QDateTime &dt)
{
	return dt.isValid() ? dt.toUTC().toString(Qt::ISODateWithMs) : QString();
}

static QDateTime fromIso(const QJsonValue &v)
{
	const QString s = v.toString();
	return s.isEmpty() ? QDateTime() : QDateTime::fromString(s, Qt::ISODateWithMs);
}

static QJsonObject queueToJson(const QueueItem &q)
{
	QJsonArray reqs;
	for (const Request &r : q.requests)
		reqs.append(QJsonObject{{"user", r.user}, {"source", r.source}, {"at", iso(r.at)}});
	return QJsonObject{{"id", q.id},         {"songId", q.songId}, {"title", q.title},
			   {"artist", q.artist}, {"requests", reqs},   {"at", iso(q.at)}};
}

static QueueItem queueFromJson(const QJsonObject &o)
{
	QueueItem q;
	q.id = o.value("id").toInt();
	q.songId = o.value("songId").toInt();
	q.title = o.value("title").toString();
	q.artist = o.value("artist").toString();
	q.at = fromIso(o.value("at"));
	for (const QJsonValue &v : o.value("requests").toArray()) {
		const QJsonObject r = v.toObject();
		q.requests.append({r.value("user").toString(), r.value("source").toString(), fromIso(r.value("at"))});
	}
	return q;
}

QJsonObject Store::toJson() const
{
	QJsonArray songs;
	for (const Song &s : m_songs)
		songs.append(QJsonObject{{"id", s.id},
					 {"title", s.title},
					 {"artist", s.artist},
					 {"categories", QJsonArray::fromStringList(s.categories)},
					 {"active", s.active},
					 {"timesPlayed", s.timesPlayed},
					 {"lastPlayed", iso(s.lastPlayed)},
					 {"createdAt", iso(s.createdAt)},
					 {"note", s.note}});
	QJsonArray queue;
	for (const QueueItem &q : m_queue)
		queue.append(queueToJson(q));
	QJsonArray history;
	for (const HistoryItem &h : m_history)
		history.append(QJsonObject{{"id", h.id},
					   {"songId", h.songId},
					   {"title", h.title},
					   {"artist", h.artist},
					   {"requesters", QJsonArray::fromStringList(h.requesters)},
					   {"playedAt", iso(h.playedAt)}});
	const Settings &c = m_settings;
	const QJsonObject settings{{"requestsOpen", c.requestsOpen}, {"maxQueue", c.maxQueue},
				   {"perUser", c.perUser},           {"allowOffList", c.allowOffList},
				   {"replies", c.replies},           {"listSeconds", c.listSeconds},
				   {"channel", c.channel},           {"botUser", c.botUser},
				   {"botToken", c.botToken}};
	QJsonObject root{{"version", 1},        {"songs", songs},
			 {"queue", queue},      {"history", history},
			 {"settings", settings}, {"nextSongId", m_nextSongId},
			 {"nextQueueId", m_nextQueueId}, {"nextHistoryId", m_nextHistoryId}};
	if (m_playing.valid())
		root.insert("playing", queueToJson(m_playing));
	return root;
}

void Store::fromJson(const QJsonObject &root)
{
	m_songs.clear();
	m_queue.clear();
	m_history.clear();
	m_playing = QueueItem();
	int maxSong = 0, maxQueue = 0, maxHist = 0;

	for (const QJsonValue &v : root.value("songs").toArray()) {
		const QJsonObject o = v.toObject();
		Song s;
		s.id = o.value("id").toInt();
		s.title = o.value("title").toString();
		s.artist = o.value("artist").toString();
		for (const QJsonValue &c : o.value("categories").toArray())
			s.categories << c.toString();
		s.active = o.value("active").toBool(true);
		s.timesPlayed = o.value("timesPlayed").toInt();
		s.lastPlayed = fromIso(o.value("lastPlayed"));
		s.createdAt = fromIso(o.value("createdAt"));
		s.note = o.value("note").toString();
		if (s.id <= 0 || s.title.isEmpty())
			continue;
		maxSong = qMax(maxSong, s.id);
		m_songs.append(s);
	}
	for (const QJsonValue &v : root.value("queue").toArray()) {
		const QueueItem q = queueFromJson(v.toObject());
		if (!q.valid())
			continue;
		maxQueue = qMax(maxQueue, q.id);
		m_queue.append(q);
	}
	if (root.contains("playing")) {
		m_playing = queueFromJson(root.value("playing").toObject());
		maxQueue = qMax(maxQueue, m_playing.id);
	}
	for (const QJsonValue &v : root.value("history").toArray()) {
		const QJsonObject o = v.toObject();
		HistoryItem h;
		h.id = o.value("id").toInt();
		h.songId = o.value("songId").toInt();
		h.title = o.value("title").toString();
		h.artist = o.value("artist").toString();
		for (const QJsonValue &r : o.value("requesters").toArray())
			h.requesters << r.toString();
		h.playedAt = fromIso(o.value("playedAt"));
		maxHist = qMax(maxHist, h.id);
		m_history.append(h);
	}
	const QJsonObject c = root.value("settings").toObject();
	Settings s;
	s.requestsOpen = c.value("requestsOpen").toBool(s.requestsOpen);
	s.maxQueue = c.value("maxQueue").toInt(s.maxQueue);
	s.perUser = c.value("perUser").toInt(s.perUser);
	s.allowOffList = c.value("allowOffList").toBool(s.allowOffList);
	s.replies = c.value("replies").toBool(s.replies);
	s.listSeconds = c.value("listSeconds").toInt(s.listSeconds);
	s.channel = c.value("channel").toString();
	s.botUser = c.value("botUser").toString();
	s.botToken = c.value("botToken").toString();
	m_settings = s;

	m_nextSongId = qMax(root.value("nextSongId").toInt(1), maxSong + 1);
	m_nextQueueId = qMax(root.value("nextQueueId").toInt(1), maxQueue + 1);
	m_nextHistoryId = qMax(root.value("nextHistoryId").toInt(1), maxHist + 1);
	emit settingsChanged();
	emit changed();
}

bool Store::load(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return false;
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return false;
	fromJson(doc.object());
	return true;
}

bool Store::save(const QString &path) const
{
	QSaveFile f(path); // escritura atomica: nunca deja el archivo a medias
	if (!f.open(QIODevice::WriteOnly))
		return false;
	f.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
	return f.commit();
}

} // namespace acsl
