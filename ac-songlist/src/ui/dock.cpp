#include "dock.hpp"
#include "song-dialog.hpp"
#include "../chat/twitch-chat.hpp"
#include "../core/csv.hpp"
#include "../core/matcher.hpp"
#include "../plugin-support.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace acsl {

namespace {

const int kIdRole = Qt::UserRole + 1;

QTableWidget *makeTable(const QStringList &headers, QWidget *parent)
{
	auto *t = new QTableWidget(0, int(headers.size()), parent);
	t->setHorizontalHeaderLabels(headers);
	t->verticalHeader()->hide();
	t->verticalHeader()->setDefaultSectionSize(t->fontMetrics().height() + 10);
	t->setSelectionBehavior(QAbstractItemView::SelectRows);
	t->setEditTriggers(QAbstractItemView::NoEditTriggers);
	t->setShowGrid(false);
	t->setAlternatingRowColors(true);
	t->setWordWrap(false);
	t->horizontalHeader()->setHighlightSections(false);
	t->horizontalHeader()->setStretchLastSection(true);
	return t;
}

// Texto emergente a prueba de HTML: lo que llega del chat (nombres, canciones fuera de
// lista) se muestra tal cual, nunca se interpreta como formato.
QString plainTip(const QStringList &lines)
{
	QStringList escaped;
	for (const QString &line : lines)
		escaped << line.toHtmlEscaped();
	return QStringLiteral("<p style='white-space:pre-wrap'>") + escaped.join(QStringLiteral("<br>")) +
	       QStringLiteral("</p>");
}

QTableWidgetItem *textItem(const QString &text, int id = 0)
{
	auto *item = new QTableWidgetItem(text);
	item->setData(kIdRole, id);
	if (!text.isEmpty())
		item->setToolTip(plainTip({text}));
	return item;
}

// Celda que se ordena como numero (o fecha) aunque muestre otro texto.
class SortItem : public QTableWidgetItem {
public:
	SortItem(const QString &text, qint64 key, int id) : QTableWidgetItem(text), m_key(key) { setData(kIdRole, id); }
	bool operator<(const QTableWidgetItem &other) const override
	{
		if (const auto *o = dynamic_cast<const SortItem *>(&other))
			return m_key < o->m_key;
		return QTableWidgetItem::operator<(other);
	}

private:
	qint64 m_key;
};

QString shortDate(const QDateTime &dt)
{
	if (!dt.isValid())
		return QStringLiteral("—");
	return QLocale().toString(dt.toLocalTime().date(), QLocale::ShortFormat);
}

QPushButton *button(const QString &text, const QString &tip, QWidget *parent)
{
	auto *b = new QPushButton(text, parent);
	b->setToolTip(tip);
	return b;
}

} // namespace

Dock::Dock(Store *store, TwitchChat *chat, QWidget *parent) : QWidget(parent), m_store(store), m_chat(chat)
{
	setMinimumWidth(320);

	// --- cabecera: estado del chat y pedidos abiertos/cerrados
	m_chatStatus = new QLabel(this);
	m_chatStatus->setTextFormat(Qt::RichText);
	m_requestsToggle = new QPushButton(this);
	m_requestsToggle->setCheckable(true);
	connect(m_requestsToggle, &QPushButton::clicked, this, [this](bool on) {
		if (m_store)
			m_store->setRequestsOpen(on);
	});
	auto *top = new QHBoxLayout;
	top->addWidget(m_chatStatus, 1);
	top->addWidget(m_requestsToggle);

	// --- sonando ahora
	auto *npBox = new QGroupBox(T("Dock.NowPlaying"), this);
	m_npTitle = new QLabel(npBox);
	QFont big = m_npTitle->font();
	big.setPointSizeF(big.pointSizeF() * 1.25);
	big.setBold(true);
	m_npTitle->setFont(big);
	m_npTitle->setWordWrap(true);
	m_npTitle->setTextFormat(Qt::PlainText);
	m_npSub = new QLabel(npBox);
	m_npSub->setWordWrap(true);
	m_npSub->setTextFormat(Qt::PlainText);
	m_btnNext = button(T("Dock.Next"), T("Dock.Next.Tip"), npBox);
	m_btnFinish = button(T("Dock.Finish"), T("Dock.Finish.Tip"), npBox);
	m_btnSkip = button(T("Dock.Skip"), T("Dock.Skip.Tip"), npBox);
	connect(m_btnNext, &QPushButton::clicked, this, [this]() {
		if (m_store)
			m_store->next();
	});
	connect(m_btnFinish, &QPushButton::clicked, this, [this]() {
		if (m_store)
			m_store->finishPlaying();
	});
	connect(m_btnSkip, &QPushButton::clicked, this, [this]() {
		if (m_store)
			m_store->skipPlaying();
	});
	auto *npButtons = new QHBoxLayout;
	npButtons->addWidget(m_btnNext, 1);
	npButtons->addWidget(m_btnFinish);
	npButtons->addWidget(m_btnSkip);
	auto *npLayout = new QVBoxLayout(npBox);
	npLayout->addWidget(m_npTitle);
	npLayout->addWidget(m_npSub);
	npLayout->addLayout(npButtons);

	// --- pestanas
	m_tabs = new QTabWidget(this);
	m_tabs->addTab(buildQueueTab(), T("Tab.Queue"));
	m_tabs->addTab(buildSongsTab(), T("Tab.Songs"));
	m_tabs->addTab(buildHistoryTab(), T("Tab.History"));
	m_tabs->addTab(buildSettingsTab(), T("Tab.Settings"));

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->addLayout(top);
	layout->addWidget(npBox);
	layout->addWidget(m_tabs, 1);

	// varias modificaciones seguidas = un solo repintado
	m_refreshTimer.setSingleShot(true);
	m_refreshTimer.setInterval(0);
	connect(&m_refreshTimer, &QTimer::timeout, this, &Dock::refresh);
	connect(store, &Store::changed, &m_refreshTimer, qOverload<>(&QTimer::start));
	connect(chat, &TwitchChat::stateChanged, this, &Dock::refreshChatStatus);

	loadSettings();
	refresh();
	refreshChatStatus();
	// biblioteca vacia: lo primero es crearla
	if (store->songs().isEmpty())
		m_tabs->setCurrentIndex(1);
}

// ------------------------------------------------------------------- cola

QWidget *Dock::buildQueueTab()
{
	auto *page = new QWidget(this);
	m_queue = makeTable({QStringLiteral("#"), T("Col.Song"), T("Col.RequestedBy")}, page);
	m_queue->setSelectionMode(QAbstractItemView::SingleSelection);
	m_queue->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_queue->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_queue->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	m_queue->horizontalHeader()->setStretchLastSection(false);

	m_btnPlayQueue = button(T("Queue.Play"), T("Queue.Play.Tip"), page);
	// flechas del propio tema: un boton de texto tan estrecho se queda sin sitio para el caracter
	m_btnUp = new QToolButton(page);
	m_btnUp->setArrowType(Qt::UpArrow);
	m_btnUp->setToolTip(T("Queue.Up.Tip"));
	m_btnDown = new QToolButton(page);
	m_btnDown->setArrowType(Qt::DownArrow);
	m_btnDown->setToolTip(T("Queue.Down.Tip"));
	m_btnRemoveQueue = button(T("Queue.Remove"), T("Queue.Remove.Tip"), page);
	m_btnClearQueue = button(T("Queue.Clear"), T("Queue.Clear.Tip"), page);

	auto play = [this]() {
		const int id = selectedQueueId();
		if (m_store && id)
			m_store->playQueueItem(id);
	};
	connect(m_btnPlayQueue, &QPushButton::clicked, this, play);
	connect(m_queue, &QTableWidget::cellDoubleClicked, this, play);
	connect(m_btnUp, &QToolButton::clicked, this, [this]() {
		if (m_store)
			m_store->moveQueueItem(selectedQueueId(), -1);
	});
	connect(m_btnDown, &QToolButton::clicked, this, [this]() {
		if (m_store)
			m_store->moveQueueItem(selectedQueueId(), +1);
	});
	connect(m_btnRemoveQueue, &QPushButton::clicked, this, [this]() {
		if (m_store)
			m_store->removeQueueItem(selectedQueueId());
	});
	connect(m_btnClearQueue, &QPushButton::clicked, this, [this]() {
		if (!m_store || m_store->queue().isEmpty())
			return;
		if (QMessageBox::question(this, T("Queue.Clear"), T("Queue.Clear.Confirm").arg(m_store->queue().size())) ==
		    QMessageBox::Yes)
			m_store->clearQueue();
	});
	connect(m_queue, &QTableWidget::itemSelectionChanged, this, [this]() {
		const bool any = selectedQueueId() != 0;
		m_btnPlayQueue->setEnabled(any);
		m_btnUp->setEnabled(any);
		m_btnDown->setEnabled(any);
		m_btnRemoveQueue->setEnabled(any);
	});

	auto *buttons = new QHBoxLayout;
	buttons->addWidget(m_btnPlayQueue, 1);
	buttons->addWidget(m_btnUp);
	buttons->addWidget(m_btnDown);
	buttons->addWidget(m_btnRemoveQueue);
	buttons->addWidget(m_btnClearQueue);

	auto *layout = new QVBoxLayout(page);
	layout->setContentsMargins(4, 6, 4, 4);
	layout->addWidget(m_queue, 1);
	layout->addLayout(buttons);
	return page;
}

int Dock::selectedQueueId() const
{
	const QList<QTableWidgetItem *> items = m_queue->selectedItems();
	return items.isEmpty() ? 0 : items.first()->data(kIdRole).toInt();
}

// -------------------------------------------------------------- biblioteca

QWidget *Dock::buildSongsTab()
{
	auto *page = new QWidget(this);

	// alta rapida: escribir titulo y artista y Enter
	m_addTitle = new QLineEdit(page);
	m_addTitle->setPlaceholderText(T("Songs.AddTitle"));
	m_addArtist = new QLineEdit(page);
	m_addArtist->setPlaceholderText(T("Songs.AddArtist"));
	auto *btnAdd = button(T("Songs.Add"), T("Songs.Add.Tip"), page);
	connect(btnAdd, &QPushButton::clicked, this, &Dock::quickAdd);
	connect(m_addTitle, &QLineEdit::returnPressed, this, [this]() {
		if (m_addArtist->text().isEmpty())
			m_addArtist->setFocus(); // Enter en el titulo pasa al artista
		else
			quickAdd();
	});
	connect(m_addArtist, &QLineEdit::returnPressed, this, &Dock::quickAdd);
	auto *addRow = new QHBoxLayout;
	addRow->addWidget(m_addTitle, 3);
	addRow->addWidget(m_addArtist, 2);
	addRow->addWidget(btnAdd);

	m_search = new QLineEdit(page);
	m_search->setPlaceholderText(T("Songs.Search"));
	m_search->setClearButtonEnabled(true);
	m_category = new QComboBox(page);
	connect(m_search, &QLineEdit::textChanged, this, &Dock::refreshSongs);
	connect(m_category, qOverload<int>(&QComboBox::currentIndexChanged), this, &Dock::refreshSongs);
	auto *filterRow = new QHBoxLayout;
	filterRow->addWidget(m_search, 3);
	filterRow->addWidget(m_category, 2);

	m_songs = makeTable({T("Col.Number"), T("Col.Title"), T("Col.Artist"), T("Col.Categories"), T("Col.Times")},
			    page);
	m_songs->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_songs->setSortingEnabled(true);
	m_songs->sortByColumn(0, Qt::AscendingOrder);
	// Titulo y artista se reparten el ancho que sobre: sin barra horizontal en un panel estrecho.
	QHeaderView *header = m_songs->horizontalHeader();
	header->setStretchLastSection(false);
	header->setMinimumSectionSize(34);
	header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(1, QHeaderView::Stretch);
	header->setSectionResizeMode(2, QHeaderView::Stretch);
	header->setSectionResizeMode(3, QHeaderView::Interactive);
	header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
	m_songs->setColumnWidth(3, 92);
	m_songs->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	connect(m_songs, &QTableWidget::cellDoubleClicked, this, &Dock::editSelectedSong);

	m_btnPlaySong = button(T("Songs.Play"), T("Songs.Play.Tip"), page);
	m_btnQueueSong = button(T("Songs.Queue"), T("Songs.Queue.Tip"), page);
	m_btnEdit = button(T("Songs.Edit"), T("Songs.Edit.Tip"), page);
	m_btnDelete = button(T("Songs.Delete"), T("Songs.Delete.Tip"), page);
	auto *more = new QToolButton(page);
	more->setText(QStringLiteral("CSV"));
	more->setToolTip(T("Songs.Csv.Tip"));
	more->setPopupMode(QToolButton::InstantPopup);
	auto *menu = new QMenu(more);
	connect(menu->addAction(T("Songs.Import")), &QAction::triggered, this, &Dock::importCsv);
	connect(menu->addAction(T("Songs.Export")), &QAction::triggered, this, &Dock::exportCsv);
	more->setMenu(menu);

	connect(m_btnPlaySong, &QPushButton::clicked, this, [this]() {
		const QList<int> ids = selectedSongIds();
		if (m_store && !ids.isEmpty())
			m_store->playSong(ids.first());
	});
	connect(m_btnQueueSong, &QPushButton::clicked, this, [this]() {
		if (!m_store)
			return;
		for (int id : selectedSongIds())
			m_store->requestSong(QString(), id, QStringLiteral("manual"), true);
	});
	connect(m_btnEdit, &QPushButton::clicked, this, &Dock::editSelectedSong);
	connect(m_btnDelete, &QPushButton::clicked, this, &Dock::deleteSelectedSongs);
	connect(m_songs, &QTableWidget::itemSelectionChanged, this, [this]() {
		const int n = int(selectedSongIds().size());
		m_btnPlaySong->setEnabled(n == 1);
		m_btnQueueSong->setEnabled(n >= 1);
		m_btnEdit->setEnabled(n == 1);
		m_btnDelete->setEnabled(n >= 1);
	});

	m_songCount = new QLabel(page);
	m_songCount->setEnabled(false);

	auto *buttons = new QHBoxLayout;
	buttons->addWidget(m_btnPlaySong, 1);
	buttons->addWidget(m_btnQueueSong, 1);
	buttons->addWidget(m_btnEdit);
	buttons->addWidget(m_btnDelete);
	buttons->addWidget(more);

	auto *layout = new QVBoxLayout(page);
	layout->setContentsMargins(4, 6, 4, 4);
	layout->addLayout(addRow);
	layout->addLayout(filterRow);
	layout->addWidget(m_songs, 1);
	layout->addWidget(m_songCount);
	layout->addLayout(buttons);
	return page;
}

QList<int> Dock::selectedSongIds() const
{
	QList<int> ids;
	const QModelIndexList rows = m_songs->selectionModel()->selectedRows();
	for (const QModelIndex &idx : rows)
		if (QTableWidgetItem *item = m_songs->item(idx.row(), 0))
			ids << item->data(kIdRole).toInt();
	return ids;
}

void Dock::quickAdd()
{
	if (!m_store)
		return;
	const QString title = m_addTitle->text().simplified();
	if (title.isEmpty()) {
		m_addTitle->setFocus();
		return;
	}
	const QString artist = m_addArtist->text().simplified();
	// evitar duplicados exactos
	for (const Song &s : m_store->songs())
		if (normalizeText(s.title) == normalizeText(title) && normalizeText(s.artist) == normalizeText(artist)) {
			QMessageBox::information(this, T("Songs.Add"), T("Songs.Duplicate").arg(s.display()).arg(s.id));
			return;
		}
	// la categoria filtrada se aplica a la cancion nueva: comodo para cargar por tandas
	QStringList categories;
	if (m_category->currentIndex() > 0)
		categories << m_category->currentText();
	m_store->addSong(title, artist, categories);
	m_addTitle->clear();
	m_addArtist->clear();
	m_addTitle->setFocus();
}

void Dock::editSelectedSong()
{
	if (!m_store)
		return;
	const QList<int> ids = selectedSongIds();
	if (ids.size() != 1)
		return;
	const Song *current = m_store->song(ids.first());
	if (!current)
		return;
	SongDialog dialog(*current, m_store->categories(), this);
	if (dialog.exec() == QDialog::Accepted && m_store)
		m_store->updateSong(dialog.song());
}

void Dock::deleteSelectedSongs()
{
	if (!m_store)
		return;
	const QList<int> ids = selectedSongIds();
	if (ids.isEmpty())
		return;
	QString question;
	if (ids.size() == 1) {
		const Song *s = m_store->song(ids.first());
		question = T("Songs.Delete.One").arg(s ? s->display() : QString());
	} else {
		question = T("Songs.Delete.Many").arg(ids.size());
	}
	if (QMessageBox::question(this, T("Songs.Delete"), question) == QMessageBox::Yes && m_store)
		m_store->removeSongs(ids);
}

void Dock::importCsv()
{
	const QString path = QFileDialog::getOpenFileName(this, T("Songs.Import"), QString(),
							  T("Songs.Csv.Filter"));
	if (path.isEmpty() || !m_store)
		return;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(this, T("Songs.Import"), T("Songs.Import.Failed"));
		return;
	}
	const QByteArray bytes = f.read(8 * 1024 * 1024);
	// UTF-8 es lo normal; si no cuadra (Excel antiguo), se lee con la codificacion del sistema
	QString text = QString::fromUtf8(bytes);
	if (text.contains(QChar(QChar::ReplacementCharacter)))
		text = QString::fromLocal8Bit(bytes);
	const QList<Song> songs = parseSongsCsv(text);
	if (songs.isEmpty()) {
		QMessageBox::warning(this, T("Songs.Import"), T("Songs.Import.Empty"));
		return;
	}
	int updated = 0;
	const int added = m_store->importSongs(songs, &updated);
	QMessageBox::information(this, T("Songs.Import"), T("Songs.Import.Done").arg(added).arg(updated));
}

void Dock::exportCsv()
{
	if (!m_store)
		return;
	QString path = QFileDialog::getSaveFileName(this, T("Songs.Export"), QStringLiteral("canciones.csv"),
						    T("Songs.Csv.Filter"));
	if (path.isEmpty())
		return;
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		QMessageBox::warning(this, T("Songs.Export"), T("Songs.Export.Failed"));
		return;
	}
	f.write("\xEF\xBB\xBF"); // BOM: Excel abre bien los acentos
	f.write(songsToCsv(m_store->songs()).toUtf8());
}

// ---------------------------------------------------------------- historial

QWidget *Dock::buildHistoryTab()
{
	auto *page = new QWidget(this);
	m_history = makeTable({T("Col.When"), T("Col.Song"), T("Col.RequestedBy")}, page);
	m_history->setSelectionMode(QAbstractItemView::SingleSelection);
	m_history->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_history->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_history->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	m_history->horizontalHeader()->setStretchLastSection(false);
	auto *clear = button(T("History.Clear"), T("History.Clear.Tip"), page);
	connect(clear, &QPushButton::clicked, this, [this]() {
		if (!m_store || m_store->history().isEmpty())
			return;
		if (QMessageBox::question(this, T("History.Clear"), T("History.Clear.Confirm")) == QMessageBox::Yes)
			m_store->clearHistory();
	});
	auto *row = new QHBoxLayout;
	row->addStretch(1);
	row->addWidget(clear);
	auto *layout = new QVBoxLayout(page);
	layout->setContentsMargins(4, 6, 4, 4);
	layout->addWidget(m_history, 1);
	layout->addLayout(row);
	return page;
}

// ------------------------------------------------------------------ ajustes

QWidget *Dock::buildSettingsTab()
{
	auto *inner = new QWidget(this);

	m_channel = new QLineEdit(inner);
	m_channel->setPlaceholderText(T("Settings.Channel.Hint"));

	m_perUser = new QSpinBox(inner);
	m_perUser->setRange(0, 50);
	m_perUser->setSpecialValueText(T("Settings.NoLimit"));
	m_maxQueue = new QSpinBox(inner);
	m_maxQueue->setRange(0, 500);
	m_maxQueue->setSpecialValueText(T("Settings.NoLimit"));
	m_listSeconds = new QSpinBox(inner);
	m_listSeconds->setRange(5, 600);
	m_listSeconds->setSuffix(QStringLiteral(" s"));
	m_offList = new QCheckBox(T("Settings.OffList"), inner);

	auto *rules = new QGroupBox(T("Settings.Rules"), inner);
	auto *rulesForm = new QFormLayout(rules);
	rulesForm->addRow(T("Settings.PerUser"), m_perUser);
	rulesForm->addRow(T("Settings.MaxQueue"), m_maxQueue);
	rulesForm->addRow(T("Settings.ListSeconds"), m_listSeconds);
	rulesForm->addRow(m_offList);

	auto *bot = new QGroupBox(T("Settings.Bot"), inner);
	m_replies = new QCheckBox(T("Settings.Replies"), bot);
	m_botUser = new QLineEdit(bot);
	m_botUser->setPlaceholderText(T("Settings.BotUser.Hint"));
	m_botToken = new QLineEdit(bot);
	m_botToken->setEchoMode(QLineEdit::Password);
	m_botToken->setPlaceholderText(QStringLiteral("oauth:..."));
	auto *botHelp = new QLabel(T("Settings.Bot.Help"), bot);
	botHelp->setWordWrap(true);
	botHelp->setEnabled(false);
	auto *botForm = new QFormLayout(bot);
	botForm->addRow(botHelp);
	botForm->addRow(m_replies);
	botForm->addRow(T("Settings.BotUser"), m_botUser);
	botForm->addRow(T("Settings.BotToken"), m_botToken);

	auto *save = button(T("Settings.Save"), T("Settings.Save.Tip"), inner);
	m_settingsSaved = new QLabel(inner);
	connect(save, &QPushButton::clicked, this, &Dock::saveSettings);
	connect(m_channel, &QLineEdit::returnPressed, this, &Dock::saveSettings);
	auto *saveRow = new QHBoxLayout;
	saveRow->addWidget(save);
	saveRow->addWidget(m_settingsSaved, 1);

	auto *help = new QLabel(T("Settings.Help"), inner);
	help->setWordWrap(true);
	help->setTextFormat(Qt::RichText);

	auto *channelForm = new QFormLayout;
	channelForm->addRow(T("Settings.Channel"), m_channel);

	auto *layout = new QVBoxLayout(inner);
	layout->setContentsMargins(6, 8, 6, 6);
	layout->addLayout(channelForm);
	layout->addWidget(rules);
	layout->addWidget(bot);
	layout->addLayout(saveRow);
	layout->addSpacing(8);
	layout->addWidget(help);
	layout->addStretch(1);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(inner);
	return scroll;
}

void Dock::loadSettings()
{
	if (!m_store)
		return;
	const Settings &c = m_store->settings();
	m_channel->setText(c.channel);
	m_perUser->setValue(c.perUser);
	m_maxQueue->setValue(c.maxQueue);
	m_listSeconds->setValue(c.listSeconds);
	m_offList->setChecked(c.allowOffList);
	m_replies->setChecked(c.replies);
	m_botUser->setText(c.botUser);
	m_botToken->setText(c.botToken);
}

void Dock::saveSettings()
{
	if (!m_store)
		return;
	const QString channel = TwitchChat::cleanChannel(m_channel->text());
	if (!m_channel->text().trimmed().isEmpty() && channel.isEmpty()) {
		m_settingsSaved->setText(T("Settings.BadChannel"));
		return;
	}
	const QString token = TwitchChat::cleanToken(m_botToken->text());
	if (!m_botToken->text().trimmed().isEmpty() && token.isEmpty()) {
		m_settingsSaved->setText(T("Settings.BadToken"));
		return;
	}
	Settings c = m_store->settings();
	c.channel = channel;
	c.perUser = m_perUser->value();
	c.maxQueue = m_maxQueue->value();
	c.listSeconds = m_listSeconds->value();
	c.allowOffList = m_offList->isChecked();
	c.replies = m_replies->isChecked();
	c.botUser = TwitchChat::cleanChannel(m_botUser->text());
	c.botToken = token;
	m_store->setSettings(c);
	m_channel->setText(c.channel);
	m_botUser->setText(c.botUser);
	m_settingsSaved->setText(T("Settings.Saved"));
	QTimer::singleShot(4000, m_settingsSaved, [label = m_settingsSaved]() { label->clear(); });
}

// ---------------------------------------------------------------- refrescos

void Dock::refresh()
{
	if (!m_store)
		return;
	{
		const QSignalBlocker block(m_requestsToggle);
		const bool open = m_store->settings().requestsOpen;
		m_requestsToggle->setChecked(open);
		m_requestsToggle->setText(open ? T("Dock.RequestsOpen") : T("Dock.RequestsClosed"));
		m_requestsToggle->setToolTip(T("Dock.Requests.Tip"));
	}
	refreshNowPlaying();
	refreshQueue();
	refreshSongs();
	refreshHistory();
	m_tabs->setTabText(0, m_store->queue().isEmpty() ? T("Tab.Queue")
							 : QStringLiteral("%1 (%2)").arg(T("Tab.Queue")).arg(m_store->queue().size()));
}

void Dock::refreshNowPlaying()
{
	const QueueItem &p = m_store->playing();
	if (p.valid()) {
		m_npTitle->setText(p.title);
		QStringList sub;
		if (!p.artist.isEmpty())
			sub << p.artist;
		const QStringList by = p.requesters();
		if (!by.isEmpty())
			sub << T("Overlay.RequestedBy").arg(by.join(QStringLiteral(", ")));
		m_npSub->setText(sub.join(QStringLiteral("  ·  ")));
		m_npSub->setVisible(!sub.isEmpty());
	} else {
		m_npTitle->setText(T("Dock.NothingPlaying"));
		m_npSub->setText(m_store->queue().isEmpty() ? T("Dock.NothingPlaying.Hint") : T("Dock.NothingPlaying.Queue"));
		m_npSub->setVisible(true);
	}
	m_npTitle->setEnabled(p.valid());
	m_btnFinish->setEnabled(p.valid());
	m_btnSkip->setEnabled(p.valid());
	m_btnNext->setEnabled(p.valid() || !m_store->queue().isEmpty());
}

void Dock::refreshQueue()
{
	const int keep = selectedQueueId();
	const QSignalBlocker block(m_queue);
	const QList<QueueItem> &queue = m_store->queue();
	m_queue->setRowCount(int(queue.size()));
	int selectRow = -1;
	for (int i = 0; i < queue.size(); i++) {
		const QueueItem &q = queue[i];
		m_queue->setItem(i, 0, textItem(QString::number(i + 1), q.id));
		m_queue->setItem(i, 1, textItem(q.display(), q.id));
		m_queue->setItem(i, 2, textItem(q.requesters().join(QStringLiteral(", ")), q.id));
		if (q.id == keep)
			selectRow = i;
	}
	if (selectRow >= 0)
		m_queue->selectRow(selectRow);
	else
		m_queue->clearSelection();
	const bool any = selectRow >= 0;
	m_btnPlayQueue->setEnabled(any);
	m_btnUp->setEnabled(any);
	m_btnDown->setEnabled(any);
	m_btnRemoveQueue->setEnabled(any);
	m_btnClearQueue->setEnabled(!queue.isEmpty());
}

void Dock::refreshSongs()
{
	if (!m_store)
		return;
	// categorias del desplegable
	{
		const QSignalBlocker block(m_category);
		const QString current = m_category->currentIndex() > 0 ? m_category->currentText() : QString();
		m_category->clear();
		m_category->addItem(T("Songs.AllCategories"));
		m_category->addItems(m_store->categories());
		const int idx = current.isEmpty() ? 0 : m_category->findText(current);
		m_category->setCurrentIndex(idx < 0 ? 0 : idx);
	}
	const QString category = m_category->currentIndex() > 0 ? m_category->currentText() : QString();

	const QList<int> keep = selectedSongIds();
	const QList<Song> &songs = m_store->songs();
	const QList<int> matches = searchSongs(songs, m_search->text());

	const QSignalBlocker block(m_songs);
	m_songs->setSortingEnabled(false);
	m_songs->clearContents();
	m_songs->setRowCount(0);
	int row = 0;
	for (int index : matches) {
		const Song &s = songs[index];
		if (!category.isEmpty() && !s.categories.contains(category, Qt::CaseInsensitive))
			continue;
		m_songs->insertRow(row);
		m_songs->setItem(row, 0, new SortItem(QString::number(s.id), s.id, s.id));
		m_songs->setItem(row, 1, textItem(s.title, s.id));
		m_songs->setItem(row, 2, textItem(s.artist, s.id));
		m_songs->setItem(row, 3, textItem(s.categories.join(QStringLiteral(", ")), s.id));
		m_songs->setItem(row, 4, new SortItem(QString::number(s.timesPlayed), s.timesPlayed, s.id));

		// el resto de datos, al pasar el raton
		QStringList tip;
		tip << QStringLiteral("%1. %2").arg(s.id).arg(s.display());
		if (!s.categories.isEmpty())
			tip << s.categories.join(QStringLiteral(", "));
		tip << T("Songs.Tip.Played").arg(s.timesPlayed).arg(shortDate(s.lastPlayed));
		if (!s.note.isEmpty())
			tip << s.note;
		if (!s.active) // desactivada: el publico no la ve ni la puede pedir
			tip << T("Songs.Inactive");
		const QString tooltip = plainTip(tip);
		for (int c = 0; c < 5; c++) {
			QTableWidgetItem *item = m_songs->item(row, c);
			item->setToolTip(tooltip);
			if (!s.active)
				item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
		}
		row++;
	}
	m_songs->setSortingEnabled(true);
	for (int r = 0; r < m_songs->rowCount(); r++)
		if (keep.contains(m_songs->item(r, 0)->data(kIdRole).toInt()))
			m_songs->selectionModel()->select(m_songs->model()->index(r, 0),
							  QItemSelectionModel::Select | QItemSelectionModel::Rows);

	const int n = int(selectedSongIds().size());
	m_btnPlaySong->setEnabled(n == 1);
	m_btnQueueSong->setEnabled(n >= 1);
	m_btnEdit->setEnabled(n == 1);
	m_btnDelete->setEnabled(n >= 1);
	m_songCount->setText(songs.isEmpty() ? T("Songs.EmptyHint")
					     : (row == songs.size() ? T("Songs.Count").arg(songs.size())
								    : T("Songs.CountFiltered").arg(row).arg(songs.size())));
	m_tabs->setTabText(1, songs.isEmpty() ? T("Tab.Songs")
					      : QStringLiteral("%1 (%2)").arg(T("Tab.Songs")).arg(songs.size()));
}

void Dock::refreshHistory()
{
	const QList<HistoryItem> &history = m_store->history();
	m_history->setRowCount(int(history.size()));
	for (int i = 0; i < history.size(); i++) {
		const HistoryItem &h = history[i];
		const QDateTime local = h.playedAt.toLocalTime();
		const QString when = local.date() == QDate::currentDate()
					     ? QLocale().toString(local.time(), QLocale::ShortFormat)
					     : QLocale().toString(local, QLocale::ShortFormat);
		const QString song =
			h.artist.isEmpty() ? h.title : h.title + QStringLiteral(" — ") + h.artist;
		m_history->setItem(i, 0, textItem(when));
		m_history->setItem(i, 1, textItem(song));
		m_history->setItem(i, 2, textItem(h.requesters.join(QStringLiteral(", "))));
	}
}

void Dock::refreshChatStatus()
{
	if (!m_chat)
		return;
	QString color, text;
	switch (m_chat->state()) {
	case TwitchChat::State::Off:
		color = QStringLiteral("#888888");
		text = T("Chat.Status.Off");
		break;
	case TwitchChat::State::Connecting:
		color = QStringLiteral("#e0a800");
		text = T("Chat.Status.Connecting").arg(m_chat->channel());
		break;
	case TwitchChat::State::Retrying:
		color = QStringLiteral("#e05555");
		text = T("Chat.Status.Retrying").arg(m_chat->channel());
		break;
	case TwitchChat::State::Connected:
		color = QStringLiteral("#3fb950");
		if (m_chat->canSend())
			text = T("Chat.Status.Bot").arg(m_chat->channel());
		else if (m_chat->authFailed())
			text = T("Chat.Status.BadToken").arg(m_chat->channel());
		else if (!m_chat->secure() && m_store && !m_store->settings().botToken.isEmpty())
			text = T("Chat.Status.NoTls").arg(m_chat->channel());
		else
			text = T("Chat.Status.ReadOnly").arg(m_chat->channel());
		break;
	}
	m_chatStatus->setText(QStringLiteral("<span style='color:%1'>●</span> %2").arg(color, text.toHtmlEscaped()));
	m_chatStatus->setToolTip(text);
}

} // namespace acsl
