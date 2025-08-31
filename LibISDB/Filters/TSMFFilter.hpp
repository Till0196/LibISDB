/*
  LibISDB
  Copyright(c) 2017-2020 DBCTRADO

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/**
 @file   TSMFFilter.hpp
 @brief  TSMFフィルタ
 @author DBCTRADO
*/


#ifndef LIBISDB_TSMF_FILTER_H
#define LIBISDB_TSMF_FILTER_H


#include "FilterBase.hpp"
#include "../TS/TSPacket.hpp"


namespace LibISDB
{

	/** TSMFフィルタクラス */
	class TSMFFilter
		: public SingleIOFilter
	{
	public:
		struct StreamDetail {
			uint16_t StreamID;
			uint16_t OriginalNetworkID;
			uint8_t ReceiveStatus;
			uint8_t StreamType;  // 0=TS, 1=TLV
			uint8_t RelativeStreamNumber;
			bool IsActive;
		};

		struct TSMFInfo {
			uint16_t FrameSync;
			uint8_t VersionNumber;
			bool RelativeStreamNumberMode;
			uint8_t FrameType;
			uint16_t StreamStatus;
			bool EmergencyIndicator;
			bool EarthquakeEarlyWarning;
			uint8_t GroupID;
			uint8_t NumberOfCarriers;
			uint8_t CarrierSequence;
			uint8_t NumberOfFrames;
			uint8_t FramePosition;
			uint32_t CRC;
			StreamDetail Streams[15];
		};

		TSMFFilter();

	// ObjectBase
		const CharType * GetObjectName() const noexcept override { return LIBISDB_STR("TSMFFilter"); }

	// FilterBase
		void Reset() override;

	// SingleIOFilter
		bool ProcessData(DataStream *pData) override;

	// TSMFFilter
		bool GetTSMFInfo(ReturnArg<TSMFInfo> Info) const;

	protected:
		void ProcessTSMFPacket(const TSPacket *pPacket);
		bool UpdateTSMFInfo(const uint8_t *pPayload);
		uint32_t ExtractBits(const uint8_t *data, int bitOffset, int numBits);

		mutable MutexLock m_Lock;
		TSMFInfo m_TSMFInfo;
		bool m_TSMFUpdated;
	};

} // namespace LibISDB


#endif // ifndef LIBISDB_TSMF_FILTER_H