#include <dos.h>
#include <stdio.h>
#include <stdlib.h>

#define BASE_FREQ 1193180       //frequency for all timers before division
#define SHARP_NOTE_MUL 1.059462	//sharp note = note * mul
#define BPS 6                   //notes per second
#define mSPB 1000/BPS           //milliseconds per note. Plug directly into delay()

//TASKS:
//- use system speaker somehow(play Megalovania ofc)
//- output "state words" for each channel in binary
//- find freq coefficient for each channel in HEX
//- make RNG(use 2nd channel for that)


//INFO:
//0x43 - commang register
//0x40-0x42 - data I/O registers for channels 1-3
//Algorithm - write command to 0x43, read/write from corr. channel port
//main commands - outp(), inp()
//Access to speaker itself - through port 0x61. 2 low bits control it


//MUSIC INFO:
//note duration - 1/5 or 1/6th of second(1/5.5?)
//notes - can be "bagfedc" (si <- do)
//sharp notes -  " AGF DC" (a bit higher in freq, have define for that)


//note - 'A' to 'g'
//octave range:
//sub-contr(0), contr(1), big(2), smol(3), 1st(4)(main), ..., 5th(8)
void playNote(char note, int octave, int dur_ms)
{
   //'-' means skip note
   if(note == '-')
   {
      delay(mSPB);
      return;
   }

   char isSharp = 0;  //indicator of sharp note
   if(note >= 'A' && note <= 'G')
   {
      //supports non-standard sharp notes
      note += 0x20;	//convert to lower case
      isSharp = 1;
   }

   float noteBaseFreq;	 //stores frequency
   switch (note)	 //load base frequency
   {
      case ('a'):        //lya
      {
	 noteBaseFreq = 440.00;
	 break;
      }
      case ('b'):        //si (highest)
      {
	 noteBaseFreq = 493.88;
	 break;
      }
      case ('c'):        //do (lowest)
      {
	 noteBaseFreq = 261.63;
	 break;
      }
      case ('d'):        //re
      {
	 noteBaseFreq = 293.66;
	 break;
      }
      case ('e'):        //mi
      {
	 noteBaseFreq = 329.63;
	 break;
      }
      case ('f'):        //fa
      {
	 noteBaseFreq = 349.23;
	 break;
      }
      case ('g'):        //sol
      {
	 noteBaseFreq = 391.96;
	 break;
      }
      default:
      {
	 return;
      }
   }

   //now, apply octave and sharp note info
   //(idk if this will work with negative powers
   if(isSharp)
   {
      noteBaseFreq *= SHARP_NOTE_MUL;
   }

   //calculate divider to pass to timer
   int divider = BASE_FREQ / noteBaseFreq;
   //apply octave info
   if (octave < 4) //">>" doesn't work with negative shift...
   {
      divider = divider << (4 - octave);
   }
   else
   {
      divider = divider >> (octave - 4);
   }

   //prepare frequency
   outp(0x43, 0xB6); //command to write 2 bytes into speaker timer
   outp(0x42, divider & 0xFF);   //write low byte
   divider = divider >> 8;
   outp(0x42, divider);    //write high byte

   outp(0x61, inp(0x61)|3);   //turn on
   delay(dur_ms);               //wait
   outp(0x61, inp(0x61)^3);   //turn off
}



void main()
{
   int i;
   int j;

	//get state of each channel

   //1100 C2 C1 C0 0 - command format
   char state[3];
   outp(0x43, 0xC2);	//request for channel 0
   state[0] = inp(0x40);//get requested data from corr. port
   outp(0x43, 0xC4);    //... for 1
   state[1] = inp(0x41);
   outp(0x43, 0xC8);    //... for 2
   state[2] = inp(0x42);
   //Output in binary
   char numBin[9];
   printf("Channel states(0-2): ");
   for(i=0;i<3;i++)
   {
      itoa(state[i], numBin, 2);
      printf("%s ", numBin);
   }
   printf("\n");



	//Get frequency divider coefficients for 3 channels

   int dividers[3] = { 0 };
   int iters = 1000; //number of samples in hope to get max value
   //get divider coefficients
   //1st channel:
   for(j = 0;j<iters;j++)
   {
      outp(0x43, 0x00);
      int val = inp(0x40);
      val += inp(0x40)*256;
      if(val > dividers[0])
      {
	 dividers[0] = val;
      }
   }
   //2nd channel:
   for(j = 0;j<iters;j++)
   {
      outp(0x43, 0x40);
      int val = inp(0x41);
      val += inp(0x41)*256;
      if(val > dividers[1])
      {
	 dividers[1] = val;
      }
   }
   //3rd channel
   outp(0x61, inp(0x61)|1); //turn on the counter
   for(j = 0;j<iters;j++)
   {
      outp(0x43, 0x80);
      int val = inp(0x42);
      val += inp(0x42)*256;
      if(val > dividers[2])
      {
	 dividers[2] = val;
      }
   }
   outp(0x61, inp(0x61)^1); //turn off the counter

   //Output in HEX
   char numHex[5];
   printf("Channel coefficients(0-2): ");
   for(i=0;i<3;i++)
   {
      itoa(dividers[i], numHex, 16);
      printf("%s ", numHex);
   }
   printf("\n");



	//Part with RNG generator

   //Algorithm: use user delay as seed, generate 1st byte,
   //XOR to result, move 2 bits to left, delay 0-2 ms based on result,
   //get next byte, XOR, etc. total 4 times
   //Without delay - repetative results.
   int maxVal;
   printf("Input max value for RNG generator(2-byte): ");
   outp(0x61, inp(0x61)|1);  //turn on counter, not speaker
   scanf("%d", &maxVal);
   fflush(stdin);
   outp(0x43, 0xB6);  //command to write divider in counter 2
   outp(0x42, 0xFF);
   outp(0x42, 0x00);  //write 255 into it, to get a byte
   unsigned int fullRndInt = 0;
   for(i = 0;i<4;i++)
   {
      outp(0x43, 0xF8);
      int rndByte = inp(0x42);
      //printf("%d\n", rndByte);
      fullRndInt ^= rndByte;
      inp(0x42); //to complete read operation
      fullRndInt = fullRndInt << 2;
      delay(fullRndInt%3);
   }
   outp(0x61, inp(0x61)^1);  //turn off counter
   unsigned int rndInt = fullRndInt % maxVal;
   printf("Random number: %u\n", rndInt);





   printf("Input 'B' for a song!\n");
   if(getchar() != 'B')
   {
      return;
   }
   fflush(stdin);

	//Playing a small song(Borlandovania)
					     //||2nd row                 ||3rd row                 ||4th row
   char notes[]   = {"ddd-a--G-g-f-dfgccd-a--G-g-f-dfgbbd-a--G-g-f-dfgAAd-a--G-g-f-dfg"};//ddd-a--G-g-f-dfgccd-a--G-g-f-dfgbbd-a--G"};
   char octaves[] = {"44504004040404444450400404040444335040040404044433504004040404444450400404040444445040040404044433404004"};

   int npos = 0;
   while(notes[npos] != '\0')  //play untill note code 0
   {
      playNote(notes[npos], octaves[npos] - '0', mSPB);
      delay(3);	//to separate notes, otherwise it sounds off
      npos++;
   }

   getchar();
}




