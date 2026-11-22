#include "matcher.hpp"

#include <algorithm>

namespace acsl {

QString normalizeText(const QString &text)
{
	const QString decomposed = text.normalized(QString::NormalizationForm_KD).toLower();
	QString out;
	out.reserve(decomposed.size());
	bool lastSpace = true;
	for (const QChar c : decomposed) {
		if (c.category() == QChar::Mark_NonSpacing || c.category() == QChar::Mark_SpacingCombining ||
		    c.category() == QChar::Mark_Enclosing)
			continue; // acentos y dieresis
		if (c.isLetterOrNumber()) {
			out.append(c);
			lastSpace = false;
		} else if (c == QLatin1Char('\'') || c == QChar(0x2019)) {
			continue; // "Can't" == "cant"
		} else if (!lastSpace) {
			out.append(QLatin1Char(' '));
			lastSpace = true;
		}
	}
	return out.trimmed();
}

static int editDistance(const QString &a, const QString &b, int limit)
{
	const int n = a.size(), m = b.size();
	if (qAbs(n - m) > limit)
		return limit + 1;
	QList<int> prev(m + 1), cur(m + 1);
	for (int j = 0; j <= m; j++)
		prev[j] = j;
	for (int i = 1; i <= n; i++) {
		cur[0] = i;
		int rowMin = cur[0];
		for (int j = 1; j <= m; j++) {
			const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
			cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
			rowMin = std::min(rowMin, cur[j]);
		}
		if (rowMin > limit)
			return limit + 1;
		std::swap(prev, cur);
	}
	return prev[m];
}

// 2 = palabra exacta, 1 = prefijo o errata pequena, 0 = nada
static int tokenHit(const QString &token, const QStringList &words)
{
	int best = 0;
	for (const QString &w : words) {
		if (w == token)
			return 2;
		if (token.size() >= 2 && w.startsWith(token))
			best = 1;
		else if (token.size() >= 4 && editDistance(token, w, token.size() >= 7 ? 2 : 1) <= (token.size() >= 7 ? 2 : 1))
			best = std::max(best, 1);
	}
	return best;
}

int matchScore(const Song &song, const QString &q)
{
	if (q.isEmpty())
		return 0;
	const QString title = normalizeText(song.title);
	const QString artist = normalizeText(song.artist);
	if (title.isEmpty())
		return 0;

	if (q == title)
		return 1000;
	if (!artist.isEmpty() && (q == title + QLatin1Char(' ') + artist || q == artist + QLatin1Char(' ') + title))
		return 990;
	if (title.startsWith(q))
		return 900 - std::min(100, int(title.size() - q.size()));
	if (title.contains(q))
		return 780 - std::min(80, int(title.size() - q.size()));

	const QString both = title + QLatin1Char(' ') + artist;
	if (both.contains(q) || (artist + QLatin1Char(' ') + title).contains(q))
		return 700 - std::min(80, int(both.size() - q.size()));

	const QStringList tokens = q.split(QLatin1Char(' '), Qt::SkipEmptyParts);
	const QStringList titleWords = title.split(QLatin1Char(' '), Qt::SkipEmptyParts);
	const QStringList artistWords = artist.split(QLatin1Char(' '), Qt::SkipEmptyParts);
	int points = 0, titleHits = 0;
	for (const QString &t : tokens) {
		const int inTitle = tokenHit(t, titleWords);
		const int inArtist = tokenHit(t, artistWords);
		if (!inTitle && !inArtist)
			return 0; // cada palabra escrita tiene que aparecer en algun sitio
		points += std::max(inTitle, inArtist);
		if (inTitle)
			titleHits++;
	}
	if (titleHits == 0 && tokens.size() < artistWords.size())
		return 300 + points * 10; // solo acerto parte del artista: coincidencia debil
	const int coverage = titleWords.isEmpty() ? 0 : (100 * titleHits) / titleWords.size();
	return 400 + std::min(200, points * 25) + coverage;
}

int findBestSong(const QList<Song> &songs, const QString &query, bool includeInactive)
{
	const QString q = normalizeText(query);
	int best = -1, bestScore = 0;
	for (int i = 0; i < songs.size(); i++) {
		if (!songs[i].active && !includeInactive)
			continue;
		const int s = matchScore(songs[i], q);
		if (s > bestScore) {
			bestScore = s;
			best = i;
		}
	}
	return bestScore >= 300 ? best : -1;
}

QList<int> searchSongs(const QList<Song> &songs, const QString &query)
{
	const QString q = normalizeText(query);
	QList<int> out;
	if (q.isEmpty()) {
		for (int i = 0; i < songs.size(); i++)
			out << i;
		return out;
	}
	QList<QPair<int, int>> scored;
	for (int i = 0; i < songs.size(); i++) {
		int s = matchScore(songs[i], q);
		if (s == 0) {
			// el buscador del panel tambien mira categorias y numero
			if (QString::number(songs[i].id) == q)
				s = 950;
			else
				for (const QString &c : songs[i].categories)
					if (normalizeText(c).contains(q))
						s = 200;
		}
		if (s > 0)
			scored.append({s, i});
	}
	std::stable_sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
	for (const auto &p : scored)
		out << p.second;
	return out;
}

} // namespace acsl
