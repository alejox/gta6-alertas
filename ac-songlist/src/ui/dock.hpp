// Panel acoplable dentro de OBS: cola de pedidos, biblioteca, historial y ajustes.
#pragma once

#include "../core/store.hpp"

#include <QPointer>
#include <QTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class QToolButton;

namespace acsl {

class TwitchChat;

class Dock : public QWidget {
	Q_OBJECT
public:
	Dock(Store *store, TwitchChat *chat, QWidget *parent = nullptr);

private:
	QWidget *buildQueueTab();
	QWidget *buildSongsTab();
	QWidget *buildHistoryTab();
	QWidget *buildSettingsTab();

	void refresh();
	void refreshNowPlaying();
	void refreshQueue();
	void refreshSongs();
	void refreshHistory();
	void refreshChatStatus();
	void loadSettings();
	void saveSettings();

	QList<int> selectedSongIds() const;
	int selectedQueueId() const;
	void quickAdd();
	void editSelectedSong();
	void deleteSelectedSongs();
	void importCsv();
	void exportCsv();

	QPointer<Store> m_store;
	QPointer<TwitchChat> m_chat;
	QTimer m_refreshTimer;

	QLabel *m_chatStatus;
	QPushButton *m_requestsToggle;
	QLabel *m_npTitle, *m_npSub;
	QPushButton *m_btnNext, *m_btnFinish, *m_btnSkip;
	QTabWidget *m_tabs;

	QTableWidget *m_queue;
	QPushButton *m_btnPlayQueue, *m_btnRemoveQueue, *m_btnClearQueue;
	QToolButton *m_btnUp, *m_btnDown;

	QLineEdit *m_addTitle, *m_addArtist, *m_search;
	QComboBox *m_category;
	QTableWidget *m_songs;
	QLabel *m_songCount;
	QPushButton *m_btnPlaySong, *m_btnQueueSong, *m_btnEdit, *m_btnDelete;

	QTableWidget *m_history;

	QLineEdit *m_channel, *m_botUser, *m_botToken;
	QCheckBox *m_replies, *m_offList;
	QSpinBox *m_perUser, *m_maxQueue, *m_listSeconds;
	QLabel *m_settingsSaved;
};

} // namespace acsl
