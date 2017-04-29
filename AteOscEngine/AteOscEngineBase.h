//AteOscEngineBase.h  Audio engine base class for Atmegatron
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

#ifndef ATEOSCENGINEBASE_H_
#define ATEOSCENGINEBASE_H_

class AteOscEngineBase
{
	public:
	virtual void engineFunctionChanged(unsigned char func, unsigned char val) = 0;
	virtual void engineOptionChanged(unsigned char func, bool opt) = 0;
	virtual void engineMinLengthChanged(unsigned char newLength) = 0; 
	virtual void engineDoEvents() = 0;
	virtual void engineStartCapture(bool way) = 0;
};



#endif /* ATEOSCENGINEBASE_H_ */