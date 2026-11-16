// Modified 2026-11-16 for the MSVC 2022 build; see MODIFICATIONS.md.
/* Copyright (c) 2020  Eric Milles <eric.milles@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "Output.h"
#include "Debugger.h"
#include "MessageDialog.h"
#include "OptionsDialog.h"
#include "OptionsUtils.h"
#include "OutputComponent.h"
#include "Platform.h"
#include "Plugin.h"
#include "Resources.h"
#include "WinampHooks.h"
#include "WinampPlayer.h"
#include <cassert>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <shlobj.h>
#include <wa_ipc.h>


#define ASSERT_NOT_NULL(ptr) if ((ptr).get() == NULL) throw (#ptr " is null")

#define GetWindowProc(hWnd) \
	(WNDPROC) (!IsWindow(hWnd) ? NULL : GetWindowLongPtr((hWnd), GWLP_WNDPROC))

#define SetWindowProc(hWnd, wndProc) \
	(WNDPROC) (!IsWindow(hWnd) ? NULL : SetWindowLongPtr((hWnd), GWLP_WNDPROC, (LONG_PTR) (wndProc)))


//------------------------------------------------------------------------------


static const std::string INVALID_TITLE("** INVALID TITLE **"); // sentinel value

static UINT metadataScheduleMessage()
{
	static const UINT message = RegisterWindowMessageW(L"Remote Speakers Output.DeferredMetadata");
	return message;
}

extern HINSTANCE dllInstance;
extern std::auto_ptr<WinampPlayer> player;

OutputComponentPtr Output::_outputComponent(NULL);
PlayerWindowHookPtr Output::_playerWindowHook(NULL);
PlayerWindowProcPtr Output::_playerWindowProc(NULL);

Output::Progress Output::_bufferProgress(0,0);
Output::Progress Output::_outputProgress(0,0);
Output::Progress::second_type Output::_msecDivisor(0);

std::string Output::_iniFilePath;
std::time_t Output::_resetToZero(0);
std::string Output::_titleString(INVALID_TITLE);
std::atomic<bool> Output::_metadataSessionReady(false);
std::atomic<unsigned> Output::_metadataRefreshAttempts(0);


//------------------------------------------------------------------------------


void Output::init()
{
	metadataScheduleMessage(); // Register on the host UI thread before subclassing.
	Debugger::print("Winamp Init: core build " + Plugin::version());
	try
	{
		// initialize plug-in options file path
		const char* const pathString = reinterpret_cast<const char*>(
			SendMessage(player->window(), WM_WA_IPC, 0, IPC_GETINIFILE));
		if (pathString != NULL && pathString[0] != '\0')
		{
			_iniFilePath.assign(pathString);
		}
		else
		{
			CHAR pathBuffer[MAX_PATH];
			if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, pathBuffer)))
			{
				_iniFilePath.assign(pathBuffer);
				_iniFilePath.append("\\");
				_iniFilePath.append(Plugin::name());
				_iniFilePath.append("\\");

				SHCreateDirectoryExA(NULL, _iniFilePath.c_str(), NULL);
			}
			else
			{
				DWORD pathLength = GetModuleFileNameA(dllInstance, pathBuffer, MAX_PATH);
				if (pathLength > 0 && pathLength <= MAX_PATH)
				{
					const std::string pathString(pathBuffer, pathLength);
					const std::string::size_type pos = pathString.find_last_of('\\');
					if (pos != std::string::npos)
						_iniFilePath.assign(pathString.substr(0, pos + 1));
				}
			}

			_iniFilePath.append("plugin.ini");
		}

		// load plug-in options from file
		OptionsUtils::loadOptions(_iniFilePath);
		Debugger::printf("Artwork diagnostics: EnableArtwork=%u.",
			GetPrivateProfileIntA(Plugin::name().c_str(), "EnableArtwork", 0, _iniFilePath.c_str()));

		_outputComponent.reset(new OutputComponent(*player));
		_outputComponent->setProgressCallback(&Output::onBytesOutput);
		Debugger::print("Winamp Init: output component ready.");

		// sniff for additional player capabilities...

		if (SendMessage(player->window(), WM_WA_IPC, 0, IPC_GETVERSION))
		{
			// subclass player window to listen for metadata changes
			_playerWindowProc = SetWindowProc(player->window(), &playerWindowProc);

			if (SendMessage(player->window(), WM_WA_IPC, 0, IPC_GETVERSIONSTRING))
			{
				// hook player window's message chain to add device submenu
				_playerWindowHook.reset(new WinampHooks(player->window()));
			}
		}
	}
	CATCH_ALL
}


void Output::quit()
{
	_metadataSessionReady = false;
	KillTimer(player->window(), reinterpret_cast<UINT_PTR>(&Output::metadataTimerProc));
	try
	{
		OutputComponentPtr outputComponent(_outputComponent);
		PlayerWindowHookPtr playerWindowHook(_playerWindowHook);

		// restore previous player window procedure if possible
		if (GetWindowProc(player->window()) == &playerWindowProc)
		{
			SetWindowProc(player->window(), _playerWindowProc);
			_playerWindowProc = NULL;
		}

		// save plug-in options to file
		OptionsUtils::saveOptions(_iniFilePath);
	}
	CATCH_ALL
}


void Output::about(HWND parentWindow)
{
	MessageDialog(Plugin::aboutText(), 0).doModal(parentWindow);
}


void Output::options(HWND parentWindow)
{
	DialogBox(dllInstance, MAKEINTRESOURCE(DIALOG_OPTIONS), parentWindow, &optionsDialogProc);
}


INT_PTR Output::optionsDialogProc(HWND dialogWindow, UINT message, WPARAM wParam, LPARAM lParam)
{
	static std::auto_ptr<OptionsDialog> optionsDialog;

	switch (message)
	{
	case WM_INITDIALOG:
		CenterWindowOverParent(dialogWindow);

		// create plug-in options pane within player-provided dialog box
		optionsDialog.reset(new OptionsDialog);
		optionsDialog->setStatusCallback(
			// create callback that will update apply button
			std::bind(
				SetWindowEnabled,
				GetDlgItem(dialogWindow, IDOK),
				std::placeholders::_1));
		optionsDialog->doModeless(
			GetDlgItem(dialogWindow, DIALOG_OPTIONS_CHILD_FRAME));
		break;

	case WM_DESTROY:
		optionsDialog.reset();
		break;

	case WM_CLOSE:
		EndDialog(dialogWindow, 0);
		break;

	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED)
		{
			switch (LOWORD(wParam))
			{
			case IDOK:
				optionsDialog->doApply();
				OptionsUtils::saveOptions(_iniFilePath);
				break;
			case IDCANCEL:
				EndDialog(dialogWindow, 0);
				break;
			}
		}
		break;
	}

	return 0;
}


//------------------------------------------------------------------------------


int Output::open(const int samplesPerSecond, const int channelCount,
	const int bitsPerSample, const int bufferLengthMilliseconds,
	const int preBufferMilliseconds)
{
	Debugger::printf("Winamp Open: %d Hz, %d channels, %d bits.",
		samplesPerSecond, channelCount, bitsPerSample);
	try
	{
		_resetToZero = 0;
		_metadataSessionReady = false;
		KillTimer(player->window(), reinterpret_cast<UINT_PTR>(&Output::metadataTimerProc));
		_titleString = INVALID_TITLE;

		// query player for metadata
		std::string title, album, artist;
		buffer_t    artworkData;
		std::string artworkType;
		shorts_t    playlistPos;
		Debugger::print("Winamp Open: postponing metadata until playback starts.");
		const time_t length = player->getPlaybackMetadata(
			title, album, artist, artworkData, artworkType, playlistPos, false);
		Debugger::print("Winamp Open: track metadata retrieved.");

		const OutputFormat format(SampleRate(samplesPerSecond),
			SampleSize(bitsPerSample / 8), ChannelCount(channelCount));
		const OutputMetadata metadata(length,
			title, album, artist, playlistPos, artworkData, artworkType);

		ASSERT_NOT_NULL(_outputComponent);

		Debugger::print("Winamp Open: opening output session.");
		_outputComponent->open(format, metadata);
		Debugger::print("Winamp Open: output session opened.");

		_titleString = title;

		// initialize progress counters
		_bufferProgress = _outputProgress = Progress(0, 0);
		_msecDivisor = static_cast<Progress::second_type>(
			samplesPerSecond * (bitsPerSample / 8) * channelCount) / 1000.0;

		// return playback latency
		const time_t latency = _outputComponent->latency();
		Debugger::printf("Winamp Open: ready, latency %lld ms.", static_cast<long long>(latency));
		assert(latency <= static_cast<time_t>(std::numeric_limits<int>::max()));
		return static_cast<int>(latency);
	}
	CATCH_ALL

	Debugger::print("Winamp Open: FAILED; returning -1 to Winamp. See previous exception.");
	return -1;
}


void Output::close()
{
	_metadataSessionReady = false;
	KillTimer(player->window(), reinterpret_cast<UINT_PTR>(&Output::metadataTimerProc));
	try
	{
		ASSERT_NOT_NULL(_outputComponent);

		_outputComponent->close();
	}
	CATCH_ALL

	_resetToZero = 0;
	_titleString = INVALID_TITLE;
}


void Output::reset(const int offset)
{
	try
	{
		ASSERT_NOT_NULL(_outputComponent);

		if (offset > 0 || (offset == 0 && _resetToZero))
		{
			_resetToZero = 0;

			_outputComponent->reset(offset);

			_bufferProgress = _outputProgress = Progress(offset, 0);
		}
		else if (offset == 0) // some players always pass zero
		{
			// delay reset so new playback position can be read

			// capture time to distinguish between reset sequence
			// (pause, reset, unpause) and user pausing/unpausing
			_resetToZero = std::time(NULL);
		}
	}
	CATCH_ALL
}


int Output::write(char* const buffer, const int length)
{
	try
	{
		ASSERT_NOT_NULL(_outputComponent);

		if (length >= 0 && length <= canWrite() &&
			_outputComponent->write((byte_t*) buffer, length))
		{
			updateProgress(_bufferProgress, length);
			// First PCM data means the input decoder has finished its Open path.
			if (length > 0 && !_metadataSessionReady.exchange(true))
			{
				_metadataRefreshAttempts = 0;
				const UINT message = metadataScheduleMessage();
				if (!message || !PostMessage(player->window(), message, 0, 0))
					Debugger::printLastError("Scheduling deferred metadata");
			}
			return 0;
		}
		else
		{
			return 1;
		}
	}
	CATCH_ALL

	return -1;
}


int Output::canWrite()
{
	try
	{
		if (_resetToZero) reset(0);

		ASSERT_NOT_NULL(_outputComponent);

		const size_t size = _outputComponent->canWrite();
		assert(size <= static_cast<size_t>(std::numeric_limits<int>::max()));
		return static_cast<int>(size);
	}
	CATCH_ALL

	return 0;
}


int Output::isPlaying()
{
	try
	{
		ASSERT_NOT_NULL(_outputComponent);

		return (_outputComponent->buffered() > 0);
	}
	CATCH_ALL

	return 0;
}


int Output::getOutputTime()
{
	// return millisecond counter from output progress
	return _outputProgress.first;
}


int Output::getWrittenTime()
{
	// return millisecond counter from buffer progress
	return _bufferProgress.first;
}


int Output::setPaused(const int state)
{
	try
	{
		ASSERT_NOT_NULL(_outputComponent);

		// check for delayed reset sequence
		if (_resetToZero == std::time(NULL))
		{
			reset((int) player->getPlaybackPosition());
		}
		_resetToZero = 0;

		return _outputComponent->setPaused(state != 0);
	}
	CATCH_ALL

	return 0;
}


void Output::setVolume(const int volume)
{
	try
	{
		ASSERT_NOT_NULL(_outputComponent);

		if (volume >= 0 && volume <= 255)
		{
			float decibels = -100.0f;

			if (volume > 0)
			{
				// translate volume from [1,255] to (-40.0,0.0]
				decibels = float(((volume / 255.0) - 1.0) * 40.0);
			}

			_outputComponent->setVolume(decibels);
		}
	}
	CATCH_ALL
}


void Output::setBalance(const int balance)
{
	try
	{
		ASSERT_NOT_NULL(_outputComponent);

		if (balance >= -128 && balance <= 128)
		{
			_outputComponent->setBalance(float(balance / 128.0));
		}
	}
	CATCH_ALL
}


void Output::onBytesOutput(const size_t bytes)
{
	updateProgress(_outputProgress, bytes);
}


void Output::updateProgress(Progress& progress, const size_t bytes)
{
	// increment byte counter
	progress.second += bytes;
	// increment millisecond counter with whole milliseconds
	progress.first += static_cast<Progress::first_type>(
		std::floor(progress.second / _msecDivisor));
	// update byte counter with remainder from bytes to milliseconds conversion
	progress.second = std::fmod(progress.second, _msecDivisor);
}


void Output::refreshMetadata()
{
	if (!_metadataSessionReady.load()) return;
	static thread_local bool refreshing = false;
	if (refreshing) return;
	struct Guard {
		bool& value;
		explicit Guard(bool& flag) : value(flag) { value = true; }
		~Guard() { value = false; }
	} guard(refreshing);
	const LRESULT state = SendMessage(player->window(), WM_WA_IPC, 0, IPC_ISPLAYING);
	if (state != 1 && state != 3) return;
	try
	{
		Debugger::print("Processing title update notification from player...");

		// re-query player for metadata
		std::string title, album, artist;
		buffer_t    artworkData;
		std::string artworkType;
		shorts_t    playlistPos;
		const time_t length = player->getPlaybackMetadata(
			title, album, artist, artworkData, artworkType, playlistPos, true,
			GetPrivateProfileIntA(Plugin::name().c_str(), "EnableArtwork", 0, _iniFilePath.c_str()) != 0);
		const time_t offset = player->getPlaybackPosition();

		Debugger::printf(
			"New title is \"%s\"; last title sent to remote speakers was \"%s\".",
			title.c_str(), _titleString.c_str());

		// check for a change in the title but ignore buffer and connect updates
		if (_titleString != title && !title.empty() && title[0] != '[')
		{
			ASSERT_NOT_NULL(_outputComponent);

			const OutputMetadata metadata(
				length, title, album, artist, playlistPos, artworkData, artworkType);

			_outputComponent->setMetadata(metadata, offset);

			_titleString = title;
		}
	}
	CATCH_ALL
}


void CALLBACK Output::metadataTimerProc(HWND window, UINT, UINT_PTR id, DWORD)
{
	if (!_metadataSessionReady.load() || ++_metadataRefreshAttempts > 20)
	{
		KillTimer(window, id);
		return;
	}
	const LRESULT state = SendMessage(window, WM_WA_IPC, 0, IPC_ISPLAYING);
	if (state != 1 && state != 3) return;
	refreshMetadata();
	if (!_titleString.empty()) KillTimer(window, id);
}


LRESULT Output::playerWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	const UINT scheduleMessage = metadataScheduleMessage();
	if (scheduleMessage && message == scheduleMessage)
	{
		if (_metadataSessionReady.load() &&
			!SetTimer(window, reinterpret_cast<UINT_PTR>(&Output::metadataTimerProc), 250, NULL))
			Debugger::printLastError("Creating UI-thread metadata timer");
		return 0;
	}
	if (message == WM_TIMER && wParam == reinterpret_cast<UINT_PTR>(&Output::metadataTimerProc))
	{
		metadataTimerProc(window, message, wParam, 0);
		return 0;
	}
	LRESULT result = CallWindowProc(_playerWindowProc, window, message, wParam, lParam);
	if (message == WM_WA_IPC && lParam == IPC_CB_MISC && wParam == IPC_CB_MISC_TITLE)
		refreshMetadata();
	return result;
}
