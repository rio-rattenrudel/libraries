//Lfo.cpp  Low frequency oscillator class
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

#include "Lfo.h"

// default constructor
Lfo::Lfo()
{
} //Lfo

// default destructor
Lfo::~Lfo()
{
} //~Lfo
void Lfo::setDivision(unsigned char newDiv)
{
	division_ = newDiv;
	divMult_ = pgm_read_word(&(LFO_MULT[division_]));
	divBs_ = pgm_read_byte(&(LFO_BS[division_]));
}
// rio: lfo additions
void Lfo::setPreDelay(unsigned char newValue)
{
	predelay_ = newValue;
}
void Lfo::setAttack(unsigned char newValue)
{
	attack_ = newValue;
}
void Lfo::resetCounter()
{
	// reset pre + output
	if (predelaycc_ > 0) {
		predelaycc_ = 0;
		output_ = 0;
	}

	// reset att
	if (attackcc_ > 0) {
		attackcc_ = 0;
		timeCC = 0;
	}
}
void Lfo::refresh(unsigned int cycleTick)
{
	timegap = cycleTick - timestart;
	timestart = cycleTick;

	// inc predelay counter
	if (predelaycc_ < predelay_ << 3) {
		predelaycc_ += timegap;
		return;
	}

	index_ = ((unsigned long)cycleTick * divMult_) >> divBs_;
	index_ &= LFO_BIT_MASK;
	output_ = pgm_read_byte(&(LFO_WAVETABLE[table_][index_]));
	if(invert_==true)
	{
		output_ *= -1;
	}

	// simple ramping according to time (att)
	if (attackcc_ < attack_) {
		output_ = attackcc_ * output_ / attack_;

		timeCC += timegap;
		if (timeCC > 127) {
			timeCC = 0;
			attackcc_++;
		}
	}
}
// rio: lfo additions end