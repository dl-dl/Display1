#pragma once

extern int listFtUsbDevices(_Out_ char* s);
extern int slaveDevIdx;
extern bool terminateComm;

DWORD WINAPI ftRecv(LPVOID);