/*--------------------------------------------------
*	File Name : win32_handmade.cpp
* 	Creation Date : 28-03-2022
*	Revision :
*	Last Modified : 2022-06-08 7:41:01 PM
*	Created By : David Dennis
---------------------------------------------------*/

/*
 * THIS IS NOT A FINAL PLATFORM LAYER
 *
 * Stuff to do:
 * - Saved Game Locations
 * - Getting a handle to our own executable
 * - Asset loading path
 * - Threading
 * - Raw input (support for multiple keyboards)
 * - Sleep
 * - ClipCursor (for multimonitor support)
 * - fullscreen support
 * - WM_SETCURSOR
 * - QueryCancelAutoplay
 * - WM_ACTIVEATE_APP
 * - Blit speed improvements
 * - Hardware Acceleration (Graphics) (OpenGL)
 *
 */




#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>


#define internal static 
#define local_persist static 
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

#include "handmade.cpp"

//struct for the bitmap data
struct win32_offscreen_buffer{
	BITMAPINFO BitmapInfo;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

//These should go away eventually
global_variable bool Running;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int XOffset;
global_variable int YOffset;
global_variable int Colour;

struct win32_window_dimension{
	int Width;
	int Height;
};





//Function pointers for controller input
//XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub){
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//XInputSetState ((Vibrations))
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub){
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

//Function pointer for direct sound initialization
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuiDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);


internal void Win32LoadXInput(void){
	HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
	if(!XInputLibrary){
		//TODO: Diagnostic (which version are we running)
		HMODULE XInputLibrary = LoadLibrary("xinput1_3.dll");
	}

	if(XInputLibrary){
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
	}else{
		//TODO: Diagnostic (Logging)
	}
}

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize){
	//load the library
	HMODULE DSoundLibrary = LoadLibrary("dsound.dll");

	if(DSoundLibrary){

		//get a direct sound object
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))){
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*4;
			WaveFormat.nBlockAlign = 4;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.cbSize = 8;
			
			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))){
				
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				//Create Primary Buffer
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))){
					if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))){
						//WE SET THE FORMAT!
					}else{
						//TODO:Diagnostic
					}
				}else{
					//TODO:Diagnostic
				}
			}else{
				//TODO: Diagnastic (Logging)
			}	

			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.dwFlags = 0;
			BufferDescription.lpwfxFormat = &WaveFormat;

			//Create Secondary Buffer
			if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0))){
				//DO NOT Start it playing
			}else{
				//TODO:Diagnostic
			}
		}else{
			//TODO: Diagnostic (Logging)
		}
	}
}




//returns the size of the window
internal win32_window_dimension Win32GetWindowDimension(HWND Window){
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return(Result);
}


//test

//On Resize
//Grab enough memory for all the pixels in the resized window
internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height){

	//free the memory if it already is allocated
	if(Buffer->Memory){
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);	
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	//set header with info
	Buffer->BitmapInfo.bmiHeader.biSize = sizeof(Buffer->BitmapInfo.bmiHeader);
	Buffer->BitmapInfo.bmiHeader.biWidth = Buffer->Width;
	Buffer->BitmapInfo.bmiHeader.biHeight = -Buffer->Height; //This is negative so the bitmap can be read from top down
	Buffer->BitmapInfo.bmiHeader.biPlanes = 1;
	Buffer->BitmapInfo.bmiHeader.biBitCount = 32;
	Buffer->BitmapInfo.bmiHeader.biCompression = BI_RGB;
	
	//Calc size of window
	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*Buffer->BytesPerPixel;
	//Allocate
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);

	Buffer->Pitch = Buffer->Width*Buffer->BytesPerPixel;

}

//Repaint window, using the bitmap
internal void Win32CopyBufferToWindow(HDC DeviceContext, 
				      int WindowWidth, int WindowHeight, 
				      win32_offscreen_buffer Buffer, 
				      int X, int Y, 
				      int Width, int Height)
{

	//TODO Aspect Ratio Correction
	StretchDIBits(
		DeviceContext, 
		/*
		X, Y, Width, Height, 
		X, Y, Width, Height, 
		*/
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer.Width, Buffer.Height,
		Buffer.Memory,	
		&Buffer.BitmapInfo, 
		DIB_RGB_COLORS, SRCCOPY);
}

//Message loop
internal LRESULT CALLBACK 
Win32MainWindowCallback(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam)
{
	LRESULT result = 0;
	switch(Message){
		case WM_SIZE:
		{	
		}
		break;
		case WM_DESTROY:
		{
			Running = false;
		}
		break;
		case WM_CLOSE:
		{
			Running = false;
		}
		break;
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATE APP\n");			
		}
		break;			
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		//case WM_KEYUP:
		{
			uint32 VKCode = WParam;
			bool WasDown = ((LParam & (1<<30)) != 0);
			bool IsDown = ((LParam & (1<<31)) != 0);
			if(VKCode == 'W'){
				YOffset += 100;
			}else if(VKCode == 'A'){
				XOffset += 100;
			}else if(VKCode == 'S'){
				YOffset -= 100;
			}else if(VKCode == 'D'){
				XOffset -= 100;
			}else if(VKCode == 'Q'){

			}else if(VKCode == 'E'){

			}else if(VKCode == VK_UP){
				YOffset += 10;
			}else if(VKCode == VK_DOWN){
				YOffset -= 10;
			}else if(VKCode == VK_LEFT){
				XOffset += 10;
			}else if(VKCode == VK_RIGHT){
				XOffset -= 10;
			}else if(VKCode == VK_ESCAPE){

			}else if(VKCode == VK_SPACE){
				if(!IsDown && !WasDown){
					if(Colour == 1){
						Colour = 0;
					}else{
						Colour = 1;
					}
				}
			}
			bool AltKeyWasDown = LParam & (1 << 29);
			if((VKCode == VK_F4) && AltKeyWasDown){
				Running = false;
			}


		}
		break;
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32CopyBufferToWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer, X, Y, Width, Height);
			EndPaint(Window, &Paint);
		}
		break;
		default:
		{
			//OutputDebugStringA("DEFAULT\n");
			result = DefWindowProc(Window, Message, WParam, LParam); 
		}
	}
	return result;
}

struct win32_sound_output{
	//Sound
	int SamplesPerSecond;
	int BytesPerSample;
	int16 ToneVolume;
	int ToneHz;
	uint32 RunningSampleIndex;
	int WavePeriod;
	int SecondaryBufferSize;
	int LatencySampleCount;
};

internal void Win32ClearSoundBuffer(win32_sound_output *SoundOutput){
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, 
						&Region1, &Region1Size, 
						&Region2, &Region2Size, 0))){

		uint8 *DestSample = (uint8 *)Region1;
		for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex){
			*DestSample++ = 0;
		}
		DestSample = (uint8 *)Region2;
		for(DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex){
			*DestSample++ = 0;
		}

		
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void Win32FillSoundBuffer(	win32_sound_output *SoundOutput, 
					DWORD ByteToLock, 
					DWORD BytesToWrite,
					game_sound_output_buffer * SourceBuffer){
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;


	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, 
						&Region1, &Region1Size, 
						&Region2, &Region2Size, 0))){

		//Assert region1/region2 are valid
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;
		DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex){
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
		DestSample = (int16 *)Region2;
		for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex){
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}

}


//MAIN
int CALLBACK 
WinMain(HINSTANCE Instance,
  	HINSTANCE PrevInstance,
  	LPSTR     CmdLine,
  	int       ShowCode)
{	

	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	Win32LoadXInput();	

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
	

	WindowClass.style = CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance; 
	//WindowClass.hIcon = ; 
	WindowClass.lpszClassName = "HandmadeWindowClass"; 

	

	if(RegisterClass(&WindowClass)){
		HWND Window = 
			CreateWindowEx(
				0, 
				WindowClass.lpszClassName, 
				"HandmadeHero", 
				WS_OVERLAPPEDWINDOW|WS_VISIBLE, 
				CW_USEDEFAULT, 
				CW_USEDEFAULT, 
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				0,
				0,
				Instance,
				0);
		if(Window){
			MSG Message;
			Running = true;
			//Graphics
			XOffset = 0;
			YOffset = 0;
			Colour = 0;	
	
			//SoundTest
			win32_sound_output SoundOutput = {};

			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.ToneVolume = 1000;
			SoundOutput.ToneHz = 256;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;

			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearSoundBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			
			LARGE_INTEGER LastCounter;
			QueryPerformanceCounter(&LastCounter);

			uint64 LastCycleCount = __rdtsc();

			while(Running){

				while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)){
					if(Message.message == WM_QUIT){
						Running = false;
					}
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				//TODO: should we poll this more frequently
				//Gamepad input
				for(DWORD i = 0; i < XUSER_MAX_COUNT; i++){
					XINPUT_STATE ControllerState;
					if(XInputGetState(i, &ControllerState) == ERROR_SUCCESS){
						//this controller is plugged in
						//ControllerState.dwPacketNumber, is something to watch if it increases too rapidly
						XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

						bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						bool BBUTTON = (Pad->wButtons & XINPUT_GAMEPAD_B);
						bool XBUTTON = (Pad->wButtons & XINPUT_GAMEPAD_X);
						bool YBUTTON = (Pad->wButtons & XINPUT_GAMEPAD_Y);

						int16 StickX = Pad->sThumbLX;
						int16 StickY = Pad->sThumbLY;
					}else{
						//controller is not available
					}
				}
				//Sound Buffer Test
				DWORD PlayCursor;
				DWORD WriteCursor;
				DWORD TargetCursor;
				DWORD ByteToLock;
				DWORD BytesToWrite;
				bool SoundIsValid = false;
				if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))){

					ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) 
								% SoundOutput.SecondaryBufferSize);

					TargetCursor = ((PlayCursor + (SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample)) 
								% SoundOutput.SecondaryBufferSize);
					if(ByteToLock > TargetCursor){
						BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
						BytesToWrite += TargetCursor;
					}else{
						BytesToWrite = TargetCursor - ByteToLock;
					}

					SoundIsValid = true;
				}


				int16 Samples[48000 * 2];
				game_sound_output_buffer SoundBuffer = {};
				SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
				SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
				SoundBuffer.Samples = Samples;
				
				game_offscreen_buffer Buffer = {};
				Buffer.Memory = GlobalBackBuffer.Memory;
				Buffer.Width = GlobalBackBuffer.Width;
				Buffer.Height = GlobalBackBuffer.Height;
				Buffer.Pitch = GlobalBackBuffer.Pitch;
				GameUpdateAndRender(&Buffer, &SoundBuffer, XOffset, YOffset, Colour, SoundOutput.ToneHz);

				if(SoundIsValid){
					Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
				}

				HDC DeviceContext = GetDC(Window);

				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				Win32CopyBufferToWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer, 0, 0, Dimension.Width, Dimension.Height);
				
				//Frame Counting code
				uint64 EndCycleCount = __rdtsc();
				LARGE_INTEGER EndCounter;
				QueryPerformanceCounter(&EndCounter);

				uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
				int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
				real32 MSPerFrame = ((1000.0f * (real32)CounterElapsed) / (real32)PerfCountFrequency);
				real32 FPS = (real32)PerfCountFrequency / (real32)CounterElapsed;
				real32 MCPF = ((real32)CyclesElapsed / (1000.0f*1000.0f));
				
				char Buff[256];
				sprintf(Buff, "%.02fms/f / %.02fFPS / %.02fmc/f\n", MSPerFrame, FPS, MCPF);
				OutputDebugStringA(Buff);

				LastCycleCount = EndCycleCount;
				LastCounter = EndCounter;


				ReleaseDC(Window, DeviceContext);

				//++XOffset;
			}
		}
	}
	return(0);	
}
