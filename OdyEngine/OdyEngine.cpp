//OdyEngine.cpp  Odytron audio engine singleton class
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

#include "OdyEngine.h"
extern void writeMemory(const void* data, void* startAddr, size_t size);
extern void readMemory(void* data, const void* startAddr, size_t size);


// default constructor
OdyEngine::OdyEngine(OdyEngineBase* base)
{
	construct(base);
} //OdyEngine

// default destructor
OdyEngine::~OdyEngine()
{
	//if(audio_!=NULL)
	//{
	//delete audio_;
	//}
	if(patch_!=NULL)
	{
		delete patch_;
	}
	if(midi_!=NULL)
	{
		delete midi_;
	}
} //~OdyEngine

void OdyEngine::construct(OdyEngineBase* base)
{
	base_ = base;
	patch_ = new OdyPatch(this);
	midi_ = new Midi(this,SYSEX_PROD_ID);
}

void OdyEngine::initialize()
{
	unsigned char i;
	
	audio_.initialize();
	
	//load vanilla
	for(i=0;i<6;++i)
	{
		patchCtrlChanged(i,patch_->getCtrlValue(i));
	}
	for(i=0;i<20;++i)
	{
		patchValueChanged(i,patch_->getFunctionValue(i));
		patchOptionChanged(i,patch_->getOptionValue(i));
	}
	
	setFunction(FUNC_OSC0FMA);
	arEnvelope_.setSustain(32767);  //it's only an AR env, so force these to full
	arEnvelope_.setDecay(32767);
	portamento_[0].setProportional(true);
	portamento_[1].setProportional(true);
}

void OdyEngine::poll(unsigned char ticksPassed)
{
	unsigned char i;
	char fmA,fmB;
	char filtA, filtB,am;
	
	noise_.refresh();
	lfo_.refresh(ticksPassed);
	adsrEnvelope_.refresh(ticksPassed);
	arEnvelope_.refresh(ticksPassed);
	
	for(i=0;i<2;++i)
	{
		oscillator_[1].refresh(adsrEnvelope_.getOutput(),lfo_.getOutput(OdyLfo::SINE));
	}
	
	if(filter_.getFmASource()==OdyFilter::S_AND_H)
	{
		filtA = lfo_.getOutput(OdyLfo::S_AND_H);
	}
	else
	{
		filtA = lfo_.getOutput(OdyLfo::SINE);
	}
	if(filter_.getFmBSource()==OdyFilter::ADSR)
	{
		filtB = adsrEnvelope_.getExpOutput();
	}
	else
	{
		filtB = arEnvelope_.getExpOutput();
	}
	
	filter_.refresh(noteHighest_,filtA,filtB);
	
	for(i=0;i<2;++i)
	{
		portamento_[i].refresh(ticksPassed);
		pitch_[i].setInput(portamento_[i].getOutput());
		if(pitch_[i].getFmASource()==OdyPitch::SINE)
		{
			fmA = lfo_.getOutput(OdyLfo::SINE);
		}
		else
		{
			fmA = lfo_.getOutput(OdyLfo::SQUARE);
		}
		if(pitch_[i].getFmBSource()==OdyPitch::S_AND_H)
		{
			fmB = lfo_.getOutput(OdyLfo::S_AND_H);
		}
		else
		{
			fmB = adsrEnvelope_.getExpOutput();
		}
		pitch_[i].refresh(pitchBend_,fmA,fmB);
		audio_.setSampleFreq(i,(unsigned long)pitch_[i].getOutput()*(WAVE_LENGTH>>2));  //Finally scale freqs back down again (/4)
	}
	
	if(amplifier_.getAmSource()==OdyAmplifier::AR)
	{
		am = arEnvelope_.getExpOutput();
	}
	else
	{
		am = adsrEnvelope_.getExpOutput();
	}
	amplifier_.refresh(am);
}

void OdyEngine::setFunction(OdyEngine::Func newFunc)
{
	if(newFunc==FUNC_ENVA && patch_->getOptionValue(FUNC_ENVA)==true)
	{
		function_ = FUNC_ENVA2;
	}
	else if(newFunc==FUNC_ENVR && patch_->getOptionValue(FUNC_ENVR)==true)
	{
		function_ = FUNC_ENVR2;
	}
	else if(newFunc==FUNC_PORTAMENTO && patch_->getOptionValue(FUNC_PORTAMENTO)==true)
	{
		function_ = FUNC_MEM;
	}
	else
	{
		function_ = newFunc;
	}
	base_->engineFunctionChanged((unsigned char)function_,patch_->getFunctionValue(function_),patch_->getOptionValue(function_));
}

void OdyEngine::triggerNote(unsigned char note)
{
	//frequency_ = pgm_read_word(&(MIDI_FREQS[note]));
	if(note==noteLowest_)
	{
		portamento_[0].setInput(pgm_read_word(&(MIDI_FREQS[note])));
	}
	if(note==noteHighest_)
	{
		portamento_[1].setInput(pgm_read_word(&(MIDI_FREQS[note])));
	}
	adsrEnvelope_.trigger();
	arEnvelope_.trigger();
	lfo_.reset();
}
void OdyEngine::releaseNote()
{
	adsrEnvelope_.release();
	arEnvelope_.release();
}

//***********************MIDI events********************************************
void OdyEngine::midiNoteOnReceived(unsigned char note, unsigned char velocity)
{

	totNotesOnLast_ = midi_->getTotNotesOn();
	if(note<noteLowest_)
	{
		noteLowest_ = note;
	}
	if(note>noteHighest_)
	{
		noteHighest_ = note;
	}

	#if LEGATO==1
	if(midi_->getTotNotesOn()>1)
	{
		portamento_.setInput(pgm_read_word(&(MIDI_FREQS[note])));
	}
	else
	{
		triggerNote(note);
	}
	#else
	triggerNote(note);
	#endif

}

void OdyEngine::midiNoteOffReceived(unsigned char note)
{
	totNotesOnLast_ = midi_->getTotNotesOn();
	if(note==noteLowest_)
	{
		noteLowest_ = 127;
		for(unsigned char i=0;i<128;++i)
		{
			if(midi_->getNoteOn(i)==true)
			{
				noteLowest_ = i;
				break;
			}
		}
	}

	if(note==noteHighest_)
	{
		noteHighest_ = 0;
		for(unsigned char i=127;i!=0;--i)
		{
			if(midi_->getNoteOn(i)==true)
			{
				noteHighest_ = i;
				break;
			}
		}
	}

	if(midi_->getTotNotesOn()==0)
	{
		releaseNote();
	}
	else
	{
		portamento_[0].setInput(pgm_read_word(&(MIDI_FREQS[noteLowest_])));
		portamento_[1].setInput(pgm_read_word(&(MIDI_FREQS[noteHighest_])));
	}
}

void OdyEngine::midiControlChangeReceived(unsigned char anlControl_, unsigned char val)
{
	switch ((MidiCC)anlControl_)
	{
		case CC_MODWHEEL:
		patch_->setFunctionValue(FUNC_OSC0FMA,val>>3);
		patch_->setFunctionValue(FUNC_OSC1FMA,val>>3);
		break;
		case CC_PORTAMENTO:
		patch_->setFunctionValue(FUNC_PORTAMENTO,val>>3);
		break;
		case CC_FILTFMB:
		patch_->setFunctionValue(FUNC_FILTFMB,val>>3);
		break;
		case CC_FILTFMBSOURCE:
		patch_->setOptionValue(FUNC_FILTFMB,(bool)val>>6);
		break;
		case CC_OSC1FMA:
		patch_->setFunctionValue(FUNC_OSC0FMA,val>>3);
		break;
		case CC_OSC1FMB:
		patch_->setFunctionValue(FUNC_OSC0FMB,val>>3);
		break;		
		case CC_OSC2FMA:
		patch_->setFunctionValue(FUNC_OSC1FMA,val>>3);
		break;
		case CC_OSC2FMB:
		patch_->setFunctionValue(FUNC_OSC1FMB,val>>3);
		break;		
		case CC_MIXFX:
		patch_->setFunctionValue(FUNC_OSCLEVELFX,val>>3);
		break;
		case CC_MIXOSC1:
		patch_->setFunctionValue(FUNC_OSCLEVEL0,val>>3);
		break;
		case CC_MIXOSC2:
		patch_->setFunctionValue(FUNC_OSCLEVEL1,val>>3);
		break;
		case CC_WAVEFX:
		patch_->setOptionValue(FUNC_OSCLEVELFX,(bool)val>>6);
		break;
		case CC_WAVEOSC1:
		patch_->setOptionValue(FUNC_OSCLEVEL0,(bool)val>>6);
		break;
		case CC_WAVEOSC2:
		patch_->setOptionValue(FUNC_OSCLEVEL1,(bool)val>>6);
		break;		
		case CC_FILTTYPE:
		patch_->setFunctionValue(FUNC_FILTTYPE, val/43);
		break;
		case CC_OSC1FMASOURCE:
		patch_->setOptionValue(FUNC_OSC0FMA,(bool)val>>6);
		break;
		case CC_OSC1FMBSOURCE:
		patch_->setOptionValue(FUNC_OSC0FMB,(bool)val>>6);
		break;
		case CC_OSC2FMASOURCE:
		patch_->setOptionValue(FUNC_OSC1FMA,(bool)val>>6);
		break;
		case CC_OSC2FMBSOURCE:
		patch_->setOptionValue(FUNC_OSC1FMB,(bool)val>>6);
		break;	
		case CC_SYNC:
		patch_->setOptionValue(FUNC_OSC1PW,(bool)val>>6);
		break;
		case CC_PWMSOURCE:
		patch_->setOptionValue(FUNC_OSC1PWM,(bool)val>>6);
		break;	
		case CC_KBRDTRACKING:
		patch_->setOptionValue(FUNC_ENVS,(bool)val>>6);
		break;
		case CC_DCASOURCE:
		patch_->setOptionValue(FUNC_ENVD,(bool)val>>6);
		break;		
		case CC_FILTRES:
		patch_->setCtrlValue(CTRL_Q,val<<1);
		break;
		case CC_AMPENVR:
		patch_->setFunctionValue(FUNC_ENVR,val>>3);
		break;
		case CC_AMPENVA:
		patch_->setFunctionValue(FUNC_ENVA,val>>3);
		break;
		case CC_FILTCUTOFF:
		patch_->setCtrlValue(CTRL_FC,val<<1);
		break;
		case CC_AMPENVD:
		patch_->setFunctionValue(FUNC_ENVD,val>>3);
		break;
		case CC_FILTFMA:
		patch_->setFunctionValue(FUNC_FILTFMA,val>>3);
		break;
		case CC_FILTFMASOURCE:
		patch_->setOptionValue(FUNC_FILTFMA,(bool)val>>6);
		break;		
		case CC_LFOSPEED:
		patch_->setCtrlValue(CTRL_LFO,val<<1);
		break;
		case CC_AMPENVS:
		patch_->setFunctionValue(FUNC_ENVS,val>>3);
		break;
		case CC_FENVA:
		patch_->setFunctionValue(FUNC_ENVA2,val>>3);
		break;
		case CC_FENVDR:
		patch_->setFunctionValue(FUNC_ENVR2,val>>3);
		break;
		case CC_PULSEWIDTH:
		patch_->setFunctionValue(FUNC_OSC1PW,val>>3);
		break;
		case CC_PWM:
		patch_->setFunctionValue(FUNC_OSC1PWM,val>>3);
		break;
		case CC_OSC1PITCH:
		patch_->setCtrlValue(CTRL_VCO1,val<<1);
		break;
		case CC_OSC2PITCH:
		patch_->setCtrlValue(CTRL_VCO2,val<<1);
		break;
		case CC_HPF:
		patch_->setCtrlValue(CTRL_HPF,val<<1);
		break;
		case CC_ALLNOTESOFF:
		midi_->reset();
		break;
	}
}

void OdyEngine::midiChannelChanged(unsigned char channel)
{
	base_->engineFunctionChanged(0,channel,false);
}
void OdyEngine::midiPitchBendReceived(char bend)
{
	pitchBend_ = bend;
}
void OdyEngine::midiProgramChangeReceived(unsigned char patchNum)
{
	if (patchNum<16)
	{
		patch_->readPatch(patchNum);
	}
}
//****************************************patch events********************************************
void OdyEngine::patchValueChanged(unsigned char func, unsigned char newValue)
{
	switch (func)
	{
		case FUNC_OSC0FMA:
		pitch_[0].setFmAAmount(newValue);
		break;
		case FUNC_OSC0FMB:
		pitch_[0].setFmBAmount(newValue);
		break;
		case FUNC_OSC1FMA:
		pitch_[1].setFmAAmount(newValue);
		break;
		case FUNC_OSC1FMB:
		pitch_[1].setFmBAmount(newValue);
		break;
		case FUNC_OSC1PW:
		oscillator_[1].setPulseWidth(127+(newValue<<3));
		//oscillator_[1].setPulseWidth(31+(newValue<<1));
		break;
		case FUNC_OSC1PWM:
		oscillator_[1].setPwmAmout(newValue<<4);
		break;
		case FUNC_OSCLEVELFX:
		fxLevel_ = newValue>>1;
		noise_.setLevel(newValue>>1);
		break;
		case FUNC_OSCLEVEL0:
		oscillator_[0].setLevel(newValue>>1);
		break;
		case FUNC_OSCLEVEL1:
		oscillator_[1].setLevel(newValue>>1);
		break;
		case FUNC_FILTFMA:
		filter_.setFmAAmount(newValue);
		break;
		case FUNC_FILTFMB:
		filter_.setFmBAmount(newValue);
		break;
		case FUNC_ENVR:
		adsrEnvelope_.setRelease(pgm_read_word(&(ENV_R_INC[newValue])));
		break;
		case FUNC_ENVS:
		adsrEnvelope_.setSustain(pgm_read_word(&(ENV_S_LEVEL[newValue])));
		break;
		case FUNC_ENVD:
		adsrEnvelope_.setDecay(pgm_read_word(&(ENV_D_INC[newValue])));
		break;
		case FUNC_ENVA:
		adsrEnvelope_.setAttack(pgm_read_word(&(ENV_A_INC[newValue])));
		break;
		case FUNC_PORTAMENTO:
		portamento_[0].setSpeed(pgm_read_word(&(PORTA_SPEED[newValue])));
		portamento_[1].setSpeed(pgm_read_word(&(PORTA_SPEED[newValue])));
		break;
		case FUNC_ENVR2:
		arEnvelope_.setRelease(pgm_read_word(&(ENV_R_INC[newValue])));
		break;
		case FUNC_ENVA2:
		arEnvelope_.setAttack(pgm_read_word(&(ENV_A_INC[newValue])));
		break;
		case FUNC_MEM:
		break;
		case FUNC_FILTTYPE:
		filter_.setType((OdyFilter::FiltType)newValue);
		break;
	}
	if(func==function_ || func==FUNC_FILTTYPE)
	{
		base_->engineFunctionChanged(func,newValue,patch_->getOptionValue(func));
	}
}

void OdyEngine::patchOptionChanged(unsigned char func, bool newOpt)
{
	switch (func)
	{
		case FUNC_OSC0FMA:
		pitch_[0].setFmASource((OdyPitch::PitchFmASource)newOpt);
		break;
		case FUNC_OSC0FMB:
		pitch_[0].setFmBSource((OdyPitch::PitchFmBSource)newOpt);
		break;
		case FUNC_OSC1FMA:
		pitch_[1].setFmASource((OdyPitch::PitchFmASource)newOpt);
		break;
		case FUNC_OSC1FMB:
		pitch_[1].setFmBSource((OdyPitch::PitchFmBSource)newOpt);
		break;
		case FUNC_OSC1PW:
		audio_.setWaveSync(newOpt);
		break;
		case FUNC_OSC1PWM:
		oscillator_[1].setPwmSource((OdyOscillator::PwmSource)newOpt);
		break;
		case FUNC_OSCLEVELFX:
		fxSource_ = (FxSource)newOpt;
		break;
		case FUNC_OSCLEVEL0:
		oscillator_[0].setWaveform((OdyOscillator::OscWave)newOpt);
		break;
		case  FUNC_OSCLEVEL1:
		oscillator_[1].setWaveform((OdyOscillator::OscWave)newOpt);
		break;
		case FUNC_FILTFMA:
		filter_.setFmASource((OdyFilter::FiltFmASource)newOpt);
		break;
		case FUNC_FILTFMB:
		filter_.setFmBSource((OdyFilter::FiltFmBSource)newOpt);
		break;
		case FUNC_ENVR:
		if(newOpt==true)
		{
			patch_->setOptionValue(FUNC_ENVR2,true);
			setFunction(FUNC_ENVR2);
		}
		break;
		case FUNC_ENVS:
		if(newOpt==true)
		{
			filter_.setKybrdAmount(15);
		}
		else
		{
			filter_.setKybrdAmount(0);
		}
		break;
		case FUNC_ENVD:
		amplifier_.setAmSource((OdyAmplifier::AmpAmSource)newOpt);
		break;
		case FUNC_ENVA:
		if(newOpt==true)
		{
			patch_->setOptionValue(FUNC_ENVA2,true);
			setFunction(FUNC_ENVA2);
		}
		break;
		case FUNC_PORTAMENTO:
		if(newOpt==true)
		{
			patch_->setOptionValue(FUNC_MEM,true);
			setFunction(FUNC_MEM);
		}
		break;
		case FUNC_ENVR2:
		if(newOpt==false)
		{
			patch_->setOptionValue(FUNC_ENVR,false);
			setFunction(FUNC_ENVR);
		}
		break;
		case FUNC_ENVA2:
		if(newOpt==false)
		{
			patch_->setOptionValue(FUNC_ENVA,false);
			setFunction(FUNC_ENVA);
		}
		break;
		case FUNC_MEM:
		if(newOpt==false)
		{
			patch_->setOptionValue(FUNC_PORTAMENTO,false);
			setFunction(FUNC_PORTAMENTO);
		}
		break;
	}
	if(func==function_)
	{
		base_->engineFunctionChanged(func,patch_->getFunctionValue(func),newOpt);
	}
}

void OdyEngine::patchCtrlChanged(unsigned char anlControl_, unsigned char newValue)
{
	switch (anlControl_)
	{
		case CTRL_VCO1:
		pitch_[0].setOffset(pgm_read_byte(&(PITCH_OFFSET[128-(newValue>>1)])));
		break;
		case CTRL_VCO2:
		pitch_[1].setOffset(pgm_read_byte(&(PITCH_OFFSET[(newValue>>1)+128])));
		break;
		case CTRL_LFO:
		lfo_.setFreq(pgm_read_byte(&(LFO_EXP_KNOB[(newValue>>1)])));
		break;
		case  CTRL_FC:
		filter_.setFc(newValue);
		break;
		case CTRL_Q:
		filter_.setQ(newValue);
		break;
		case  CTRL_HPF:
		hpf_.setFc(newValue);
		break;
	}
}

