/*
 * SetupDialog.cpp - dialog for setting up LMMS
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

#include <QComboBox>
#include <QImageReader>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSlider>
#include <QWhatsThis>
#include <QScrollArea>

#include "SetupDialog.h"
#include "TabBar.h"
#include "TabButton.h"
#include "TabWidget.h"
#include "gui_templates.h"
#include "Mixer.h"
#include "ProjectJournal.h"
#include "ConfigManager.h"
#include "embed.h"
#include "Engine.h"
#include "debug.h"
#include "ToolTip.h"
#include "LedCheckbox.h"
#include "LcdSpinBox.h"
#include "FileDialog.h"


// platform-specific audio-interface-classes
#include "AudioAlsa.h"
#include "AudioAlsaSetupWidget.h"
#include "AudioJack.h"
#include "AudioOss.h"
#include "AudioPortAudio.h"
#include "AudioSoundIo.h"
#include "AudioPulseAudio.h"
#include "AudioSdl.h"
#include "AudioDummy.h"

// platform-specific midi-interface-classes
#include "MidiAlsaRaw.h"
#include "MidiAlsaSeq.h"
#include "MidiOss.h"
#include "MidiWinMM.h"
#include "MidiApple.h"
#include "MidiDummy.h"


ConfigVar::ConfigVar(QString section, QString name, QString uiName, bool isOwnTab, QObject *parent)
: QObject(parent),
  m_section(section),
  m_name(name),
  m_uiName(uiName),
  m_isOwnTab(isOwnTab)
{
}

QWidget* ConfigVar::getWidget(QWidget *parent) const
{
	if (m_isOwnTab)
	{
		TabWidget *tab = new TabWidget( m_uiName, parent );
		// add the widget to the tab
		implGetWidget(tab);
		return tab;
	}
	else
	{
		// just return the normal widget
		return implGetWidget(parent);
	}
}


BoolConfigVar::BoolConfigVar(QString section, QString name, QString uiName, bool inverted, QObject *parent)
: ConfigVar(section, name, uiName, false, parent),
  m_inverted(inverted)
{
	// read the current value from the configuration file
	m_value = (ConfigManager::inst()->value(section, name).toInt()
				!= inverted);
}

QWidget* BoolConfigVar::implGetWidget(QWidget *parent) const
{
	LedCheckBox *box = new LedCheckBox(m_uiName, parent);
	box->setChecked(m_value);
	connect( box, SIGNAL( toggled(bool) ), this, SLOT( onToggle(bool) ) );
	return box;
}
void BoolConfigVar::writeToConfig() const
{
	ConfigManager::inst()->setValue( m_section, m_name,
			QString::number( m_value != m_inverted ) );
}
void BoolConfigVar::onToggle(bool newValue)
{
	// called whenever the user edits this value through the UI
	m_value = newValue;
}



PathConfigVar* PathConfigVar::newFileVar(QString section, QString name, QString uiName, QString dialogTitle, QString fileFilter, QObject *parent)
{
	return new PathConfigVar(section, name, uiName, dialogTitle, false, fileFilter, parent);
}
PathConfigVar* PathConfigVar::newDirVar(QString section, QString name, QString uiName, QString dialogTitle, QObject *parent)
{
	return new PathConfigVar(section, name, uiName, dialogTitle, false, "", parent);
}

PathConfigVar* PathConfigVar::newDirListVar(QString section, QString name, QString uiName, QString dialogTitle, QObject *parent)
{
	return new PathConfigVar(section, name, uiName, dialogTitle, true, "", parent);
}


PathConfigVar::PathConfigVar(QString section, QString name, QString uiName, QString dialogTitle, bool allowMultipleSelections, QString fileFilter, QObject *parent)
: ConfigVar(section, name, uiName, true, parent),
  m_dialogTitle(dialogTitle),
  m_allowMultipleSelections(allowMultipleSelections),
  m_fileFilter(fileFilter)
{
	// load path from config
	m_path = QDir::toNativeSeparators(
		ConfigManager::inst()->value( section, name ) );
}

QWidget* PathConfigVar::implGetWidget(QWidget *parent) const
{
	PathConfigWidget *widget = new PathConfigWidget( m_path,
		m_dialogTitle, m_allowMultipleSelections, m_fileFilter, parent );
	connect( widget, SIGNAL( onPathChanged( const QString& ) ),
		this, SLOT( onPathChanged( const QString& ) ) );
	return widget;
}

void PathConfigVar::onPathChanged( const QString &newPath )
{
	m_path = newPath;
}

void PathConfigVar::writeToConfig() const
{
	ConfigManager::inst()->setValue( m_section, m_name, m_path );
}



PathConfigWidget::PathConfigWidget(const QString &defaultPath, const QString &dialogTitle, bool allowMultipleSelections, QString fileFilter, QWidget *parent)
: QWidget(parent),
  m_dialogTitle(dialogTitle),
  m_allowMultipleSelections(allowMultipleSelections),
  m_fileFilter(fileFilter)
{
	const int txtLength = 284;
	const int btnStart = 297;

	m_lineEdit = new QLineEdit(defaultPath, this);
	m_lineEdit->setGeometry( 10, 20, txtLength, 16);

	// add a button to open a dialog for choosing the path
	QString pixmapName = allowMultipleSelections ? "add_folder" : "project_open";

	QPushButton *selectBtn = new QPushButton(
		embed::getIconPixmap( pixmapName.toLatin1().constData(), 16, 16 ),
		"", this );
	selectBtn->setFixedSize( 24, 24 );
	selectBtn->move( btnStart, 16 );

	// monitor signals that indicate the value has changed:
	connect( m_lineEdit, SIGNAL( textChanged( const QString & ) ), this,
		SLOT( onLineEditChanged( const QString & ) ) );
	connect( selectBtn, SIGNAL( clicked() ), this, SLOT( onOpenBtnClicked() ) );
}

void PathConfigWidget::onOpenBtnClicked()
{
	QString newPath;
	if (m_fileFilter.isEmpty())
	{
		// we're choosing a directory
		newPath = FileDialog::getExistingDirectory( this,
			m_dialogTitle, m_lineEdit->text() );
		// add a trailing slash
		if ( !newPath.isEmpty() && newPath.right(1) != QDir::separator() )
		{
			newPath += QDir::separator();
		}
	}
	else
	{
		// we're choosing a file
		newPath = FileDialog::getOpenFileName( this,
			m_dialogTitle, m_lineEdit->text(), m_fileFilter );
	}
	if( newPath != QString::null )
	{
		if ( !m_allowMultipleSelections || m_lineEdit->text().isEmpty() )
		{
			// only one path allowed, or no path yet set
			m_lineEdit->setText( newPath );
		}
		else
		{
			// append selected path to the current path list
			m_lineEdit->setText( m_lineEdit->text() + "," + newPath );
		}
		emit onPathChanged( m_lineEdit->text() );
	}
}

void PathConfigWidget::onLineEditChanged(const QString &newPath)
{
	emit onPathChanged( newPath );
}



inline void labelWidget( QWidget * _w, const QString & _txt )
{
	QLabel * title = new QLabel( _txt, _w );
	QFont f = title->font();
	f.setBold( true );
	title->setFont( pointSize<12>( f ) );


	assert( dynamic_cast<QBoxLayout *>( _w->layout() ) != NULL );

	dynamic_cast<QBoxLayout *>( _w->layout() )->addSpacing( 5 );
	dynamic_cast<QBoxLayout *>( _w->layout() )->addWidget( title );
	dynamic_cast<QBoxLayout *>( _w->layout() )->addSpacing( 10 );
}




SetupDialog::SetupDialog( ConfigTabs _tab_to_open ) :
	m_bufferSize( ConfigManager::inst()->value( "mixer",
					"framesperaudiobuffer" ).toInt() ),
	m_lang( ConfigManager::inst()->value( "app",
							"language" ) ),
	m_backgroundArtwork( QDir::toNativeSeparators( ConfigManager::inst()->backgroundArtwork() ) ),
	m_smoothScroll( ConfigManager::inst()->value( "ui", "smoothscroll" ).toInt() ),
	m_enableAutoSave( ConfigManager::inst()->value( "ui", "enableautosave" ).toInt() ),
	m_animateAFP(ConfigManager::inst()->value( "ui",
						   "animateafp").toInt() )
{
	setWindowIcon( embed::getIconPixmap( "setup_general" ) );
	setWindowTitle( tr( "Setup LMMS" ) );
	setModal( true );
	setFixedSize( 452, 520 );

	Engine::projectJournal()->setJournalling( false );

	QVBoxLayout * vlayout = new QVBoxLayout( this );
	vlayout->setSpacing( 0 );
	vlayout->setMargin( 0 );
	QWidget * settings = new QWidget( this );
	QHBoxLayout * hlayout = new QHBoxLayout( settings );
	hlayout->setSpacing( 0 );
	hlayout->setMargin( 0 );

	m_tabBar = new TabBar( settings, QBoxLayout::TopToBottom );
	m_tabBar->setExclusive( true );
	m_tabBar->setFixedWidth( 72 );

	QWidget * ws = new QWidget( settings );
	int wsHeight = 370;
#ifdef LMMS_HAVE_STK
	wsHeight += 50;
#endif
#ifdef LMMS_HAVE_FLUIDSYNTH
	wsHeight += 50;
#endif
	ws->setFixedSize( 360, wsHeight );
	QWidget * general = new QWidget( ws );
	general->setFixedSize( 360, 240 );
	QVBoxLayout * gen_layout = new QVBoxLayout( general );
	gen_layout->setSpacing( 0 );
	gen_layout->setMargin( 0 );
	labelWidget( general, tr( "General settings" ) );

	TabWidget * bufsize_tw = new TabWidget( tr( "BUFFER SIZE" ), general );
	bufsize_tw->setFixedHeight( 80 );

	m_bufSizeSlider = new QSlider( Qt::Horizontal, bufsize_tw );
	m_bufSizeSlider->setRange( 1, 256 );
	m_bufSizeSlider->setTickPosition( QSlider::TicksBelow );
	m_bufSizeSlider->setPageStep( 8 );
	m_bufSizeSlider->setTickInterval( 8 );
	m_bufSizeSlider->setGeometry( 10, 16, 340, 18 );
	m_bufSizeSlider->setValue( m_bufferSize / 64 );

	connect( m_bufSizeSlider, SIGNAL( valueChanged( int ) ), this,
						SLOT( setBufferSize( int ) ) );

	m_bufSizeLbl = new QLabel( bufsize_tw );
	m_bufSizeLbl->setGeometry( 10, 40, 200, 24 );
	setBufferSize( m_bufSizeSlider->value() );

	QPushButton * bufsize_reset_btn = new QPushButton(
			embed::getIconPixmap( "reload" ), "", bufsize_tw );
	bufsize_reset_btn->setGeometry( 290, 40, 28, 28 );
	connect( bufsize_reset_btn, SIGNAL( clicked() ), this,
						SLOT( resetBufSize() ) );
	ToolTip::add( bufsize_reset_btn, tr( "Reset to default-value" ) );

	QPushButton * bufsize_help_btn = new QPushButton(
			embed::getIconPixmap( "help" ), "", bufsize_tw );
	bufsize_help_btn->setGeometry( 320, 40, 28, 28 );
	connect( bufsize_help_btn, SIGNAL( clicked() ), this,
						SLOT( displayBufSizeHelp() ) );

	// declare all the miscellaneous config vars:
	m_miscVars.push_back(new BoolConfigVar("tooltips", "disabled", tr( "Enable tooltips" ), true, this));
	m_miscVars.push_back(new BoolConfigVar("app", "nomsgaftersetup", tr( "Show restart warning after changing settings" ), true, this));
	m_miscVars.push_back(new BoolConfigVar("app", "displaydbv", tr( "Display volume as dBV " ), false, this));
	m_miscVars.push_back(new BoolConfigVar("app", "nommpz", tr( "Compress project files per default" ), true, this));
	m_miscVars.push_back(new BoolConfigVar("ui", "oneinstrumenttrackwindow", tr( "One instrument track window mode" ), false, this));
	m_miscVars.push_back(new BoolConfigVar("mixer", "hqaudio", tr( "HQ-mode for output audio-device" ), false, this));
	m_miscVars.push_back(new BoolConfigVar("ui", "compacttrackbuttons", tr( "Compact track buttons" ), false, this));
	m_miscVars.push_back(new BoolConfigVar("ui", "syncvstplugins", tr( "Sync VST plugins to host playback" ), false, this));
	m_miscVars.push_back(new BoolConfigVar("ui", "printnotelabels", tr( "Enable note labels in piano roll" ), false, this));
	m_miscVars.push_back(new BoolConfigVar("ui", "displaywaveform", tr( "Enable waveform display by default" ), false, this));
	m_miscVars.push_back(new BoolConfigVar("ui", "disableautoquit", tr( "Keep effects running even without input" ), false, this));
	m_miscVars.push_back(new BoolConfigVar("app", "disablebackup", tr( "Create backup file when saving a project" ), true, this));
	m_miscVars.push_back(new BoolConfigVar("app", "openlastproject", tr( "Reopen last project on start" ), false, this));

	TabWidget * misc_tw = new TabWidget( tr( "MISC" ), general );
	const int XDelta = 10;
	const int YDelta = 18;
	const int HeaderSize = 30;
	int labelNumber = 0;

	// add all misc config vars to the UI
	for (ConfigVar *var : m_miscVars)
	{
		QWidget *widget = var->getWidget( misc_tw );
		++labelNumber;
		widget->move( XDelta, YDelta*labelNumber );
	}

	misc_tw->setFixedHeight( YDelta*labelNumber + HeaderSize );


	// Create the language tab
	TabWidget * lang_tw = new TabWidget( tr( "LANGUAGE" ), general );
	lang_tw->setFixedHeight( 48 );
	QComboBox * changeLang = new QComboBox( lang_tw );
	changeLang->move( XDelta, YDelta );

	QDir dir( ConfigManager::inst()->localeDir() );
	QStringList fileNames = dir.entryList( QStringList( "*.qm" ) );
	for( int i = 0; i < fileNames.size(); ++i )
	{
		// get locale extracted by filename
		fileNames[i].truncate( fileNames[i].lastIndexOf( '.' ) );
		m_languages.append( fileNames[i] );
		QString lang = QLocale( m_languages.last() ).nativeLanguageName();
		changeLang->addItem( lang );
	}
	connect( changeLang, SIGNAL( currentIndexChanged( int ) ),
							this, SLOT( setLanguage( int ) ) );

	//If language unset, fallback to system language when available
	if( m_lang == "" )
	{
		QString tmp = QLocale::system().name().left( 2 );
		if( m_languages.contains( tmp ) )
		{
			m_lang = tmp;
		}
		else
		{
			m_lang = "en";
		}
	}

	for( int i = 0; i < changeLang->count(); ++i )
	{
		if( m_lang == m_languages.at( i ) )
		{
			changeLang->setCurrentIndex( i );
			break;
		}
	}

	gen_layout->addWidget( bufsize_tw );
	gen_layout->addSpacing( 10 );
	gen_layout->addWidget( misc_tw );
	gen_layout->addSpacing( 10 );
	gen_layout->addWidget( lang_tw );
	gen_layout->addStretch();



	QWidget * paths = new QWidget( ws );
	int pathsHeight = 370;
#ifdef LMMS_HAVE_STK
	pathsHeight += 55;
#endif
#ifdef LMMS_HAVE_FLUIDSYNTH
	pathsHeight += 55;
#endif
	paths->setFixedSize( 360, pathsHeight );
	QVBoxLayout * dir_layout = new QVBoxLayout( paths );
	dir_layout->setSpacing( 0 );
	dir_layout->setMargin( 0 );
	labelWidget( paths, tr( "Paths" ) );
	QLabel * title = new QLabel( tr( "Directories" ), paths );
	QFont f = title->font();
	f.setBold( true );
	title->setFont( pointSize<12>( f ) );


	QScrollArea *pathScroll = new QScrollArea( paths );

	QWidget *pathSelectors = new QWidget( ws );
	QVBoxLayout *pathSelectorLayout = new QVBoxLayout;
	pathScroll->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
	pathScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	pathScroll->resize( 362, pathsHeight - 50  );
	pathScroll->move( 0, 30 );
	pathSelectors->resize( 360, pathsHeight - 50 );

	const int txtLength = 284;
	const int btnStart = 297;

	m_pathVars.push_back(PathConfigVar::newDirVar( "paths", "workingdir", tr( "LMMS working directory").toUpper(),
		tr( "Choose LMMS working directory" ), this ));

	m_pathVars.push_back(PathConfigVar::newDirVar( "paths", "gigdir", tr( "GIG directory").toUpper(),
		tr( "Choose your GIG directory" ), this ));

	m_pathVars.push_back(PathConfigVar::newDirVar( "paths", "sf2dir", tr( "SF2 Directory").toUpper(),
		tr( "Choose your SF2 directory" ), this ));

	m_pathVars.push_back(PathConfigVar::newDirVar( "paths", "vstdir", tr( "VST-plugin directory").toUpper(),
		tr( "Choose your VST-plugin directory" ), this ));

	m_pathVars.push_back(PathConfigVar::newDirListVar( "paths", "laddir", tr( "LADSPA plugin directory").toUpper(),
		tr( "Choose LADSPA plugin directory" ), this ));


#ifdef LMMS_HAVE_STK
	m_pathVars.push_back(PathConfigVar::newDirVar( "paths", "stkdir", tr( "STK rawwave directory").toUpper(),
		tr( "Choose STK rawwave directory" ), this ));
#endif

#ifdef LMMS_HAVE_FLUIDSYNTH
	m_pathVars.push_back(PathConfigVar::newFileVar( "paths", "defaultsf2", tr( "Default Soundfont File").toUpper(),
		tr( "Choose default SoundFont" ), "SoundFont2 Files (*.sf2)", this ));
#endif

	m_pathVars.push_back(PathConfigVar::newDirVar( "paths", "fldir", tr( "FL Studio installation directory").toUpper(),
		tr( "Choose FL Studio installation directory" ), this ));

	m_pathVars.push_back(PathConfigVar::newDirVar( "paths", "artwork", tr( "Themes directory").toUpper(),
		tr( "Choose artwork-theme directory" ), this ));


	// background artwork file
	TabWidget * backgroundArtwork_tw = new TabWidget( tr(
			"Background artwork" ).toUpper(), paths );
	backgroundArtwork_tw->setFixedHeight( 48 );

	m_baLineEdit = new QLineEdit( m_backgroundArtwork, 
			backgroundArtwork_tw );
	m_baLineEdit->setGeometry( 10, 20, txtLength, 16 );
	connect( m_baLineEdit, SIGNAL( textChanged( const QString & ) ), this,
			SLOT( setBackgroundArtwork( const QString & ) ) );

	QPushButton * backgroundartworkdir_select_btn = new QPushButton(
			embed::getIconPixmap( "project_open", 16, 16 ),
			"", backgroundArtwork_tw );
	backgroundartworkdir_select_btn->setFixedSize( 24, 24 );
	backgroundartworkdir_select_btn->move( btnStart, 16 );
	connect( backgroundartworkdir_select_btn, SIGNAL( clicked() ), this,
					SLOT( openBackgroundArtwork() ) );



	// add all the path options to the gui
	for (ConfigVar *var : m_pathVars)
	{
		QWidget *widget = var->getWidget( pathSelectors );
		widget->setFixedHeight( 48 );
		pathSelectorLayout->addWidget( widget );
		pathSelectorLayout->addSpacing( 10 );
	}

	pathSelectorLayout->addStretch();
	pathSelectorLayout->addWidget( backgroundArtwork_tw );
	pathSelectorLayout->addSpacing( 10 );

	dir_layout->addWidget( pathSelectors );

	pathScroll->setWidget( pathSelectors );
	pathScroll->setWidgetResizable( true );



	QWidget * performance = new QWidget( ws );
	performance->setFixedSize( 360, 240 );
	QVBoxLayout * perf_layout = new QVBoxLayout( performance );
	perf_layout->setSpacing( 0 );
	perf_layout->setMargin( 0 );
	labelWidget( performance, tr( "Performance settings" ) );

	TabWidget * ui_fx_tw = new TabWidget( tr( "UI effects vs. "
						"performance" ).toUpper(),
								performance );
	ui_fx_tw->setFixedHeight( 80 );

	LedCheckBox * smoothScroll = new LedCheckBox(
			tr( "Smooth scroll in Song Editor" ), ui_fx_tw );
	smoothScroll->move( 10, 20 );
	smoothScroll->setChecked( m_smoothScroll );
	connect( smoothScroll, SIGNAL( toggled( bool ) ),
				this, SLOT( toggleSmoothScroll( bool ) ) );


	LedCheckBox * autoSave = new LedCheckBox(
			tr( "Enable auto save feature" ), ui_fx_tw );
	autoSave->move( 10, 40 );
	autoSave->setChecked( m_enableAutoSave );
	connect( autoSave, SIGNAL( toggled( bool ) ),
				this, SLOT( toggleAutoSave( bool ) ) );


	LedCheckBox * animAFP = new LedCheckBox(
				tr( "Show playback cursor in AudioFileProcessor" ),
								ui_fx_tw );
	animAFP->move( 10, 60 );
	animAFP->setChecked( m_animateAFP );
	connect( animAFP, SIGNAL( toggled( bool ) ),
				this, SLOT( toggleAnimateAFP( bool ) ) );



	perf_layout->addWidget( ui_fx_tw );
	perf_layout->addStretch();



	QWidget * audio = new QWidget( ws );
	audio->setFixedSize( 360, 200 );
	QVBoxLayout * audio_layout = new QVBoxLayout( audio );
	audio_layout->setSpacing( 0 );
	audio_layout->setMargin( 0 );
	labelWidget( audio, tr( "Audio settings" ) );

	TabWidget * audioiface_tw = new TabWidget( tr( "AUDIO INTERFACE" ),
									audio );
	audioiface_tw->setFixedHeight( 60 );

	m_audioInterfaces = new QComboBox( audioiface_tw );
	m_audioInterfaces->setGeometry( 10, 20, 240, 22 );


	QPushButton * audio_help_btn = new QPushButton(
			embed::getIconPixmap( "help" ), "", audioiface_tw );
	audio_help_btn->setGeometry( 320, 20, 28, 28 );
	connect( audio_help_btn, SIGNAL( clicked() ), this,
						SLOT( displayAudioHelp() ) );


	// create ifaces-settings-widget
	QWidget * asw = new QWidget( audio );
	asw->setFixedHeight( 60 );

	QHBoxLayout * asw_layout = new QHBoxLayout( asw );
	asw_layout->setSpacing( 0 );
	asw_layout->setMargin( 0 );
	//asw_layout->setAutoAdd( true );

#ifdef LMMS_HAVE_JACK
	m_audioIfaceSetupWidgets[AudioJack::name()] =
					new AudioJack::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_ALSA
	m_audioIfaceSetupWidgets[AudioAlsa::name()] =
					new AudioAlsaSetupWidget( asw );
#endif

#ifdef LMMS_HAVE_PULSEAUDIO
	m_audioIfaceSetupWidgets[AudioPulseAudio::name()] =
					new AudioPulseAudio::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_PORTAUDIO
	m_audioIfaceSetupWidgets[AudioPortAudio::name()] =
					new AudioPortAudio::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_SOUNDIO
	m_audioIfaceSetupWidgets[AudioSoundIo::name()] =
					new AudioSoundIo::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_SDL
	m_audioIfaceSetupWidgets[AudioSdl::name()] =
					new AudioSdl::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_OSS
	m_audioIfaceSetupWidgets[AudioOss::name()] =
					new AudioOss::setupWidget( asw );
#endif
	m_audioIfaceSetupWidgets[AudioDummy::name()] =
					new AudioDummy::setupWidget( asw );


	for( AswMap::iterator it = m_audioIfaceSetupWidgets.begin();
				it != m_audioIfaceSetupWidgets.end(); ++it )
	{
		m_audioIfaceNames[tr( it.key().toLatin1())] = it.key();
	}
	for( trMap::iterator it = m_audioIfaceNames.begin();
				it != m_audioIfaceNames.end(); ++it )
	{
		QWidget * audioWidget = m_audioIfaceSetupWidgets[it.value()];
		audioWidget->hide();
		asw_layout->addWidget( audioWidget );
		m_audioInterfaces->addItem( it.key() );
	}

	QString audioDevName = 
		ConfigManager::inst()->value( "mixer", "audiodev" );
	if( audioDevName.length() == 0 )
	{
		audioDevName = Engine::mixer()->audioDevName();
		ConfigManager::inst()->setValue(
					"mixer", "audiodev", audioDevName );
	}
	m_audioInterfaces->
		setCurrentIndex( m_audioInterfaces->findText( audioDevName ) );
	m_audioIfaceSetupWidgets[audioDevName]->show();

	connect( m_audioInterfaces, SIGNAL( activated( const QString & ) ),
		this, SLOT( audioInterfaceChanged( const QString & ) ) );


	audio_layout->addWidget( audioiface_tw );
	audio_layout->addSpacing( 20 );
	audio_layout->addWidget( asw );
	audio_layout->addStretch();




	QWidget * midi = new QWidget( ws );
	QVBoxLayout * midi_layout = new QVBoxLayout( midi );
	midi_layout->setSpacing( 0 );
	midi_layout->setMargin( 0 );
	labelWidget( midi, tr( "MIDI settings" ) );

	TabWidget * midiiface_tw = new TabWidget( tr( "MIDI INTERFACE" ),
									midi );
	midiiface_tw->setFixedHeight( 60 );

	m_midiInterfaces = new QComboBox( midiiface_tw );
	m_midiInterfaces->setGeometry( 10, 20, 240, 22 );


	QPushButton * midi_help_btn = new QPushButton(
			embed::getIconPixmap( "help" ), "", midiiface_tw );
	midi_help_btn->setGeometry( 320, 20, 28, 28 );
	connect( midi_help_btn, SIGNAL( clicked() ), this,
						SLOT( displayMIDIHelp() ) );


	// create ifaces-settings-widget
	QWidget * msw = new QWidget( midi );
	msw->setFixedHeight( 60 );

	QHBoxLayout * msw_layout = new QHBoxLayout( msw );
	msw_layout->setSpacing( 0 );
	msw_layout->setMargin( 0 );
	//msw_layout->setAutoAdd( true );

#ifdef LMMS_HAVE_ALSA
	m_midiIfaceSetupWidgets[MidiAlsaSeq::name()] =
					MidiSetupWidget::create<MidiAlsaSeq>( msw );
	m_midiIfaceSetupWidgets[MidiAlsaRaw::name()] =
					MidiSetupWidget::create<MidiAlsaRaw>( msw );
#endif

#ifdef LMMS_HAVE_OSS
	m_midiIfaceSetupWidgets[MidiOss::name()] =
					MidiSetupWidget::create<MidiOss>( msw );
#endif

#ifdef LMMS_BUILD_WIN32
	m_midiIfaceSetupWidgets[MidiWinMM::name()] =
					MidiSetupWidget::create<MidiWinMM>( msw );
#endif

#ifdef LMMS_BUILD_APPLE
    m_midiIfaceSetupWidgets[MidiApple::name()] =
                    MidiSetupWidget::create<MidiApple>( msw );
#endif

	m_midiIfaceSetupWidgets[MidiDummy::name()] =
					MidiSetupWidget::create<MidiDummy>( msw );


	for( MswMap::iterator it = m_midiIfaceSetupWidgets.begin();
				it != m_midiIfaceSetupWidgets.end(); ++it )
	{
		m_midiIfaceNames[tr( it.key().toLatin1())] = it.key();
	}
	for( trMap::iterator it = m_midiIfaceNames.begin();
				it != m_midiIfaceNames.end(); ++it )
	{
		QWidget * midiWidget = m_midiIfaceSetupWidgets[it.value()];
		midiWidget->hide();
		msw_layout->addWidget( midiWidget );
		m_midiInterfaces->addItem( it.key() );
	}

	QString midiDevName = 
		ConfigManager::inst()->value( "mixer", "mididev" );
	if( midiDevName.length() == 0 )
	{
		midiDevName = Engine::mixer()->midiClientName();
		ConfigManager::inst()->setValue(
					"mixer", "mididev", midiDevName );
	}
	m_midiInterfaces->setCurrentIndex( 
		m_midiInterfaces->findText( midiDevName ) );
	m_midiIfaceSetupWidgets[midiDevName]->show();

	connect( m_midiInterfaces, SIGNAL( activated( const QString & ) ),
		this, SLOT( midiInterfaceChanged( const QString & ) ) );


	midi_layout->addWidget( midiiface_tw );
	midi_layout->addSpacing( 20 );
	midi_layout->addWidget( msw );
	midi_layout->addStretch();


	m_tabBar->addTab( general, tr( "General settings" ), 0, false, true 
			)->setIcon( embed::getIconPixmap( "setup_general" ) );
	m_tabBar->addTab( paths, tr( "Paths" ), 1, false, true 
			)->setIcon( embed::getIconPixmap(
							"setup_directories" ) );
	m_tabBar->addTab( performance, tr( "Performance settings" ), 2, false,
				true )->setIcon( embed::getIconPixmap(
							"setup_performance" ) );
	m_tabBar->addTab( audio, tr( "Audio settings" ), 3, false, true
			)->setIcon( embed::getIconPixmap( "setup_audio" ) );
	m_tabBar->addTab( midi, tr( "MIDI settings" ), 4, true, true
			)->setIcon( embed::getIconPixmap( "setup_midi" ) );


	m_tabBar->setActiveTab( _tab_to_open );

	hlayout->addWidget( m_tabBar );
	hlayout->addSpacing( 10 );
	hlayout->addWidget( ws );
	hlayout->addSpacing( 10 );
	hlayout->addStretch();

	QWidget * buttons = new QWidget( this );
	QHBoxLayout * btn_layout = new QHBoxLayout( buttons );
	btn_layout->setSpacing( 0 );
	btn_layout->setMargin( 0 );
	QPushButton * ok_btn = new QPushButton( embed::getIconPixmap( "apply" ),
						tr( "OK" ), buttons );
	connect( ok_btn, SIGNAL( clicked() ), this, SLOT( accept() ) );

	QPushButton * cancel_btn = new QPushButton( embed::getIconPixmap(
								"cancel" ),
							tr( "Cancel" ),
							buttons );
	connect( cancel_btn, SIGNAL( clicked() ), this, SLOT( reject() ) );

	btn_layout->addStretch();
	btn_layout->addSpacing( 10 );
	btn_layout->addWidget( ok_btn );
	btn_layout->addSpacing( 10 );
	btn_layout->addWidget( cancel_btn );
	btn_layout->addSpacing( 10 );

	vlayout->addWidget( settings );
	vlayout->addSpacing( 10 );
	vlayout->addWidget( buttons );
	vlayout->addSpacing( 10 );
	vlayout->addStretch();

	show();
}




SetupDialog::~SetupDialog()
{
	Engine::projectJournal()->setJournalling( true );
}




void SetupDialog::accept()
{
	// save all the misc config variables
	for (const ConfigVar *var : m_miscVars)
	{
		var->writeToConfig();
	}

	ConfigManager::inst()->setValue( "mixer", "framesperaudiobuffer",
					QString::number( m_bufferSize ) );
	ConfigManager::inst()->setValue( "mixer", "audiodev",
			m_audioIfaceNames[m_audioInterfaces->currentText()] );
	ConfigManager::inst()->setValue( "mixer", "mididev",
			m_midiIfaceNames[m_midiInterfaces->currentText()] );
	ConfigManager::inst()->setValue( "ui", "smoothscroll",
					QString::number( m_smoothScroll ) );
	ConfigManager::inst()->setValue( "ui", "enableautosave",
					QString::number( m_enableAutoSave ) );
	ConfigManager::inst()->setValue( "ui", "animateafp",
					QString::number( m_animateAFP ) );
	ConfigManager::inst()->setValue( "app", "language", m_lang );


	// save all the path config variables
	for (const ConfigVar *var : m_pathVars)
	{
		var->writeToConfig();
	}

	ConfigManager::inst()->setBackgroundArtwork( m_backgroundArtwork );

	// tell all audio-settings-widget to save their settings
	for( AswMap::iterator it = m_audioIfaceSetupWidgets.begin();
				it != m_audioIfaceSetupWidgets.end(); ++it )
	{
		it.value()->saveSettings();
	}
	// tell all MIDI-settings-widget to save their settings
	for( MswMap::iterator it = m_midiIfaceSetupWidgets.begin();
				it != m_midiIfaceSetupWidgets.end(); ++it )
	{
		it.value()->saveSettings();
	}

	ConfigManager::inst()->saveConfigFile();

	QDialog::accept();
	if( !ConfigManager::inst()->value("app", "nomsgaftersetup").toInt() )
	{
		QMessageBox::information( NULL, tr( "Restart LMMS" ),
					tr( "Please note that most changes "
						"won't take effect until "
						"you restart LMMS!" ),
					QMessageBox::Ok );
	}
}




void SetupDialog::setBufferSize( int _value )
{
	const int step = DEFAULT_BUFFER_SIZE / 64;
	if( _value > step && _value % step )
	{
		int mod_value = _value % step;
		if( mod_value < step / 2 )
		{
			m_bufSizeSlider->setValue( _value - mod_value );
		}
		else
		{
			m_bufSizeSlider->setValue( _value + step - mod_value );
		}
		return;
	}

	if( m_bufSizeSlider->value() != _value )
	{
		m_bufSizeSlider->setValue( _value );
	}

	m_bufferSize = _value * 64;
	m_bufSizeLbl->setText( tr( "Frames: %1\nLatency: %2 ms" ).arg(
					m_bufferSize ).arg(
						1000.0f * m_bufferSize /
				Engine::mixer()->processingSampleRate(),
						0, 'f', 1 ) );
}




void SetupDialog::resetBufSize()
{
	setBufferSize( DEFAULT_BUFFER_SIZE / 64 );
}




void SetupDialog::displayBufSizeHelp()
{
	QWhatsThis::showText( QCursor::pos(),
			tr( "Here you can setup the internal buffer-size "
					"used by LMMS. Smaller values result "
					"in a lower latency but also may cause "
					"unusable sound or bad performance, "
					"especially on older computers or "
					"systems with a non-realtime "
					"kernel." ) );
}


void SetupDialog::toggleSmoothScroll( bool _enabled )
{
	m_smoothScroll = _enabled;
}



void SetupDialog::toggleAutoSave( bool _enabled )
{
	m_enableAutoSave = _enabled;
}



void SetupDialog::toggleAnimateAFP( bool _enabled )
{
	m_animateAFP = _enabled;
}


void SetupDialog::setLanguage( int lang )
{
	m_lang = m_languages[lang];
}


void SetupDialog::openBackgroundArtwork()
{
	QList<QByteArray> fileTypesList = QImageReader::supportedImageFormats();
	QString fileTypes;
	for( int i = 0; i < fileTypesList.count(); i++ )
	{
		if( fileTypesList[i] != fileTypesList[i].toUpper() )
		{
			if( !fileTypes.isEmpty() )
			{
				fileTypes += " ";
			}
			fileTypes += "*." + QString( fileTypesList[i] );
		}
	}

	// Have the file dialog default to the artwork directory
	// if the background-artwork directory is unset
	QString dir = ( m_backgroundArtwork.isEmpty() ) ?
		ConfigManager::inst()->value("paths", "artwork") :
		m_backgroundArtwork;
	QString new_file = FileDialog::getOpenFileName( this,
			tr( "Choose background artwork" ), dir, 
			"Image Files (" + fileTypes + ")" );
	
	if( new_file != QString::null )
	{
		m_baLineEdit->setText( new_file );
	}
}


void SetupDialog::setBackgroundArtwork( const QString & _ba )
{
	// TODO: Why does background artwork require fluidsynth?
#ifdef LMMS_HAVE_FLUIDSYNTH
	m_backgroundArtwork = _ba;
#endif
}




void SetupDialog::audioInterfaceChanged( const QString & _iface )
{
	for( AswMap::iterator it = m_audioIfaceSetupWidgets.begin();
				it != m_audioIfaceSetupWidgets.end(); ++it )
	{
		it.value()->hide();
	}

	m_audioIfaceSetupWidgets[m_audioIfaceNames[_iface]]->show();
}




void SetupDialog::displayAudioHelp()
{
	QWhatsThis::showText( QCursor::pos(),
				tr( "Here you can select your preferred "
					"audio-interface. Depending on the "
					"configuration of your system during "
					"compilation time you can choose "
					"between ALSA, JACK, OSS and more. "
					"Below you see a box which offers "
					"controls to setup the selected "
					"audio-interface." ) );
}




void SetupDialog::midiInterfaceChanged( const QString & _iface )
{
	for( MswMap::iterator it = m_midiIfaceSetupWidgets.begin();
				it != m_midiIfaceSetupWidgets.end(); ++it )
	{
		it.value()->hide();
	}

	m_midiIfaceSetupWidgets[m_midiIfaceNames[_iface]]->show();
}




void SetupDialog::displayMIDIHelp()
{
	QWhatsThis::showText( QCursor::pos(),
				tr( "Here you can select your preferred "
					"MIDI-interface. Depending on the "
					"configuration of your system during "
					"compilation time you can choose "
					"between ALSA, OSS and more. "
					"Below you see a box which offers "
					"controls to setup the selected "
					"MIDI-interface." ) );
}
