/***************************************************************************
    copyright            : (C) 2001 by mean
    email                : fixounet@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <math.h>
#include "ADM_inttype.h"
#include <QtCore/QEvent>
#include <QtGui/QCloseEvent>
#include "Q_encoding.h"


#include "prefs.h"
#include "DIA_encoding.h"
#include "DIA_coreToolkit.h"
#include "ADM_vidMisc.h"
#include "ADM_misc.h"
#include "ADM_toolkitQt.h"
#include "GUI_ui.h"
#include "ADM_coreUtils.h"
#include "ADM_prettyPrint.h"
#include "../ADM_gui/ADM_systemTrayProgress.h" // this is Qt only

//*******************************************
#define WIDGET(x) (ui->x)
#define WRITEM(x,y) ui->x->setText(QString::fromUtf8(y))
#define WRITE(x) WRITEM(x,stringMe)
/*************************************/

extern bool ADM_slaveReportProgress(uint32_t percent);



void DIA_encodingQt4::useTrayButtonPressed(void)
{
    hide();
    UI_iconify();
    if(!tray)
        tray=DIA_createTray(this);
}

void DIA_encodingQt4::pauseButtonPressed(void)
{
	ADM_info("StopReq\n");
	stopRequest=true;
}

void DIA_encodingQt4::priorityChanged(int priorityLevel)
{
#ifndef _WIN32
	if (getuid() != 0)
	{
		ui->comboBoxPriority->disconnect(SIGNAL(currentIndexChanged(int)));
		ui->comboBoxPriority->setCurrentIndex(2);

		GUI_Error_HIG(QT_TRANSLATE_NOOP("qencoding","Privileges Required"), QT_TRANSLATE_NOOP("qencoding","Root privileges are required to perform this operation."));

		return;
	}
#endif

	#ifndef __HAIKU__
	setpriority(PRIO_PROCESS, 0, ADM_getNiceValue(priorityLevel));
	#endif
}

void DIA_encodingQt4::shutdownChanged(int state)
{
    stayOpen = (state == 1);
}

void DIA_encodingQt4::deleteStatsChanged(int state)
{
    deleteStats=!!state;
}

static char stringMe[80];

DIA_encodingQt4::DIA_encodingQt4(uint64_t duration) : DIA_encodingBase(duration)
{
        stopRequest=false;
        stayOpen=false;
        multiPass=false;
        firstPass=false;
        UI_getTaskBarProgress()->enable();
        ui=new Ui_encodingDialog;
	ui->setupUi(this);

#ifndef _WIN32
#if 0   // running with root privileges is BAD
	//check for root privileges
	if (getuid() == 0)
	{
		// set priority to normal, regardless of preferences
		ui->comboBoxPriority->setCurrentIndex(2);
                
	}else
#endif
        {
            ui->comboBoxPriority->setVisible(false);
            ui->labelPrio->setVisible(false);
        }
        // remove suspend and shutdown options
        while(ui->comboBoxFinished->count() > 2)
        {
            ui->comboBoxFinished->removeItem(2);
        }
#endif

        if(!prefs->get(DEFAULT_DELETE_FIRST_PASS_LOG_FILES,&deleteStats))
            deleteStats = false;
        ui->checkBoxDeleteStats->setChecked(deleteStats);

	connect(ui->comboBoxFinished, SIGNAL(currentIndexChanged(int)), this, SLOT(shutdownChanged(int)));
	connect(ui->checkBoxDeleteStats, SIGNAL(stateChanged(int)), this, SLOT(deleteStatsChanged(int)));
	connect(ui->pushButton1, SIGNAL(pressed()), this, SLOT(useTrayButtonPressed()));
	connect(ui->pushButton2, SIGNAL(pressed()), this, SLOT(pauseButtonPressed()));
	connect(ui->comboBoxPriority, SIGNAL(currentIndexChanged(int)), this, SLOT(priorityChanged(int)));

	// set priority
	uint32_t priority;

#ifndef _WIN32
 #if 0  // running with root privileges is BAD
	// check for root privileges
	if (getuid() == 0)
	{
		ui->comboBoxPriority->setCurrentIndex(priority);
	}
 #endif
 #ifndef __HAIKU__
        // set priority here (base class constructor already has saved original priority)
        if (prefs->get(PRIORITY_ENCODING, &priority))
        {
            setpriority(PRIO_PROCESS, 0, ADM_getNiceValue(priority));
        }
 #endif
#else
	prefs->get(PRIORITY_ENCODING,&priority);	
	ui->comboBoxPriority->setCurrentIndex(priority);
#endif

        setModal(true);
        qtRegisterDialog(this);
        show();
        tray=NULL;
        ADM_usleep(30*1000);
        QCoreApplication::processEvents();
}
/**
    \fn setFps(uint32_t fps)
    \brief Memorize fps, it will be used later for bitrate computation
*/

void DIA_encodingQt4::setFps(uint32_t fps)
{
      snprintf(stringMe,79,"%" PRIu32" fps",fps);
      WRITE(labelFps);
}

/**
    \fn dtpor
*/
DIA_encodingQt4::~DIA_encodingQt4( )
{
    ADM_info("Destroying encoding qt4\n");
    UI_getTaskBarProgress()->disable();
    int ix = ui->comboBoxFinished->currentIndex();
    if(tray)
    {
        UI_deiconify();
        delete tray;
        tray=NULL;
    }
    if(ui)
    {
        qtUnregisterDialog(this);
        delete ui;
        ui=NULL;
    }
    if(ix > 1)
    {
        prefs->save(); // won't get another chance
        bool suspend = (ix == 2);
        ADM_info("Requesting %s...\n", suspend ? "suspend" : "shutdown");
        if(!ADM_shutdown(suspend))
            ADM_warning("%s request failed\n", suspend ? "Suspend" : "Shutdown");
    }
}
/**
    \fn setPhase
    \brief Setup dialog for given phase, show its description
*/
void DIA_encodingQt4::setPhase(ADM_ENC_PHASE_TYPE phase, const char *n)
{
    ADM_assert(ui);
    firstPass = false;
    const char *description = n;
    switch(phase)
    {
        case ADM_ENC_PHASE_FIRST_PASS:
        {
            multiPass = firstPass = true;
            if(!description)
                description = QT_TRANSLATE_NOOP("qencoding","First Pass");
            break;
        }
        case ADM_ENC_PHASE_LAST_PASS:
        {
            if(!description)
                description = multiPass ?
                    QT_TRANSLATE_NOOP("qencoding","Second Pass") :
                    QT_TRANSLATE_NOOP("qencoding","Encoding...");
            break;
        }
        case ADM_ENC_PHASE_OTHER:
            break;
        default:
        {
            ADM_warning("Invalid encoding phase value %d ignored.\n",(int)phase);
            break;
        }
    }
    ui->tabWidget->setTabEnabled(1, !firstPass); // disable the "Advanced" tab during the first pass
    WRITEM(labelPhase,description);
    ui->checkBoxDeleteStats->setVisible(multiPass);
    QCoreApplication::processEvents();
}

/**
    \fn setFileName
*/
void DIA_encodingQt4::setFileName(const char *n)
{
    ui->lineEditFN->clear();
    if(!n)
    {
        outputFileName.clear();
        ui->labelFileName->setVisible(false);
        ui->lineEditFN->setVisible(false);
        return;
    }
    outputFileName = n;
    ui->labelFileName->setVisible(true);
    ui->lineEditFN->setVisible(true);
    ui->lineEditFN->insert(QString::fromUtf8(outputFileName.c_str()));
    ui->lineEditFN->setCursorPosition(0);
}

/**
    \fn setLogFileName
*/
void DIA_encodingQt4::setLogFileName(const char *n)
{
    if(!n)
    {
        logFileName.clear();
        return;
    }
    logFileName = n;
}

/**
    \fn    setFrameCount
    \brief display the # of processed frames
*/

void DIA_encodingQt4::setFrameCount(uint32_t nb)
{
          ADM_assert(ui);
          snprintf(stringMe,79,"%" PRIu32,nb);
          printf("frames: %s\t",stringMe);
          WRITE(labelFrame);

}
/**
    \fn    setPercent
    \brief display percent of saved file
*/

void DIA_encodingQt4::setPercent(uint32_t p)
{
          ADM_assert(ui);
          printf("%*" PRIu32"%% done%s",3,p,firstPass? "\n" : "\t");
          WIDGET(progressBar)->setValue(p);
          ADM_slaveReportProgress(p);
          if(tray)
                tray->setPercent(p);
          UI_getTaskBarProgress()->setProgress(p);
          UI_purge();
}
/**
    \fn setAudioCodec(const char *n)
    \brief Display parameters as audio codec
*/

void DIA_encodingQt4::setAudioCodec(const char *n)
{
          ADM_assert(ui);
          WRITEM(labelAudCodec,n);
}
/**
    \fn setCodec(const char *n)
    \brief Display parameters as video codec
*/

void DIA_encodingQt4::setVideoCodec(const char *n)
{
          ADM_assert(ui);
          WRITEM(labelVidCodec,n);
}
/**
    \fn setBitrate(uint32_t br,uint32_t globalbr)
    \brief Display parameters as instantaneous bitrate and average bitrate
*/

void DIA_encodingQt4::setBitrate(uint32_t br,uint32_t globalbr)
{
          ADM_assert(ui);
          snprintf(stringMe,79,"%" PRIu32" kB/s",br);
          WRITE(labelVidBitrate);

}
/**
    \fn setContainer(const char *container)
    \brief Display parameter as container field
*/

void DIA_encodingQt4::setContainer(const char *container)
{
        ADM_assert(ui);
        WRITEM(labelContainer,container);
}

/**
    \fn setQuantIn(int size)
    \brief display parameter as quantizer
*/

void DIA_encodingQt4::setQuantIn(int size)
{
          ADM_assert(ui);
          sprintf(stringMe,"%" PRIu32,size);
          WRITE(labelQz);

}
/**
    \fn setSize(int size)
    \brief display parameter as total size
*/

void DIA_encodingQt4::setTotalSize(uint64_t size)
{
          ADM_assert(ui);
          uint64_t mb=size>>20;
          sprintf(stringMe,"%" PRIu32" MB",(int)mb);
          WRITE(labelTotalSize);

}

/**
    \fn setVideoSize(int size)
    \brief display parameter as total size
*/

void DIA_encodingQt4::setVideoSize(uint64_t size)
{
          ADM_assert(ui);
          uint64_t mb=size>>20;
          sprintf(stringMe,"%" PRIu32" MB",(int)mb);
          WRITE(labelVideoSize);

}
/**
    \fn setAudioSizeIn(int size)
    \brief display parameter as audio size
*/

void DIA_encodingQt4::setAudioSize(uint64_t size)
{
          ADM_assert(ui);
          uint64_t mb=size>>20;
          sprintf(stringMe,"%" PRIu32" MB",(int)mb);
          WRITE(labelAudioSize);

}

/**
    \fn setAudioSizeIn(int size)
    \brief display elapsed time since saving start
*/
void DIA_encodingQt4::setElapsedTimeMs(uint32_t nb)
{
          ADM_assert(ui);
          uint64_t mb=nb;
          mb*=1000;
          strcpy(stringMe,ADM_us2plain(mb));
          printf("elapsed: %s\n",stringMe);
          WRITE(labelElapsed);
}
/**
    \fn setAverageQz(int size)
    \brief display average quantizer used
*/

void DIA_encodingQt4::setAverageQz(uint32_t nb)
{
          ADM_assert(ui);
          snprintf(stringMe,79,"%" PRIu32,nb);
          WRITE(labelQz);
}
/**
    \fn setAverageBitrateKbits(int size)
    \brief display average bitrate in kb/s
*/

void DIA_encodingQt4::setAverageBitrateKbits(uint32_t kb)
{
          ADM_assert(ui);
          snprintf(stringMe,79,"%" PRIu32" kbits/s",kb);
          WRITE(labelVidBitrate);
}

/**
    \fn setRemainingTimeMS
    \brief display remaining time (ETA)
*/
void DIA_encodingQt4::setRemainingTimeMS(uint32_t milliseconds)
{
          ADM_assert(ui);
          std::string s;
          ADM_durationToString(milliseconds,s);
          ui->labelETA->setText(QString::fromUtf8(s.c_str()));
}

/**
    \fn isAlive( void )
    \brief return 0 if the window was killed or cancel button pressed, 1 otherwise
*/
bool DIA_encodingQt4::isAlive( void )
{
    if(!stopRequest)
        return true;

    if(0 == GUI_Alternate((char*)QT_TRANSLATE_NOOP("qencoding","The encoding is paused. Do you want to resume or abort?"),
                     (char*)QT_TRANSLATE_NOOP("qencoding","Resume"),(char*)QT_TRANSLATE_NOOP("qencoding","Abort")))
    {
        stopRequest=false;
        return true;
    }
    // encoding aborted
    ui->comboBoxFinished->setCurrentIndex(0);
    return false;
}

static bool tryDeleteLogFile(bool single, std::string fname)
{
    if(!ADM_fileExist(fname.c_str()))
        return false;
    if(single)
    {
        ADM_warning("Single pass operation, not deleting old first pass log file \"%s\"\n",fname.c_str());
        return false;
    }
    if(ADM_eraseFile(fname.c_str()))
    {
        ADM_info("Deleting first pass log file \"%s\"\n",fname.c_str());
        return true;
    }
    ADM_warning("Could not delete first pass log file \"%s\"\n",fname.c_str());
    return false;
}

/**
    \fn keepOpen
    \brief Allow user to read the muxing stats calmly
*/
void DIA_encodingQt4::keepOpen(void)
{
    if(stayOpen)
    {
        stopRequest=true;
        UI_getTaskBarProgress()->disable();
        if(tray)
        {
            UI_deiconify();
            delete tray;
            tray=NULL;
        }
        // remove suspend and shutdown options, if present
        while(ui->comboBoxFinished->count() > 2)
        {
            ui->comboBoxFinished->removeItem(2);
        }
        ui->pushButton1->setEnabled(false);
        ui->pushButton2->setEnabled(false);
        ui->comboBoxPriority->setEnabled(false);
        show();
        while(stayOpen)
        {
            ADM_usleep(100*1000);
            QCoreApplication::processEvents();
        }
    }

    if (deleteStats && !logFileName.empty())
    {
        // try to delete first pass log files
        tryDeleteLogFile(!multiPass,logFileName);
        // libx264 creates an additional log file with extension .mbtree appended
        tryDeleteLogFile(!multiPass,logFileName + ".mbtree");
        // libx265 creates an additional log file with extension .cutree appended
        tryDeleteLogFile(!multiPass,logFileName + ".cutree");
    }
}

/**
 * \fn closeEvent
 * @param event
 */
void DIA_encodingQt4::closeEvent(QCloseEvent *event)
{
    if(stopRequest && stayOpen)
    {
        stayOpen=false;
        return;
    }
    stopRequest=true;
    event->ignore(); // keep window
}

/**
 * \fn reject
 * \brief Prevent dialog from being closed by pressing the Esc key
 */
void DIA_encodingQt4::reject(void)
{
    if(stopRequest && stayOpen) // close dialog when done
    {
        stayOpen=false;
        return;
    }
    stopRequest=true;
}

/**
        \fn createEncoding
*/
namespace ADM_Qt4CoreUIToolkit
{
DIA_encodingBase *createEncoding(uint64_t duration)
{
        return new DIA_encodingQt4(duration);
}
}
//********************************************
//EOF
