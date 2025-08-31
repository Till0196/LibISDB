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
 @file   TSMFFilter.cpp
 @brief  TSMFフィルタ
 @author DBCTRADO
*/


#include "../LibISDBPrivate.hpp"
#include "TSMFFilter.hpp"
#include "../LibISDBConsts.hpp"
#include "../Utilities/Utilities.hpp"
#include "../Utilities/CRC.hpp"
#include "../Base/DebugDef.hpp"


namespace LibISDB
{


TSMFFilter::TSMFFilter()
	: m_TSMFUpdated(false)
{
	Reset();
}


void TSMFFilter::Reset()
{
	BlockLock Lock(m_Lock);

	std::memset(&m_TSMFInfo, 0, sizeof(m_TSMFInfo));
	m_TSMFUpdated = false;
}


bool TSMFFilter::ProcessData(DataStream *pData)
{
	if (pData->Is<TSPacket>()) {
		const TSPacket *pPacket = pData->Get<TSPacket>();
		if (pPacket->GetPID() == PID_TSMF) {
			ProcessTSMFPacket(pPacket);
		}
	}

	return OutputData(pData);
}


void TSMFFilter::ProcessTSMFPacket(const TSPacket *pPacket)
{
	if (pPacket == nullptr) {
		LIBISDB_TRACE(LIBISDB_STR("TSMFFilter: pPacket is null\n"));
		return;
	}

	const uint8_t *pPayload = pPacket->GetPayloadData();
	size_t PayloadSize = pPacket->GetPayloadSize();

	LIBISDB_TRACE(LIBISDB_STR("TSMFFilter: PayloadSize = %zu\n"), PayloadSize);

	if (PayloadSize >= 184) {
		BlockLock Lock(m_Lock);
		if (UpdateTSMFInfo(pPayload)) {
			m_TSMFUpdated = true;
			LIBISDB_TRACE(LIBISDB_STR("TSMFFilter: TSMF info updated - FrameSync = 0x%04x\n"), m_TSMFInfo.FrameSync);
		} else {
			LIBISDB_TRACE(LIBISDB_STR("TSMFFilter: UpdateTSMFInfo failed\n"));
		}
	} else {
		LIBISDB_TRACE(LIBISDB_STR("TSMFFilter: PayloadSize too small (%zu < 184)\n"), PayloadSize);
	}
}


bool TSMFFilter::UpdateTSMFInfo(const uint8_t *pPayload)
{
	if (pPayload == nullptr)
		return false;

	static constexpr uint16_t FRAME_SYNC_MASK = 0x1fff;
	static constexpr uint16_t FRAME_SYNC_F = 0x1a86;
	static constexpr uint16_t FRAME_SYNC_I = ~FRAME_SYNC_F & FRAME_SYNC_MASK;

	// フレーム同期信号検証（ペイロード先頭2バイト）
	uint16_t frame_sync = ((pPayload[0] << 8) | pPayload[1]) & FRAME_SYNC_MASK;
	if (frame_sync != FRAME_SYNC_F && frame_sync != FRAME_SYNC_I)
		return false;

	// バージョン番号
	m_TSMFInfo.VersionNumber = (pPayload[2] & 0xE0) >> 5;

	// 相対ストリーム番号モード
	m_TSMFInfo.RelativeStreamNumberMode = ((pPayload[2] & 0x10) >> 4) != 0;

	// 多重フレーム形式
	m_TSMFInfo.FrameType = pPayload[2] & 0x0f;

	// フレーム同期信号
	m_TSMFInfo.FrameSync = frame_sync;

	// 相対ストリーム番号ごとの情報
	for (int i = 0; i < 15; i++) {
		// ストリーム状態
		m_TSMFInfo.Streams[i].IsActive = ((pPayload[3 + (i / 8)] & (0x80 >> (i % 8))) >> (7 - (i % 8))) != 0;
		
		// ストリーム識別
		m_TSMFInfo.Streams[i].StreamID = (pPayload[5 + (i * 4)] << 8) | pPayload[6 + (i * 4)];
		
		// オリジナルネットワーク識別
		m_TSMFInfo.Streams[i].OriginalNetworkID = (pPayload[7 + (i * 4)] << 8) | pPayload[8 + (i * 4)];
		
		// 受信状態
		m_TSMFInfo.Streams[i].ReceiveStatus = (pPayload[65 + (i / 4)] & (0xc0 >> ((i % 4) * 2))) >> ((3 - (i % 4)) * 2);
	}

	// 緊急警報指示
	m_TSMFInfo.EmergencyIndicator = (pPayload[68] & 0x01) != 0;

	// 相対ストリーム番号
	for (int i = 0; i < 52; i++) {
		if (i < 15) {
			m_TSMFInfo.Streams[i].RelativeStreamNumber = (pPayload[69 + (i / 2)] & (0xf0 >> ((i % 2) * 4))) >> ((1 - (i % 2)) * 4);
		}
	}

	// ストリームタイプ
	uint16_t streamTypeBits = (pPayload[121] << 7) | (pPayload[122] >> 1);
	for (int i = 0; i < 15; ++i) {
		m_TSMFInfo.Streams[i].StreamType = (streamTypeBits >> (14 - i)) & 0x01;
	}

	// 搬送波群の識別
	m_TSMFInfo.GroupID = pPayload[123];

	// 搬送波の総数
	m_TSMFInfo.NumberOfCarriers = pPayload[124];

	// 搬送波の順序
	m_TSMFInfo.CarrierSequence = pPayload[125];

	// フレーム数、フレーム位置
	uint8_t frameRaw = pPayload[126];
	m_TSMFInfo.NumberOfFrames = (frameRaw >> 4) & 0x0F;
	m_TSMFInfo.FramePosition = frameRaw & 0x0F;

	// CRC32
	m_TSMFInfo.CRC = (pPayload[180] << 24) | (pPayload[181] << 16) | (pPayload[182] << 8) | pPayload[183];

	return true;
}

bool TSMFFilter::GetTSMFInfo(ReturnArg<TSMFInfo> Info) const
{
	if (!Info)
		return false;

	BlockLock Lock(m_Lock);

	if (!m_TSMFUpdated)
		return false;

	*Info = m_TSMFInfo;
	return true;
}


} // namespace LibISDB