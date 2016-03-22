
/*
 * SetupDialog.h - dialog for setting up LMMS
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of LMMS - http://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef SETUP_DIALOG_H
#define SETUP_DIALOG_H

#include <QDialog>
#include <QtCore/QMap>

#include "lmmsconfig.h"
#include "AudioDevice.h"
#include "MidiClient.h"
#include "MidiSetupWidget.h"

#include "AudioDeviceSetupWidget.h"


class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;

class TabBar;

class ConfigVar;

class SetupDialog : public QDialog
{
	Q_OBJECT
public:
	enum ConfigTabs
	{
		GeneralSettings,
		PathSettings,
		PerformanceSettings,
		AudioSettings,
		MidiSettings
	} ;

	SetupDialog( ConfigTabs _tab_to_open = GeneralSettings );
	virtual ~SetupDialog();


protected slots:
	virtual void accept();


private slots:
	// general settings widget
	void setBufferSize( int _value );
	void resetBufSize();
	void displayBufSizeHelp();

	// audio settings widget
	void audioInterfaceChanged( const QString & _driver );
	void displayAudioHelp();

	// MIDI settings widget
	void midiInterfaceChanged( const QString & _driver );
	void displayMIDIHelp();

	void setLanguage( int lang );


private:
	TabBar * m_tabBar;

	QSlider * m_bufSizeSlider;
	QLabel * m_bufSizeLbl;
	int m_bufferSize;

	// all configurations variables that are to be shown under the 'misc' tab
	QVector<ConfigVar*> m_miscVars;
	// all config vars that appear in the paths tab
	QVector<ConfigVar*> m_pathVars;
	// all config vars that appear in the Performance Settings tab
	QVector<ConfigVar*> m_perfVars;

	QString m_lang;
	QStringList m_languages;


	typedef QMap<QString, AudioDeviceSetupWidget *> AswMap;
	typedef QMap<QString, MidiSetupWidget *> MswMap;
	typedef QMap<QString, QString> trMap;

	QComboBox * m_audioInterfaces;
	AswMap m_audioIfaceSetupWidgets;
	trMap m_audioIfaceNames;

	QComboBox * m_midiInterfaces;
	MswMap m_midiIfaceSetupWidgets;
	trMap m_midiIfaceNames;

} ;

class ConfigVar : public QObject
{
	Q_OBJECT
	public:
		ConfigVar(QString section, QString name, QString uiName, bool isOwnTab=false, QObject *parent=NULL);
		QWidget* getWidget(QWidget *parent=NULL) const;
		void writeToConfig() const;
	protected:
		// the main getWidget method may optionally wrap the actual widget in
		// a tabview or similar; separate implementation from wrapping
		virtual QWidget* implGetWidget(QWidget *parent) const = 0;
		QString curValue() const;
		void setNewValue(QString newValue);
		// section/name used to identify the variable in the configuration file
		QString m_section, m_name;
		QString m_uiName;
	private:
		// Is the ui widget supposed to go in its own TabWidget?
		bool m_isOwnTab;
		QString m_value;
		QString m_newValue;
};

// boolean configuration variable, i.e. a variable that could be configured with a checkbox
class BoolConfigVar : public ConfigVar
{
	Q_OBJECT
	public:
		BoolConfigVar(QString section, QString name, QString uiName, bool inverted=false, QObject *parent=NULL);
	protected:
		QWidget* implGetWidget(QWidget *parent) const;
	private slots:
		void onToggle(bool newValue);
	private:
		// section/name used to identify the variable in the configuration file
		// m_inverted is true if the option is stored in the text file in the
		// *opposite* way in which it's displayed.
		bool m_inverted;
};

// string variable that indicates a folder/file path
class PathConfigVar : public ConfigVar
{
	Q_OBJECT
	public:
		typedef enum {
			AnyFormat, // allow selection of any FILE (no directory)
			DirFormat, // only allow directory selection
			SF2Format,
			ImageFormats // allow selection of all images supported by Qt
		} FileFormat;
		PathConfigVar(QString section, QString name, QString uiName, QString dialogTitle, bool allowMultipleSelections=false, FileFormat fileFilter=AnyFormat, QObject *parent=NULL);
		// If the variable blank when the dialog is opened,
		// the dialog will point to some default directory
		void setDefaultDir( QString dir );
		void setFileFilter( FileFormat f );

		// new variable that represents a *file*
		static PathConfigVar* newFileVar(QString section, QString name, QString uiName, QString dialogTitle, QObject *parent=NULL);
		// new variable that represents a single *directory*
		static PathConfigVar* newDirVar(QString section, QString name, QString uiName, QString dialogTitle, QObject *parent=NULL);
		// new variable that represents a *list* of directories
		static PathConfigVar* newDirListVar(QString section, QString name, QString uiName, QString dialogTitle, QObject *parent=NULL);
	protected:
		QWidget* implGetWidget(QWidget *parent=NULL) const;
	private slots:
		void onPathChanged(const QString &newPath);
	private:
		// titlebar text to apply to the FileDialog when choosing a path
		QString m_dialogTitle;
		// can the path variable represent *multiple* paths, separated by commas?
		bool m_allowMultipleSelections;
		// empty for directory choosing, else describes the valid filetypes
		FileFormat m_fileFilter;
		QString m_defaultDir;
	friend class PathConfigWidget;
};

// implement the gui layout for editing a PathConfigVar
class PathConfigWidget : public QWidget
{
	Q_OBJECT
	public:
		PathConfigWidget(QString path, const PathConfigVar &pathVar, QWidget *parent=NULL);
	signals:
		void onPathChanged(const QString &newPath);
	private slots:
		// called when user uses the open button to edit the path
		void onOpenBtnClicked();
		// called when user directly enters a path into the text field
		void onLineEditChanged(const QString &newPath);
	private:
		const PathConfigVar &m_pathVar;
		QLineEdit *m_lineEdit;
};

#endif
