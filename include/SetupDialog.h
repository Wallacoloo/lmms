
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

	// path settings widget
	void setWorkingDir( const QString & _wd );
	void setVSTDir( const QString & _vd );
	void setGIGDir( const QString & _gd );
	void setSF2Dir( const QString & _sfd );
	void setArtworkDir( const QString & _ad );
	void setFLDir( const QString & _fd );
	void setLADSPADir( const QString & _ld );
	void setSTKDir( const QString & _sd );
	void setDefaultSoundfont( const QString & _sf );
	void setBackgroundArtwork( const QString & _ba );

	// audio settings widget
	void audioInterfaceChanged( const QString & _driver );
	void displayAudioHelp();

	// MIDI settings widget
	void midiInterfaceChanged( const QString & _driver );
	void displayMIDIHelp();



	void openWorkingDir();
	void openVSTDir();
	void openGIGDir();
	void openSF2Dir();
	void openArtworkDir();
	void openFLDir();
	void openLADSPADir();
	void openSTKDir();
	void openDefaultSoundfont();
	void openBackgroundArtwork();

	void toggleSmoothScroll( bool _enabled );
	void toggleAutoSave( bool _enabled );
	void toggleAnimateAFP( bool _enabled );

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

	QString m_lang;
	QStringList m_languages;


	QLineEdit * m_wdLineEdit;
	QLineEdit * m_vdLineEdit;
	QLineEdit * m_adLineEdit;
	QLineEdit * m_fdLineEdit;
	QLineEdit * m_ladLineEdit;
	QLineEdit * m_gigLineEdit;
	QLineEdit * m_sf2LineEdit;
#ifdef LMMS_HAVE_FLUIDSYNTH
	QLineEdit * m_sfLineEdit;
#endif
#ifdef LMMS_HAVE_STK
	QLineEdit * m_stkLineEdit;
#endif
	QLineEdit * m_baLineEdit;

	QString m_workingDir;
	QString m_vstDir;
	QString m_artworkDir;
	QString m_flDir;
	QString m_ladDir;
	QString m_gigDir;
	QString m_sf2Dir;
#ifdef LMMS_HAVE_FLUIDSYNTH
	QString m_defaultSoundfont;
#endif
#ifdef LMMS_HAVE_STK
	QString m_stkDir;
#endif
	QString m_backgroundArtwork;

	bool m_smoothScroll;
	bool m_enableAutoSave;
	bool m_animateAFP;

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
		virtual void writeToConfig() const = 0;
	protected:
		// the main getWidget method may optionally wrap the actual widget in
		// a tabview or similar; separate implementation from wrapping
		virtual QWidget* implGetWidget(QWidget *parent) const = 0;
		// section/name used to identify the variable in the configuration file
		QString m_section, m_name;
		QString m_uiName;
	private:
		// Is the ui widget supposed to go in its own TabWidget?
		bool m_isOwnTab;
};

// boolean configuration variable, i.e. a variable that could be configured with a checkbox
class BoolConfigVar : public ConfigVar
{
	Q_OBJECT
	public:
		BoolConfigVar(QString section, QString name, QString uiName, bool inverted=false, QObject *parent=NULL);
		void writeToConfig() const;
	protected:
		QWidget* implGetWidget(QWidget *parent) const;
	private slots:
		void onToggle(bool newValue);
	private:
		// section/name used to identify the variable in the configuration file
		// m_inverted is true if the option is stored in the text file in the
		// *opposite* way in which it's displayed.
		bool m_inverted;
		bool m_value;
};

// string variable that indicates a folder/file path
class PathConfigVar : public ConfigVar
{
	Q_OBJECT
	public:
		PathConfigVar(QString section, QString name, QString uiName, QString dialogTitle, QObject *parent=NULL);
		void writeToConfig() const;
	protected:
		QWidget* implGetWidget(QWidget *parent=NULL) const;
	private slots:
		void onPathChanged(const QString &newPath);
	private:
		QString m_path;
		// titlebar text to apply to the FileDialog when choosing a path
		QString m_dialogTitle;
};

// implement the gui layout for editing a PathConfigVar
class PathConfigWidget : public QWidget
{
	Q_OBJECT
	public:
		PathConfigWidget(const QString &defaultPath, const QString &dialogTitle, QWidget *parent=NULL);
	signals:
		void onPathChanged(const QString &newPath);
	private slots:
		// called when user uses the open button to edit the path
		void onOpenBtnClicked();
		// called when user directly enters a path into the text field
		void onLineEditChanged(const QString &newPath);
	private:
		QString m_dialogTitle;
		QLineEdit *m_lineEdit;
		//QPushButton *m_selectBtn;
};

#endif
