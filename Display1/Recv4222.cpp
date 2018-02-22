#include "stdafx.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include "Recv4222.h"
#include "Ftd2xx.h"
#include "LibFT4222.h"
#include "Protocol.h"
#include "Graph.h"

extern void __cdecl _translateSEH(unsigned int, EXCEPTION_POINTERS*);

int slaveDevIdx = 0;
bool terminateComm = false;

static void showMessage(_In_z_ const char* text)
{
	MessageBoxA(NULL, text, "FT4222 Interface", MB_OK);
}

static int processSpiHeader(uint8_t code)
{
	if (0x80 == (code & 0xBB)) // 3-bit data update
		return 3;
	else if (0x90 == (code & 0xB3)) // 4-bit data update
		return 4;
	return 0;
}
/*
static uint8 bitReverse(uint8_t b)
{
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return b;
}
*/
static inline uint8_t processSpiAddr(uint8_t code)
{
	return code;
}

static void processSpiMsg3(_In_ const uint8_t* data, uint8_t addr)
{
	PointDataMsg* p = new PointDataMsg;
	p->addr = addr;
	ASSERT_DBG(SCREEN_DX * 3 <= sizeof(p->data));
	for (size_t i = 0; i < SCREEN_DX; ++i)
	{
		size_t n = (i * 3) / 8;
		size_t m = (i * 3) % 8;
		uint8_t v = data[n];
		v >>= m;
		uint16_t vh = (i < SCREEN_DX - 1) ? data[n + 1] : 0;
		vh <<= (8 - m);
		v |= (uint8_t)vh;
		p->data[i * 3] = ((v >> 2) & 1) * 255;
		p->data[i * 3 + 1] = ((v >> 1) & 1) * 255;
		p->data[i * 3 + 2] = (v & 1) * 255;
	}

	gPostMsg(WM_USER_MSG_LINE_DATA, 0, p);
}

static void processSpiMsg4(_In_ const uint8_t* data, uint8_t addr)
{
	PointDataMsg* p = new PointDataMsg;
	p->addr = addr;
	ASSERT_DBG(SCREEN_DX * 3 <= sizeof(p->data));
	for (size_t i = 0; i < SCREEN_DX; ++i)
	{
		size_t n = (i * 4) / 8;
		size_t m = (i * 4) % 8;
		const uint8_t v = data[n] >> m;
		p->data[i * 3] = ((v >> 2) & 1) * 255;
		p->data[i * 3 + 1] = ((v >> 1) & 1) * 255;
		p->data[i * 3 + 2] = (v & 1) * 255;
	}

	gPostMsg(WM_USER_MSG_LINE_DATA, 0, p);
}

DWORD WINAPI ftRecv(LPVOID)
{
	_set_se_translator(_translateSEH);
	try
	{
		DWORD numOfDevices = 0;
		if (FT_CreateDeviceInfoList(&numOfDevices) != FT_OK)
			throw "FT_CreateDeviceInfoList failed";
		if ((int)numOfDevices <= slaveDevIdx)
			throw "Slave not found";

		FT_DEVICE_LIST_INFO_NODE devInfo = { 0 };

		if (FT_GetDeviceInfoDetail(slaveDevIdx, &devInfo.Flags, &devInfo.Type, &devInfo.ID, &devInfo.LocId,
								   devInfo.SerialNumber, devInfo.Description, &devInfo.ftHandle) != FT_OK)
			throw "FT_GetDeviceInfoDetail failed";

		FT_HANDLE ftHandle = NULL;
		if (FT_OpenEx((PVOID)devInfo.LocId, FT_OPEN_BY_LOCATION, &ftHandle) != FT_OK)
			throw "FT_OpenEx failed";
		devInfo.ftHandle = ftHandle;

		if (FT_SetTimeouts(ftHandle, 1000, 1000) != FT_OK)
			throw "FT_SetTimeouts failed";
		if (FT_SetLatencyTimer(ftHandle, 0) != FT_OK)
			throw "FT_SetLatencyTimer failed";
		if (FT_SetUSBParameters(ftHandle, 64 * 1024, 0) != FT_OK)
			throw "FT_SetUSBParameters failed!";
		if (FT4222_SPISlave_InitEx(ftHandle, SPI_SLAVE_NO_PROTOCOL) != FT4222_OK)
			throw "FT4222_SPISlave_InitEx failed";
		if (FT4222_SPI_SetDrivingStrength(ftHandle, DS_12MA, DS_16MA, DS_16MA) != FT4222_OK)
			throw "FT4222_SPI_SetDrivingStrength failed";

		HANDLE hEvent = CreateEvent(NULL, false, false, NULL);
		if (NULL == hEvent)
			throw "CreateEvent failed";

		if (FT_SetEventNotification(ftHandle, FT_EVENT_RXCHAR, hEvent) != FT_OK)
			// if ( FT4222_SetEventNotification(FtHandle, FT4222_EVENT_RXCHAR, hEvent) != FT_OK ) // does not work
			throw "FT_SetEventNotification failed";

		const uint16_t headerSz = 2;
		const uint16_t tailSz = 2;
		const uint16_t dataSz3 = SCREEN_DX * 3 / 8 + tailSz;
		const uint16_t dataSz4 = SCREEN_DX * 4 / 8 + tailSz;
		ASSERT_DBG(0 == (SCREEN_DX * 3) % 8);
		uint8_t rxHeader[headerSz];
		uint8_t rxBuffer[1024];
		while (!terminateComm)
		{
			WaitForSingleObject(hEvent, 1000);

			uint16_t rxSize = 0;
			if (FT4222_SPISlave_GetRxStatus(ftHandle, &rxSize) != FT_OK)
				throw "FT4222_SPISlave_GetRxStatus failed";

			while (rxSize >= headerSz + min(dataSz3, dataSz4))
			{
				uint16_t sizeTransferred = 0;
				if (FT4222_SPISlave_Read(ftHandle, rxHeader, headerSz, &sizeTransferred) != FT_OK)
					throw "FT4222_SPISlave_Read failed";
				if (headerSz != sizeTransferred)
					throw "Header Read Failed";
				rxSize -= sizeTransferred;
				int mode = processSpiHeader(rxHeader[0]);
				uint8_t addr = processSpiAddr(rxHeader[1]);
				ASSERT_DBG(addr < SCREEN_DY);

				if ((3 == mode) && (rxSize >= dataSz3))
				{
					if (FT4222_SPISlave_Read(ftHandle, rxBuffer, dataSz3, &sizeTransferred) != FT_OK)
						throw "FT4222_SPISlave_Read failed";
					if (dataSz3 != sizeTransferred)
						throw "Data Read Failed";
					rxSize -= sizeTransferred;
					processSpiMsg3(rxBuffer, addr);
				}
				else if ((4 == mode) && (rxSize >= dataSz4))
				{
					if (FT4222_SPISlave_Read(ftHandle, rxBuffer, dataSz4, &sizeTransferred) != FT_OK)
						throw "FT4222_SPISlave_Read failed";
					if (dataSz4 != sizeTransferred)
						throw "Data Read Failed";
					rxSize -= sizeTransferred;
					processSpiMsg4(rxBuffer, addr);
				}
			}
			while (rxSize) // normally should be 0
			{
				uint16_t sizeTransferred = 0;
				if (FT4222_SPISlave_Read(ftHandle, rxBuffer, 1, &sizeTransferred) != FT_OK)
					throw "FT4222_SPISlave_Read failed";
				rxSize -= sizeTransferred;
			}
		}
		FT_Close(ftHandle);
		CloseHandle(hEvent);
	}
	catch (const char* err)
	{
		showMessage(err);
	}
	catch (...)
	{
		showMessage("General error");
	}

	return 0;
}

static const char* deviceFlag1ToString(DWORD flags)
{
	return (flags & 0x1) ? "DEVICE_OPEN" : "DEVICE_CLOSED";
}

static const char* deviceFlag2ToString(DWORD flags)
{
	return (flags & 0x2) ? "High-speed USB" : "Full-speed USB";
}

int listFtUsbDevices(_Out_ char* s)
{
	DWORD numOfDevices = 0;
	FT_STATUS ftStatus = FT_CreateDeviceInfoList(&numOfDevices);

	for (DWORD iDev = 0; iDev < numOfDevices; ++iDev)
	{
		FT_DEVICE_LIST_INFO_NODE devInfo;
		memset(&devInfo, 0, sizeof(devInfo));

		ftStatus = FT_GetDeviceInfoDetail(iDev, &devInfo.Flags, &devInfo.Type, &devInfo.ID, &devInfo.LocId,
										  devInfo.SerialNumber, devInfo.Description, &devInfo.ftHandle);

		if (FT_OK == ftStatus)
		{
			s += wsprintfA(s, "Dev %d:\r\n", iDev);
			s += wsprintfA(s, "  Flags= 0x%x, (%s, %s)\r\n", devInfo.Flags, deviceFlag1ToString(devInfo.Flags), deviceFlag2ToString(devInfo.Flags));
			s += wsprintfA(s, "  Type= 0x%x    ", devInfo.Type);
			s += wsprintfA(s, "  ID= 0x%x    ", devInfo.ID);
			s += wsprintfA(s, "  LocId= 0x%X    ", devInfo.LocId);
//			s += wsprintfA(s, "  SerialNumber= %s     ", devInfo.SerialNumber);
			s += wsprintfA(s, "  Description= %s\r\n", devInfo.Description);
//			s += wsprintfA(s, "  ftHandle= 0x%x\r\n", (int)devInfo.ftHandle);
		}
	}
	return numOfDevices;
}
