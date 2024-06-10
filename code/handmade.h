#if !defined(HANDMADE_H)
/*--------------------------------------------------
*	File Name : handmade.h
* 	Creation Date : 21-05-2022
*	Revision :
*	Last Modified : 2022-06-08 7:33:14 PM
*	Created By : David Dennis
---------------------------------------------------*/

//Services that the platform layer provides to the game
//....nothing for now


//Services that the game provides to the platform layer

//Input: keyboard input, bitmap buffer, sound buffer, timing
struct game_offscreen_buffer{
	void *Memory;
	int Width;
	int Height;
	int Pitch;
};

struct game_sound_output_buffer{
	int SamplesPerSecond;
	int SampleCount;
	int16 *Samples;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer, int BlueOffset, int GreenOffset, int Colour, int ToneHz);



#define HANDMADE_H
#endif
