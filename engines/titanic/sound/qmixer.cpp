/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "titanic/sound/qmixer.h"

namespace Titanic {

QMixer::QMixer(Audio::Mixer *mixer) : _mixer(mixer) {
}

bool QMixer::qsWaveMixInitEx(const QMIXCONFIG &config) {
	assert(_channels.empty());
	assert(config.iChannels > 0 && config.iChannels < 256);
	
	_channels.resize(config.iChannels);
	return true;
}

void QMixer::qsWaveMixActivate(bool fActivate) {
	// Not currently implemented in ScummVM
}

int QMixer::qsWaveMixOpenChannel(int iChannel, QMixFlag mode) {
	// Not currently implemented in ScummVM
	return 0;
}

int QMixer::qsWaveMixEnableChannel(int iChannel, uint flags, bool enabled) {
	// Not currently implemented in ScummVM
	return 0;
}

void QMixer::qsWaveMixCloseSession() {
	_mixer->stopAll();
	_channels.clear();
}

void QMixer::qsWaveMixFreeWave(Audio::SoundHandle &handle) {
	_mixer->stopHandle(handle);
}

void QMixer::qsWaveMixFlushChannel(int iChannel, uint flags) {
	// Not currently implemented in ScummVM
}

void QMixer::qsWaveMixSetPanRate(int iChannel, uint flags, uint rate) {
	// Not currently implemented in ScummVM
}

void QMixer::qsWaveMixSetVolume(int iChannel, uint flags, uint volume) {
	// Not currently implemented in ScummVM
}

void QMixer::qsWaveMixSetSourcePosition(int iChannel, uint flags, const QSVECTOR &position) {
	// Not currently implemented in ScummVM
}

void QMixer::qsWaveMixSetPolarPosition(int iChannel, uint flags, const QSPOLAR &position) {
	// Not currently implemented in ScummVM
}

void QMixer::qsWaveMixSetListenerPosition(const QSVECTOR &position, uint flags) {
	// Not currently implemented in ScummVM
}

void QMixer::qsWaveMixSetListenerOrientation(const QSVECTOR &direction, const QSVECTOR &up, uint flags) {
	// Not currently implemented in ScummVM
}

void QMixer::qsWaveMixSetDistanceMapping(int iChannel, uint flags, const QMIX_DISTANCES &distances) {
	// Not currently implemented in ScummVM
}

void QMixer::qsWaveMixSetFrequency(int iChannel, uint flags, uint frequency) {
	// Not currently implemented in ScummVM
}

void QMixer::qsWaveMixSetSourceVelocity(int iChannel, uint flags, const QSVECTOR &velocity) {
	// Not currently implemented in ScummVM
}

int QMixer::qsWaveMixPlayEx(int iChannel, uint flags, CWaveFile *waveFile, int loops, const QMIXPLAYPARAMS &params) {
	if (iChannel == -1) {
		// Find a free channel
		for (iChannel = 0; iChannel < (int)_channels.size(); ++iChannel) {
			if (_channels[iChannel]._sounds.empty())
				break;
		}
		assert(iChannel != (int)_channels.size());
	}

	// If the new sound replaces current ones, then clear the channel
	ChannelEntry &channel = _channels[iChannel];
	if (flags & QMIX_CLEARQUEUE) {
		if (!channel._sounds.empty() && channel._sounds.front()._started)
			_mixer->stopHandle(channel._sounds.front()._soundHandle);

		channel._sounds.clear();
	}

	// Add the sound to the channel
	channel._sounds.push_back(SoundEntry(waveFile, params.callback, loops, params.dwUser));
	qsWaveMixPump();

	return 0;
}

bool QMixer::qsWaveMixIsChannelDone(int iChannel) const {
	return _channels[iChannel]._sounds.empty();
}

void QMixer::qsWaveMixPump() {
	// Iterate through each of the channels
	for (uint iChannel = 0; iChannel < _channels.size(); ++iChannel) {
		ChannelEntry &channel = _channels[iChannel];

		// If the playing sound on the channel is finished, then call
		// the callback registered for it, and remove it from the list
		if (!channel._sounds.empty()) {
			SoundEntry &sound = channel._sounds.front();
			if (sound._started && !_mixer->isSoundHandleActive(sound._soundHandle)) {
				if (sound._loops == -1 || sound._loops-- > 0) {
					// Need to loop the sound again
					sound._waveFile->_stream->rewind();
					_mixer->playStream(sound._waveFile->_soundType,
						&sound._soundHandle, sound._waveFile->_stream,
						-1, 0xff, 0, DisposeAfterUse::NO);
				} else {
					// Sound is finished
					if (sound._callback)
						// Call the callback to signal end
						sound._callback(iChannel, sound._waveFile, sound._userData);

					// Remove sound record from channel
					channel._sounds.erase(channel._sounds.begin());
				}
			}
		}

		// If there's an unstarted sound at the front of a channel's
		// sound list, then start it playing
		if (!channel._sounds.empty()) {
			SoundEntry &sound = channel._sounds.front();
			if (!sound._started) {
				_mixer->playStream(sound._waveFile->_soundType,
					&sound._soundHandle, sound._waveFile->_stream,
					-1, 0xff, 0, DisposeAfterUse::NO);
				sound._started = true;
			}
		}
	}
}

} // End of namespace Titanic z
