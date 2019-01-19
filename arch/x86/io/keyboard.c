// File author is Ítalo Lima Marconato Matias
//
// Created on October 25 of 2018, at 15:17 BRT
// Last edited on December 21 of 2018, at 23:09 BRT

#include <chicago/port.h>

UInt8 KbdReadKey(Void)
{
	UInt8 ret = 0;
	
	while (!(PortInByte(0x64) & 1)) ;				// Wait for data
	
	while (PortInByte(0x64) & 1) {					// Read the data
		ret = PortInByte(0x60);
	}

	return ret;
}