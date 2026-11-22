// Pruebas del nucleo: busqueda, pedidos, cola, comandos de chat, CSV y chat IRC.
#include "chat/twitch-chat.hpp"
#include "core/bot.hpp"
#include "core/csv.hpp"
#include "core/matcher.hpp"
#include "core/store.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

using namespace acsl;

static int g_failed = 0, g_run = 0;
#define CHECK(cond)                                                              \
	do {                                                                     \
		g_run++;                                                         \
		if (!(cond)) {                                                   \
			g_failed++;                                              \
			std::printf("FALLO %s:%d  %s\n", __FILE__, __LINE__, #cond); \
		}                                                                \
	} while (0)
#define CHECK_EQ(a, b)                                                                                   \
	do {                                                                                             \
		g_run++;                                                                                 \
		if (!((a) == (b))) {                                                                     \
			g_failed++;                                                                      \
			std::printf("FALLO %s:%d  %s == %s\n   obtuve:   %s\n   esperaba: %s\n", __FILE__, __LINE__, #a, #b, \
				    qPrintable(QVariant(a).toString()), qPrintable(QVariant(b).toString())); \
		}                                                                                        \
	} while (0)

static void fill(Store &s)
{
	s.addSong("Dancing Queen", "ABBA", {"Pop"});              // 1
	s.addSong("Gimme Gimme Gimme", "ABBA", {"Pop"});          // 2
	s.addSong("Take On Me", "A-HA", {"Rock"});                // 3
	s.addSong(QStringLiteral("Sof\u00eda"), QStringLiteral("\u00c1lvaro Soler")); // 4
	s.addSong("Mi Corazón Encantado", "Dragon Ball GT", {"Anime"}); // 5
	s.addSong("Can't Help Falling In Love", "Elvis Presley"); // 6
	s.addSong("Perfect", "Ed Sheeran");                       // 7
	s.addSong("The Scientist", "Coldplay");                   // 8
	s.addSong("1979", "The Smashing Pumpkins");               // 9
	s.addSong("Love Again", "Dua Lipa");                      // 10
}

static QString titleOf(Store &s, const QString &q)
{
	const int i = findBestSong(s.songs(), q);
	return i < 0 ? QString("<nada>") : s.songs()[i].title;
}

static void testMatcher()
{
	Store s;
	fill(s);
	CHECK_EQ(normalizeText(QStringLiteral("  Sof\u00cdA,  \u00c1lvaro! ")), QString("sofia alvaro"));
	CHECK_EQ(normalizeText("Can't Help"), QString("cant help"));
	CHECK_EQ(titleOf(s, "dancing queen"), QString("Dancing Queen"));
	CHECK_EQ(titleOf(s, "DANCING"), QString("Dancing Queen"));
	CHECK_EQ(titleOf(s, "sofia"), QStringLiteral("Sof\u00eda"));
	CHECK_EQ(titleOf(s, "corazon encantado"), QString("Mi Corazón Encantado"));
	CHECK_EQ(titleOf(s, "abba gimme"), QString("Gimme Gimme Gimme"));
	CHECK_EQ(titleOf(s, "cant help falling in love"), QString("Can't Help Falling In Love"));
	CHECK_EQ(titleOf(s, "take on me a-ha"), QString("Take On Me"));
	CHECK_EQ(titleOf(s, "scientist coldplay"), QString("The Scientist"));
	CHECK_EQ(titleOf(s, "sientist"), QString("The Scientist")); // errata
	CHECK_EQ(titleOf(s, "perfec"), QString("Perfect"));
	CHECK_EQ(titleOf(s, "dragon ball"), QString("Mi Corazón Encantado")); // por artista
	CHECK_EQ(titleOf(s, "despacito"), QString("<nada>"));
	CHECK_EQ(titleOf(s, "love"), QString("Love Again")); // empieza por "love" > lo contiene
	CHECK_EQ(searchSongs(s.songs(), "abba").size(), 2);
	CHECK_EQ(searchSongs(s.songs(), "anime").size(), 1); // por categoria
	CHECK_EQ(searchSongs(s.songs(), "").size(), 10);
}

static void testRequests()
{
	Store s;
	fill(s);
	Settings c = s.settings();
	c.perUser = 2;
	c.maxQueue = 4;
	s.setSettings(c);

	ReqResult r = s.request("ana", "3", "twitch");
	CHECK(r.status == ReqStatus::Added);
	CHECK_EQ(r.song.title, QString("Take On Me"));
	CHECK_EQ(r.position, 1);

	r = s.request("ana", "#1", "twitch");
	CHECK(r.status == ReqStatus::Added);
	CHECK_EQ(r.position, 2);

	r = s.request("ana", "perfect", "twitch");
	CHECK(r.status == ReqStatus::UserLimit);

	r = s.request("ANA", "take on me", "twitch"); // ya la pidio (sin distinguir mayusculas)
	CHECK(r.status == ReqStatus::Duplicate);

	r = s.request("beto", "take on me", "twitch"); // otro usuario se suma
	CHECK(r.status == ReqStatus::Joined);
	CHECK_EQ(r.position, 1);
	CHECK_EQ(s.queue().size(), 2);
	CHECK_EQ(s.queue()[0].requesters().join(","), QString("ana,beto"));

	r = s.request("beto", "no existe esta", "twitch");
	CHECK(r.status == ReqStatus::NotFound);
	r = s.request("beto", "   ", "twitch");
	CHECK(r.status == ReqStatus::Empty);

	// "1979" es un titulo; el numero 1979 no existe, asi que busca por texto
	r = s.request("caro", "1979", "twitch");
	CHECK(r.status == ReqStatus::Added);
	CHECK_EQ(r.song.artist, QString("The Smashing Pumpkins"));

	r = s.request("dani", "sofia", "twitch");
	CHECK(r.status == ReqStatus::Added);
	r = s.request("eli", "perfect", "twitch");
	CHECK(r.status == ReqStatus::QueueFull);
	r = s.request("", "perfect", "manual", true); // el streamer se salta los limites
	CHECK(r.status == ReqStatus::Added);
	CHECK_EQ(s.queue().size(), 5);

	s.setRequestsOpen(false);
	r = s.request("eli", "love again", "twitch");
	CHECK(r.status == ReqStatus::Closed);
	s.setRequestsOpen(true);

	// cancion desactivada: el publico no la puede pedir
	Song scientist = *s.song(8);
	scientist.active = false;
	s.updateSong(scientist);
	r = s.request("eli", "the scientist", "twitch");
	CHECK(r.status == ReqStatus::NotFound);
	r = s.request("eli", "8", "twitch");
	CHECK(r.status == ReqStatus::NotFound);

	// quitar el ultimo pedido
	QueueItem removed;
	CHECK(s.removeLastRequest("beto", &removed));
	CHECK_EQ(removed.title, QString("Take On Me"));
	CHECK_EQ(s.queue().size(), 5);                              // ana la sigue pidiendo
	CHECK_EQ(s.queue()[0].requesters().join(","), QString("ana"));
	CHECK(s.removeLastRequest("dani"));
	CHECK_EQ(s.queue().size(), 4);                              // nadie mas: sale de la cola
	CHECK(!s.removeLastRequest("nadie"));
}

static void testQueueFlow()
{
	Store s;
	fill(s);
	s.request("ana", "1", "twitch");
	s.request("beto", "2", "twitch");
	s.request("caro", "3", "twitch");
	CHECK(!s.playing().valid());

	CHECK(s.next()); // empieza la primera
	CHECK_EQ(s.playing().title, QString("Dancing Queen"));
	CHECK_EQ(s.queue().size(), 2);
	CHECK_EQ(s.history().size(), 0);

	ReqResult r = s.request("dani", "dancing queen", "twitch");
	CHECK(r.status == ReqStatus::AlreadyPlaying);

	// tocar otra del medio: la actual (pedida) vuelve al principio de la cola
	s.playQueueItem(s.queue()[1].id);
	CHECK_EQ(s.playing().title, QString("Take On Me"));
	CHECK_EQ(s.queue()[0].title, QString("Dancing Queen"));
	CHECK_EQ(s.queue().size(), 2);

	CHECK(s.finishPlaying());
	CHECK(!s.playing().valid());
	CHECK_EQ(s.history().size(), 1);
	CHECK_EQ(s.history()[0].requesters.join(","), QString("caro"));
	CHECK_EQ(s.song(3)->timesPlayed, 1);
	CHECK(s.song(3)->lastPlayed.isValid());
	CHECK(!s.finishPlaying());

	// tocar directo desde la biblioteca, sin pedido
	s.playSong(7);
	CHECK_EQ(s.playing().title, QString("Perfect"));
	CHECK(s.playing().requesters().isEmpty());
	s.playSong(2); // ya estaba pedida por beto: conserva el pedido
	CHECK_EQ(s.playing().requesters().join(","), QString("beto"));
	CHECK_EQ(s.queue().size(), 1); // "Perfect" no vuelve a la cola: nadie la pidio

	s.moveQueueItem(s.queue()[0].id, 5);
	s.skipPlaying();
	CHECK(!s.playing().valid());
	CHECK_EQ(s.song(2)->timesPlayed, 0); // saltada no cuenta
	CHECK(s.next());
	CHECK(!s.next()); // cola vacia: termina la actual y no queda nada
	CHECK(!s.playing().valid());
	CHECK_EQ(s.history().size(), 2);

	// editar una cancion actualiza lo que hay en cola
	s.request("ana", "10", "twitch");
	Song love = *s.song(10);
	love.title = "Love Again (acustico)";
	s.updateSong(love);
	CHECK_EQ(s.queue()[0].title, QString("Love Again (acustico)"));
}

static void testPersistence()
{
	QTemporaryDir dir;
	const QString path = dir.filePath("songlist.json");
	{
		Store s;
		fill(s);
		s.request("ana", "1", "twitch");
		s.request("beto", "5", "twitch");
		s.next();
		s.next();
		Settings c = s.settings();
		c.channel = "micanal";
		c.perUser = 3;
		s.setSettings(c);
		s.setRequestsOpen(false);
		CHECK(s.save(path));
	}
	Store s;
	CHECK(s.load(path));
	CHECK_EQ(s.songs().size(), 10);
	CHECK_EQ(s.playing().title, QString("Mi Corazón Encantado"));
	CHECK_EQ(s.playing().requesters().join(","), QString("beto"));
	CHECK_EQ(s.history().size(), 1);
	CHECK_EQ(s.song(1)->timesPlayed, 1);
	CHECK_EQ(s.settings().channel, QString("micanal"));
	CHECK_EQ(s.settings().perUser, 3);
	CHECK(!s.settings().requestsOpen);
	CHECK_EQ(s.addSong("Nueva", "Alguien"), 11); // los numeros no se reutilizan
	CHECK(!s.load(dir.filePath("no-existe.json")));
}

static void testCsv()
{
	// export de StreamerSonglist
	const QString ssl = "id,title,artist,active,capo,chords,comment,lyrics,tabs,minAmount,bypassRequestLimits,attributeNames,attributeInsertType,createdAt,lastActivation,duration\n"
			    "4628547,Dancing Queen,ABBA,true,,,,,,0,false,Rock|Pop,replace,2024-07-19T15:39:45.953228Z,2024-07-19T15:39:45.953Z,\n"
			    "4628600,\"Hello, Goodbye\",\"The \"\"Beatles\"\"\",false,,,nota,,,0,false,,replace,,,\n";
	QList<Song> songs = parseSongsCsv(ssl);
	CHECK_EQ(songs.size(), 2);
	CHECK_EQ(songs[0].title, QString("Dancing Queen"));
	CHECK_EQ(songs[0].categories.join(","), QString("Rock,Pop"));
	CHECK(songs[0].active);
	CHECK_EQ(songs[1].title, QString("Hello, Goodbye"));
	CHECK_EQ(songs[1].artist, QString("The \"Beatles\""));
	CHECK(!songs[1].active);
	CHECK_EQ(songs[1].note, QString("nota"));

	// Excel en espanol: punto y coma, cabeceras en espanol, BOM
	songs = parseSongsCsv(QStringLiteral("\uFEFFT\u00edtulo;Artista;Categor\u00edas\r\nSof\u00eda;\u00c1lvaro Soler;Pop, Latino\r\n\r\n"));
	CHECK_EQ(songs.size(), 1);
	CHECK_EQ(songs[0].artist, QStringLiteral("\u00c1lvaro Soler"));
	CHECK_EQ(songs[0].categories.join("|"), QString("Pop|Latino"));

	// sin cabeceras y texto plano
	songs = parseSongsCsv("Perfect,Ed Sheeran\nFaded,Alan Walker\n");
	CHECK_EQ(songs.size(), 2);
	CHECK_EQ(songs[1].artist, QString("Alan Walker"));
	songs = parseSongsCsv("Take On Me - A-HA\nHabanera\n");
	CHECK_EQ(songs.size(), 2);
	CHECK_EQ(songs[0].title, QString("Take On Me"));
	CHECK_EQ(songs[0].artist, QString("A-HA"));
	CHECK_EQ(songs[1].title, QString("Habanera"));

	// ida y vuelta, sin duplicar al reimportar
	Store s;
	fill(s);
	const QString csv = songsToCsv(s.songs());
	Store s2;
	int updated = 0;
	CHECK_EQ(s2.importSongs(parseSongsCsv(csv), &updated), 10);
	CHECK_EQ(s2.song(5)->title, QString("Mi Corazón Encantado"));
	CHECK_EQ(s2.song(5)->categories.join(","), QString("Anime"));
	CHECK_EQ(s2.importSongs(parseSongsCsv(csv), &updated), 0);
	CHECK_EQ(updated, 10);
	CHECK_EQ(s2.songs().size(), 10);
}

static void testBot()
{
	Store s;
	fill(s);
	Bot bot(&s, [](const char *key) {
		const QString k = QString::fromLatin1(key);
		if (k == "Chat.Added") return QString("ADDED %1|%2|%3");
		if (k == "Chat.Joined") return QString("JOINED %1|%2|%3");
		if (k == "Chat.Queue") return QString("QUEUE %1: %2");
		if (k == "Chat.Current") return QString("NOW %1");
		if (k == "Chat.CurrentBy") return QString("NOW %1 BY %2");
		if (k == "Chat.Removed") return QString("REMOVED %1|%2");
		if (k == "Chat.List") return QString("LIST %1");
		if (k == "Chat.Mine") return QString("MINE %1: %2");
		if (k == "Chat.UserLimit") return QString("LIMIT %1|%2");
		return k;
	});
	bot.setCooldownMs(0);
	int listShown = 0;
	QObject::connect(&s, &Store::listRequested, [&]() { listShown++; });

	ChatUser ana{"ana", "Ana", false, false};
	ChatUser mod{"moder", "Moder", true, false};

	CHECK_EQ(bot.handle(ana, "hola a todos"), QString());
	CHECK_EQ(bot.handle(ana, "!comandoraro"), QString());
	CHECK_EQ(bot.handle(ana, "!sr"), QString("Chat.Usage"));
	CHECK_EQ(bot.handle(ana, "!sr 1"), QString("ADDED Ana|Dancing Queen \u2014 ABBA|1"));
	CHECK_EQ(bot.handle(ana, "!SR   take on me  "), QString("ADDED Ana|Take On Me \u2014 A-HA|2"));
	CHECK_EQ(bot.handle(ana, "!pedir perfect"), QString("LIMIT Ana|2"));
	CHECK_EQ(bot.handle(mod, "!sr dancing"), QString("JOINED Moder|Dancing Queen \u2014 ABBA|1"));
	CHECK_EQ(bot.handle(ana, "!sr zzzz"), QString("Chat.NotFound"));
	CHECK_EQ(bot.handle(ana, "!lista"), QString("LIST 10"));
	CHECK_EQ(listShown, 1);
	CHECK_EQ(bot.handle(ana, "!cola"), QString("QUEUE 2: 1) Dancing Queen \u2014 ABBA \u00b7 2) Take On Me \u2014 A-HA"));
	CHECK_EQ(bot.handle(ana, "!mipedido"), QString("MINE Ana: 1) Dancing Queen \u2014 ABBA \u00b7 2) Take On Me \u2014 A-HA"));
	CHECK_EQ(bot.handle(ana, "!cancion"), QString("Chat.NoCurrent"));

	// comandos de moderacion: un viewer no puede
	CHECK_EQ(bot.handle(ana, "!cerrar"), QString());
	CHECK(s.settings().requestsOpen);
	CHECK_EQ(bot.handle(ana, "!tocada"), QString());
	CHECK(!s.playing().valid());
	CHECK_EQ(bot.handle(mod, "!tocada"), QString("NOW Dancing Queen \u2014 ABBA"));
	CHECK_EQ(bot.handle(ana, "!song"), QString("NOW Dancing Queen \u2014 ABBA BY Ana, Moder"));
	CHECK_EQ(bot.handle(mod, "!cerrar"), QString("Chat.ClosedNow"));
	CHECK_EQ(bot.handle(ana, "!sr sofia"), QString("Chat.Closed"));
	CHECK_EQ(bot.handle(mod, "!abrir"), QString("Chat.Opened"));
	CHECK_EQ(bot.handle(ana, "!quitar"), QString("REMOVED Ana|Take On Me \u2014 A-HA"));
	CHECK_EQ(bot.handle(ana, "!ws"), QString("Chat.NothingToRemove"));
	CHECK_EQ(bot.handle(ana, "!siguiente"), QString("Chat.QueueEmpty"));

	// antispam de comandos informativos
	Bot slow(&s, [](const char *key) { return QString::fromLatin1(key); });
	slow.setCooldownMs(60000);
	CHECK_EQ(slow.handle(ana, "!cola"), QString("Chat.QueueEmpty"));
	CHECK_EQ(slow.handle(mod, "!cola"), QString());
	CHECK(!slow.handle(ana, "!lista").isEmpty());
	CHECK_EQ(slow.handle(ana, "!lista"), QString());
	CHECK_EQ(listShown, 3); // la lista se muestra aunque no se responda
}

static void testIrc()
{
	ChatUser u;
	QString text;
	CHECK(TwitchChat::parsePrivmsg(
		"@badge-info=;badges=moderator/1,subscriber/6;color=#FF0000;display-name=Juan\\sP;mod=1;user-type=mod "
		":juanp!juanp@juanp.tmi.twitch.tv PRIVMSG #micanal :!sr dancing queen",
		&u, &text));
	CHECK_EQ(u.login, QString("juanp"));
	CHECK_EQ(u.display, QString("Juan P"));
	CHECK(u.moderator);
	CHECK(!u.broadcaster);
	CHECK_EQ(text, QString("!sr dancing queen"));

	CHECK(TwitchChat::parsePrivmsg("@badges=broadcaster/1;display-name=MiCanal :micanal!a@a.tmi.twitch.tv PRIVMSG #micanal :hola : mundo", &u, &text));
	CHECK(u.broadcaster);
	CHECK_EQ(text, QString("hola : mundo"));

	CHECK(TwitchChat::parsePrivmsg(":viewer!v@v.tmi.twitch.tv PRIVMSG #c :\x01" "ACTION !sr 3\x01", &u, &text));
	CHECK_EQ(text, QString("!sr 3"));
	CHECK_EQ(u.name(), QString("viewer"));

	CHECK(!TwitchChat::parsePrivmsg("PING :tmi.twitch.tv", &u, &text));
	CHECK(!TwitchChat::parsePrivmsg(":tmi.twitch.tv 001 justinfan123 :Welcome, GLHF!", &u, &text));
	CHECK(!TwitchChat::parsePrivmsg("@msg-id=x :tmi.twitch.tv NOTICE * :Login authentication failed", &u, &text));

	CHECK_EQ(TwitchChat::cleanChannel(" https://www.twitch.tv/MiCanal?x=1 "), QString("micanal"));
	CHECK_EQ(TwitchChat::cleanChannel("#Mi_Canal"), QString("mi_canal"));
	CHECK_EQ(TwitchChat::cleanChannel("canal con espacios"), QString());
	CHECK_EQ(TwitchChat::cleanChannel("x\r\nJOIN #otro"), QString()); // sin inyeccion de comandos
	CHECK_EQ(TwitchChat::cleanToken("oauth:abcdef1234567890"), QString("abcdef1234567890"));
	CHECK_EQ(TwitchChat::cleanToken("abc\r\nPRIVMSG"), QString());
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	// Utilidad: acsl-tests --import canciones.csv songlist.json
	if (argc == 4 && QString::fromLocal8Bit(argv[1]) == "--import") {
		QFile in(QString::fromLocal8Bit(argv[2]));
		if (!in.open(QIODevice::ReadOnly))
			return 2;
		Store s;
		const int added = s.importSongs(parseSongsCsv(QString::fromUtf8(in.readAll())));
		std::printf("%d canciones importadas\n", added);
		return s.save(QString::fromLocal8Bit(argv[3])) ? 0 : 3;
	}
	testMatcher();
	testRequests();
	testQueueFlow();
	testPersistence();
	testCsv();
	testBot();
	testIrc();
	std::printf("%d comprobaciones, %d fallos\n", g_run, g_failed);
	return g_failed ? 1 : 0;
}
