/*
 * Messenger: class to abstract the routing of Open Sound Control messages from the core
 *   to another area of the core and/or the gui
 *
 * (c) 2015 Colin Wallace (https://github.com/Wallacoloo)
 *
 * This file is part of LMMS - http://lmms.io
 *
 * This file is released under the MIT License.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "Messenger.h"

#include <memory>

#include <QDebug>
#include <QReadLocker>
#include <QString>
#include <QWriteLocker>

#include "OscMsgListener.h"


// define endpoint names
const char *Messenger::Endpoints::Warning              = "/status/warning";
const char *Messenger::Endpoints::Error                = "/status/error";
const char *Messenger::Endpoints::WaveTableInit        = "/wavetable/init";
const char *Messenger::Endpoints::MixerDevInit         = "/mixer/devices/init";
const char *Messenger::Endpoints::MixerProcessingStart = "/mixer/processing/start";
const char *Messenger::Endpoints::FxMixerPeaks         = "/fxmixer/peaks";


void Messenger::broadcastWaveTableInit()
{
	broadcast(Endpoints::WaveTableInit);
}
void Messenger::broadcastMixerDevInit()
{
	broadcast(Endpoints::MixerDevInit);
}
void Messenger::broadcastMixerProcessing()
{
	broadcast(Endpoints::MixerProcessingStart);
}


void Messenger::broadcastFxMixerPeaks(std::size_t numFxCh, const float peaks[][2])
{
	lo_message oscMsg = lo_message_new();

	for (std::size_t fxNo=0; fxNo<numFxCh; ++fxNo)
	{
		for (int ch=0; ch<2; ++ch)
		{
			lo_message_add_float(oscMsg, peaks[fxNo][ch]);
		}
	}
	broadcast(Endpoints::FxMixerPeaks, oscMsg);
	lo_message_free(oscMsg);
}

void Messenger::broadcastWarning(const QString &brief, const QString &msg)
{
	lo_message oscMsg = lo_message_new();
	lo_message_add_string(oscMsg, brief.toUtf8().data());
	lo_message_add_string(oscMsg, msg.toUtf8().data());
	broadcast(Endpoints::Warning, oscMsg);
	lo_message_free(oscMsg);
}

void Messenger::broadcastError(const QString &brief, const QString &msg)
{
	lo_message oscMsg = lo_message_new();
	lo_message_add_string(oscMsg, brief.toUtf8().data());
	lo_message_add_string(oscMsg, msg.toUtf8().data());
	broadcast(Endpoints::Error, oscMsg);
	lo_message_free(oscMsg);
}

void Messenger::addListener(const QString &endpoint, lo_address address)
{
	QWriteLocker listenerLocker(&m_listenersLock);
	m_listeners[endpoint].insert(address);
}


void Messenger::broadcast(const char *endpoint, lo_message msg)
{
	QReadLocker listenerLocker(&m_listenersLock);
	// send the message to all addresses listening at the given endpoint
	for (const lo_address &addr : m_listeners[endpoint])
	{
		lo_send_message(addr, endpoint, msg);
	}
}

void Messenger::broadcast(const char *endpoint)
{
	lo_message msg = lo_message_new();
	broadcast(endpoint, msg);
	lo_message_free(msg);
}