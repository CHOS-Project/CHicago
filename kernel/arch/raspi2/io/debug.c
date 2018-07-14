// File author is Ítalo Lima Marconato Matias
//
// Created on May 12 of 2018, at 00:11 BRT
// Last edited on June 30 of 2018, at 17:05 BRT

#include <chicago/arch/mmio.h>
#include <chicago/arch/uart.h>

Void DbgWriteCharacter(Char c) {
	while (MMIORead(0x3F201018) & (1 << 5)) ;
	MMIOWrite(0x3F201000, c);
}

Void DbgWriteString(Const PChar data) {
	for (UInt32 i = 0; data[i] != '\0'; i++) {
		DbgWriteCharacter(data[i]);
	}
}

Void DbgWriteInteger(UIntPtr data, UInt8 base) {
	if (data == 0) {
		DbgWriteCharacter('0');
		return;
	}
	
	static Char buf[32] = { 0 };
	Int i = 30;
	
	for (; data && i; i--, data /= base) {
		buf[i] = "0123456789ABCDEF"[data % base];
	}
	
	DbgWriteString(&buf[i + 1]);
}

Void DbgWriteFormated(Const PChar data, ...) {
	VariadicList va;
	VariadicStart(va, data);									// Let's start our va list with the arguments provided by the user (if any)
	
	for (UInt32 i = 0; data[i] != '\0'; i++) {
		if (data[i] != '%') {									// It's an % (integer, string, character or other)?
			DbgWriteCharacter(data[i]);							// Nope
		} else {
			switch (data[++i]) {								// Yes! So let's parse it!
			case 's': {											// String
				DbgWriteString((PChar)VariadicArg(va, PChar));
				break;
			}
			case 'c': {											// Character
				DbgWriteCharacter((Char)VariadicArg(va, Int));
				break;
			}
			case 'd': {											// Decimal Number
				DbgWriteInteger((UIntPtr)VariadicArg(va, UIntPtr), 10);
				break;
			}
			case 'x': {											// Hexadecimal Number
				DbgWriteInteger((UIntPtr)VariadicArg(va, UIntPtr), 16);
				break;
			}
			default: {											// None of the others...
				DbgWriteCharacter(data[i]);
				break;
			}
			}
		}
	}
	
	VariadicEnd(va);
}

Void UARTInit(Void) {
	MMIOWrite(0x3F201030, 0);									// Disable UART0
	
	MMIOWrite(0x3F200094, 0);									// Disable pull up/down for all GPIO pins
	MMIODelay(150);												// Delay for 150 cycles
	
	MMIOWrite(0x3F200098, (1 << 14) | (1 << 15));				// Disable pull up/down for pin 14 and 15
	MMIODelay(150);												// Delay for another 1510 cycles
	
	MMIOWrite(0x3F200098, 0);									// Write 0 to GPPUDCLK0 to make it take effect
	
	MMIOWrite(0x3F201044, 0x7FF);								// Clear pending interrupts
	
	MMIOWrite(0x3F201024, 1);									// Set integer part of baud rate
	MMIOWrite(0x3F201028, 40);									// And fractional part of baud rate
	
	MMIOWrite(0x3F20102C, (1 << 4) | (1 << 5) | (1 << 6));		// Enable FIFO & 8 bit data transmission
	
	MMIOWrite(0x3F201038, (1 << 1) | (1 << 4) | (1 << 5) |		// Mask all interrupts
						  (1 << 7) | (1 << 8) | (1 << 9) |
						  (1 << 10));
	
	MMIOWrite(0x3F201030, (1 << 0) | (1 << 8) | (1 << 9));		// Enable UART0
}