//AnalogueControl.h  Hardware analogue control (ie potentiometer)
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

#ifndef __ANALOGUECONTROL_H__
#define __ANALOGUECONTROL_H__

#include "AnalogueControlBase.h"
#include <stdlib.h>

class AnalogueControl
{
//variables
public:
protected:
private:
	AnalogueControlBase* base_;
	static const unsigned char kCtrlMoveThreshold = 4;
	static const unsigned int kCtrlMoveTimeOut = 2000;
	bool moving_ = false;
	unsigned char valueLast_ = 0;
	unsigned int moveTick_ = 0;
	unsigned char value_ = 0;
	bool latching_;
	bool latched_;
	unsigned char latchVal_;
	unsigned char index_;
//functions
public:
	AnalogueControl() {}
	AnalogueControl(unsigned char index, unsigned char initValue, AnalogueControlBase* base);
	AnalogueControl(unsigned char initValue);
	~AnalogueControl();
	void setValue(unsigned char newValue){value_ = newValue;}
	unsigned char getValue(){return value_;}
	bool getMoving(){return moving_;}
	void setLatching(bool newValue){latching_ = newValue;}
	bool getLatching(){return latching_;}
	void setLatched(bool newValue){latched_ = newValue;}
	bool getLatched(){return latched_;}
	bool hasChanged(unsigned char ticksPassed);  //deprocated
	void poll(unsigned char ticksPassed);
protected:
private:
	
	//AnalogueControl( const AnalogueControl &c );
	//AnalogueControl& operator=( const AnalogueControl &c );

}; //AnalogueControl

#endif //__ANALOGUECONTROL_H__
