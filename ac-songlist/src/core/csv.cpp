#include "csv.hpp"

#include <QHash>

namespace acsl {

static QList<QStringList> parseRows(const QString &text, QChar sep)
{
	QList<QStringList> rows;
	QStringList row;
	QString field;
	bool quoted = false, any = false;
	const int n = text.size();
	for (int i = 0; i < n; i++) {
		const QChar c = text[i];
		if (quoted) {
			if (c == QLatin1Char('"')) {
				if (i + 1 < n && text[i + 1] == QLatin1Char('"')) {
					field.append(c);
					i++;
				} else {
					quoted = false;
				}
			} else {
				field.append(c);
			}
			continue;
		}
		if (c == QLatin1Char('"') && field.isEmpty()) {
			quoted = true;
			any = true;
		} else if (c == sep) {
			row << field;
			field.clear();
			any = true;
		} else if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
			if (c == QLatin1Char('\r') && i + 1 < n && text[i + 1] == QLatin1Char('\n'))
				i++;
			if (any || !field.isEmpty()) {
				row << field;
				rows << row;
			}
			row.clear();
			field.clear();
			any = false;
		} else {
			field.append(c);
		}
	}
	if (any || !field.isEmpty()) {
		row << field;
		rows << row;
	}
	return rows;
}

static QChar detectSeparator(const QString &text)
{
	const QString firstLine = text.section(QLatin1Char('\n'), 0, 0);
	QChar best = QLatin1Char(',');
	int bestCount = 0;
	for (const QChar c : {QLatin1Char(','), QLatin1Char(';'), QLatin1Char('\t')}) {
		const int count = firstLine.count(c);
		if (count > bestCount) {
			bestCount = count;
			best = c;
		}
	}
	return best;
}

static QString headerKey(const QString &h)
{
	QString out;
	for (const QChar c : h.normalized(QString::NormalizationForm_KD).toLower())
		if (c.isLetterOrNumber() && c.unicode() < 128)
			out.append(c);
	return out;
}

enum Field { FTitle, FArtist, FCategories, FActive, FTimes, FLast, FNote, FCreated };

static const QHash<QString, Field> &headerMap()
{
	static const QHash<QString, Field> map{
		{"title", FTitle},           {"titulo", FTitle},        {"cancion", FTitle},
		{"song", FTitle},            {"nombre", FTitle},        {"name", FTitle},
		{"artist", FArtist},         {"artista", FArtist},      {"autor", FArtist},
		{"banda", FArtist},          {"attributenames", FCategories},
		{"attributes", FCategories}, {"categorias", FCategories}, {"categoria", FCategories},
		{"genero", FCategories},     {"generos", FCategories},  {"category", FCategories},
		{"categories", FCategories}, {"tags", FCategories},     {"etiquetas", FCategories},
		{"active", FActive},         {"activa", FActive},       {"activo", FActive},
		{"timesplayed", FTimes},     {"veces", FTimes},         {"vecestocada", FTimes},
		{"lastplayed", FLast},       {"ultimavez", FLast},      {"comment", FNote},
		{"comentario", FNote},       {"nota", FNote},           {"note", FNote},
		{"createdat", FCreated},
	};
	return map;
}

static QStringList splitCategories(const QString &value)
{
	QStringList out;
	const QChar sep = value.contains(QLatin1Char('|')) ? QLatin1Char('|') : QLatin1Char(',');
	for (const QString &part : value.split(sep, Qt::SkipEmptyParts)) {
		const QString name = part.simplified();
		if (!name.isEmpty() && !out.contains(name, Qt::CaseInsensitive))
			out << name;
	}
	return out;
}

static bool parseBool(const QString &v, bool fallback)
{
	const QString s = v.trimmed().toLower();
	if (s == "true" || s == "1" || s == "si" || s == "yes" || s == QStringLiteral("sí") || s == "x")
		return true;
	if (s == "false" || s == "0" || s == "no")
		return false;
	return fallback;
}

static QDateTime parseDate(const QString &v)
{
	const QString s = v.trimmed();
	if (s.isEmpty())
		return {};
	QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
	if (!dt.isValid())
		dt = QDateTime::fromString(s, Qt::ISODate);
	return dt;
}

QList<Song> parseSongsCsv(const QString &rawText, QStringList *warnings)
{
	QString text = rawText;
	if (text.startsWith(QChar(0xFEFF)))
		text.remove(0, 1); // BOM de Excel
	QList<Song> songs;
	const QList<QStringList> rows = parseRows(text, detectSeparator(text));
	if (rows.isEmpty())
		return songs;

	// cabeceras?
	QHash<int, Field> columns;
	for (int i = 0; i < rows[0].size(); i++) {
		auto it = headerMap().constFind(headerKey(rows[0][i]));
		if (it != headerMap().constEnd() && !columns.values().contains(*it))
			columns.insert(i, *it);
	}
	const bool hasHeader = columns.values().contains(FTitle);
	int start = 1;
	if (!hasHeader) {
		columns.clear();
		columns.insert(0, FTitle);
		columns.insert(1, FArtist);
		columns.insert(2, FCategories);
		start = 0;
	}

	for (int r = start; r < rows.size(); r++) {
		const QStringList &row = rows[r];
		Song s;
		for (auto it = columns.constBegin(); it != columns.constEnd(); ++it) {
			if (it.key() >= row.size())
				continue;
			const QString v = row[it.key()];
			switch (it.value()) {
			case FTitle: s.title = v.simplified(); break;
			case FArtist: s.artist = v.simplified(); break;
			case FCategories: s.categories = splitCategories(v); break;
			case FActive: s.active = parseBool(v, true); break;
			case FTimes: s.timesPlayed = qMax(0, v.trimmed().toInt()); break;
			case FLast: s.lastPlayed = parseDate(v); break;
			case FNote: s.note = v.trimmed(); break;
			case FCreated: s.createdAt = parseDate(v); break;
			}
		}
		// texto plano de una sola columna: "Titulo - Artista"
		if (!hasHeader && row.size() == 1) {
			const int dash = s.title.indexOf(QStringLiteral(" - "));
			if (dash > 0) {
				s.artist = s.title.mid(dash + 3).simplified();
				s.title = s.title.left(dash).simplified();
			}
		}
		if (s.title.isEmpty()) {
			if (warnings)
				*warnings << QString::number(r + 1);
			continue;
		}
		songs.append(s);
	}
	return songs;
}

static QString escapeCsv(const QString &v)
{
	if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"')) || v.contains(QLatin1Char('\n')) ||
	    v.contains(QLatin1Char('\r'))) {
		QString q = v;
		q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
		return QLatin1Char('"') + q + QLatin1Char('"');
	}
	return v;
}

QString songsToCsv(const QList<Song> &songs)
{
	QStringList lines;
	lines << QStringLiteral("id,title,artist,attributeNames,active,timesPlayed,lastPlayed,comment");
	for (const Song &s : songs) {
		QStringList f;
		f << QString::number(s.id) << escapeCsv(s.title) << escapeCsv(s.artist)
		  << escapeCsv(s.categories.join(QLatin1Char('|'))) << (s.active ? "true" : "false")
		  << QString::number(s.timesPlayed)
		  << (s.lastPlayed.isValid() ? s.lastPlayed.toUTC().toString(Qt::ISODate) : QString())
		  << escapeCsv(s.note);
		lines << f.join(QLatin1Char(','));
	}
	return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace acsl
