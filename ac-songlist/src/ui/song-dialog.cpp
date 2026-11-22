#include "song-dialog.hpp"
#include "../plugin-support.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace acsl {

SongDialog::SongDialog(const Song &song, const QStringList &knownCategories, QWidget *parent)
	: QDialog(parent), m_song(song)
{
	setWindowTitle(song.id > 0 ? T("Dialog.EditSong") : T("Dialog.AddSong"));
	setMinimumWidth(420);

	m_title = new QLineEdit(song.title, this);
	m_artist = new QLineEdit(song.artist, this);
	m_categories = new QLineEdit(song.categories.join(QStringLiteral(", ")), this);
	m_categories->setPlaceholderText(T("Dialog.CategoriesHint"));
	m_note = new QLineEdit(song.note, this);
	m_note->setPlaceholderText(T("Dialog.NoteHint"));
	m_active = new QCheckBox(T("Dialog.Active"), this);
	m_active->setChecked(song.active);

	auto *form = new QFormLayout;
	form->addRow(T("Col.Title"), m_title);
	form->addRow(T("Col.Artist"), m_artist);
	form->addRow(T("Col.Categories"), m_categories);
	if (!knownCategories.isEmpty()) {
		auto *known = new QLabel(T("Dialog.Existing").arg(knownCategories.join(QStringLiteral(", "))), this);
		known->setWordWrap(true);
		known->setEnabled(false);
		form->addRow(QString(), known);
	}
	form->addRow(T("Col.Note"), m_note);
	form->addRow(QString(), m_active);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
	ok->setEnabled(!song.title.trimmed().isEmpty());
	connect(m_title, &QLineEdit::textChanged, ok,
		[ok](const QString &text) { ok->setEnabled(!text.trimmed().isEmpty()); });
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(buttons);
	m_title->setFocus();
}

Song SongDialog::song() const
{
	Song s = m_song;
	s.title = m_title->text().simplified();
	s.artist = m_artist->text().simplified();
	s.categories.clear();
	for (const QString &part : m_categories->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
		const QString name = part.simplified();
		if (!name.isEmpty() && !s.categories.contains(name, Qt::CaseInsensitive))
			s.categories << name;
	}
	s.note = m_note->text().trimmed();
	s.active = m_active->isChecked();
	return s;
}

} // namespace acsl
