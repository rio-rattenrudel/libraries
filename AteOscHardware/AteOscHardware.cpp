//AteOscHardware.cpp  Atmegatron Euro Oscitron Hardware control.
//Copyright (C) 2015  Paul Soulsby info@soulsbysynths.com
//
//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "AteOscHardware.h"

//#define DEBUG 1

static volatile unsigned char audioCurrent = 0;

static const char rotEncoderIncrementAmount[16] PROGMEM = {0,1,-1,2,-1,0,-2,1,1,-2,0,-1,2,-1,1,0};
static const unsigned char rotEncoderBitMask[2] PROGMEM = {0x30,0x84};
static const unsigned char rotEncoderPins[2][2] PROGMEM = {{PIND4,PIND5},{PIND7,PIND2}};

static volatile int rotEncoderCount[2] = {0};
static volatile unsigned char switchState[3] = {0};
static volatile char audioBuffer[AUDIO_BUFFER_SIZE] = {0};
static volatile unsigned char audioWriteIndex = 0;
static volatile unsigned char audioMinLength = 4;
static volatile AteOscHardware::AudioBufferStatus audioBufferStatus =  AteOscHardware::BUFFER_IDLE;

static volatile bool i2cTxing = false;
static volatile bool i2cRxing = false;
static volatile unsigned char i2cWritePos = 0;
static volatile unsigned char i2cReadPos = 0;
static volatile unsigned char i2cAddr = 0;
static volatile unsigned char i2cBuffer[I2C_BUFFER_SIZE] = {0};

//static const unsigned char pinReadOrder[CV_READ_ORDER_SIZE] PROGMEM = {0,1,2,3,4,5,2,3,6,7,2,3}; //{4,5,0,1,6,7}; //{4,4,4,4,4,4};

#define TW_TX_NACK (1 << TWINT) | (1 << TWEN) | (1 << TWIE)
#define TW_TX_ACK (1 << TWINT) | (1 << TWEN) | (1 << TWIE) | (1 << TWEA)
#define TW_STOP (1 << TWSTO) | (1 << TWINT) | (1 << TWEN)

void writeMemory(const void* data, void* startAddr, size_t size)
{
	eeprom_update_block(data, startAddr, size );
}

void readMemory(void* data, const void* startAddr, size_t size)
{
	eeprom_read_block(data,startAddr,size);

}

void writeFram(const void* data, unsigned int startAddr, size_t size)
{
	unsigned char i;
	while (i2cTxing==true || i2cRxing==true || bitRead(TWCR,TWSTO)==true)
	{
	}
	i2cBuffer[0] = startAddr >> 8;
	i2cBuffer[1] = startAddr & 0xFF;
	memcpy((void*)&i2cBuffer[2],data,size);
	i2cWritePos = size + 2;
	i2cTxing = true;
	i2cAddr = TW_WRITE;
	i2cAddr |= FM24C64B_DEFAULT_ADDRESS << 1;
	TWCR = ((1 << TWSTA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE) | (1 << TWEA));
}

void readFram(void* data, unsigned int startAddr, size_t size)
{
	while (i2cTxing==true || i2cRxing==true || bitRead(TWCR,TWSTO)==true)
	{
	}
	i2cBuffer[0] = startAddr >> 8;
	i2cBuffer[1] = startAddr & 0xFF;
	i2cWritePos = 2;
	i2cTxing = true;
	i2cAddr = TW_WRITE;
	i2cAddr |= FM24C64B_DEFAULT_ADDRESS << 1;
	TWCR = ((1 << TWSTA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE) | (1 << TWEA));
	while (i2cTxing==true || bitRead(TWCR,TWSTO)==true)
	{
	}
	i2cWritePos = size;
	i2cRxing = true;
	i2cAddr = TW_READ;
	i2cAddr |= FM24C64B_DEFAULT_ADDRESS << 1;
	TWCR = ((1 << TWSTA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE) | (1 << TWEA));
	while (i2cRxing==true || bitRead(TWCR,TWSTO)==true)
	{
	}
	memcpy(data,(const void*)i2cBuffer,size);
}
bool getFramBusy()
{
	if(i2cTxing || i2cRxing)
	{
		return true;
	}
	else
	{
		return false;
	}
}
// default constructor
AteOscHardware::AteOscHardware(AteOscHardwareBase* base)
{
	construct(base);
} //AteOscHardware

// default destructor
AteOscHardware::~AteOscHardware()
{
	if(base_!=NULL)
	{
		delete base_;
	}

} //~AteOscHardware

void AteOscHardware::construct(AteOscHardwareBase* base)
{
	unsigned char i,j,a;
	
	base_ = base;

	cli();
	
	beginSpi();
	
	//setup analogue controls
	//init variables
	readCvCalib(EEPROM_PITCH_LOW);
	readCvCalib(EEPROM_PITCH_HIGH);
	readCvCalib(EEPROM_FILT_LOW);
	readCvCalib(EEPROM_FILT_HIGH);

	for(a=0;a<INPUTS;++a)
	{
		if(inputMode_[a]==IP_CV)
		{
			if(a==CV_PITCH || a==CV_FILT)
			{
				i = ((a == CV_PITCH) ? 0 : 1);
				long mapped = (((long)readMCP3208input(a) - cvCalib_[i][0]) << 11) / cvCalib_[i][2] + 1024;  //outmax = 3072 = 3.75V, outmin = 1024 = 1.25V, max-min = 2048 (<<11)
				if(mapped<0)
				{
					cvInput_[a] = CvInput(0);
				}
				else if (mapped>4095)
				{
					cvInput_[a] = CvInput(4095);
				}
				else
				{
					cvInput_[a] = CvInput((unsigned int)mapped);
				}
			}
			else
			{
				cvInput_[a] = CvInput(readMCP3208input(a));
			}
		}
		else
		{
			cvInput_[a] = CvInput(0);
		}
		if(inputMode_[a]==IP_GATE)
		{
			gateInput_[a] = readMCP3208input(a) > CV_HALF_SCALE ? true : false;
		}
		else
		{
			gateInput_[a] = 0;  //already done in header in fact
		}
	}
	cvInput_[CV_PITCH].setLockOut(false);
	cvInput_[CV_FILT].setLockOut(false);
	
	//Start_ADC interuupt and first conversion.;
	ADMUX = AUDIO_INPUT;  // set the analog reference (high two bits of ADMUX) and select the channel (low 4 bits).  this also sets ADLAR (left-adjust result) to 0 (the default).
	bitSet(ADMUX,REFS0);
	bitSet(ADMUX,ADLAR);

	//setup rotary encoders
	for(i=0;i<2;i++)
	{
		for(j=0;j<2;j++)
		{
			bitClear(DDRD,pgm_read_byte(&(rotEncoderPins[i][j])));
			bitSet(PORTD,pgm_read_byte(&(rotEncoderPins[i][j])));
		}
	}
	//setup pins
	//PB0 = function, PB1 = value, PD6 = bank, PD3 = audio output
	bitClear(DDRB,PINB0);
	bitClear(PORTB,PINB0);
	bitClear(DDRB,PINB1);
	bitClear(PORTB,PINB1);
	bitClear(DDRD,PIND6);
	bitClear(PORTD,PIND6);
	bitSet(DDRD,PIND3);
	bitClear(PORTD,PIND3);
	//initialize variables
	unsigned char invPinB = ~PINB;
	switchState[0] = bitRead(invPinB,PINB0);
	switch_[0] = Switch(bitRead(invPinB,PINB0),HOLD_EVENT_TICKS);
	
	switchState[1] = bitRead(invPinB,PINB1);
	switch_[1] = Switch(bitRead(invPinB,PINB1),HOLD_EVENT_TICKS);
	unsigned char invPind = ~PIND;
	switchState[2] = bitRead(invPind,PIND6);
	switch_[2] = Switch(bitRead(invPind,PIND6),HOLD_EVENT_TICKS);
	#ifndef _DEBUG!=1
	//PCINT0 = function, PCINT1 = value, PCINT22 = bank, PCINT20 = func rotEncoder_ A, PCINT21 = func rotEncoder_ B, PCINT23 = val rotEncoder_ A, PCINT18 = val rotEncoder_ B
	bitSet(PCMSK0,PCINT0);
	bitSet(PCMSK0,PCINT1);
	bitSet(PCIFR,PCIF0);
	bitSet(PCICR,PCIE0);
	bitSet(PCMSK2,PCINT18);
	bitSet(PCMSK2,PCINT20);
	bitSet(PCMSK2,PCINT21);
	bitSet(PCMSK2,PCINT22);
	bitSet(PCMSK2,PCINT23);
	bitSet(PCIFR,PCIF2);
	bitSet(PCICR,PCIE2);
	#endif

	
	rotEncoder_[FUNCTION].setMaxValue(8);
	
	sei();
	
	beginI2c();
	for(i=0;i<2;++i)
	{
		writeI2cByte(i,MCP23017_IODIRA,0x00);
		writeI2cByte(i,MCP23017_IODIRB,0x00);
	}

}
void AteOscHardware::calcCvCalib(unsigned int eepromAddress)
{
	const unsigned char CV_READS = 8;
	unsigned char calibVal[2] = {0};
	unsigned long calibRead = 0;
	unsigned char cvIp, i, j;
	if(eepromAddress==EEPROM_PITCH_LOW || eepromAddress==EEPROM_PITCH_HIGH)
	{
		cvIp = CV_PITCH;
	}
	else
	{
		cvIp = CV_FILT;
	}
	i = (eepromAddress - EEPROM_PITCH_LOW) >> 2;
	j = (eepromAddress - EEPROM_PITCH_LOW - (i<<2)) >> 1;
	for(unsigned char i=0;i<CV_READS;++i)
	{
		calibRead += readMCP3208input(cvIp);
	}
	calibRead = calibRead / CV_READS;
	calibVal[0] = (unsigned char)((calibRead >> 8) & 0xFF);  //msb
	calibVal[1] = (unsigned char)(calibRead & 0xFF);  //lsb
	writeMemory((const void*)calibVal,(void*)eepromAddress,sizeof(calibVal));
	cvCalib_[i][j] = (unsigned int)calibRead;
	cvCalib_[i][2] = cvCalib_[i][1] - cvCalib_[i][0];  //used in mapping equation
}
void AteOscHardware::readCvCalib(unsigned int eepromAddress)
{
	unsigned char i,j;
	unsigned char data[2] = {0};
	i = (eepromAddress - EEPROM_PITCH_LOW) >> 2;
	j = (eepromAddress - EEPROM_PITCH_LOW - (i<<2)) >> 1;
	readMemory((void*)data,(const void*)eepromAddress,sizeof(data));
	cvCalib_[i][j] = ((unsigned int)data[0]<<8) + data[1];
	cvCalib_[i][2] = cvCalib_[i][1] - cvCalib_[i][0];  //used in mapping equation
}
unsigned char AteOscHardware::readEepromByte(unsigned int address)
{
	unsigned char data[1];
	readMemory((void*)data,(const void*)address,sizeof(data));
	return data[0];
}
void AteOscHardware::writeEepromByte(unsigned int address, unsigned char newValue)
{
	unsigned char data[1] = {newValue};
	writeMemory((const void*)data,(void*)address,sizeof(data));
}

char AteOscHardware::getAudioBuffer(unsigned char sample)
{
	return audioBuffer[sample];
}
unsigned char AteOscHardware::getAudioBufferLength()
{
	return audioWriteIndex;  //audioWriteIndex starts and stops on zero cross, so top index is audioWriteIndex-1 and len = audioWriteIndex
}
unsigned char AteOscHardware::getAudioMinLength()
{
	return audioMinLength;
}
void AteOscHardware::setAudioMinLength(unsigned char newLength)
{
	audioMinLength = newLength;
}
void AteOscHardware::setAudioBufferStatus(AudioBufferStatus newValue)
{
	audioBufferStatus = newValue;
	switch (newValue)
	{
		case BUFFER_IDLE:
		break;
		case BUFFER_WAITZCROSS:
		audioWriteIndex = 0;
		audioBuffer[AUDIO_BUFFER_SIZE-1] = 0x7F;  //make max value, so not instantly read as zero crossing
		ADCSRA = 0xCE;  //11001110
		break;
	}
	
}
AteOscHardware::AudioBufferStatus AteOscHardware::getAudioBufferStatus()
{
	return audioBufferStatus;
}
void AteOscHardware::refreshFlash(unsigned char ticksPassed)
{
	unsigned char i;
	unsigned char tickInc = 0;

	ledFlashTickCnt += ticksPassed;
	while (ledFlashTickCnt>=LED_FLASH_SCALE)
	{
		ledFlashTickCnt -= LED_FLASH_SCALE;
		tickInc++;
	}
	
	if(tickInc>0)
	{
		//can only flash 16 leds at the mo. must make array dymanic
		//for(i=0;i<2;++i)
		//{
		ledCircular_[1].refreshFlash(tickInc);
		//}

		for(i=0;i<2;++i)
		{
			ledSwitch_[i].refreshFlash(tickInc);
		}
	}
	


}
void AteOscHardware::refreshLeds()
{
	unsigned char i;
	LedRgb::LedRgbColour col;
	unsigned int circ = 0;
	static unsigned int mcpState[2];

	for(i=0;i<2;++i)
	{
		circ = ledCircular_[i].getState();
		if(i==0)
		{
			col = ledSwitch_[FUNCTION].getColour();
			if(col==LedRgb::RED || col==LedRgb::YELLOW)
			{
				bitSet(circ,10);
			}
			if(col==LedRgb::GREEN || col==LedRgb::YELLOW)
			{
				bitSet(circ,11);
			}
			col = ledSwitch_[VALUE].getColour();
			if(col==LedRgb::RED || col==LedRgb::YELLOW)
			{
				bitSet(circ,8);
			}
			if(col==LedRgb::GREEN || col==LedRgb::YELLOW)
			{
				bitSet(circ,9);
			}
		}
		if(circ!=mcpState[i])
		{
			mcpState[i] = circ;
			writeI2cWord(1-i, MCP23017_GPIOA, mcpState[i]);
			
		}
	}
}

void AteOscHardware::pollMappedCvInput(CvInputName input)
{
	unsigned char i = ((input == CV_PITCH) ? 0 : 1);
	long mapped = (((long)readMCP3208input(input) - cvCalib_[i][0]) << 11) / cvCalib_[i][2] + 1024;  //outmax = 3072 = 3.75V, outmin = 1024 = 1.25V, max-min = 2048 (<<11)
	if(mapped<0)
	{
		cvInput_[input].setValue(0);
	}
	else if (mapped>4095)
	{
		cvInput_[input].setValue(4095);
	}
	else
	{
		cvInput_[input].setValue((unsigned int)mapped);
	}
	if(cvInput_[input].hasChanged(0)==true)  //ticks not used coz lockout off
	{
		base_->hardwareCvInputChanged(input,cvInput_[input].getOutput());
	}
}
void AteOscHardware::pollCvInputs(unsigned char ticksPassed)
{
	unsigned char a,i;

	for(a=0;a<INPUTS;++a)
	{
		if(inputMode_[a]==IP_CV)
		{
			if(a==CV_PITCH || a==CV_FILT)
			{
				pollMappedCvInput((CvInputName)a);
			}
			else
			{
				cvInput_[a].setValue(readMCP3208input(a));
				if(cvInput_[a].hasChanged(ticksPassed)==true)
				{
					if(a==CV_PWM && bitRead(ctrlMode_,0))
					{
						cvInput_[CV_PITCH].setGain(cvInput_[a].getValue() >> 4);
					}
					else if(a==CV_FLANGE && bitRead(ctrlMode_,1))
					{
						cvInput_[CV_FILT].setGain(cvInput_[a].getValue() >> 4);
					}
					else
					{
						base_->hardwareCvInputChanged(a,cvInput_[a].getValue());
					}
					
				}
			}
		}
	}
}
void AteOscHardware::pollGateInputs()
{
	bool newValue;
	for(unsigned char a=0;a<INPUTS;++a)
	{
		if(inputMode_[a]==IP_GATE)
		{
			newValue = readMCP3208input(a) > CV_HALF_SCALE ? true : false;
			if(newValue!=gateInput_[a])
			{
				gateInput_[a] = newValue;
				base_->hardwareGateInputChanged(a,newValue);
			}
		}
	}
}
void AteOscHardware::pollSwitches(unsigned char ticksPassed)
{
	for(unsigned char i=0;i<2;++i)
	{
		switch_[i].setState(switchState[i]);
		if(switch_[i].hasHeld(ticksPassed)==true)
		{
			base_->hardwareSwitchHeld(i);
		}
		if(switch_[i].hasChanged(ticksPassed)==true)
		{
			base_->hardwareSwitchChanged(i,switch_[i].getState());
		}
	}
}

void AteOscHardware::pollRotEncoders(unsigned char ticksPassed)
{
	unsigned char i;
	for(i=0;i<2;++i)
	{
		rotEncoder_[i].setCount(rotEncoderCount[i]);
		if(rotEncoder_[i].hasChanged(ticksPassed)==true)
		{
			base_->hardwareRotaryEncoderChanged(i,rotEncoder_[i].getValue(),rotEncoder_[i].getDirection());
		}
	}
}

void AteOscHardware::pollAudioBufferStatus()
{
	static AudioBufferStatus last = BUFFER_IDLE;
	if(last!=audioBufferStatus)
	{
		last = audioBufferStatus;
		base_->hardwareAudioBufferStatusChanged(last);
	}
}

void AteOscHardware::beginI2c()
{
	// initialize twi prescaler and bit rate
	bitClear(TWSR, TWPS0);
	bitClear(TWSR, TWPS1);
	TWBR = ((F_CPU / F_SCL) - 16) / 2;
	
	// enable twi module, acks, and twi interrupt
	TWCR = ((1 << TWEN) | (1 << TWIE) | (1 << TWEA));
}
void AteOscHardware::writeI2cByte(unsigned char address, unsigned char reg, unsigned char value)
{
	while (i2cTxing==true || i2cRxing==true || bitRead(TWCR,TWSTO)==true)
	{
	}
	i2cBuffer[0] = reg;
	i2cBuffer[1] = value;
	i2cWritePos = 2;
	i2cTxing = true;
	i2cAddr = TW_WRITE;
	i2cAddr |= (MCP23017_ADDRESS | address) << 1;
	TWCR = ((1 << TWSTA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE) | (1 << TWEA));
}
void AteOscHardware::writeI2cWord(unsigned char address, unsigned char reg, unsigned int word)
{
	while (i2cTxing==true || i2cRxing==true || bitRead(TWCR,TWSTO)==true)
	{
	}
	i2cBuffer[0] = reg;
	i2cBuffer[1] = word & 0xFF;
	i2cBuffer[2] = word >> 8;
	i2cWritePos = 3;
	i2cTxing = true;
	i2cAddr = TW_WRITE;
	i2cAddr |= (MCP23017_ADDRESS | address) << 1;
	TWCR = ((1 << TWSTA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE) | (1 << TWEA));
}
void AteOscHardware::beginSpi()
{
	//setup expanders
	// START up the SPI bus
	bitSet(PORTB,PINB2);
	bitSet(DDRB,PINB2);
	bitSet(SPCR,MSTR);
	bitSet(SPCR,SPE);
	bitSet(DDRB,PINB3);
	bitSet(DDRB,PINB5);
	
	// Sets the SPI bus speed  Fosc/2
	bitSet(SPSR,SPI2X);
	bitClear(SPCR,SPR1);
	bitSet(SPCR,SPR0);
	
	// Sets SPI bus bit order (this is the default, setting it for good form!)
	bitClear(SPCR,DORD);
	// Sets the SPI bus timing mode (this is the default, setting it for good form!)
	bitClear(SPCR,CPOL);
	bitClear(SPCR,CPHA);
}
unsigned char AteOscHardware::writeSpi(unsigned char cData)
{
	SPDR = cData;  /* START transmission */
	while(!(SPSR & (1<<SPIF)));  /* Wait for transmission complete */
	return SPDR;
}

unsigned int AteOscHardware::readMCP3208input(unsigned char input)
{
	unsigned char h,l,addr;
	int out;
	PORTB &= 0xFB;                                 // Direct port manipulation speeds taking Slave select LOW before SPI action
	addr = bitRead(input,2);
	h = writeSpi(MCP3208_SINGLE | addr);
	addr = (input & 0x03) << 6;
	h = writeSpi(addr);       //unsure of top 4 bits, so chop them off
	l = writeSpi(0x00);
	PORTB |= 0x04;									// Direct port manipulation speeds taking Slave select HIGH after SPI action
	out = h & 0x0F;
	out <<= 8;
	out |= l;
	if(out<0)
	{
		return 0;
	}
	else if(out>4095)
	{
		return 4095;
	}
	else
	{
		return out;
	}
}


ISR(ADC_vect)
{
	unsigned char samp = ADCH;
	switch (audioBufferStatus)
	{
		case AteOscHardware::BUFFER_WAITZCROSS:
		audioBuffer[audioWriteIndex] = samp - 128;
		if(audioBuffer[audioWriteIndex]>=0 && audioBuffer[audioWriteIndex-1]<0)
		{
			audioBufferStatus = AteOscHardware::BUFFER_CAPTURING;
			audioBuffer[0] = audioBuffer[audioWriteIndex];
			audioWriteIndex = 0;
		}
		audioWriteIndex++;
		ADCSRA = 0xCE; //11001110
		break;
		case AteOscHardware::BUFFER_CAPTURING:
		audioBuffer[audioWriteIndex] = samp - 128;
		if(audioWriteIndex==0xFF)
		{
			audioBufferStatus = AteOscHardware::BUFFER_OVERFLOW;
		}
		else if(audioBuffer[audioWriteIndex]>=0 && audioBuffer[audioWriteIndex-1]<0 && audioWriteIndex>=audioMinLength)
		{
			audioBufferStatus = AteOscHardware::BUFFER_CAPTURED;
		}
		else
		{
			audioWriteIndex++;  //don't want to inc if finished, want zero cross to next zero cross-1 (waveform looping)
			ADCSRA = 0xCE; //11001110
		}
		
		break;
	}
	

}
ISR (PCINT0_vect)
{
	unsigned char invPinB = ~PINB;
	//func button
	switchState[0] = bitRead(invPinB,PINB0);
	//val button
	switchState[1] = bitRead(invPinB,PINB1);
}
ISR (PCINT2_vect)
{
	unsigned char i,j;
	static unsigned char rotEncoderStateLast[2] = {0,0};
	static unsigned char rotEncoderState[2] = {0,0};
	
	unsigned char new_rot;
	unsigned char invPind = ~PIND;

	//encoders
	for(i=0;i<2;i++)
	{
		new_rot = invPind & pgm_read_byte(&(rotEncoderBitMask[i]));
		if(new_rot!=rotEncoderStateLast[i])
		{
			rotEncoderState[i] >>= 2;
			for(j=0;j<2;j++)
			{
				rotEncoderState[i] |= (bitRead(invPind,pgm_read_byte(&(rotEncoderPins[i][j]))) << (j+2));
			}
			rotEncoderCount[i] -= (char)pgm_read_byte(&(rotEncoderIncrementAmount[rotEncoderState[i]]));
			rotEncoderStateLast[i] = new_rot;
		}
	}
}
ISR(TWI_vect)
{
	
	switch(TW_STATUS){
		// All Master
		case TW_START:     // sent start condition
		case TW_REP_START: // sent repeated start condition
		// copy device address and r/w bit to output register and ack
		TWDR = i2cAddr;
		TWCR = TW_TX_ACK;
		i2cReadPos = 0;
		break;

		// Master Transmitter
		case TW_MT_SLA_ACK:  // slave receiver acked address
		case TW_MT_DATA_ACK: // slave receiver acked data
		// if there is data to send, send it, otherwise stop
		if(i2cReadPos<i2cWritePos)
		{
			// copy data to output register and ack
			TWDR = i2cBuffer[i2cReadPos];
			i2cReadPos++;
			TWCR = TW_TX_ACK;
		}
		else
		{
			TWCR = TW_STOP;
			i2cTxing = false;
		}
		break;
		case TW_MT_SLA_NACK:  // address sent, nack received
		case TW_MT_DATA_NACK: // data sent, nack received
		case TW_BUS_ERROR: // bus error, illegal stop/start
		TWCR = TW_STOP;
		i2cTxing = false;
		break;
		case TW_MT_ARB_LOST: // lost bus arbitration
		TWCR = ((1 << TWSTO) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE));
		i2cTxing = false;
		break;

		// Master Receiver
		case TW_MR_DATA_ACK: // data received, ack sent
		// put byte into buffer
		i2cBuffer[i2cReadPos] = TWDR;
		i2cReadPos++;  //****NO BREAK HERE - IT SENDS THE ACK
		case TW_MR_SLA_ACK:  // address sent, ack received
		// ack if more bytes are expected, otherwise nack
		if(i2cReadPos<i2cWritePos)
		{
		// copy data to output register and ack
		TWCR = TW_TX_ACK;
		}
		else
		{
		TWCR = TW_TX_NACK;
		}
		break;
		case TW_MR_DATA_NACK: // data received, nack sent
		// put final byte into buffer
		i2cBuffer[i2cReadPos] = TWDR;
		TWCR = TW_STOP;
		i2cRxing = false;
		break;
		case TW_MR_SLA_NACK: // address sent, nack received
		TWCR = TW_STOP;
		i2cRxing = false;
		break;

		// All
		case TW_NO_INFO:   // no state information
		break;
		}
		}
