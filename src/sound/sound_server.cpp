//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name sound_server.cpp - The sound server (hardware layer and so on) */
//
//      (c) Copyright 1998-2006 by Lutz Sammer, Fabrice Rossi, and
//                                 Jimmy Salmon
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//


//@{

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include "stratagus.h"

#include "sound_server.h"

#include "iocompat.h"
#include "iolib.h"
#include "unit.h"
#include "script.h"

#include "SDL.h"
#include "SDL_mixer.h"

/*----------------------------------------------------------------------------
--  Variables
----------------------------------------------------------------------------*/

static bool SoundInitialized;    /// is sound initialized
static bool MusicEnabled = true;
static bool EffectsEnabled = true;
static int EffectsVolume = 255;
static int MusicVolume = 255;

volatile bool MusicFinished;

/// Channels for sound effects and unit speech
struct SoundChannel {
	Origin *Unit;          /// pointer to unit, who plays the sound, if any
	void (*FinishedCallback)(int channel); /// Callback for when a sample finishes playing
};

#define MaxChannels 64     /// How many channels are supported

static SoundChannel Channels[MaxChannels];

static void ChannelFinished(int channel);

/**
**  Check if this sound is already playing
*/
bool SampleIsPlaying(Mix_Chunk *sample)
{
	for (int i = 0; i < MaxChannels; ++i) {
		if (Mix_GetChunk(i) == sample && Mix_Playing(i)) {
			return true;
		}
	}
	return false;
}

bool UnitSoundIsPlaying(Origin *origin)
{
	for (int i = 0; i < MaxChannels; ++i) {
		if (origin && Channels[i].Unit && origin->Id && Channels[i].Unit->Id
			&& origin->Id == Channels[i].Unit->Id && Mix_Playing(i)) {
			return true;
		}
	}
	return false;
}

/**
**  A channel is finished playing
*/
static void ChannelFinished(int channel)
{
	if (Channels[channel].FinishedCallback) {
		Channels[channel].FinishedCallback(channel);
	}
	if (Channels[channel].Unit) {
		delete Channels[channel].Unit;
		Channels[channel].Unit = NULL;
	}
}

/**
**  Set the channel volume
**
**  @param channel  Channel to set
**  @param volume   New volume 0-255, <0 will not set the volume
**
**  @return         Current volume of the channel, -1 for error
*/
int SetChannelVolume(int channel, int volume)
{
	return Mix_Volume(channel, volume);
}

/**
**  Set the channel stereo
**
**  @param channel  Channel to set
**  @param stereo   -128 to 127, out of range will not set the stereo
**
**  @return         Current stereo of the channel, -1 for error
*/
void SetChannelStereo(int channel, int stereo)
{
	if (Preference.StereoSound == false) {
		Mix_SetPanning(channel, 255, 255);
	} else {
		int left, right;
		if (stereo > 0) {
			left = 255 - stereo;
			right = 255;
		} else {
			left = 255;
			right = 255 + stereo;
		}
		Mix_SetPanning(channel, left, right);
	}
}

/**
**  Set the channel's callback for when a sound finishes playing
**
**  @param channel   Channel to set
**  @param callback  Callback to call when the sound finishes
*/
void SetChannelFinishedCallback(int channel, void (*callback)(int channel))
{
	if (channel < 0 || channel >= MaxChannels) {
		return;
	}
	Channels[channel].FinishedCallback = callback;
}

/**
**  Get the sample playing on a channel
*/
Mix_Chunk *GetChannelSample(int channel)
{
	if (Mix_Playing(channel)) {
		return Mix_GetChunk(channel);
	}
	return NULL;
}

/**
**  Stop a channel
**
**  @param channel  Channel to stop
*/
void StopChannel(int channel)
{
	Mix_HaltChannel(channel);
}

/**
**  Stop all channels
*/
void StopAllChannels()
{
	Mix_HaltChannel(-1);
}

static Mix_Chunk *currentMusic = NULL;
static int currentMusicChannel = -1;

static Mix_Chunk *LoadMusic(const char *name)
{
	if (currentMusicChannel >= 0) {
		Mix_HaltChannel(currentMusicChannel);
	}
	if (currentMusic) {
		Mix_FreeChunk(currentMusic);
		currentMusic = NULL;
	}
	currentMusic = LoadSample(name);
	return currentMusic;
}

static Mix_Chunk *LoadSample(const char *name)
{
	Mix_Chunk *r = Mix_LoadWAV(name);
	if (r) {
		return r;
	}
	CFile *f = new CFile;
	if (f->open(name, CL_OPEN_READ) == -1) {
		printf("Can't open file '%s'\n", name);
		delete f;
		return NULL;
	}
	return Mix_LoadWAV_RW(f->as_SDL_RWops(), 0);
}

/**
**  Load a music file
**
**  @param name  File name
**
**  @return      Mix_Music pointer
*/
Mix_Chunk *LoadMusic(const std::string &name)
{
	const std::string filename = LibraryFileName(name.c_str());
	Mix_Chunk *music = LoadMusic(filename.c_str());

	if (music == NULL) {
		fprintf(stderr, "Can't load the music '%s'\n", name.c_str());
	}
	return music;
}

/**
**  Load a sample
**
**  @param name  File name of sample (short version).
**
**  @return      General sample loaded from file into memory.
**
**  @todo  Add streaming, caching support.
*/
Mix_Chunk *LoadSample(const std::string &name)
{
	const std::string filename = LibraryFileName(name.c_str());
	Mix_Chunk *sample = LoadSample(filename.c_str());

	if (sample == NULL) {
		fprintf(stderr, "Can't load the sound '%s': %s\n", name.c_str(), Mix_GetError());
	}
	return sample;
}

/**
**  Play a sound sample
**
**  @param sample  Sample to play
**
**  @return        Channel number, -1 for error
*/
int PlaySample(Mix_Chunk *sample, Origin *origin)
{
	int channel = -1;
	DebugPrint("play sample %d\n" _C_ sample->volume);
	if (SoundEnabled() && EffectsEnabled && sample) {
		channel = Mix_PlayChannel(-1, sample, 0);
		SetChannelVolume(channel, EffectsVolume);
		Channels[channel].FinishedCallback = NULL;
		if (origin && origin->Base) {
			Origin *source = new Origin;
			source->Base = origin->Base;
			source->Id = origin->Id;
			Channels[channel].Unit = source;
		}
	}
	return channel;
}

/**
**  Set the global sound volume.
**
**  @param volume  the sound volume 0-255
*/
void SetEffectsVolume(int volume)
{
	EffectsVolume = volume;
}

/**
**  Get effects volume
*/
int GetEffectsVolume()
{
	return EffectsVolume;
}

/**
**  Set effects enabled
*/
void SetEffectsEnabled(bool enabled)
{
	EffectsEnabled = enabled;
}

/**
**  Check if effects are enabled
*/
bool IsEffectsEnabled()
{
	return EffectsEnabled;
}

/*----------------------------------------------------------------------------
--  Music
----------------------------------------------------------------------------*/

bool CallbackMusic;                       /// flag true callback ccl if stops

/*----------------------------------------------------------------------------
-- Functions
----------------------------------------------------------------------------*/


/**
**  Check if music is finished and play the next song
*/
void CheckMusicFinished(bool force)
{
	// this races, but that's just fine, since we'll just miss a frame of we're unlucky
	bool proceed = MusicFinished;
	if (!(((proceed || force) && SoundEnabled() && IsMusicEnabled() && CallbackMusic))) {
		return;
	}
	lua_getglobal(Lua, "MusicStopped");
	if (!lua_isfunction(Lua, -1)) {
		fprintf(stderr, "No MusicStopped function in Lua\n");
	} else {
		LuaCall(0, 1);
	}
	MusicFinished = false;
}

/**
**  Callback for when music has finished
**  Note: we are in the sdl audio thread
*/
static void MusicFinishedCallback(int channel)
{
	currentMusicChannel = -1;
	MusicFinished = true;
}

/**
**  Play a music file.
**
**  @param sample  Music sample.
**
**  @return        0 if music is playing, -1 if not.
*/
int PlayMusic(Mix_Chunk *sample)
{
	if (sample) {
		MusicFinished = false;
		Mix_VolumeChunk(sample, MusicVolume);
		int channel = Mix_FadeInChannel(-1, sample, 0, 500);
		Channels[channel].FinishedCallback = MusicFinishedCallback;
		return 0;
	} else {
		DebugPrint("Could not play sample\n");
		return -1;
	}
}

/**
**  Play a music file.
**
**  @param file  Name of music file, format is automatically detected.
**
**  @return      0 if music is playing, -1 if not.
*/
int PlayMusic(const std::string &file)
{
	if (!SoundEnabled() || !IsMusicEnabled()) {
		return -1;
	}
	DebugPrint("play music %s\n" _C_ file.c_str());
	Mix_Chunk *music = LoadMusic(file);
	return PlayMusic(music);
}

/**
**  Stop the current playing music.
*/
void StopMusic()
{
	if (currentMusicChannel) {
		Mix_FadeOutChannel(currentMusicChannel, 500);
	}
}

/**
**  Set the music volume.
**
**  @param volume  the music volume 0-255
*/
void SetMusicVolume(int volume)
{
	MusicVolume = volume;
	if (currentMusicChannel) {
		Mix_Volume(currentMusicChannel, volume);
	}
}

/**
**  Get music volume
*/
int GetMusicVolume()
{
	return MusicVolume;
}

/**
**  Set music enabled
*/
void SetMusicEnabled(bool enabled)
{
	if (enabled) {
		MusicEnabled = true;
	} else {
		MusicEnabled = false;
		StopMusic();
	}
}

/**
**  Check if music is enabled
*/
bool IsMusicEnabled()
{
	return MusicEnabled;
}

/**
**  Check if music is playing
*/
bool IsMusicPlaying()
{
	return !MusicFinished;
}

/*----------------------------------------------------------------------------
--  Init
----------------------------------------------------------------------------*/

/**
**  Check if sound is enabled
*/
bool SoundEnabled()
{
	return SoundInitialized;
}

/**
**  Initialize sound card hardware part with SDL.
**
**  @param freq  Sample frequency (44100,22050,11025 hz).
**  @param size  Sample size (8bit, 16bit)
**
**  @return      True if failure, false if everything ok.
*/
static int InitSdlSound()
{
	// just activate everything we can by setting all bits
	Mix_Init(std::numeric_limits<unsigned int>::max());
	if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 1024)) {
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
		return -1;
	} else {
		printf("Supported sound decoders:");
		for (int i = 0; i < Mix_GetNumChunkDecoders(); i++) {
			printf(" %s", Mix_GetChunkDecoder(i));
		}
		printf("\nSupported music decoders:");
		for (int i = 0; i < Mix_GetNumMusicDecoders(); i++) {
			printf(" %s", Mix_GetMusicDecoder(i));
		}
		printf("\n");
	}
	return 0;
}

/**
**  Initialize sound card.
**
**  @return  True if failure, false if everything ok.
*/
int InitSound()
{
	//
	// Open sound device, 8bit samples, stereo.
	//
	if (InitSdlSound()) {
		SoundInitialized = false;
		return 1;
	}
	MusicFinished = false;
	SoundInitialized = true;
	Mix_AllocateChannels(MaxChannels);
	Mix_ChannelFinished(ChannelFinished);

	// Now we're ready for the callback to run
	Mix_Resume(-1);
	return 0;
}

/**
**  Cleanup sound server.
*/
void QuitSound()
{
	Mix_CloseAudio();
	Mix_Quit();
	SoundInitialized = false;
}

//@}
