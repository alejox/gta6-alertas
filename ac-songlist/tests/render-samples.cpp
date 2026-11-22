// Genera imagenes de muestra del overlay para revisar el diseno sin abrir OBS.
#include "overlay/renderer.hpp"

#include <QDir>
#include <QGuiApplication>
#include <QPainter>

using namespace acsl;

static OverlayData sampleData()
{
	OverlayData d;
	d.hasPlaying = true;
	d.title = QStringLiteral("Mi Coraz\u00f3n Encantado");
	d.artist = "Dragon Ball GT";
	d.requesters = {"luna_gamer"};
	d.queue = {{1, "Dancing Queen", "ABBA", "pianofan22"},
		   {2, "Take On Me", "A-HA", "luna_gamer, el_mati"},
		   {3, "The Girl Who Stole The Stars", "Chrono Cross", "un_nombre_de_usuario_largo"},
		   {4, QStringLiteral("Sof\u00eda"), QStringLiteral("\u00c1lvaro Soler"), "sofi_music"}};
	d.queueTotal = 7;
	const char *songs[][2] = {{"Summer Moved On", "a-ha"}, {"Dancing Queen", "ABBA"}, {"Gimme Gimme Gimme", "ABBA"},
				  {"Quest for Butter (Main Theme)", "Age Of Empires II - The Age Of Kings"},
				  {"Take On Me", "A-HA"}, {"Part of Your World", "Alan Menken"},
				  {"Un Mundo Ideal", "Alan Menken"}, {"La Bella y la Bestia", "Alan Menken"},
				  {"Faded", "Alan Walker"}, {"Frenesi", "Alberto Dominguez"},
				  {"Caminando en el aire", "Aled Jones"}, {"Sofia", "Alvaro Soler"},
				  {"El Fantasma de la Opera", "Andrew Lloyd Webber"}, {"Sayonara no Natsu", "Aoi Teshima"},
				  {"How Deep Is Your Love", "Bee Gees"}, {"Words", "Bee Gees"},
				  {"Just the Two of Us", "Bill Withers"}, {"Habanera", "Bizet"},
				  {"Sunny", "Bobby Hebb"}, {"Bed of Roses", "Bon Jovi"},
				  {"Count On Me", "Bruno Mars"}, {"Marry You", "Bruno Mars"},
				  {"Treasure", "Bruno Mars"}, {"Close To You", "Carpenters"}};
	int n = 1;
	for (auto &s : songs)
		d.songs.append({n++, s[0], s[1], {}});
	d.songsTotal = 141;
	d.page = 0;
	d.pages = 6;
	d.lblNowPlaying = "Sonando ahora";
	d.lblQueue = "Cola de pedidos";
	d.lblSongList = "Lista de canciones";
	d.lblRequestedBy = "pedida por %1";
	d.lblHowTo = QStringLiteral("Pide con !sr n\u00famero");
	d.lblMore = QStringLiteral("+%1 m\u00e1s");
	d.lblClosed = "Pedidos cerrados";
	d.lblEmptyQueue = "Sin pedidos";
	d.lblEmptyList = QStringLiteral("A\u00fan no hay canciones");
	return d;
}

// Fondo tipo "captura de juego" para ver el overlay como en un stream.
static void saveOnBackdrop(const QImage &overlay, const QString &path, bool light = false)
{
	QImage out(overlay.width() + 80, overlay.height() + 80, QImage::Format_ARGB32_Premultiplied);
	QPainter p(&out);
	QLinearGradient g(0, 0, out.width(), out.height());
	if (light) {
		g.setColorAt(0, QColor(214, 222, 235));
		g.setColorAt(1, QColor(240, 226, 208));
	} else {
		g.setColorAt(0, QColor(38, 52, 84));
		g.setColorAt(0.5, QColor(92, 54, 96));
		g.setColorAt(1, QColor(30, 34, 46));
	}
	p.fillRect(out.rect(), g);
	p.drawImage(40, 40, overlay);
	p.end();
	out.save(path);
}

int main(int argc, char **argv)
{
	QGuiApplication app(argc, argv);
	const QString dir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
	QDir().mkpath(dir);
	const OverlayData d = sampleData();

	OverlayStyle np;
	np.mode = OverlayStyle::NowPlaying;
	np.width = 900;
	np.height = 170;
	saveOnBackdrop(renderOverlay(np, d), dir + "/01-sonando.png");

	OverlayStyle np2 = np; // sin fondo, alineada a la derecha, otro acento
	np2.backgroundOpacity = 0;
	np2.align = OverlayStyle::Right;
	np2.accentColor = QColor(255, 196, 0);
	np2.fontSize = 46;
	saveOnBackdrop(renderOverlay(np2, d), dir + "/02-sonando-sin-fondo.png");

	OverlayStyle np3 = np; // claro
	np3.backgroundColor = QColor(255, 255, 255);
	np3.backgroundOpacity = 92;
	np3.textColor = QColor(20, 22, 28);
	np3.mutedColor = QColor(20, 22, 28, 160);
	np3.accentColor = QColor(225, 29, 72);
	np3.radius = 4;
	saveOnBackdrop(renderOverlay(np3, d), dir + "/03-sonando-claro.png", true);

	OverlayData longData = d; // titulo larguisimo: tiene que recortarse con "..."
	longData.title = "Quest for Butter (Main Theme) - Version extendida para piano y orquesta sinfonica";
	longData.artist = "Age Of Empires II - The Age Of Kings Definitive Edition Soundtrack";
	longData.requesters = {"un_nombre_de_usuario_largo", "otro_usuario_mas"};
	saveOnBackdrop(renderOverlay(np, longData), dir + "/04-sonando-largo.png");

	OverlayStyle q;
	q.mode = OverlayStyle::Queue;
	q.width = 640;
	q.height = 420;
	q.rows = 4;
	saveOnBackdrop(renderOverlay(q, d), dir + "/05-cola.png");

	OverlayData empty = d;
	empty.queue.clear();
	empty.queueTotal = 0;
	saveOnBackdrop(renderOverlay(q, empty), dir + "/06-cola-vacia.png");

	OverlayStyle list;
	list.mode = OverlayStyle::SongList;
	list.width = 1500;
	list.height = 560;
	list.rows = 8;
	list.columns = 3;
	saveOnBackdrop(renderOverlay(list, d), dir + "/07-lista.png");
	return 0;
}
