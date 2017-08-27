/*
###############################################################################
# Copyright (c) 2017, PulseRain Technology LLC 
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License (LGPL) as 
# published by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but 
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
# or FITNESS FOR A PARTICULAR PURPOSE.  
# See the GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
###############################################################################
*/
const uint8_t wav_file_name [] = "SPIDER.WAV";
//============================================================================
// Demo: Home Automation with DTMF Control
//     This demo can be used along with an answer machine. When the answer 
// machine picks up the call, the caller can key in DTMF passcode and commands
// to start home automation. In this demo, it simply reads out temperature 
// measured by onboard TSD. 
//
//     This demo involves almost all the onboard peripherals:
//  *) The onboard microphone and speaker audio jack through Si3000 CODEC
//  *) The onboard SRAM (M23XX1024)
//  *) The SD card (audio files are saved on a microSD card)
//  *) PWM (If phone is picked up by using PWM, the Sparkfun Ardumoto - 
//     Motor Driver Shield is needed
//============================================================================

#include <M10CODEC.h>
#include <M10DTMF.h>
#include <M10SD.h>
#include <M10SRAM.h>
#include <M10PWM.h>
#include <M10ADC.h>

#define WAV_PLAY_SD_STEP_SIZE 64
#define ASTERISK 14
#define POUND 15


//----------------------------------------------------------------------------
// get_u16()
//
// Parameters:
//      ptr    : pointer to BYTE
//
// Return Value:
//      (*ptr) + ((*(ptr+1)) << 8)
//
// Remarks:
//      get a word from the pointer
//----------------------------------------------------------------------------

uint16_t get_u16 (uint8_t *ptr)
{
  uint16_t t;
  t = (uint16_t)(*ptr);
  ++ptr;
  t += (uint16_t)(*ptr) << 8;

  return t;
} // get_u16()


//----------------------------------------------------------------------------
// get_u32()
//
// Parameters:
//      ptr    : pointer to BYTE
//
// Return Value:
//      (*ptr) + ((*(ptr+1)) << 8) + ((*(ptr+2)) << 16) + ((*(ptr+3)) << 24) 
//
// Remarks:
//      get a DWORD from the pointer
//----------------------------------------------------------------------------

uint32_t get_u32(uint8_t *ptr)
{
  uint16_t t;
  uint32_t ret;
  
  ret = get_u16(ptr);
  
  ptr += 2;
  
  t = get_u16(ptr);

  ret += (uint32_t)t << 16;
  
  return ret;  
} // End of get_u32()


//----------------------------------------------------------------------------
// sample buffer and scratch buffer
//----------------------------------------------------------------------------

uint8_t wav_samp_buffer [256];
uint8_t wav_samp_scratch_buffer[256];



//----------------------------------------------------------------------------
// str_cmp_case_insensitive()
//
// Parameters:
//      a    : pointer to buffer A
//      b    : pointer to buffer B
//      cnt  : buffer size to compare 
//
// Return Value:
//      return 0 if buffer A and buffer B are the same (case insensitive)
//
// Remarks:
//      compare byte buffer with case insensitive
//----------------------------------------------------------------------------

uint8_t str_cmp_case_insensitive (uint8_t *a, uint8_t *b, uint8_t cnt)
{
  uint8_t i, ta, tb;

  for (i = 0; i < cnt; ++i) {
    ta = a[i]; 
    tb = b[i];

    if ((ta >= 'a') && (ta <= 'z')) {
      ta = ta - 'a' + 'A';
    }

    if ((tb >= 'a') && (tb <= 'z')) {
      tb = tb - 'a' + 'A';
    }

    if (ta != tb) {
      return 1;
    }
  }

  return 0;
  
} // End of str_cmp_case_insensitive()


//----------------------------------------------------------------------------
// read_file()
//
// Parameters:
//      cnt  : buffer size to read
//      buf  : string to compare against (set it to 0 for no string comparison
//
// Return Value:
//      return 0 if data is read ok and is the same as the buf (case insensitive)
//
// Remarks:
//      read data into wav_samp_buffer, and compare it against buf
//----------------------------------------------------------------------------

uint8_t read_file(uint8_t cnt, uint8_t *buf)
{
  uint16_t br, btr;
  uint8_t res;

  btr = cnt; br = 0;
  res = SD.fread(wav_samp_buffer, btr, &br);
  if ((res) || (btr != br)) {
    Serial.write ("wav read error\n");
    return 0xff;
  }

  if(buf) {
      res = str_cmp_case_insensitive(wav_samp_buffer, buf, btr);
    
      if (res) {
   //     Serial.write ("!!!!!!!! not match the subtrunk of");
   //     Serial.write (buf, cnt);
   //     Serial.write ("\n");
        return 0xff;
      } else {
   //     Serial.write ("==> match ");
   //     Serial.write (buf, cnt);
   //     Serial.write ("\n");
      }
  }

  return 0;
  
} // End of read_file()


//----------------------------------------------------------------------------
// flags for wav file
//----------------------------------------------------------------------------

uint16_t block_align;
uint16_t num_of_channels;
uint32_t num_of_samples;
uint8_t wav_file_header_size;


//----------------------------------------------------------------------------
// parse_wav_file_head()
//
// Parameters:
//      None
//
// Return Value:
//      return 0 if wav file head is parsed ok 
//
// Remarks:
//      parse wav file head, only mono 8KHz sample rate is supported.
//----------------------------------------------------------------------------

uint8_t parse_wav_file_head ()
{
  uint16_t t;
  uint8_t res;
  uint32_t subchunk_size, sample_rate;
  
  uint8_t cnt = 0;

//  Serial.write ("==========================================\n");
//  Serial.write (" Start parsing head for wav file\n");
//  Serial.write ("==========================================\n");

  // chunk_ID
      read_file (4, "RIFF");
      cnt += 4;

  // chunk_size 
      read_file (4, 0);
      cnt += 4;
      
  // format 
      read_file (4, "WAVE");
      cnt += 4;
      
  // subchunk1_ID
      read_file (4, "fmt ");
      cnt += 4;
      
  // subchunk1_size
      read_file (4, 0);
      cnt += 4;
      subchunk_size = get_u32(wav_samp_buffer);
  //    Serial.write ("subchunk1_size ");
  //    Serial.println(subchunk_size);


  // audio_format
      read_file (2, 0);
      cnt += 2;
  //    Serial.write ("audio format ");
  //    Serial.println(get_u16(wav_samp_buffer));

  // num_of_channels
      read_file (2, 0);
      cnt += 2;
      num_of_channels = get_u16(wav_samp_buffer);
  //    Serial.write ("num_of_channels ");
  //    Serial.println(num_of_channels);

      if (num_of_channels != 1) {
  //      Serial.write ("only mono (single channel) is supported!\n");
        return 0xff;  
      }

  // sample_rate 
      read_file (4, 0);
      cnt += 4;
      sample_rate = get_u32(wav_samp_buffer);
  //    Serial.write ("sample_rate ");
  //    Serial.println(sample_rate);

      if (sample_rate != 8000) {
  //      Serial.write ("not 8KHz sample rate!\n");
        return 0xff;
      }

  // byte_rate
      read_file (4, 0);
      cnt += 4;
  //    Serial.write ("byte_rate ");
  //    Serial.println(get_u32(wav_samp_buffer));

  // block_align
      read_file (2, 0);
      cnt += 2;
      block_align = get_u16(wav_samp_buffer);
  //    Serial.write ("block_align ");
  //    Serial.println(block_align);


  // bits_per_sample
      read_file (2, 0);
      cnt += 2;
  //    Serial.write ("bits_per_sample ");
  //    Serial.println(get_u16(wav_samp_buffer));

  // read extra
      if ((20 + subchunk_size) > cnt) {
        t = 20 + subchunk_size - cnt;
 //       Serial.write ("read extra pad ");
 //       Serial.print(t);
 //       Serial.write (" bytes\n");
        read_file (t, 0);
      }
      
 // data
      t = 4;
      do { 
       res = read_file (4, "data");
       cnt += 4; 
       read_file (4, 0);
       cnt += 4;
       subchunk_size = get_u32(wav_samp_buffer);

       if (res) {
  //        Serial.write ("skip this subtrunk\n");
          read_file (subchunk_size, 0);
          cnt += subchunk_size;
       }

       --t;
       
      } while (res && t);

      if (!t) {
        Serial.write ("can not find data subtrunk\n");
        return 0xff; 
      }

      wav_file_header_size = cnt;

//      Serial.write ("wav_file_header_size ");
//      Serial.println(wav_file_header_size);
      
      num_of_samples = subchunk_size / block_align;
//      Serial.write ("num_of_samples = ");
//      Serial.println(num_of_samples);


//  Serial.write ("==========================================\n");
//  Serial.write (" End parsing the head for wav file\n");
//  Serial.write ("==========================================\n");
  
  return 0;
  
} // End of parse_wav_file_head()

//----------------------------------------------------------------------------
// load_wav_into_SRAM()
//
// Parameters:
//      None
//
// Return Value:
//      None
//
// Remarks:
//      load wav file samples (The first 128KB) into SRAM
//----------------------------------------------------------------------------

void load_wav_into_SRAM()
{
  uint32_t i, max_num_of_samp, addr;
  uint16_t sample;
  uint8_t t, res;

  if (num_of_samples > 65536) {
    max_num_of_samp = 65536;
  } else {
    max_num_of_samp = num_of_samples;
  }

  addr = 0;
  for (i = 0; i < max_num_of_samp; ++i) {
    res = read_file (block_align, 0);
    if (res) {
      break;
    }

    sample = get_u16(wav_samp_buffer);
    
    t = sample & 0xFF;
    SRAM.write(addr, t);
    
    t = (sample >> 8) & 0xFF;
    ++addr;
          
    SRAM.write(addr, t);
    ++addr;
  }
  
} // End of load_wav_into_SRAM()


//----------------------------------------------------------------------------
// play_wav_from_SRAM()
//
// Parameters:
//      None
//
// Return Value:
//      None
//
// Remarks:
//      play audio samples stored in the SRAM
//----------------------------------------------------------------------------

void play_wav_from_SRAM()
{
  uint8_t t;
  uint32_t i, max_num_of_samp, addr;
  uint16_t sample;

  uint8_t delta;
  uint16_t br = 0, btr = 0;

  
  if (num_of_samples > 65536) {
      max_num_of_samp = 65536;
  } else {
      max_num_of_samp = num_of_samples;
  }

  t = 0;
  addr = 0;
  for (i = 0; i < max_num_of_samp; ++i) {
   
      t = SRAM.read(addr);
      sample = t;

      ++addr;
      t = SRAM.read(addr);
      sample += (uint16_t)t << 8;

      ++addr;
    
      CODEC.sampleWrite(sample);
    
  } // End of for loop

} // End of play_wav_from_SRAM()

//----------------------------------------------------------------------------
// play_wav_file_on_sd()
//
// Parameters:
//      file_name: file name on the microSD card to be played
//
// Return Value:
//      None
//
// Remarks:
//      play audio samples stored in the SRAM
//----------------------------------------------------------------------------

void play_wav_file_on_sd (uint8_t *file_name)
{ 
    uint8_t t;

    Serial.write ("Play ");
    Serial.write(file_name);

    t = SD.fopen(file_name);
    
    if (t) {
      Serial.write("\n SD open fail\n");
    } else {
      // Serial.write("\n SD open success\n");
    }
  
    t = parse_wav_file_head();

  
    if (t) {
        Serial.write("\n wav file open error\n");
        return 1;
    } else {
      // Serial.write ("\n Playing ... \n");
    }


    load_wav_into_SRAM();
    play_wav_from_SRAM();
    
    Serial.write("  - done\n");

} // End of play_wav_file_on_sd()

uint8_t* digit_file_name[] = {"ZERO.WAV", 
                              "ONE.WAV", 
                              "TWO.WAV", 
                              "THREE.WAV", 
                              "FOUR.WAV",
                              "FIVE.WAV",
                              "SIX.WAV",
                              "SEVEN.WAV",
                              "EIGHT.WAV",
                              "NINE.WAV"};

//----------------------------------------------------------------------------
// temperature_broadcast()
//
// Parameters:
//      None
//
// Return Value:
//      None
//
// Remarks:
//      read celsius degree from TSD, and read it out by playing audio files
//----------------------------------------------------------------------------
                              
void temperature_broadcast() 
{
    
    int16_t celsius, t;
    uint8_t i, n, digit, minus = 0;
    uint8_t num[3];
   
    Serial.write ("Temperature: ");
    celsius = ADC.getCelsius();
    Serial.println(celsius);
    
    if (celsius < 0) {
        minus = 1;
    }

    for (i = 0; i < 3; ++i) {
        t = celsius % 10;
        num[i] = (uint8_t) t;
        celsius /= 10;
    } // End of for loop

    play_wav_file_on_sd ("GREETING.WAV");      
 
    if (minus) {
        play_wav_file_on_sd ("MINUS.WAV");      
    }

    // get the celsius into digits
    i = 3;
    do {
        --i;
        if (num[i] != 0) {
            break;              
        }
    } while (i > 0);

    n = i;
    
    for (i = 0; i <= n; ++i) {
        digit = num[n - i];
        play_wav_file_on_sd(digit_file_name[digit]);
    } // End of for loop

    play_wav_file_on_sd ("DEGREE.WAV");
    
} // End of temperature_broadcast()

enum FSM_State {
    STATE_INIT,
    STATE_OBTAIN_NUMBER,
    STATE_ACTION
};

int8_t dtmf_buffer[256];
uint8_t dtmf_buffer_index;

#define PASSCODE_LENGTH 6
int8_t passcode[PASSCODE_LENGTH] = {1, 2, 3, 4, 5, 6};

#define BYEBYE_LENGTH 3
int8_t byebye[BYEBYE_LENGTH] = {9, 9, 9};

#define TEMPERATURE_LENGTH 3
int8_t temperature[TEMPERATURE_LENGTH] = {8, 8, 8};

//----------------------------------------------------------------------------
// cmd_match()
//
// Parameters:
//      cmd    : command string to be matched against
//      length : length of the command string
//
// Return Value:
//      1 : positive match
//      0 : no match
//
// Remarks:
//      function to match the given command string against what's in the
// dtmf_buffer
//----------------------------------------------------------------------------

uint8_t cmd_match(int8_t *cmd, uint8_t length)
{ 
  uint8_t i;
  
  for (i = 0; i < length; ++i) {
      
      if (cmd[i] != dtmf_buffer[i]) {
          return 0;
      }
  } // End of for loop
           
  return 1;
  
} // End of cmd_match()

uint8_t command_enable = 0;



//----------------------------------------------------------------------------
// FSM()
//
// Parameters:
//      dtmf_value  : dtmf key (0-15) received/decoded 
//
// Return Value:
//      None
//
// Remarks:
//      FSM to act on the DTMF key received
//----------------------------------------------------------------------------

void FSM(int8_t dtmf_value)  
{
    uint8_t i, next;
    static enum FSM_State state = STATE_INIT;
  
    if (dtmf_value < 0) {
        return;
    }

    do {
        next = 0;
        switch (state) {
          
            case STATE_INIT:
                dtmf_buffer_index = 0;
                if (dtmf_value != ASTERISK) {
                    state = STATE_OBTAIN_NUMBER; 
                    dtmf_buffer[dtmf_buffer_index++] = dtmf_value;
                }
                break;
               
            case STATE_OBTAIN_NUMBER:
                if (dtmf_value ==  ASTERISK) {
                    state = STATE_INIT;
                } else if (dtmf_value == POUND) {
                    state = STATE_ACTION;
                    next = 1; 
                } else {
                    dtmf_buffer[dtmf_buffer_index++] = dtmf_value;
                }
                
                break;
    
            case STATE_ACTION:
                state = STATE_INIT;

                Serial.write ("\n Action: ");

                for (i = 0; i < dtmf_buffer_index; ++i) {
                    Serial.print (dtmf_buffer[i]);  
                }

                Serial.write (" \n");
                
                if (cmd_match(passcode, PASSCODE_LENGTH)) {
                    Serial.write ("Passcode match!\n");
                    play_wav_file_on_sd ("HELLO.WAV");
                    command_enable = 1;
                          
                }

                if (command_enable) {

                    if (cmd_match(temperature, TEMPERATURE_LENGTH)) {
                       Serial.write ("Temperature!\n");
                       temperature_broadcast();    
                    }
                    
                    if (cmd_match(byebye, BYEBYE_LENGTH)) {
                       Serial.write ("Byebye!\n");
                       play_wav_file_on_sd ("BYE.WAV");
                       play_wav_file_on_sd ("GOODDAY.WAV");
                       command_enable = 0;
                    }
                }
                  
                break;
               
            default:
                state = STATE_INIT;
                break;
        } // End of switch
    } while (next);
    
} // End of FSM()





//============================================================================
// setup()
//============================================================================

void setup() {
    uint8_t t;
    
    delay(1000);
    Serial.begin(115200);
    delay(1000);
    
    DTMF.begin();
    SRAM.begin();

    t = SD.begin();

    if (t) {
      Serial.write("SD init fail\n");
    } else {
      Serial.write("SD init success\n");
    }

    ADC.begin();
    ADC.getCelsius();
    
    Serial.write("DTMF Detector\n");

  
} // End of setup()



//============================================================================
// loop()
//============================================================================

void loop()  {
  int8_t t;
  uint16_t adc_value;

  t = DTMF.decode();

  if (t >= 0) {
      Serial.print(t, HEX);
  }
  
  FSM(t);
  

/*  if (t >= 0) {
    Serial.print(t, HEX);
    Serial.print(ADC.getCelsius());
//    play_wav_file_on_sd ("SPIDER.WAV");
     
  }       
*/
      
} // End of loop()

