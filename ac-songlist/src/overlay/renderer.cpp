#include "renderer.hpp"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

namespace acsl {

namespace {

const QString kDash = QStringLiteral(" — ");
const QString kDot = QStringLiteral("  ·  ");

QFont makeFont(const OverlayStyle &s, double px, bool bold, double spacing = 0)
{
	QFont f;
	if (!s.fontFamily.isEmpty())
		f.setFamily(s.fontFamily);
	f.setPixelSize(qMax(8, int(px + 0.5)));
	f.setBold(bold);
	f.setItalic(s.italic);
	f.setHintingPreference(QFont::PreferNoHinting);
	if (spacing > 0)
		f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
	return f;
}

double textWidth(const QFont &f, const QString &t)
{
	return QFontMetricsF(f).horizontalAdvance(t);
}

QString elide(const QFont &f, const QString &t, double width)
{
	return QFontMetricsF(f).elidedText(t, Qt::ElideRight, qMax(0.0, width));
}

void drawCard(QPainter &p, const OverlayStyle &s, const QRectF &rect)
{
	if (s.backgroundOpacity <= 0)
		return;
	QColor bg = s.backgroundColor;
	bg.setAlphaF(qBound(0, s.backgroundOpacity, 100) / 100.0);
	QPainterPath path;
	const double r = qMin(double(s.radius), qMin(rect.width(), rect.height()) / 2.0);
	path.addRoundedRect(rect, r, r);
	p.fillPath(path, bg);
}

// Coloca un bloque de (w x h) dentro del lienzo segun la alineacion elegida.
QRectF placeCard(const OverlayStyle &s, double w, double h)
{
	w = qMin(w, double(s.width));
	h = qMin(h, double(s.height));
	double x = 0, y = 0;
	if (s.align == OverlayStyle::Center)
		x = (s.width - w) / 2.0;
	else if (s.align == OverlayStyle::Right)
		x = s.width - w;
	if (s.valign == OverlayStyle::Middle)
		y = (s.height - h) / 2.0;
	else if (s.valign == OverlayStyle::Bottom)
		y = s.height - h;
	return QRectF(x, y, w, h);
}

// Una linea con dos partes: principal (color normal) y secundaria (apagada).
void drawTwoTone(QPainter &p, const OverlayStyle &s, const QFont &f, const QRectF &r, const QString &main,
		 const QString &secondary)
{
	const QFontMetricsF fm(f);
	p.setFont(f);
	const double mainW = fm.horizontalAdvance(main);
	if (secondary.isEmpty() || mainW >= r.width() - fm.averageCharWidth() * 3) {
		p.setPen(s.textColor);
		p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elide(f, main, r.width()));
		return;
	}
	p.setPen(s.textColor);
	p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, main);
	QRectF rest = r;
	rest.setLeft(r.left() + mainW);
	p.setPen(s.mutedColor);
	p.drawText(rest, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elide(f, secondary, rest.width()));
}

void drawNowPlaying(QPainter &p, const OverlayStyle &s, const OverlayData &d)
{
	const bool idle = !d.hasPlaying;
	if (idle && s.idleText.trimmed().isEmpty())
		return;

	const QString title = idle ? s.idleText.trimmed() : d.title;
	QString sub;
	if (!idle) {
		if (s.showArtist && !d.artist.isEmpty())
			sub = d.artist;
		if (s.showRequester && !d.requesters.isEmpty()) {
			const QString by = d.lblRequestedBy.arg(d.requesters.join(QStringLiteral(", ")));
			sub = sub.isEmpty() ? by : sub + kDot + by;
		}
	}
	const QString label = (s.label.trimmed().isEmpty() ? d.lblNowPlaying : s.label.trimmed()).toUpper();
	const bool showLabel = s.showLabel && !idle && !label.isEmpty();

	const QFont titleFont = makeFont(s, s.fontSize, s.bold);
	const QFont labelFont = makeFont(s, s.fontSize * 0.36, true, s.fontSize * 0.07);
	const QFont subFont = makeFont(s, s.fontSize * 0.56, false);
	const QFontMetricsF tfm(titleFont), lfm(labelFont), sfm(subFont);

	const double pad = s.padding;
	const double bar = s.accentBar ? qMax(3.0, s.fontSize * 0.11) : 0;
	const double barGap = s.accentBar ? s.fontSize * 0.42 : 0;
	const double gap = s.fontSize * 0.14;

	double textH = tfm.height();
	if (showLabel)
		textH += lfm.height() + gap;
	if (!sub.isEmpty())
		textH += sfm.height() + gap * 0.6;

	const double maxTextW = s.width - pad * 2 - bar - barGap;
	double textW = textWidth(titleFont, title);
	if (showLabel)
		textW = qMax(textW, textWidth(labelFont, label));
	if (!sub.isEmpty())
		textW = qMax(textW, textWidth(subFont, sub));
	textW = qMin(textW + 2, maxTextW);

	const QRectF card = placeCard(s, textW + pad * 2 + bar + barGap, textH + pad * 2);
	drawCard(p, s, card);

	double x = card.left() + pad;
	double y = card.top() + pad;
	if (s.accentBar) {
		QPainterPath barPath;
		barPath.addRoundedRect(QRectF(x, y, bar, textH), bar / 2, bar / 2);
		p.fillPath(barPath, s.accentColor);
		x += bar + barGap;
	}
	const double w = card.right() - pad - x;

	if (showLabel) {
		p.setFont(labelFont);
		p.setPen(s.accentColor);
		p.drawText(QRectF(x, y, w, lfm.height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			   elide(labelFont, label, w));
		y += lfm.height() + gap;
	}
	p.setFont(titleFont);
	p.setPen(idle ? s.mutedColor : s.textColor);
	p.drawText(QRectF(x, y, w, tfm.height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
		   elide(titleFont, title, w));
	y += tfm.height() + gap * 0.6;
	if (!sub.isEmpty()) {
		p.setFont(subFont);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(x, y, w, sfm.height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			   elide(subFont, sub, w));
	}
}

// Cabecera comun de cola y lista. Devuelve la altura ocupada.
double drawHeader(QPainter &p, const OverlayStyle &s, const QRectF &inner, const QString &left, const QString &right)
{
	const QFont labelFont = makeFont(s, s.fontSize * 0.36, true, s.fontSize * 0.07);
	const QFont rightFont = makeFont(s, s.fontSize * 0.36, false);
	const QFontMetricsF lfm(labelFont);
	const double h = lfm.height();
	double rightW = 0;
	if (!right.isEmpty()) {
		rightW = qMin(textWidth(rightFont, right) + 2, inner.width() * 0.6);
		p.setFont(rightFont);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(inner.right() - rightW, inner.top(), rightW, h),
			   Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, elide(rightFont, right, rightW));
	}
	p.setFont(labelFont);
	p.setPen(s.accentColor);
	const double leftW = inner.width() - rightW - (rightW > 0 ? s.fontSize * 0.4 : 0);
	p.drawText(QRectF(inner.left(), inner.top(), leftW, h), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
		   elide(labelFont, left.toUpper(), leftW));

	const double lineY = inner.top() + h + s.fontSize * 0.2;
	QColor line = s.mutedColor;
	line.setAlphaF(line.alphaF() * 0.35);
	p.fillRect(QRectF(inner.left(), lineY, inner.width(), qMax(1.0, s.fontSize * 0.03)), line);
	return h + s.fontSize * 0.42;
}

void drawQueue(QPainter &p, const OverlayStyle &s, const OverlayData &d)
{
	if (d.queue.isEmpty() && s.hideWhenEmpty)
		return;

	const QFont rowFont = makeFont(s, s.fontSize * 0.6, s.bold);
	const QFont rowFontLight = makeFont(s, s.fontSize * 0.6, false);
	const QFont numFont = makeFont(s, s.fontSize * 0.6, true);
	const QFont byFont = makeFont(s, s.fontSize * 0.42, false);
	const QFontMetricsF rfm(rowFont);
	const double rowH = rfm.height() * 1.28;
	const double pad = s.padding;
	const int shown = qMin(int(d.queue.size()), qMax(1, s.rows));
	const int extra = d.queueTotal - shown;

	const QFont labelFont = makeFont(s, s.fontSize * 0.36, true, s.fontSize * 0.07);
	const double headerH = s.showLabel ? QFontMetricsF(labelFont).height() + s.fontSize * 0.42 : 0;
	double bodyH = shown > 0 ? shown * rowH : rowH;
	if (extra > 0)
		bodyH += QFontMetricsF(byFont).height() * 1.3;

	const QRectF card = placeCard(s, s.width, headerH + bodyH + pad * 2);
	drawCard(p, s, card);
	const QRectF inner = card.adjusted(pad, pad, -pad, -pad);
	double y = inner.top();

	if (s.showLabel) {
		const QString label = s.label.trimmed().isEmpty() ? d.lblQueue : s.label.trimmed();
		const QString right = !d.requestsOpen ? d.lblClosed : (d.queueTotal > 0 ? QString::number(d.queueTotal) : QString());
		y += drawHeader(p, s, inner, label, right);
	}

	if (shown == 0) {
		p.setFont(rowFontLight);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(inner.left(), y, inner.width(), rowH),
			   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			   elide(rowFontLight, d.requestsOpen ? d.lblEmptyQueue : d.lblClosed, inner.width()));
		return;
	}

	const double numW = s.showNumbers ? textWidth(numFont, QString::number(qMax(shown, 9))) + s.fontSize * 0.4 : 0;
	for (int i = 0; i < shown; i++) {
		const OverlayRow &row = d.queue[i];
		double x = inner.left();
		double right = inner.right();
		if (s.showNumbers) {
			p.setFont(numFont);
			p.setPen(s.accentColor);
			p.drawText(QRectF(x, y, numW, rowH), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
				   QString::number(i + 1));
			x += numW;
		}
		if (s.showRequester && !row.requester.isEmpty()) {
			const double byW = qMin(textWidth(byFont, row.requester) + 2, (right - x) * 0.38);
			p.setFont(byFont);
			p.setPen(s.mutedColor);
			p.drawText(QRectF(right - byW, y, byW, rowH), Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
				   elide(byFont, row.requester, byW));
			right -= byW + s.fontSize * 0.4;
		}
		const QString artist = s.showArtist && !row.artist.isEmpty() ? kDash + row.artist : QString();
		// el artista va en trazo fino para que el titulo destaque
		const QFontMetricsF fm(rowFont);
		const double titleW = fm.horizontalAdvance(row.title);
		const QRectF r(x, y, right - x, rowH);
		if (artist.isEmpty() || titleW >= r.width() - fm.averageCharWidth() * 3) {
			p.setFont(rowFont);
			p.setPen(s.textColor);
			p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elide(rowFont, row.title, r.width()));
		} else {
			p.setFont(rowFont);
			p.setPen(s.textColor);
			p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, row.title);
			QRectF rest = r;
			rest.setLeft(r.left() + titleW);
			p.setFont(rowFontLight);
			p.setPen(s.mutedColor);
			p.drawText(rest, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
				   elide(rowFontLight, artist, rest.width()));
		}
		y += rowH;
	}
	if (extra > 0) {
		p.setFont(byFont);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(inner.left() + numW, y, inner.width() - numW, QFontMetricsF(byFont).height() * 1.3),
			   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, d.lblMore.arg(extra));
	}
}

void drawSongList(QPainter &p, const OverlayStyle &s, const OverlayData &d)
{
	if (d.songs.isEmpty() && s.hideWhenEmpty)
		return;

	const int cols = qMax(1, s.columns);
	const int rows = qMax(1, s.rows);
	const QFont rowFont = makeFont(s, s.fontSize * 0.52, false);
	const QFont numFont = makeFont(s, s.fontSize * 0.52, true);
	const QFontMetricsF rfm(rowFont);
	const double rowH = rfm.height() * 1.3;
	const double pad = s.padding;

	const QFont labelFont = makeFont(s, s.fontSize * 0.36, true, s.fontSize * 0.07);
	const double headerH = s.showLabel ? QFontMetricsF(labelFont).height() + s.fontSize * 0.42 : 0;
	const int usedRows = d.pages > 1 ? rows : qMax(1, qMin(rows, int((d.songs.size() + cols - 1) / cols)));

	const QRectF card = placeCard(s, s.width, headerH + usedRows * rowH + pad * 2);
	drawCard(p, s, card);
	const QRectF inner = card.adjusted(pad, pad, -pad, -pad);
	double top = inner.top();

	if (s.showLabel) {
		QString label = s.label.trimmed().isEmpty() ? d.lblSongList : s.label.trimmed();
		if (d.songsTotal > 0)
			label += QStringLiteral("  ·  %1").arg(d.songsTotal);
		QString right = d.requestsOpen ? d.lblHowTo : d.lblClosed;
		if (d.pages > 1)
			right += QStringLiteral("    %1 / %2").arg(d.page + 1).arg(d.pages);
		top += drawHeader(p, s, inner, label, right);
	}

	if (d.songs.isEmpty()) {
		p.setFont(rowFont);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(inner.left(), top, inner.width(), rowH),
			   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, d.lblEmptyList);
		return;
	}

	int maxNumber = 9;
	for (const OverlayRow &row : d.songs)
		maxNumber = qMax(maxNumber, row.number);
	const double numW = s.showNumbers ? textWidth(numFont, QString::number(maxNumber)) + s.fontSize * 0.3 : 0;
	const double colGap = s.fontSize * 0.8;
	const double colW = (inner.width() - colGap * (cols - 1)) / cols;

	// se llena por columnas: se lee de arriba abajo, como una lista
	for (int i = 0; i < d.songs.size() && i < rows * cols; i++) {
		const OverlayRow &row = d.songs[i];
		const int c = i / rows, r = i % rows;
		double x = inner.left() + c * (colW + colGap);
		const double y = top + r * rowH;
		if (s.showNumbers) {
			p.setFont(numFont);
			p.setPen(s.accentColor);
			p.drawText(QRectF(x, y, numW - s.fontSize * 0.3, rowH),
				   Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, QString::number(row.number));
			x += numW;
		}
		const QString artist = s.showArtist && !row.artist.isEmpty() ? kDash + row.artist : QString();
		drawTwoTone(p, s, rowFont, QRectF(x, y, inner.left() + c * (colW + colGap) + colW - x, rowH), row.title,
			    artist);
	}
}

} // namespace

QImage renderOverlay(const OverlayStyle &style, const OverlayData &data, double opacity)
{
	OverlayStyle s = style;
	s.width = qBound(16, s.width, 4096);
	s.height = qBound(16, s.height, 4096);
	s.fontSize = qBound(10, s.fontSize, 400);

	QImage img(s.width, s.height, QImage::Format_RGBA8888_Premultiplied);
	img.fill(Qt::transparent);
	if (opacity <= 0.001)
		return img;

	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setRenderHint(QPainter::TextAntialiasing, true);
	p.setOpacity(qBound(0.0, opacity, 1.0));
	switch (s.mode) {
	case OverlayStyle::NowPlaying: drawNowPlaying(p, s, data); break;
	case OverlayStyle::Queue: drawQueue(p, s, data); break;
	case OverlayStyle::SongList: drawSongList(p, s, data); break;
	}
	p.end();
	return img;
}

} // namespace acsl
