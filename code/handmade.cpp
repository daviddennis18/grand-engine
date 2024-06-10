/*--------------------------------------------------
*	File Name : handmade.cpp
* 	Creation Date : 21-05-2022
*	Revision :
*	Last Modified : 2024-06-07 3:13:17 PM
*	Created By : David Dennis
---------------------------------------------------*/

#include "handmade.h"


internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz){

	local_persist real32 tSine;
	int16 ToneVolume = 1000;
	//int ToneHz = 256;
	int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;

	int16 *SampleOut = SoundBuffer->Samples;
	for(DWORD SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex){
		real32 SineValue = sinf(tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;

		tSine += 2.0f*Pi32*1.0f/(real32)WavePeriod;
	}
}

internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset, int Colour){

	//int Colour = 1;
	//fill memory
	uint8 *Row = (uint8 *)Buffer->Memory;
	for(int Y = 0; Y<Buffer->Height; ++Y){
		uint32 *Pixel = (uint32 *)Row;
		for(int X = 0; X<Buffer->Width; ++X){
			uint8 Blue = (X+BlueOffset);
			uint8 Green = (Y+GreenOffset);
			if(Colour == 1){
				*Pixel = ((Green << 8) | Blue);
			}else{
				*Pixel = ((Green << 16) | Blue);
			}
			Pixel++;
		}
		Row += Buffer->Pitch;
	}
}

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer, int BlueOffset, int GreenOffset, int Colour, int ToneHz){

	//TODO: allow sample offsets here for more robust platform options
	GameOutputSound(SoundBuffer, ToneHz);


	RenderWeirdGradient(Buffer, BlueOffset, GreenOffset, Colour);
}
