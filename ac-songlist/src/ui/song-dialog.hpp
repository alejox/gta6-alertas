#pragma once

#include "../core/model.hpp"

#include <QDialog>

class QCheckBox;
class QLineEdit;

namespace acsl {

// Ventana para agregar o editar una cancion de la biblioteca.
class SongDialog : public QDialog {
	Q_OBJECT
public:
	SongDialog(const Song &song, const QStringList &knownCategories, QWidget *parent = nullptr);
	Song song() const;

private:
	Song m_song;
	QLineEdit *m_title;
	QLineEdit *m_artist;
	QLineEdit *m_categories;
	QLineEdit *m_note;
	QCheckBox *m_active;
};

} // namespace acsl
