/*
//AteOdyEngineProgmem.cpp  Odytron for Oscitron audio engine - this is the exp curve for osc 2 offset knob
//Copyright (C) 2017  Paul Soulsby info@soulsbysynths.com
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
*/

#ifndef ATEODYENGINEPROGMEM_H_
#define ATEODYENGINEPROGMEM_H_

#include <avr/pgmspace.h>

static const unsigned char PITCH_OFFSET[256] PROGMEM = {64,64,65,65,65,66,66,66,67,67,68,68,68,69,69,69,70,70,71,71,71,72,72,72,73,73,74,74,74,75,75,76,76,77,77,77,78,78,79,79,79,80,80,81,81,82,82,83,83,83,84,84,85,85,86,86,87,87,88,88,89,89,90,90,91,91,91,92,92,93,93,94,95,95,96,96,97,97,98,98,99,99,100,100,101,101,102,103,103,104,104,105,105,106,106,107,108,108,109,109,110,111,111,112,112,113,114,114,115,115,116,117,117,118,119,119,120,121,121,122,123,123,124,125,125,126,127,127,128,129,129,130,131,132,132,133,134,134,135,136,137,137,138,139,140,140,141,142,143,143,144,145,146,147,147,148,149,150,151,151,152,153,154,155,156,156,157,158,159,160,161,162,162,163,164,165,166,167,168,169,170,171,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,200,201,202,203,204,205,206,207,208,210,211,212,213,214,215,216,218,219,220,221,222,224,225,226,227,228,230,231,232,233,235,236,237,239,240,241,243,244,245,246,248,249,251,252,253,255};

#endif /* ATEODYENGINEPROGMEM_H_ */