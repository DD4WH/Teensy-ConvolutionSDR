/*********************************************************************************************
 * (c) Frank DD4WH 2017_01_26
 *  
 * "Teensy Convolution SDR" 
 * 
 * FFT Fast Convolution = Digital Convolution
 * with overlap - save = overlap-discard
 * dynamically creates FIR coefficients  
 * 
 * in floating point 32bit 
 * tested on Teensy 3.6
 * compile with 180MHz F_CPU, other speeds not supported
 * 
 * Part of the evolution of this project has been documented here:
 * https://forum.pjrc.com/threads/40188-Fast-Convolution-filtering-in-floating-point-with-Teensy-3-6/page2
 * 
 * FEATURES
 * - 12kHz to 30MHz Receive
 * - I & Q - correction in software
 * - efficient frequency translation without multiplication
 * - efficient spectrum display using a 256 point FFT on the first 256 samples of every 4096 sample-cycle 
 * - efficient AM demodulation with ARM functions
 * - efficient DC elimination after AM demodulation
 * - implemented nine different AM demodulation algorithms for comparison
 * - real SAM - synchronous AM demodulation with phase determination by atan2 implemented
 * - Stereo-SAM and sideband-selected SAM
 * - sample rate 96k and decimation-by-8 for efficient realtime calculations
 * - spectrum Zoom function 1x, 2x, 4x, 8x, 16x
 * - Automatic gain control (high end algorithm by Warren Pratt, wdsp)
 * - plays MP3 and M4A (iTunes files) from SD card with the awesome lib by Frank Bösing
 * - automatic IQ amplitude and phase imbalance correction
 * - dynamic frequency indicator figures and graticules on spectrum display x-axis 
 *  
 * TODO:
 * - save settings in EEPROM
 * - load settings from EEPROM
 * - record and playback IQ audio stream ;-)
 * - filter bandwidth limiting dependent on sample rate
 * - reset codec (is that possible???), when twin-peak syndrome appears
 * 
 * some parts of the code modified from and/or inspired by
 * my good old Teensy SDR (rheslip & DD4WH): https://github.com/DD4WH/Teensy-SDR-Rx
 * mcHF (KA7OEI, DF8OE, DB4PLE, DD4WH): https://github.com/df8oe/mchf-github/
 * libcsdr (András Retzler): https://github.com/simonyiszk/csdr 
 * wdsp (Warren Pratt): http://svn.tapr.org/repos_sdr_hpsdr/trunk/W5WC/PowerSDR_HPSDR_mRX_PS/Source/wdsp/
 * Wheatley (2011): cuteSDR https://github.com/satrian/cutesdr-se 
 * sample-rate-change-on-the-fly code by Frank Bösing
 * GREAT THANKS FOR ALL THE HELP AND INPUT BY WALTER, WMXZ ! 
 * Audio queue optimized by Pete El Supremo 2016_10_27, thanks Pete!
 * An important hint on the implementation came from Alberto I2PHD, thanks for that!
 * 
 * Audio processing in float32_t with the NEW ARM CMSIS lib, --> https://forum.pjrc.com/threads/40590-Teensy-Convolution-SDR-(Software-Defined-Radio)?p=129081&viewfull=1#post129081
 * 
 * MIT license
 * 
 */

#include <Time.h>
#include <TimeLib.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Metro.h>
#include "font_Arial.h"
#include <ILI9341_t3.h>
#include <arm_math.h>
#include <arm_const_structs.h>
#include <si5351.h>
#include <Encoder.h>
#include <EEPROM.h>
#include <Bounce.h>
#include <play_sd_mp3.h> //mp3 decoder
#include <play_sd_aac.h> // AAC decoder

time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}

// Settings for the hardware QSD
// Joris PCB uses a 27MHz crystal and CLOCK 2 output
// Elektor SDR PCB uses a 25MHz crystal and the CLOCK 1 output
//#define Si_5351_clock  SI5351_CLK1 
//#define Si_5351_crystal 25000000  
#define Si_5351_clock  SI5351_CLK2
#define Si_5351_crystal 27000000

unsigned long long calibration_factor = 1000000000 ;// 10002285;
long calibration_constant = -8000; // this is for the Joris PCB !
//long calibration_constant = 108000; // this is for the Elektor PCB !
unsigned long long hilfsf;

// Optical Encoder connections
Encoder tune(16, 17);
Encoder filter(1, 2);
Encoder encoder3(26, 28);

Si5351 si5351;
#define MASTER_CLK_MULT  4  // QSD frontend requires 4x clock

#define BACKLIGHT_PIN   0
#define TFT_DC          20
#define TFT_CS          21
#define TFT_RST         32  // 255 = unused. connect to 3.3V
#define TFT_MOSI        7
#define TFT_SCLK        14
#define TFT_MISO        12

ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);

// push-buttons
#define   BUTTON_1_PIN      33
#define   BUTTON_2_PIN      34
#define   BUTTON_3_PIN      35
#define   BUTTON_4_PIN      36
#define   BUTTON_5_PIN      38 // this is the pushbutton pin of the tune encoder
#define   BUTTON_6_PIN       3 // this is the pushbutton pin of the filter encoder
#define   BUTTON_7_PIN      37 // this is the menu button pin
#define   BUTTON_8_PIN      27 // this is the pushbutton pin of encoder 3


Bounce button1 = Bounce(BUTTON_1_PIN, 50); //
Bounce button2 = Bounce(BUTTON_2_PIN, 50); //
Bounce button3 = Bounce(BUTTON_3_PIN, 50);//
Bounce button4 = Bounce(BUTTON_4_PIN, 50);//
Bounce button5 = Bounce(BUTTON_5_PIN, 50); //
Bounce button6 = Bounce(BUTTON_6_PIN, 50); // 
Bounce button7 = Bounce(BUTTON_7_PIN, 50); // 
Bounce button8 = Bounce(BUTTON_8_PIN, 50); // 

Metro five_sec=Metro(2000); // Set up a Metro
Metro ms_500 = Metro(500); // Set up a Metro
Metro encoder_check = Metro(100); // Set up a Metro
//Metro dbm_check = Metro(25); 
uint8_t wait_flag = 0;

// this audio comes from the codec by I2S2
AudioInputI2S            i2s_in; 
           
AudioRecordQueue         Q_in_L;    
AudioRecordQueue         Q_in_R;    

AudioPlaySdMp3           playMp3; 
AudioPlaySdAac           playAac; 
AudioMixer4              mixleft;
AudioMixer4              mixright;
AudioPlayQueue           Q_out_L; 
AudioPlayQueue           Q_out_R; 
AudioOutputI2S           i2s_out;           

AudioConnection          patchCord1(i2s_in, 0, Q_in_L, 0);
AudioConnection          patchCord2(i2s_in, 1, Q_in_R, 0);

//AudioConnection          patchCord3(Q_out_L, 0, i2s_out, 1);
//AudioConnection          patchCord4(Q_out_R, 0, i2s_out, 0);
AudioConnection          patchCord3(Q_out_L, 0, mixleft, 0);
AudioConnection          patchCord4(Q_out_R, 0, mixright, 0);
AudioConnection          patchCord5(playMp3, 0, mixleft, 1);
AudioConnection          patchCord6(playMp3, 1, mixright, 1);
AudioConnection          patchCord7(playAac, 0, mixleft, 2);
AudioConnection          patchCord8(playAac, 1, mixright, 2);
AudioConnection          patchCord9(mixleft, 0,  i2s_out, 1);
AudioConnection          patchCord10(mixright, 0, i2s_out, 0);

AudioControlSGTL5000     sgtl5000_1;     

int idx_t = 0;
int idx = 0;
int64_t sum;
float32_t mean;
int n_L;
int n_R;
long int n_clear;

//float32_t FFT_magn[4096];
float32_t FFT_spec[256];
float32_t FFT_spec_old[256];
int16_t pixelnew[256];
int16_t pixelold[256];
float32_t LPF_spectrum = 0.35;
float32_t spectrum_display_scale = 30.0;
#define SPECTRUM_ZOOM_MIN         0
#define SPECTRUM_ZOOM_1           0
#define SPECTRUM_ZOOM_2           1
#define SPECTRUM_ZOOM_4           2
#define SPECTRUM_ZOOM_8           3
#define SPECTRUM_ZOOM_16          4
#define SPECTRUM_ZOOM_32          5
#define SPECTRUM_ZOOM_64          6
#define SPECTRUM_ZOOM_MAX         4
     
int8_t spectrum_zoom = SPECTRUM_ZOOM_2;

// Text and position for the FFT spectrum display scale

#define SAMPLE_RATE_MIN               6
#define SAMPLE_RATE_8K                0
#define SAMPLE_RATE_11K               1
#define SAMPLE_RATE_16K               2  
#define SAMPLE_RATE_22K               3
#define SAMPLE_RATE_32K               4
#define SAMPLE_RATE_44K               5
#define SAMPLE_RATE_48K               6
#define SAMPLE_RATE_88K               7
#define SAMPLE_RATE_96K               8
#define SAMPLE_RATE_100K              9
#define SAMPLE_RATE_176K              10
#define SAMPLE_RATE_192K              11
#define SAMPLE_RATE_MAX               10

//uint8_t sr =                     SAMPLE_RATE_96K;
uint8_t SAMPLE_RATE =            SAMPLE_RATE_96K; 

typedef struct SR_Descriptor
{
    const uint8_t SR_n;
    const uint32_t rate;
    const char* const text;
    const char* const f1;
    const char* const f2;
    const char* const f3;
    const char* const f4;
    const float32_t x_factor;
    const uint8_t x_offset;
} SR_Desc;
const SR_Descriptor SR [12] =
{
    //   SR_n , rate, text, f1, f2, f3, f4, x_factor = pixels per f1 kHz in spectrum display
    {  SAMPLE_RATE_8K, 8000,  "  8k", " 1", " 2", " 3", " 4", 64.0, 11}, // not OK
    {  SAMPLE_RATE_11K, 11025, " 11k", " 1", " 2", " 3", " 4", 43.1, 17}, // not OK
    {  SAMPLE_RATE_16K, 16000, " 16k",  " 4", " 4", " 8", "12", 64.0, 1}, // OK
    {  SAMPLE_RATE_22K, 22050, " 22k",  " 5", " 5", "10", "15", 58.05, 6}, // OK 
    {  SAMPLE_RATE_32K, 32000,  " 32k", " 5", " 5", "10", "15", 40.0, 24}, // OK, one more indicator? 
    {  SAMPLE_RATE_44K, 44100,  " 44k", "10", "10", "20", "30", 58.05, 6}, // OK
    {  SAMPLE_RATE_48K, 48000,  " 48k", "10", "10", "20", "30", 53.33, 11}, // OK
    {  SAMPLE_RATE_88K, 88200,  " 88k", "20", "20", "40", "60", 58.05, 6}, // OK
    {  SAMPLE_RATE_96K, 96000,  " 96k", "20", "20", "40", "60", 53.33, 12}, // OK 
    {  SAMPLE_RATE_100K, 100000,  "100k", "20", "20", "40", "60", 53.33, 12}, // NOT OK 
    {  SAMPLE_RATE_176K, 176400,  "176k", "40", "40", "80", "120", 58.05, 6}, // OK
    {  SAMPLE_RATE_192K, 192000,  "192k", "40", "40", "80", "120", 53.33, 12} // not OK
};    
int32_t IF_FREQ = SR[SAMPLE_RATE].rate / 4;     // IF (intermediate) frequency
#define F_MAX 3600000000
#define F_MIN 1200000

ulong samp_ptr = 0;

const int myInput = AUDIO_INPUT_LINEIN;

float32_t IQ_amplitude_correction_factor = 1.0038;
float32_t IQ_phase_correction_factor =  0.0058;
// experiment: automatic IQ imbalance correction
// Chang, R. C.-H. & C.-H. Lin (2010): Implementation of carrier frequency offset and 
//     IQ imbalance copensation in OFDM systems. - Int J Electr Eng 17(4): 251-259.
float32_t K_est = 1.0;
float32_t K_est_old = 0.0;
float32_t K_est_mult = 1.0 / K_est;
float32_t P_est = 0.0;
float32_t P_est_old = 0.0;
float32_t P_est_mult = 1.0 / (sqrtf(1.0 - P_est * P_est));
float32_t Q_sum = 0.0;
float32_t I_sum = 0.0;
float32_t IQ_sum = 0.0;
uint8_t   IQ_state = 1;
uint32_t n_para = 256;
uint32_t IQ_counter = 0;
float32_t K_dirty = 0.868;
float32_t P_dirty = 1.0;
int8_t auto_IQ_correction = 0;
#define CHANG         0
#define MOSELEY       0
float32_t teta1 = 0.0;
float32_t teta2 = 0.0;
float32_t teta3 = 0.0;
float32_t teta1_old = 0.0;
float32_t teta2_old = 0.0;
float32_t teta3_old = 0.0;
float32_t M_c1 = 0.0;
float32_t M_c2 = 0.0;

#define BUFFER_SIZE 128

// complex FFT with the new library CMSIS V4.5
const static arm_cfft_instance_f32 *S;

// create coefficients on the fly for custom filters
// and let the algorithm define which FFT size you need
// input variables by the user
// Fpass, Fstop, Astop (stopband attenuation)  
// fixed: sample rate, scale = 1.0 ???
// tested & working samplerates (processor load):
// 96k (46%), 88.2k, 48k, 44.1k (18%), 32k (13.6%),

float32_t LP_Fpass = 3500;
uint32_t LP_F_help = 3500;
float32_t LP_Fstop = 3600;
float32_t LP_Astop = 90;
float32_t LP_Fpass_old = 0.0;

//int RF_gain = 0;
int audio_volume = 50;
int8_t bass_gain_help = 30;
int8_t treble_gain_help = -10;
float32_t bass = bass_gain_help / 100.0;
float32_t treble = treble_gain_help / 100.0;

//float32_t FIR_Coef[2049];
float32_t FIR_Coef[513];
#define MAX_NUMCOEF 513
#define TPI           6.28318530717959f
#define PIH           1.57079632679490f
#define FOURPI        2.0 * TPI
#define SIXPI         3.0 * TPI

uint32_t m_NumTaps = 513;
//const uint32_t FFT_L = 4096;
const float32_t DF = 8.0; // decimation factor
const uint32_t FFT_L = 1024;
uint32_t FFT_length = FFT_L;
uint32_t N_BLOCKS = 32;
const uint32_t N_B = 32; //FFT_L / 2 / BUFFER_SIZE;
// decimation by 8 --> 32 / 8 = 4
const uint32_t N_DEC_B = 4; 
float32_t float_buffer_L [BUFFER_SIZE * N_B];
float32_t float_buffer_R [BUFFER_SIZE * N_B];
//float32_t float_buffer_L_2 [BUFFER_SIZE * N_DEC_B * 2];
//float32_t float_buffer_R_2 [BUFFER_SIZE * N_DEC_B * 2];

//float32_t float_buffer_L [BUFFER_SIZE * N_DEC_B * 2]; // * 2: as this has to also hold the samples after the first decimation-by-4 !
//float32_t float_buffer_R [BUFFER_SIZE * N_DEC_B * 2];
float32_t FFT_buffer [FFT_L * 2] __attribute__ ((aligned (4)));
float32_t last_sample_buffer_L [BUFFER_SIZE * N_DEC_B];
float32_t last_sample_buffer_R [BUFFER_SIZE * N_DEC_B];
uint8_t flagg = 0;
// complex iFFT with the new library CMSIS V4.5
const static arm_cfft_instance_f32 *iS;
float32_t iFFT_buffer [FFT_L * 2] __attribute__ ((aligned (4)));

// FFT instance for direct calculation of the filter mask
// from the impulse response of the FIR - the coefficients
const static arm_cfft_instance_f32 *maskS;
float32_t FIR_filter_mask [FFT_L * 2] __attribute__ ((aligned (4))); 

const static arm_cfft_instance_f32 *spec_FFT;
float32_t buffer_spec_FFT [512] __attribute__ ((aligned (4))); 

//////////////////////////////////////
// constants for display
//////////////////////////////////////
int spectrum_y = 120; // upper edge
int spectrum_x = 10;
int spectrum_height = 90;
int spectrum_pos_centre_f = 64;
#define pos_x_smeter 11 //5
#define pos_y_smeter (spectrum_y - 12) //94
#define s_w 10
uint8_t freq_flag = 0;
uint8_t digits_old [] = {9,9,9,9,9,9,9,9,9,9};
#define pos 50 // position of spectrum display, has to be < 124
#define pos_version 119 // position of version number printing
#define pos_x_tunestep 100
#define pos_y_tunestep 119 // = pos_y_menu 91
#define pos_x_frequency 5 //21 //100
#define pos_y_frequency 48 //52 //61  //119
#define notchpos 2
#define notchL 15
#define notchColour YELLOW
int pos_centre_f = 98; // 
//#define spectrum_span 44.118
#define font_width 16
uint8_t sch = 0;
float32_t dbm = -145.0;
float32_t dbmhz = -145.0;
float32_t m_AttackAvedbm = 0.0;
float32_t m_DecayAvedbm = 0.0;
float32_t m_AverageMagdbm = 0.0;
float32_t m_AttackAvedbmhz = 0.0;
float32_t m_DecayAvedbmhz = 0.0;
float32_t m_AverageMagdbmhz = 0.0;
// ALPHA = 1 - e^(-T/Tau), T = 0.02s (because dbm routine is called every 20ms!)
// Tau     ALPHA
//  10ms   0.8647
//  30ms   0.4866
//  50ms   0.3297
// 100ms   0.1812
// 500ms   0.0391
float32_t m_AttackAlpha = 0.2;
float32_t m_DecayAlpha  = 0.05;
int16_t pos_x_dbm = pos_x_smeter + 170;
int16_t pos_y_dbm = pos_y_smeter - 7;
#define DISPLAY_S_METER_DBM       0
#define DISPLAY_S_METER_DBMHZ     1
uint8_t display_dbm = DISPLAY_S_METER_DBM;
uint8_t dbm_state = 0;

// out of the nine implemented AM detectors, only
// two proved to be of acceptable quality:
// AM2 and AM_ME2
// however, SAM outperforms all demodulators ;-)
#define       DEMOD_MIN           0
#define       DEMOD_USB           0
#define       DEMOD_LSB           1
//#define       DEMOD_AM1           2
#define       DEMOD_AM2           2
//#define       DEMOD_AM3           4
//#define       DEMOD_AM_AE1        5
//#define       DEMOD_AM_AE2        6
//#define       DEMOD_AM_AE3        7
//#define       DEMOD_AM_ME1        8
#define       DEMOD_AM_ME2        3
//#define       DEMOD_AM_ME3       10
#define       DEMOD_SAM          4 // synchronous AM demodulation
#define       DEMOD_SAM_USB      5 // synchronous AM demodulation
#define       DEMOD_SAM_LSB      6 // synchronous AM demodulation
#define       DEMOD_SAM_STEREO   7 // SAM, with pseude-stereo effect
#define       DEMOD_DCF77        8 // set the clock with the time signal station DCF77
#define       DEMOD_AUTOTUNE     9 // AM demodulation with autotune function
#define       DEMOD_STEREO_DSB   17 // double sideband: SSB without eliminating the opposite sideband
#define       DEMOD_DSB          18 // double sideband: SSB without eliminating the opposite sideband
#define       DEMOD_STEREO_LSB   19 
#define       DEMOD_AM_USB       20 // AM demodulation with lower sideband suppressed
#define       DEMOD_AM_LSB       21 // AM demodulation with upper sideband suppressed
#define       DEMOD_STEREO_USB   22 
#define       DEMOD_MAX          7
#define BAND_LW     0   
#define BAND_MW     1
#define BAND_120M   2
#define BAND_90M    3
#define BAND_75M    4    
#define BAND_60M    5
#define BAND_49M    6
#define BAND_41M    7
#define BAND_31M    8
#define BAND_25M    9
#define BAND_22M   10
#define BAND_19M   11
#define BAND_16M   12
#define BAND_15M   13
#define BAND_13M   14
#define BAND_11M   15

#define FIRST_BAND BAND_LW
#define LAST_BAND  BAND_15M
#define NUM_BANDS  16
#define STARTUP_BAND BAND_MW    // 

uint8_t autotune_wait = 10;

struct band {
  unsigned long long freq; // frequency in Hz
  String name; // name of band
  int mode; 
  int bandwidthU;
  int bandwidthL;
  int RFgain; 
};
// f, band, mode, bandwidth, RFgain
struct band bands[NUM_BANDS] = {
//  7750000 ,"VLF", DEMOD_AM, 3600,3600,0,  
  22500000,"LW", DEMOD_SAM, 3600,3600,0,
  63900000,"MW",  DEMOD_SAM, 3600,3600,0,
  248500000,"120M",  DEMOD_SAM, 3600,3600,0,
  350000000,"90M",  DEMOD_LSB, 3600,3600,6,
  390500000,"75M",  DEMOD_SAM, 3600,3600,4,
  502500000,"60M",  DEMOD_SAM, 3600,3600,7,
  593200000,"49M",  DEMOD_SAM, 3600,3600,0,
  712000000,"41M",  DEMOD_SAM, 3600,3600,0,
  942000000,"31M",  DEMOD_SAM, 3600,3600,0,
  1173500000,"25M", DEMOD_SAM, 3600,3600,2,
  1357000000,"22M", DEMOD_SAM, 3600,3600,2,
  1514000000,"19M", DEMOD_SAM, 3600,3600,4,
  1748000000,"16M", DEMOD_SAM, 3600,3600,5,
  1890000000,"15M", DEMOD_SAM, 3600,3600,5,
  2145000000,"13M", DEMOD_SAM, 3600,3600,6,
  2567000000,"11M", DEMOD_SAM, 3600,3600,6
};
int band = STARTUP_BAND;

#define TUNE_STEP_MIN   0
#define TUNE_STEP1   0    // shortwave
#define TUNE_STEP2   1   // fine tuning
#define TUNE_STEP3   2    //
#define TUNE_STEP4   3    //
#define TUNE_STEP_MAX 3 
#define first_tunehelp 1
#define last_tunehelp 3
uint8_t tune_stepper = 0;
uint32_t tunestep = 5000; //TUNE_STEP1;
String tune_text = "Fast Tune";
uint8_t autotune_flag = 0;
 
int8_t first_block = 1;
uint32_t i = 0;
int32_t FFT_shift = 2048; // which means 1024 bins!

// used for AM demodulation
float32_t audiotmp = 0.0f;
float32_t w = 0.0f;
float32_t wold = 0.0f;
float32_t last_dc_level = 0.0f;
uint8_t audio_flag = 1;

typedef struct DEMOD_Descriptor
{   const uint8_t DEMOD_n;
    const char* const text;
} DEMOD_Desc;
const DEMOD_Descriptor DEMOD [14] =
{
    //   DEMOD_n, name
    {  DEMOD_USB, " USB  "}, 
    {  DEMOD_LSB, " LSB  "}, 
//    {  DEMOD_AM1,  " AM 1 "}, 
    {  DEMOD_AM2,  " AM 2 "}, 
//    {  DEMOD_AM3,  " AM 3 "}, 
//    {  DEMOD_AM_AE1,  "AM-AE1"}, 
//    {  DEMOD_AM_AE2,  "AM-AE2"}, 
//    {  DEMOD_AM_AE3,  "AM-AE3"}, 
//    {  DEMOD_AM_ME1,  "AM-ME1"}, 
    {  DEMOD_AM_ME2,  "AM-ME2"}, 
//    {  DEMOD_AM_ME3,  "AM-ME3"}, 
    {  DEMOD_SAM, " SAM  "},
    {  DEMOD_SAM_USB, "SAM-U "},
    {  DEMOD_SAM_LSB, "SAM-L "},
    {  DEMOD_SAM_STEREO, "SAM-St"},
    {  DEMOD_STEREO_LSB, "StLSB "}, 
    {  DEMOD_STEREO_USB, "StUSB "}, 
    {  DEMOD_DCF77, "DCF 77"}, 
    {  DEMOD_AUTOTUNE, "AUTO_T"}, 
    {  DEMOD_DSB, " DSB  "}, 
    {  DEMOD_STEREO_DSB, "StDSB "},
};

// Menus !
#define MENU_FILTER_BANDWIDTH             0
#define MENU_SPECTRUM_ZOOM                1
#define MENU_SAMPLE_RATE                  2
#define MENU_PLAYER                       3
#define MENU_IQ_AMPLITUDE                 4
#define MENU_IQ_PHASE                     5 
#define MENU_CALIBRATION_FACTOR           6
#define MENU_CALIBRATION_CONSTANT         7
#define MENU_LPF_SPECTRUM                 8
#define MENU_SAVE_EEPROM                  9
#define MENU_LOAD_EEPROM                  10
#define MENU_TIME_SET                     11
#define MENU_DATE_SET                     12  

#define first_menu                        0
#define last_menu                         12
#define start_menu                        0
int8_t Menu_pointer =                    start_menu;

#define MENU_RF_GAIN                      13
#define MENU_VOLUME                       14
#define MENU_TREBLE                       15
#define MENU_BASS                         16
#define MENU_SAM_ZETA                     17
#define MENU_SAM_OMEGA                    18
#define MENU_SAM_CATCH_BW                 19
#define MENU_AGC_MODE                     20
#define first_menu2                       13
#define last_menu2                        20  
int8_t Menu2 =                           MENU_RF_GAIN;
uint8_t which_menu = 1;

typedef struct Menu_Descriptor
{
    const uint8_t no; // Menu ID
    const char* const text1; // upper text
    const char* const text2; // lower text
    const uint8_t menu2; // 0 = belongs to Menu, 1 = belongs to Menu2
} Menu_D;

const Menu_D Menus [21] {
{ MENU_FILTER_BANDWIDTH, "  Filter", "   BW  ", 0 },
{ MENU_SPECTRUM_ZOOM, " Spectr", " Zoom ", 0 },
{ MENU_SAMPLE_RATE, "Sample", " Rate ", 0 },
{ MENU_PLAYER, "  MP3  ", " Player", 0 },
{ MENU_IQ_AMPLITUDE, "  IQ  ", " gain ", 0 },
{ MENU_IQ_PHASE, "   IQ  ", "  phase ", 0 },
{ MENU_CALIBRATION_FACTOR, "F-calib", "factor", 0 },
{ MENU_CALIBRATION_CONSTANT, "F-calib", "const", 0 },
{ MENU_LPF_SPECTRUM, "Spectr", " LPF  ", 0 },
{ MENU_SAVE_EEPROM, " Save ", "Eeprom", 0 },
{ MENU_LOAD_EEPROM, " Load ", "Eeprom", 0 },
{ MENU_TIME_SET, " Time ", " Set  ", 0},
{ MENU_DATE_SET, " Date ", " Set  ", 0},  
{ MENU_RF_GAIN, "   RF  ", " gain ", 1},
{ MENU_VOLUME, "Volume", "      ", 1},
{ MENU_TREBLE, "Treble", "  gain ", 1},
{ MENU_BASS, "  Bass ", "  gain ", 1},
{ MENU_SAM_ZETA, "  SAM  ", "  zeta ", 1},
{ MENU_SAM_OMEGA, "  SAM  ", " omega ", 1},
{ MENU_SAM_CATCH_BW, "  SAM  ", "catchB", 1},
{ MENU_AGC_MODE, "  AGC  ", "  mode  ", 1}
};


// SD card & MP3 playing
int track;
int tracknum;
int trackext[255]; // 0= nothing, 1= mp3, 2= aac, 3= wav.
String tracklist[255];
File root;
char playthis[15];
boolean trackchange;
boolean paused;
int eeprom_adress = 1900;

// AGC
#define MAX_SAMPLE_RATE     (12000.0)
#define MAX_N_TAU           (8)
#define MAX_TAU_ATTACK      (0.01)
#define RB_SIZE       (int) (MAX_SAMPLE_RATE * MAX_N_TAU * MAX_TAU_ATTACK + 1)

int8_t AGC_mode = 1;
int pmode = 1; // if 0, calculate magnitude by max(|I|, |Q|), if 1, calculate sqrtf(I*I+Q*Q)
float32_t out_sample[2];
float32_t abs_out_sample;
float32_t tau_attack;
float32_t tau_decay;
int n_tau;
float32_t max_gain;
float32_t var_gain;
float32_t fixed_gain = 1.0;
float32_t max_input;
float32_t out_targ;
float32_t tau_fast_backaverage;
float32_t tau_fast_decay;
float32_t pop_ratio;
uint8_t hang_enable;
float32_t tau_hang_backmult;
float32_t hangtime;
float32_t hang_thresh;
float32_t tau_hang_decay;
float32_t ring[RB_SIZE * 2];
float32_t abs_ring[RB_SIZE];
//assign constants
int ring_buffsize = RB_SIZE;
//do one-time initialization
int out_index = -1;
float32_t ring_max = 0.0;
float32_t volts = 0.0;
float32_t save_volts = 0.0;
float32_t fast_backaverage = 0.0;
float32_t hang_backaverage = 0.0;
int hang_counter = 0;
uint8_t decay_type = 0;
uint8_t state = 0;
int attack_buffsize;
uint32_t in_index;
float32_t attack_mult;
float32_t decay_mult;
float32_t fast_decay_mult;
float32_t fast_backmult;
float32_t onemfast_backmult;
float32_t out_target;
float32_t min_volts;
float32_t inv_out_target;
float32_t tmp;
float32_t slope_constant;
float32_t inv_max_input;
float32_t hang_level;
float32_t hang_backmult;
float32_t onemhang_backmult;
float32_t hang_decay_mult;

// new synchronous AM PLL & PHASE detector
// wdsp Warren Pratt, 2016
float32_t Sin = 0.0;
float32_t Cos = 0.0;
float32_t pll_fmax = +4000.0;
int zeta_help = 65;
float32_t zeta = (float32_t)zeta_help / 100.0; // PLL step response: smaller, slower response 1.0 - 0.1
float32_t omegaN = 200.0; // PLL bandwidth 50.0 - 1000.0
  
  //pll
float32_t omega_min = TPI * - pll_fmax * DF / SR[SAMPLE_RATE].rate;
float32_t omega_max = TPI * pll_fmax * DF / SR[SAMPLE_RATE].rate;
float32_t g1 = 1.0 - exp(-2.0 * omegaN * zeta * DF / SR[SAMPLE_RATE].rate);
float32_t g2 = - g1 + 2.0 * (1 - exp(- omegaN * zeta * DF / SR[SAMPLE_RATE].rate) * cosf(omegaN * DF / SR[SAMPLE_RATE].rate * sqrtf(1.0 - zeta * zeta)));
float32_t phzerror = 0.0;
float32_t det = 0.0;
float32_t fil_out = 0.0;
float32_t del_out = 0.0;
float32_t omega2 = 0.0;

  //fade leveler
float32_t tauR = 0.02; // original 0.02;
float32_t tauI = 1.4; // original 1.4;  
float32_t dc = 0.0;
float32_t dc_insert = 0.0;
float32_t dcu = 0.0;
float32_t dc_insertu = 0.0;
float32_t mtauR = exp(- DF / (SR[SAMPLE_RATE].rate * tauR)); 
float32_t onem_mtauR = 1.0 - mtauR;
float32_t mtauI = exp(- DF / (SR[SAMPLE_RATE].rate * tauI)); 
float32_t onem_mtauI = 1.0 - mtauI;  
uint8_t fade_leveler = 1;
uint8_t WDSP_SAM = 1;
#define SAM_PLL_HILBERT_STAGES 7
#define OUT_IDX   (3 * SAM_PLL_HILBERT_STAGES)
float32_t c0[SAM_PLL_HILBERT_STAGES];
float32_t c1[SAM_PLL_HILBERT_STAGES];
float32_t ai, bi, aq, bq;
float32_t ai_ps, bi_ps, aq_ps, bq_ps;
float32_t a[3 * SAM_PLL_HILBERT_STAGES + 3];     // Filter a variables
float32_t b[3 * SAM_PLL_HILBERT_STAGES + 3];     // Filter b variables
float32_t c[3 * SAM_PLL_HILBERT_STAGES + 3];     // Filter c variables
float32_t d[3 * SAM_PLL_HILBERT_STAGES + 3];     // Filter d variables
float32_t dsI;             // delayed sample, I path
float32_t dsQ;             // delayed sample, Q path
float32_t corr[2];
float32_t audio;
float32_t audiou;
int j,k;
float32_t SAM_carrier = 0.0;
float32_t SAM_lowpass = 0.0;
float32_t SAM_carrier_freq_offset = 0.0;
uint16_t  SAM_display_count = 0;

//***********************************************************************
bool timeflag = 0;
const int8_t pos_x_date = 14;
const int8_t pos_y_date = 68;
const int16_t pos_x_time = 225; // 14;
const int16_t pos_y_time = pos_y_frequency; //114;
int helpmin; // definitions for time and date adjust - Menu
int helphour;
int helpday;
int helpmonth;
int helpyear;
int helpsec;
uint8_t hour10_old;
uint8_t hour1_old;
uint8_t minute10_old;
uint8_t minute1_old;
uint8_t second10_old;
uint8_t second1_old;
uint8_t precision_flag = 0;
int8_t mesz = -1;
int8_t mesz_old = 0;

const float displayscale = 0.25;

//const char* const Days[7] = { "Samstag", "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag"};
const char* const Days[7] = { "Saturday", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday"};

/****************************************************************************************
 *  init decimation and interpolation
 ****************************************************************************************/

// decimation with FIR lowpass
// pState points to a state array of size numTaps + blockSize - 1
arm_fir_decimate_instance_f32 FIR_dec1_I;
float32_t FIR_dec1_I_state [50 + BUFFER_SIZE * N_B - 1];
arm_fir_decimate_instance_f32 FIR_dec1_Q;
float32_t FIR_dec1_Q_state [50 + BUFFER_SIZE * N_B - 1];
float32_t FIR_dec1_coeffs[50];

arm_fir_decimate_instance_f32 FIR_dec2_I;
float32_t FIR_dec2_I_state [88 + BUFFER_SIZE * N_B / 4 - 1];
arm_fir_decimate_instance_f32 FIR_dec2_Q;
float32_t FIR_dec2_Q_state [88 + BUFFER_SIZE * N_B / 4 - 1];
float32_t FIR_dec2_coeffs[88];

// interpolation with FIR lowpass
// pState is of length (numTaps/L)+blockSize-1 words where blockSize is the number of input samples processed by each call
arm_fir_interpolate_instance_f32 FIR_int1_I;
float32_t FIR_int1_I_state [(16 / 2) + BUFFER_SIZE * N_B / 8 - 1];
arm_fir_interpolate_instance_f32 FIR_int1_Q;
float32_t FIR_int1_Q_state [(16 / 2) + BUFFER_SIZE * N_B / 8 - 1];
float32_t FIR_int1_coeffs[16];

arm_fir_interpolate_instance_f32 FIR_int2_I;
float32_t FIR_int2_I_state [(16 / 4) + BUFFER_SIZE * N_B / 4 - 1];
arm_fir_interpolate_instance_f32 FIR_int2_Q;
float32_t FIR_int2_Q_state [(16 / 4) + BUFFER_SIZE * N_B / 4 - 1];
float32_t FIR_int2_coeffs[16];

// decimation with FIR lowpass for Zoom FFT
arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_I;
arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_Q;
float32_t Fir_Zoom_FFT_Decimate_I_state [4 + BUFFER_SIZE * N_B - 1];
float32_t Fir_Zoom_FFT_Decimate_Q_state [4 + BUFFER_SIZE * N_B - 1];

float32_t Fir_Zoom_FFT_Decimate_coeffs[4];

/****************************************************************************************
 *  init IIR filters
 ****************************************************************************************/
float32_t coefficient_set[5] = {0, 0, 0, 0, 0};
// 2-pole biquad IIR - definitions and Initialisation
const uint32_t N_stages_biquad_lowpass1 = 1;
float32_t biquad_lowpass1_state [N_stages_biquad_lowpass1 * 4];
float32_t biquad_lowpass1_coeffs[5 * N_stages_biquad_lowpass1] = {0,0,0,0,0}; 
arm_biquad_casd_df1_inst_f32 biquad_lowpass1;

// 4-stage IIRs for Zoom FFT, one each for I & Q
const uint32_t IIR_biquad_Zoom_FFT_N_stages = 4;
float32_t IIR_biquad_Zoom_FFT_I_state [IIR_biquad_Zoom_FFT_N_stages * 4];
float32_t IIR_biquad_Zoom_FFT_Q_state [IIR_biquad_Zoom_FFT_N_stages * 4];
arm_biquad_casd_df1_inst_f32 IIR_biquad_Zoom_FFT_I;
arm_biquad_casd_df1_inst_f32 IIR_biquad_Zoom_FFT_Q;
int zoom_sample_ptr = 0;
uint8_t zoom_display = 0;

static float32_t* mag_coeffs[6] =
{
// for Index 0 [1xZoom == no zoom] the mag_coeffs will consist of  a NULL  ptr, since the filter is not going to be used in this  mode!
(float32_t*)NULL,

(float32_t*)(const float32_t[]){
      // 2x magnify - index 1
      // 12kHz, sample rate 48k, 60dB stopband, elliptic
      // a1 and a2 negated! order: b0, b1, b2, a1, a2
      // Iowa Hills IIR Filter Designer, DD4WH Aug 16th 2016
0.228454526413293696,
0.077639329099949764,
0.228454526413293696,
0.635534925142242080,
-0.170083307068779194,

0.436788292542003964,
0.232307972937606161,
0.436788292542003964,
0.365885230717786780,
-0.471769788739400842,

0.535974654742658707,
0.557035600464780845,
0.535974654742658707,
0.125740787233286133,
-0.754725697183384336,

0.501116342273565607,
0.914877831284765408,
0.501116342273565607,
0.013862536615004284,
-0.930973052446900984  },

(float32_t*)(const float32_t[]){
      // 4x magnify - index 2
      // 6kHz, sample rate 48k, 60dB stopband, elliptic
      // a1 and a2 negated! order: b0, b1, b2, a1, a2
      // Iowa Hills IIR Filter Designer, DD4WH Aug 16th 2016
0.182208761527446556,
-0.222492493114674145,
0.182208761527446556,
1.326111070880959810,
-0.468036100821178802,

0.337123762652097259,
-0.366352718812586853,
0.337123762652097259,
1.337053579516321200,
-0.644948386007929031,

0.336163175380826074,
-0.199246162162897811,
0.336163175380826074,
1.354952684569386670,
-0.828032873168141115,

0.178588201750411041,
0.207271695028067304,
0.178588201750411041,
1.386486967455699220,
-0.950935065984588657  },

(float32_t*)(const float32_t[]){
        // 8x magnify - index 3
        // 3kHz, sample rate 48k, 60dB stopband, elliptic
        // a1 and a2 negated! order: b0, b1, b2, a1, a2
        // Iowa Hills IIR Filter Designer, DD4WH Aug 16th 2016
0.185643392652478922,
-0.332064345389014803,
0.185643392652478922,
1.654637402827731090,
-0.693859842743674182,

0.327519300813245984,
-0.571358085216950418,
0.327519300813245984,
1.715375037176782860,
-0.799055553586324407,

0.283656142708241688,
-0.441088976843048652,
0.283656142708241688,
1.778230635987093860,
-0.904453944560528522,

0.079685368654848945,
-0.011231810140649204,
0.079685368654848945,
1.825046003243238070,
-0.973184930412286708  },

(float32_t*)(const float32_t[]){
        // 16x magnify - index 4
      // 1k5, sample rate 48k, 60dB stopband, elliptic
      // a1 and a2 negated! order: b0, b1, b2, a1, a2
      // Iowa Hills IIR Filter Designer, DD4WH Aug 16th 2016
 0.194769868656866380,
 -0.379098413160710079,
 0.194769868656866380,
 1.824436402073870810,
 -0.834877726226893380,

 0.333973874901496770,
-0.646106479315673776,
 0.333973874901496770,
 1.871892825636887640,
-0.893734096124207178,

 0.272903880596429671,
-0.513507745397738469,
 0.272903880596429671,
 1.918161772571113750,
-0.950461788366234739,

 0.053535383722369843,
-0.069683422367188122,
 0.053535383722369843,
 1.948900719896301760,
-0.986288064973853129 },

(float32_t*)(const float32_t[]){
        // 32x magnify - index 5
      // 750Hz, sample rate 48k, 60dB stopband, elliptic
        // a1 and a2 negated! order: b0, b1, b2, a1, a2
        // Iowa Hills IIR Filter Designer, DD4WH Aug 16th 2016
0.201507402588557594,
-0.400273615727755550,
0.201507402588557594,
1.910767558906650840,
-0.913508748356010480,

0.340295203367131205,
-0.674930558961690075,
0.340295203367131205,
1.939398230905991390,
-0.945058078678563840,

0.271859921641011359,
-0.535453706265515361,
0.271859921641011359,
1.966439529620203740,
-0.974705666636711099,

0.047026497485465592,
-0.084562104085501480,
0.047026497485465592,
1.983564238653704900,
-0.993055129539134551 }
};


void setup() {
  Serial.begin(115200);
  delay(100);

  // for the large queue sizes at 192ksps sample rate we need a lot of buffers
  AudioMemory(130);
  delay(100);

  // get TIME from real time clock with 3V backup battery  
  setSyncProvider(getTeensy3Time);

// initialize SD card slot
  if (!(SD.begin(BUILTIN_SDCARD))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
  //Starting to index the SD card for MP3/AAC.
  root = SD.open("/");

  // reads the last track what was playing.
  track = EEPROM.read(eeprom_adress); 


  while(true) {

    File files =  root.openNextFile();
    if (!files) {
      //If no more files, break out.
      break;
    }
    String curfile = files.name(); //put file in string
    //look for MP3 or AAC files
    int m = curfile.lastIndexOf(".MP3");
    int a = curfile.lastIndexOf(".AAC");
    int a1 = curfile.lastIndexOf(".MP4");
    int a2 = curfile.lastIndexOf(".M4A");
    //int w = curfile.lastIndexOf(".WAV");

    // if returned results is more then 0 add them to the list.
    if(m > 0 || a > 0 || a1 > 0 || a2 > 0 ){  

      tracklist[tracknum] = files.name();
      if(m > 0) trackext[tracknum] = 1;
      if(a > 0) trackext[tracknum] = 2;  
      if(a1 > 0) trackext[tracknum] = 2;
      if(a2 > 0) trackext[tracknum] = 2;
      //  if(w > 0) trackext[tracknum] = 3;
      tracknum++;  
    }
    // close 
    files.close();
  }
  //check if tracknum exist in tracklist from eeprom, like if you deleted some files or added.
  if(track > tracknum){
    //if it is too big, reset to 0
    EEPROM.write(eeprom_adress,0);
    track = 0;
  }
      Serial.print("tracknum = "); Serial.println(tracknum);

  tracklist[track].toCharArray(playthis, sizeof(tracklist[track]));

  // Enable the audio shield. select input. and enable output
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(myInput);
  sgtl5000_1.volume(0.6); // 
  sgtl5000_1.adcHighPassFilterDisable(); // does not help too much!
  sgtl5000_1.lineInLevel(0);
  sgtl5000_1.audioPostProcessorEnable(); // enables the DAP chain of the codec post audio processing before the headphone out
  sgtl5000_1.eqSelect (2); // Tone Control
//  sgtl5000_1.eqBands (0.3, - 0.1); // (float bass, float treble) in % -100 to +100
  sgtl5000_1.eqBands (bass, treble); // (float bass, float treble) in % -100 to +100
  sgtl5000_1.enhanceBassEnable();
  sgtl5000_1.dacVolumeRamp();
  mixleft.gain(0,1.0);
  mixright.gain(0,1.0);

  pinMode( BACKLIGHT_PIN, OUTPUT );
  analogWrite( BACKLIGHT_PIN, 1023 );
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  pinMode(BUTTON_4_PIN, INPUT_PULLUP);
  pinMode(BUTTON_5_PIN, INPUT_PULLUP);
  pinMode(BUTTON_6_PIN, INPUT_PULLUP);  
  pinMode(BUTTON_7_PIN, INPUT_PULLUP);  
  pinMode(BUTTON_8_PIN, INPUT_PULLUP);

  tft.begin();
  tft.setRotation( 3 );
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(10, 1);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_ORANGE);
  tft.setFont(Arial_14);
  tft.print("Teensy Convolution SDR ");
  tft.setFont(Arial_10);
  prepare_spectrum_display();

 /****************************************************************************************
 *  set filter bandwidth
 ****************************************************************************************/

  // this routine does all the magic of calculating the FIR coeffs (Bessel-Kaiser window)
    calc_FIR_coeffs (FIR_Coef, 513, (float32_t)LP_F_help, LP_Astop, 0, 0.0, (float)SR[SAMPLE_RATE].rate / DF);
    FFT_length = 1024;
    m_NumTaps = 513;
//    N_BLOCKS = FFT_length / 2 / BUFFER_SIZE;

/*  // set to zero all other coefficients in coefficient array    
  for(i = 0; i < MAX_NUMCOEF; i++)
  {
//    Serial.print (FIR_Coef[i]); Serial.print(", ");
      if (i >= m_NumTaps) FIR_Coef[i] = 0.0;
  }
*/    
 /****************************************************************************************
 *  init complex FFTs
 ****************************************************************************************/
      S = &arm_cfft_sR_f32_len1024;
      iS = &arm_cfft_sR_f32_len1024;
      maskS = &arm_cfft_sR_f32_len1024;
      spec_FFT = &arm_cfft_sR_f32_len256;

 /****************************************************************************************
 *  Calculate the FFT of the FIR filter coefficients once to produce the FIR filter mask
 ****************************************************************************************/
    init_filter_mask();
 
 /****************************************************************************************
 *  show Filter response
 ****************************************************************************************/
/*    setI2SFreq (8000);
//    SAMPLE_RATE = 8000;
    delay(200);
    for(uint32_t y=0; y < FFT_length * 2; y++)
    FFT_buffer[y] = 180 * FIR_filter_mask[y];
//    calc_spectrum_mags(16,0.2);
//    show_spectrum();
//    delay(1000);
    SAMPLE_RATE = sr;
*/
 /****************************************************************************************
 *  Set sample rate
 ****************************************************************************************/
    setI2SFreq (SR[SAMPLE_RATE].rate);
    delay(200); // essential ?

    biquad_lowpass1.numStages = N_stages_biquad_lowpass1; // set number of stages
    biquad_lowpass1.pCoeffs = biquad_lowpass1_coeffs; // set pointer to coefficients file
    for(i = 0; i < 4 * N_stages_biquad_lowpass1; i++)
    {
        biquad_lowpass1_state[i] = 0.0; // set state variables to zero   
    }
    biquad_lowpass1.pState = biquad_lowpass1_state; // set pointer to the state variables
 
  /****************************************************************************************
 *  set filter bandwidth of IIR filter 
 ****************************************************************************************/
 // also adjust IIR AM filter
    // calculate IIR coeffs
    set_IIR_coeffs ((float32_t)LP_F_help, 1.3, (float32_t)SR[SAMPLE_RATE].rate / DF, 0); // 1st stage
    for(i = 0; i < 5; i++)
    {   // fill coefficients into the right file
        biquad_lowpass1_coeffs[i] = coefficient_set[i];
    }

      set_tunestep();
      show_bandwidth (band[bands].mode, LP_F_help);

 /****************************************************************************************
 *  Initiate decimation and interpolation FIR filters
 ****************************************************************************************/

    // Decimation filter 1, M1 = 4
//    calc_FIR_coeffs (FIR_dec1_coeffs, 25, (float32_t)5100.0, 80, 0, 0.0, (float32_t)SR[SAMPLE_RATE].rate);
    calc_FIR_coeffs (FIR_dec1_coeffs, 50, (float32_t)5100.0, 80, 0, 0.0, (float32_t)SR[SAMPLE_RATE].rate);
    if(arm_fir_decimate_init_f32(&FIR_dec1_I, 50, 4, FIR_dec1_coeffs, FIR_dec1_I_state, BUFFER_SIZE * N_BLOCKS)) {
      Serial.println("Init of decimation failed");
      while(1);
    }
    if(arm_fir_decimate_init_f32(&FIR_dec1_Q, 50,  4, FIR_dec1_coeffs, FIR_dec1_Q_state, BUFFER_SIZE * N_BLOCKS)) {
      Serial.println("Init of decimation failed");
      while(1);
    }
    
    // Decimation filter 2, M2 = 2
    calc_FIR_coeffs (FIR_dec2_coeffs, 88, (float32_t)5100.0, 80, 0, 0.0, SR[SAMPLE_RATE].rate / 4.0);
    if(arm_fir_decimate_init_f32(&FIR_dec2_I, 88, 2, FIR_dec2_coeffs, FIR_dec2_I_state, BUFFER_SIZE * N_BLOCKS / 4)) {
      Serial.println("Init of decimation failed");
      while(1);
    }
    if(arm_fir_decimate_init_f32(&FIR_dec2_Q, 88, 2, FIR_dec2_coeffs, FIR_dec2_Q_state, BUFFER_SIZE * N_BLOCKS / 4)) {
      Serial.println("Init of decimation failed");
      while(1);
    }

    // Interpolation filter 1, L1 = 2
    // not sure whether I should design with the final sample rate ??
    // yes, because the interpolation filter is AFTER the upsampling, so it has to be in the target sample rate!
//    calc_FIR_coeffs (FIR_int1_coeffs, 8, (float32_t)5000.0, 80, 0, 0.0, 12000);
    calc_FIR_coeffs (FIR_int1_coeffs, 16, (float32_t)5100.0, 80, 0, 0.0, SR[SAMPLE_RATE].rate / 4.0);
    if(arm_fir_interpolate_init_f32(&FIR_int1_I, 2, 16, FIR_int1_coeffs, FIR_int1_I_state, BUFFER_SIZE * N_BLOCKS / 8)) {
      Serial.println("Init of interpolation failed");
      while(1);  
    }
    if(arm_fir_interpolate_init_f32(&FIR_int1_Q, 2, 16, FIR_int1_coeffs, FIR_int1_Q_state, BUFFER_SIZE * N_BLOCKS / 8)) {
      Serial.println("Init of interpolation failed");
      while(1);  
    }    
    
    // Interpolation filter 2, L2 = 4
    // not sure whether I should design with the final sample rate ??
    // yes, because the interpolation filter is AFTER the upsampling, so it has to be in the target sample rate!
//    calc_FIR_coeffs (FIR_int2_coeffs, 4, (float32_t)5000.0, 80, 0, 0.0, 24000);
    calc_FIR_coeffs (FIR_int2_coeffs, 16, (float32_t)5100.0, 80, 0, 0.0, (float32_t)SR[SAMPLE_RATE].rate);

    if(arm_fir_interpolate_init_f32(&FIR_int2_I, 4, 16, FIR_int2_coeffs, FIR_int2_I_state, BUFFER_SIZE * N_BLOCKS / 4)) {
      Serial.println("Init of interpolation failed");
      while(1);  
    }
    if(arm_fir_interpolate_init_f32(&FIR_int2_Q, 4, 16, FIR_int2_coeffs, FIR_int2_Q_state, BUFFER_SIZE * N_BLOCKS / 4)) {
      Serial.println("Init of interpolation failed");
      while(1);  
    }    
 /****************************************************************************************
 *  Coefficients for SAM sideband selection Hilbert filters
 *  Are these Hilbert transformers built from half-band filters??
 ****************************************************************************************/
    c0[0] = -0.328201924180698;
    c0[1] = -0.744171491539427;
    c0[2] = -0.923022915444215;
    c0[3] = -0.978490468768238;
    c0[4] = -0.994128272402075;
    c0[5] = -0.998458978159551;
    c0[6] = -0.999790306259206;

    c1[0] = -0.0991227952747244;
    c1[1] = -0.565619728761389;
    c1[2] = -0.857467122550052;
    c1[3] = -0.959123933111275;
    c1[4] = -0.988739372718090;
    c1[5] = -0.996959189310611;
    c1[6] = -0.999282492800792;

 /****************************************************************************************
 *  Zoom FFT: Initiate decimation and interpolation FIR filters AND IIR filters
 ****************************************************************************************/
    // Fstop = 0.5 * sample_rate / 2^spectrum_zoom 
    float32_t Fstop_Zoom = 0.5 * (float32_t) SR[SAMPLE_RATE].rate / (1<<spectrum_zoom); 
    calc_FIR_coeffs (Fir_Zoom_FFT_Decimate_coeffs, 4, Fstop_Zoom, 60, 0, 0.0, (float32_t)SR[SAMPLE_RATE].rate);
    if(arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_I, 4, 1<<spectrum_zoom, Fir_Zoom_FFT_Decimate_coeffs, Fir_Zoom_FFT_Decimate_I_state, BUFFER_SIZE * N_BLOCKS)) {
      Serial.println("Init of decimation failed");
      while(1);
    }
    // same coefficients, but specific state variables
    if(arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_Q, 4, 1<<spectrum_zoom, Fir_Zoom_FFT_Decimate_coeffs, Fir_Zoom_FFT_Decimate_Q_state, BUFFER_SIZE * N_BLOCKS)) {
      Serial.println("Init of decimation failed");
      while(1);
    }

    IIR_biquad_Zoom_FFT_I.numStages = IIR_biquad_Zoom_FFT_N_stages; // set number of stages
    IIR_biquad_Zoom_FFT_Q.numStages = IIR_biquad_Zoom_FFT_N_stages; // set number of stages
    for(i = 0; i < 4 * IIR_biquad_Zoom_FFT_N_stages; i++)
    {
        IIR_biquad_Zoom_FFT_I_state[i] = 0.0; // set state variables to zero   
        IIR_biquad_Zoom_FFT_Q_state[i] = 0.0; // set state variables to zero   
    }
    IIR_biquad_Zoom_FFT_I.pState = IIR_biquad_Zoom_FFT_I_state; // set pointer to the state variables
    IIR_biquad_Zoom_FFT_Q.pState = IIR_biquad_Zoom_FFT_Q_state; // set pointer to the state variables

    // this sets the coefficients for the ZoomFFT decimation filter
    // according to the desired magnification mode 
    // for 0 the mag_coeffs will a NULL  ptr, since the filter is not going to be used in this  mode!
    IIR_biquad_Zoom_FFT_I.pCoeffs = mag_coeffs[spectrum_zoom];
    IIR_biquad_Zoom_FFT_Q.pCoeffs = mag_coeffs[spectrum_zoom];

    Zoom_FFT_prep();

 /****************************************************************************************
 *  Initialize AGC variables
 ****************************************************************************************/
 
            AGC_prep();

 /****************************************************************************************
 *  IQ imbalance correction
 ****************************************************************************************/
        Serial.print("1 / K_est: "); Serial.println(1.0 / K_est);
        Serial.print("1 / sqrt(1 - P_est^2): "); Serial.println(P_est_mult);
        Serial.print("Phasenfehler in Grad: "); Serial.println(- asinf(P_est)); 
            
 /****************************************************************************************
 *  start local oscillator Si5351
 ****************************************************************************************/
  si5351.init(SI5351_CRYSTAL_LOAD_10PF, Si_5351_crystal, calibration_constant);
  setfreq();
  delay(100); 
  show_frequency(bands[band].freq);  

 /****************************************************************************************
 *  load saved settings from EEPROM
 ****************************************************************************************/
//   EEPROMLOAD();
 /****************************************************************************************
 *  begin to queue the audio from the audio library
 ****************************************************************************************/
    delay(100);
    Q_in_L.begin();
    Q_in_R.begin();
//    delay(100);    
} // END SETUP

int16_t *sp_L;
int16_t *sp_R;
float32_t hh1 = 0.0;
float32_t hh2 = 0.0;
uint16_t autotune_counter = 0;

void loop() {
//  asm(" wfi"); // does this save battery power ? https://forum.pjrc.com/threads/40315-Reducing-Power-Consumption
  elapsedMicros usec = 0;
/**********************************************************************************
 *  Get samples from queue buffers
 **********************************************************************************/
    // we have to ensure that we have enough audio samples: we need
    // N_BLOCKS = 32
    // decimate by 8
    // FFT1024 point --> = 1024 / 2 / 128 = 4 
    // when these buffers are available, read them in, decimate and perform
    // the FFT - cmplx-mult - iFFT
    //
    // are there at least N_BLOCKS buffers in each channel available ?
    if (Q_in_L.available() > N_BLOCKS && Q_in_R.available() > N_BLOCKS && Menu_pointer != MENU_PLAYER)
    {   
// get audio samples from the audio  buffers and convert them to float
// read in 32 blocks á 128 samples in I and Q
    for (i = 0; i < N_BLOCKS; i++)
    {  
    sp_L = Q_in_L.readBuffer();
    sp_R = Q_in_R.readBuffer();

      // convert to float one buffer_size
      // float_buffer samples are now standardized from > -1.0 to < 1.0
     arm_q15_to_float (sp_L, &float_buffer_L[BUFFER_SIZE * i], BUFFER_SIZE); // convert int_buffer to float 32bit
     arm_q15_to_float (sp_R, &float_buffer_R[BUFFER_SIZE * i], BUFFER_SIZE); // convert int_buffer to float 32bit
     Q_in_L.freeBuffer();
     Q_in_R.freeBuffer();
//     blocks_read ++;
    }

    // this is supposed to prevent overfilled queue buffers
    // rarely the Teensy audio queue gets a hickup
    // in that case this keeps the whole audio chain running smoothly 
    if (Q_in_L.available() >  1) 
    {
      AudioNoInterrupts();
      Q_in_L.clear();
      n_clear ++; // just for debugging to check how often this occurs [about once in an hour of playing . . .]
      AudioInterrupts();
    }      
    if (Q_in_R.available() >  1)
    {
      AudioNoInterrupts();
      Q_in_R.clear();
      n_clear ++; // just for debugging to check how often this occurs [about once in an hour of playing . . .]
      AudioInterrupts();
    }

/***********************************************************************************************
 *  just for checking: plotting min/max and mean of the samples
 ***********************************************************************************************/
//    if (ms_500.check() == 1)
    if(0)
    {
    float32_t sample_min = 0.0;
    float32_t sample_max = 0.0;
    float32_t sample_mean = 0.0;
    uint32_t min_index, max_index;
    arm_mean_f32(float_buffer_L, BUFFER_SIZE * N_BLOCKS, &sample_mean);
    arm_max_f32(float_buffer_L, BUFFER_SIZE * N_BLOCKS, &sample_max, &max_index);
    arm_min_f32(float_buffer_L, BUFFER_SIZE * N_BLOCKS, &sample_min, &min_index);
    Serial.print("Sample min: "); Serial.println(sample_min);   
    Serial.print("Sample max: "); Serial.println(sample_max); 
    Serial.print("Max index: "); Serial.println(max_index); 
      
    Serial.print("Sample mean: "); Serial.println(sample_mean); 
    Serial.print("FFT_length: "); Serial.println(FFT_length); 
    Serial.print("N_BLOCKS: "); Serial.println(N_BLOCKS); 
    Serial.println(BUFFER_SIZE * N_BLOCKS / 8);      
    }


/***********************************************************************************************
 *  IQ amplitude and phase correction
 ***********************************************************************************************/

    if(!MOSELEY && !CHANG)
    {
       // Manual IQ amplitude correction
        // to be honest: we only correct the amplitude of the I channel ;-)
        arm_scale_f32 (float_buffer_L, IQ_amplitude_correction_factor, float_buffer_L, BUFFER_SIZE * N_BLOCKS);
        // IQ phase correction
        IQ_phase_correction(float_buffer_L, float_buffer_R, IQ_phase_correction_factor, BUFFER_SIZE * N_BLOCKS);
        Serial.println("Manual IQ correction");
    }
    else
    {
        // introduce artificial amplitude imbalance
        arm_scale_f32 (float_buffer_R, 0.97, float_buffer_R, BUFFER_SIZE * N_BLOCKS);
        // introduce artificial phase imbalance
        IQ_phase_correction(float_buffer_L, float_buffer_R, +0.05, BUFFER_SIZE * N_BLOCKS);
    }

/*******************************************************************************************************
*
* algorithm by Moseley & Slump
********************************************************************************************************/
    if(MOSELEY)
    {   
        teta1 = 0.0;
        teta2 = 0.0;
        teta3 = 0.0;
        for(i = 0; i < n_para; i++)
        {
            teta1 += sign(float_buffer_L[i]) * float_buffer_R[i]; // eq (34)
            teta2 += sign(float_buffer_L[i]) * float_buffer_L[i]; // eq (35)
            teta3 += sign (float_buffer_R[i]) * float_buffer_R[i]; // eq (36)
        }
        teta1 = -0.01 * (teta1 / n_para) + 0.99 * teta1_old; // eq (34) and first order lowpass
        teta2 = 0.01 * (teta2 / n_para) + 0.99 * teta2_old; // eq (35) and first order lowpass
        teta3 = 0.01 * (teta3 / n_para) + 0.99 * teta3_old; // eq (36) and first order lowpass
        M_c1 = teta1 / teta2; // eq (30)
        M_c2 = sqrtf((teta3 * teta3 - teta1 * teta1) / (teta2 * teta2)); // eq (31)
        teta1_old = teta1;
        teta2_old = teta2;
        teta3_old = teta3;
        // first correct Q and then correct I --> this order is crucially important!
        for(i = 0; i < BUFFER_SIZE * N_BLOCKS; i++)
        {   // see fig. 5
            float_buffer_R[i] += M_c1 * float_buffer_L[i];
        }
        // see fig. 5
        arm_scale_f32 (float_buffer_L, M_c2, float_buffer_L, BUFFER_SIZE * N_BLOCKS);
    }

/*******************************************************************************************************
*
* algorithm by Chang et al. 2010
*
*
********************************************************************************************************/

  if(CHANG)
  {
// this is the IQ imbalance correction algorithm by Chang et al. 2010
// 1.) estimate K_est
// 2.) correct for K_est_mult
// 3.) estimate P_est
// 4.) correct for P_est_mult

// new experiment

    // calculate the coefficients for the imbalance correction
    // once at system start or when changing frequency band
    // IQ_state 0: do nothing --> automatic IQ imbalance correction switched off
    // IQ_state 1: estimate amplitude coefficient K_est
    // IQ_state 2: K_est estimated, wait for next stage
    // IQ_state 3: estimate phase coefficient P_est
    // IQ_state 4: everything calculated and corrected

/*
    switch(IQ_state) 
    {
      case 0:
        break;
      case 1: // Chang & Lin (2010): eq. (9)
          AudioNoInterrupts();
          Q_sum = 0.0;
          I_sum = 0.0;
          for (i = 0; i < n_para; i++)
          {
               Q_sum += float_buffer_R[i] * float_buffer_R[i + n_para];
               I_sum += float_buffer_L[i] * float_buffer_L[i + n_para];
          }
          K_est = sqrtf(Q_sum / I_sum);
          K_est_mult = 1.0 / K_est;
          IQ_state++;
          Serial.print("New 1 / K_est: "); Serial.println(1.0 / K_est);
          AudioInterrupts();
        break;    
      case 2: // Chang & Lin (2010): eq. (10)
          AudioNoInterrupts();
          IQ_sum = 0.0;
          I_sum = 0.0;
          for (i = 0; i < n_para; i++)
          {
               IQ_sum += float_buffer_L[i] * float_buffer_R[i + n_para];
               I_sum += float_buffer_L[i] * float_buffer_L[i + n_para];
          }
          P_est = IQ_sum / I_sum;
          P_est_mult = 1.0 / (sqrtf(1.0 - P_est * P_est));
          IQ_state = 1;
          Serial.print("1 / sqrt(1 - P_est^2): "); Serial.println(P_est_mult);
          if(P_est > -1.0 && P_est < 1.0) {
            Serial.print("New: Phasenfehler in Grad: "); Serial.println(- asinf(P_est)); 
          }
          AudioInterrupts();
        break;
    }
*/
  
      // only correct, if signal strength is above a threshold
      // 
      if(IQ_counter >= 0 && 1 )
      {
// 1.)  
    // K_est estimation
          Q_sum = 0.0;
          I_sum = 0.0;
          for (i = 0; i < n_para; i++)
          {
               Q_sum += float_buffer_R[i] * float_buffer_R[i + n_para];
               I_sum += float_buffer_L[i] * float_buffer_L[i + n_para];
          }
          if(I_sum != 0.0)
          {
            if(Q_sum / I_sum < 0) {
            Serial.println("ACHTUNG WURZEL AUS NEGATIVER ZAHL");
            Serial.println("ACHTUNG WURZEL AUS NEGATIVER ZAHL");
            Serial.println("ACHTUNG WURZEL AUS NEGATIVER ZAHL");
            Serial.println("ACHTUNG WURZEL AUS NEGATIVER ZAHL");
            Serial.println("ACHTUNG WURZEL AUS NEGATIVER ZAHL");
            Serial.println("ACHTUNG WURZEL AUS NEGATIVER ZAHL");
            Serial.println("ACHTUNG WURZEL AUS NEGATIVER ZAHL");
            Serial.println("ACHTUNG WURZEL AUS NEGATIVER ZAHL");
            Serial.println("ACHTUNG WURZEL AUS NEGATIVER ZAHL");
            K_est = K_est_old;
            }
            else 
            {
                if(IQ_counter != 0) K_est = 0.001 * sqrtf(Q_sum / I_sum) + 0.999 * K_est_old;
                else 
                {
                    K_est = sqrtf(Q_sum / I_sum);
                }
                K_est_old = K_est;
            }
          }
          else K_est = K_est_old;
          Serial.print("New 1 / K_est: "); Serial.println(100.0 / K_est);

// 3.)
          // phase estimation 
          IQ_sum = 0.0;
          for (i = 0; i < n_para; i++)
          {    // amplitude correction inside the formula  --> K_est_mult !
               IQ_sum += float_buffer_L[i] * float_buffer_R[i + n_para];// * K_est_mult;
          }
          if(I_sum == 0.0) I_sum = IQ_sum;
          if(IQ_counter != 0) P_est = 0.001 * (IQ_sum / I_sum) + 0.999 * P_est_old;
          else P_est = (IQ_sum / I_sum);
          P_est_old = P_est;
          if(P_est > -1.0 && P_est < 1.0) P_est_mult = 1.0 / (sqrtf(1.0 - P_est * P_est));
          else P_est_mult = 1.0;
          // dirty fix !!!

          Serial.print("1 / sqrt(1 - P_est^2): "); Serial.println(P_est_mult * 100.0);
          if(P_est > -1.0 && P_est < 1.0) {
          Serial.print("New: Phasenfehler in Grad: "); Serial.println(- asinf(P_est) * 100.0); 
          }

// 4.)
    // Chang & Lin (2010): eq. 12; phase correction
    for(i = 0; i < BUFFER_SIZE * N_BLOCKS; i++)
    {
        float_buffer_R[i] = P_est_mult * float_buffer_R[i] - P_est * float_buffer_L[i];
    }
      }
       IQ_counter++;
    if(IQ_counter >= 1000) IQ_counter = 1;
  
  } // end if (Chang)

 
/***********************************************************************************************
 *  Perform a 256 point FFT for the spectrum display on the basis of the first 256 complex values
 *  of the raw IQ input data
 *  this saves about 3% of processor power compared to calculating the magnitudes and means of 
 *  the 4096 point FFT for the display [41.8% vs. 44.53%]
 ***********************************************************************************************/
    // only go there from here, if magnification == 1
    if (spectrum_zoom == SPECTRUM_ZOOM_1)
    {
//        Zoom_FFT_exe(BUFFER_SIZE * N_BLOCKS);
          zoom_display = 1;
          calc_256_magn();
    }
   
/**********************************************************************************
 *  Frequency translation by Fs/4 without multiplication
 *  Lyons (2011): chapter 13.1.2 page 646
 *  together with the savings of not having to shift/rotate the FFT_buffer, this saves
 *  about 1% of processor use (40.78% vs. 41.70% [AM demodulation])
 **********************************************************************************/
      // this is for +Fs/4 [moves receive frequency to the left in the spectrum display]
        for(i = 0; i < BUFFER_SIZE * N_BLOCKS; i += 4)
    {   // float_buffer_L contains I = real values
        // float_buffer_R contains Q = imaginary values
        // xnew(0) =  xreal(0) + jximag(0)
        // leave as it is!
        // xnew(1) =  - ximag(1) + jxreal(1)
        hh1 = - float_buffer_R[i + 1];
        hh2 =   float_buffer_L[i + 1];
            float_buffer_L[i + 1] = hh1;
            float_buffer_R[i + 1] = hh2;
        // xnew(2) = -xreal(2) - jximag(2)
        hh1 = - float_buffer_L[i + 2];
        hh2 = - float_buffer_R[i + 2];
            float_buffer_L[i + 2] = hh1;
            float_buffer_R[i + 2] = hh2;
        // xnew(3) = + ximag(3) - jxreal(3)
        hh1 =   float_buffer_R[i + 3];
        hh2 = - float_buffer_L[i + 3];
            float_buffer_L[i + 3] = hh1;
            float_buffer_R[i + 3] = hh2;
    }

      // this is for -Fs/4 [moves receive frequency to the right in the spectrumdisplay]
/*    for(i = 0; i < BUFFER_SIZE * N_BLOCKS; i += 4)
    {   // float_buffer_L contains I = real values
        // float_buffer_R contains Q = imaginary values
        // xnew(0) =  xreal(0) + jximag(0)
        // leave as it is!
        // xnew(1) =  ximag(1) - jxreal(1)
        hh1 = float_buffer_R[i + 1];
        hh2 = - float_buffer_L[i + 1];
        float_buffer_L[i + 1] = hh1;
        float_buffer_R[i + 1] = hh2;
        // xnew(2) = -xreal(2) - jximag(2)
        hh1 = - float_buffer_L[i + 2];
        hh2 = - float_buffer_R[i + 2];
        float_buffer_L[i + 2] = hh1;
        float_buffer_R[i + 2] = hh2;
        // xnew(3) = -ximag(3) + jxreal(3)
        hh1 = - float_buffer_R[i + 3];
        hh2 = float_buffer_L[i + 3];
        float_buffer_L[i + 3] = hh1;
        float_buffer_R[i + 3] = hh2;
    }
*/  
/***********************************************************************************************
 *  just for checking: plotting min/max and mean of the samples
 ***********************************************************************************************/
//    if (ms_500.check() == 1)
    if(0)
    {
    float32_t sample_min = 0.0;
    float32_t sample_max = 0.0;
    float32_t sample_mean = 0.0;
    flagg = 1;
    uint32_t min_index, max_index;
    arm_mean_f32(float_buffer_L, BUFFER_SIZE * N_BLOCKS, &sample_mean);
    arm_max_f32(float_buffer_L, BUFFER_SIZE * N_BLOCKS, &sample_max, &max_index);
    arm_min_f32(float_buffer_L, BUFFER_SIZE * N_BLOCKS, &sample_min, &min_index);
    Serial.print("VOR DECIMATION: ");
    Serial.print("Sample min: "); Serial.println(sample_min);   
    Serial.print("Sample max: "); Serial.println(sample_max); 
    Serial.print("Max index: "); Serial.println(max_index); 
      
    Serial.print("Sample mean: "); Serial.println(sample_mean); 
//    Serial.print("FFT_length: "); Serial.println(FFT_length); 
//    Serial.print("N_BLOCKS: "); Serial.println(N_BLOCKS); 
//Serial.println(BUFFER_SIZE * N_BLOCKS / 8);      
    }

      // SPECTRUM_ZOOM_2 and larger here after frequency conversion!
      if(spectrum_zoom != SPECTRUM_ZOOM_1)
      {
          Zoom_FFT_exe (BUFFER_SIZE * N_BLOCKS);
      }
          if(zoom_display)
          {
            show_spectrum();
            zoom_display = 0;
            zoom_sample_ptr = 0;
          }

/**********************************************************************************
 *  S-Meter & dBm-display
 **********************************************************************************/
        Calculatedbm();
        Display_dbm();

/**********************************************************************************
 *  Decimation
 **********************************************************************************/
      // lowpass filter 5kHz, 80dB stopband att, fstop = 19k, 25 taps
      // decimation-by-4 in-place!
      arm_fir_decimate_f32(&FIR_dec1_I, float_buffer_L, float_buffer_L, BUFFER_SIZE * N_BLOCKS);
      arm_fir_decimate_f32(&FIR_dec1_Q, float_buffer_R, float_buffer_R, BUFFER_SIZE * N_BLOCKS);

      // lowpass filter 5kHz, 80dB stopband att, fstop = 7k, 44 taps
      // decimation-by-2 in-place
      arm_fir_decimate_f32(&FIR_dec2_I, float_buffer_L, float_buffer_L, BUFFER_SIZE * N_BLOCKS / 4);
      arm_fir_decimate_f32(&FIR_dec2_Q, float_buffer_R, float_buffer_R, BUFFER_SIZE * N_BLOCKS / 4);

/***********************************************************************************************
 *  just for checking: plotting min/max and mean of the samples
 ***********************************************************************************************/
//    if (flagg == 1)
    if(0)
    {
    flagg = 0;  
    float32_t sample_min = 0.0;
    float32_t sample_max = 0.0;
    float32_t sample_mean = 0.0;
    uint32_t min_index, max_index;
    arm_mean_f32(float_buffer_L, BUFFER_SIZE * N_BLOCKS / 8, &sample_mean);
    arm_max_f32(float_buffer_L, BUFFER_SIZE * N_BLOCKS / 8, &sample_max, &max_index);
    arm_min_f32(float_buffer_L, BUFFER_SIZE * N_BLOCKS/ 8, &sample_min, &min_index);
    Serial.print("NACH DECIMATION: ");
    Serial.print("Sample min: "); Serial.println(sample_min);   
    Serial.print("Sample max: "); Serial.println(sample_max); 
    Serial.print("Max index: "); Serial.println(max_index); 
      
    Serial.print("Sample mean: "); Serial.println(sample_mean); 
//    Serial.print("FFT_length: "); Serial.println(FFT_length); 
//    Serial.print("N_BLOCKS: "); Serial.println(N_BLOCKS); 
//Serial.println(BUFFER_SIZE * N_BLOCKS / 8);      
    }

/**********************************************************************************
 *  Digital convolution
 **********************************************************************************/
//  basis for this was Lyons, R. (2011): Understanding Digital Processing.
//  "Fast FIR Filtering using the FFT", pages 688 - 694
//  numbers for the steps taken from that source
//  Method used here: overlap-and-save

// 4.) ONLY FOR the VERY FIRST FFT: fill first samples with zeros 
      if(first_block) // fill real & imaginaries with zeros for the first BLOCKSIZE samples
      {
        for(i = 0; i < BUFFER_SIZE * N_BLOCKS / 4; i++)
        {
          FFT_buffer[i] = 0.0;
        }
        first_block = 0;
      }
      else
      
// HERE IT STARTS for all other instances
// 6 a.) fill FFT_buffer with last events audio samples
      for(i = 0; i < BUFFER_SIZE * N_BLOCKS / 8; i++)
      {
        FFT_buffer[i * 2] = last_sample_buffer_L[i]; // real
        FFT_buffer[i * 2 + 1] = last_sample_buffer_R[i]; // imaginary
      }

   
    // copy recent samples to last_sample_buffer for next time!
      for(i = 0; i < BUFFER_SIZE * N_BLOCKS / 8; i++)
      {
         last_sample_buffer_L [i] = float_buffer_L[i]; 
         last_sample_buffer_R [i] = float_buffer_R[i]; 
      }
    
// 6. b) now fill audio samples into FFT_buffer (left channel: re, right channel: im)
      for(i = 0; i < BUFFER_SIZE * N_BLOCKS / 8; i++)
      {
        FFT_buffer[FFT_length + i * 2] = float_buffer_L[i]; // real
        FFT_buffer[FFT_length + i * 2 + 1] = float_buffer_R[i]; // imaginary
      }

// perform complex FFT
// calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
      arm_cfft_f32(S, FFT_buffer, 0, 1);

// AUTOTUNE, slow down process in order for Si5351 to settle 
      if(autotune_flag != 0)
      {
          if(autotune_counter < autotune_wait) autotune_counter++;
          else 
          {
              autotune_counter = 0;
              autotune();
          }
      }

/*
// frequency shift & choice of FFT_bins for "demodulation"
    switch (demod_mode) 
    {
      case DEMOD_AM:
////////////////////////////////////////////////////////////////////////////////
// AM demodulation - shift everything FFT_length / 4 bins around IF 
// leave both: USB & LSB
////////////////////////////////////////////////////////////////////////////////
// shift USB part by FFT_shift
    for(i = 0; i < FFT_shift; i++)
    {
        FFT_buffer[i] = FFT_buffer[i + FFT_length * 2 - FFT_shift];
    }
    
// now shift LSB part by FFT_shift
    for(i = (FFT_length * 2) - FFT_shift - 2; i > FFT_length - FFT_shift;i--)
     {
        FFT_buffer[i + FFT_shift] = FFT_buffer[i]; // shift LSB part by FFT_shift bins
     }
     break;
///// END AM-demodulation, seems to be OK (at least sounds ok)     
////////////////////////////////////////////////////////////////////////////////
// USB DEMODULATION
////////////////////////////////////////////////////////////////////////////////
    case DEMOD_USB:
    case DEMOD_STEREO_USB:
    for(i = FFT_length / 2; i < FFT_length ; i++) // maybe this is not necessary? yes, it is!
     {
        FFT_buffer[i] = FFT_buffer [i - FFT_shift]; // shift USB, 2nd part, to the right
     }

    // frequency shift of the USB 1st part - but this is essential 
    for(i = FFT_length * 2 - FFT_shift; i < FFT_length * 2; i++)
    {
        FFT_buffer[i - FFT_length * 2 + FFT_shift] = FFT_buffer[i];
    }
    // fill LSB with zeros
    for(i = FFT_length; i < FFT_length *2; i++)
     {
        FFT_buffer[i] = 0.0; // fill rest with zero
     }
    break;
// END USB: audio seems to be OK 
////////////////////////////////////////////////////////////////////////////////    
    case DEMOD_LSB:
    case DEMOD_STEREO_LSB:    
    case DEMOD_DCF77:
////////////////////////////////////////////////////////////////////////////////
// LSB demodulation
////////////////////////////////////////////////////////////////////////////////
    for(i = 0; i < FFT_length ; i++)
     {
        FFT_buffer[i] = 0.0; // fill USB with zero
     }
    // frequency shift of the LSB part 
    ////////////////////////////////////////////////////////////////////////////////
    for(i = (FFT_length * 2) - FFT_shift - 2; i > FFT_length - FFT_shift;i--)
     {
        FFT_buffer[i + FFT_shift] = FFT_buffer[i]; // shift LSB part by FFT_shift bins
     }
// fill USB with zeros
    for(i = FFT_length; i < FFT_length + FFT_shift; i++)
     {
        FFT_buffer[i] = 0.0; // fill rest with zero
     }
    break;
// END LSB: audio seems to be OK 
////////////////////////////////////////////////////////////////////////////////    
    } // END switch demod_mode
*/     

// choice of FFT_bins for "demodulation" WITHOUT frequency translation
// (because frequency translation has already been done in time domain with
// "frequency translation without multiplication" - DSP trick R. Lyons (2011))
    switch (band[bands].mode) 
    {
//      case DEMOD_AM1:
      case DEMOD_AUTOTUNE:
      case DEMOD_DSB:
      case DEMOD_SAM_USB:
      case DEMOD_SAM_LSB:

////////////////////////////////////////////////////////////////////////////////
// AM & SAM demodulation 
// DO NOTHING !
// leave both: USB & LSB
////////////////////////////////////////////////////////////////////////////////
     break;
////////////////////////////////////////////////////////////////////////////////
// USB DEMODULATION
////////////////////////////////////////////////////////////////////////////////
    case DEMOD_USB:
    case DEMOD_STEREO_USB:
    // fill LSB with zeros
    for(i = FFT_length; i < FFT_length * 2; i++)
     {
        FFT_buffer[i] = 0.0;
     }
    break;
////////////////////////////////////////////////////////////////////////////////    
    case DEMOD_LSB:
    case DEMOD_STEREO_LSB:    
    case DEMOD_DCF77:
////////////////////////////////////////////////////////////////////////////////
// LSB demodulation
////////////////////////////////////////////////////////////////////////////////
    for(i = 4; i < FFT_length ; i++) 
    // when I delete all the FFT_buffer products (from i = 0 to FFT_length), LSB is much louder! --> effect of the AGC !!1
    // so, I leave indices 0 to 3 in the buffer 
     {
        FFT_buffer[i] = 0.0; // fill USB with zero
     }
    break;
////////////////////////////////////////////////////////////////////////////////    
    } // END switch demod_mode


// complex multiply with filter mask
     arm_cmplx_mult_cmplx_f32 (FFT_buffer, FIR_filter_mask, iFFT_buffer, FFT_length);

// perform iFFT (in-place)
     arm_cfft_f32(iS, iFFT_buffer, 1, 1);

/**********************************************************************************
 *  AGC - automatic gain control
 *  
 *  we´re back in time domain
 *  AGC acts upon I & Q before demodulation on the decimated audio data in iFFT_buffer
 **********************************************************************************/

    AGC();

/**********************************************************************************
 *  Demodulation
 **********************************************************************************/

// our desired output is a combination of the real part (left channel) AND the imaginary part (right channel) of the second half of the FFT_buffer
// which one and how they are combined is dependent upon the demod_mode . . .


      if(band[bands].mode == DEMOD_SAM || band[bands].mode == DEMOD_SAM_LSB ||band[bands].mode == DEMOD_SAM_USB ||band[bands].mode == DEMOD_SAM_STEREO)
      {   // taken from Warren Pratt´s WDSP, 2016
          // http://svn.tapr.org/repos_sdr_hpsdr/trunk/W5WC/PowerSDR_HPSDR_mRX_PS/Source/wdsp/

        for(i = 0; i < FFT_length / 2; i++)
        {
            sincosf(phzerror,&Sin,&Cos);
            ai = Cos * iFFT_buffer[FFT_length + i * 2];
            bi = Sin * iFFT_buffer[FFT_length + i * 2];
            aq = Cos * iFFT_buffer[FFT_length + i * 2 + 1];
            bq = Sin * iFFT_buffer[FFT_length + i * 2 + 1];
 
            if (band[bands].mode != DEMOD_SAM)
            {
              a[0] = dsI;
              b[0] = bi;
              c[0] = dsQ;
              d[0] = aq;
              dsI = ai;
              dsQ = bq;

              for (int j = 0; j < SAM_PLL_HILBERT_STAGES; j++)
              {
                k = 3 * j;
                a[k + 3] = c0[j] * (a[k] - a[k + 5]) + a[k + 2];
                b[k + 3] = c1[j] * (b[k] - b[k + 5]) + b[k + 2];
                c[k + 3] = c0[j] * (c[k] - c[k + 5]) + c[k + 2];
                d[k + 3] = c1[j] * (d[k] - d[k + 5]) + d[k + 2];
              }
              ai_ps = a[OUT_IDX];
              bi_ps = b[OUT_IDX];
              bq_ps = c[OUT_IDX];
              aq_ps = d[OUT_IDX];

              for (j = OUT_IDX + 2; j > 0; j--)
              {
                a[j] = a[j - 1];
                b[j] = b[j - 1];
                c[j] = c[j - 1];
                d[j] = d[j - 1];
              }
            }

            corr[0] = +ai + bq;
            corr[1] = -bi + aq;

            switch(band[bands].mode)
            {
            case DEMOD_SAM:
              {
                audio = corr[0];
                break;
              }
            case DEMOD_SAM_USB:
              {
                audio = (ai_ps - bi_ps) + (aq_ps + bq_ps);
                break;
              }
            case DEMOD_SAM_LSB:
              {
                audio = (ai_ps + bi_ps) - (aq_ps - bq_ps);
                break;
              }
            case DEMOD_SAM_STEREO:
              {
                audio = (ai_ps + bi_ps) - (aq_ps - bq_ps);
                audiou = (ai_ps - bi_ps) + (aq_ps + bq_ps);
                break;
              }
            }
            if(fade_leveler)
            {
            dc = mtauR * dc + onem_mtauR * audio;
            dc_insert = mtauI * dc_insert + onem_mtauI * corr[0];
            audio = audio + dc_insert - dc;
            }
            float_buffer_L[i] = audio;
            if(band[bands].mode == DEMOD_SAM_STEREO)
            {            
              if(fade_leveler)
              {
                  dcu = mtauR * dcu + onem_mtauR * audiou;
                  dc_insertu = mtauI * dc_insertu + onem_mtauI * corr[0];
                  audiou = audiou + dc_insertu - dcu;
              }
              float_buffer_R[i] = audiou;
            }
            
            det = atan2f(corr[1], corr[0]);
//            Serial.println(corr[1] * 100000);
//            Serial.println(corr[0] * 100000);
              // is not at all faster than atan2f !
//            det = atan2_fast(corr[1], corr[0]);

                del_out = fil_out;
                omega2 = omega2 + g2 * det;
                if (omega2 < omega_min) omega2 = omega_min;
                else if (omega2 > omega_max) omega2 = omega_max;
                fil_out = g1 * det + omega2;
                phzerror = phzerror + del_out;
           
            // wrap round 2PI, modulus
            while (phzerror >= TPI) phzerror -= TPI;
            while (phzerror < 0.0) phzerror += TPI;
        } 
        if(band[bands].mode != DEMOD_SAM_STEREO) arm_copy_f32(float_buffer_L, float_buffer_R, FFT_length/2);
                SAM_display_count++;
        if(SAM_display_count > 50) // to display the exact carrier frequency that the PLL is tuned to
//        if(0)
        // in the small frequency display
            // we calculate carrier offset here and the display function is
            // then called in UiDriver_MainHandler approx. every 40-80ms
        { // to make this smoother, a simple lowpass/exponential averager here . . .
            SAM_carrier = 0.1 * (omega2 * SR[SAMPLE_RATE].rate) / (DF * TPI);
            SAM_carrier = SAM_carrier + 0.9 * SAM_lowpass;
            SAM_carrier_freq_offset =  (int)SAM_carrier;
            SAM_display_count = 0;
            SAM_lowpass = SAM_carrier;
        }
      }
      else
/*      if(demod_mode == DEMOD_AM1)
      { // E(t) = sqrt(I*I + Q*Q) with arm function --> moving average DC removal --> lowpass IIR 2nd order
        // this AM demodulator with arm_cmplx_mag_f32 saves 1.23% of processor load at 96ksps compared to using arm_sqrt_f32
        arm_cmplx_mag_f32(&iFFT_buffer[FFT_length], float_buffer_R, FFT_length/2);
        // DC removal
        last_dc_level = fastdcblock_ff(float_buffer_R, float_buffer_L, FFT_length / 2, last_dc_level);
        // lowpass-filter the AM output to reduce high frequency noise produced by the envelope demodulator
        // see Whiteley 2011 for an explanation of this problem
        // 45.18% with IIR; 43.29% without IIR 
        arm_biquad_cascade_df1_f32 (&biquad_lowpass1, float_buffer_L, float_buffer_R, FFT_length / 2);
        arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length/2);
      }
      else */
      if(band[bands].mode == DEMOD_AM2)
      { // // E(t) = sqrtf(I*I + Q*Q) --> highpass IIR 1st order for DC removal --> lowpass IIR 2nd order
          for(i = 0; i < FFT_length / 2; i++)
          { // 
                audiotmp = sqrtf(iFFT_buffer[FFT_length + (i * 2)] * iFFT_buffer[FFT_length + (i * 2)] 
                                    + iFFT_buffer[FFT_length + (i * 2) + 1] * iFFT_buffer[FFT_length + (i * 2) + 1]); 
                // DC removal filter ----------------------- 
                w = audiotmp + wold * 0.9999f; // yes, I want a superb bass response ;-) 
                float_buffer_L[i] = w - wold; 
                wold = w; 
          }
          arm_biquad_cascade_df1_f32 (&biquad_lowpass1, float_buffer_L, float_buffer_R, FFT_length / 2);
          arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length/2);
      }
      else 
/*      if(band[bands].mode == DEMOD_AM3)
      { // // E(t) = sqrt(I*I + Q*Q) --> highpass IIR 1st order for DC removal --> no lowpass 
          for(i = 0; i < FFT_length / 2; i++)
          { // 
                audiotmp = sqrtf(iFFT_buffer[FFT_length + (i * 2)] * iFFT_buffer[FFT_length + (i * 2)] 
                                    + iFFT_buffer[FFT_length + (i * 2) + 1] * iFFT_buffer[FFT_length + (i * 2) + 1]); 
                // DC removal filter ----------------------- 
                w = audiotmp + wold * 0.9999f; // yes, I want a superb bass response ;-) 
                float_buffer_L[i] = w - wold; 
                wold = w; 
          }
          arm_copy_f32(float_buffer_L, float_buffer_R, FFT_length/2);
      }
      else 
      if(band[bands].mode == DEMOD_AM_AE1)
      {  // E(n) = |I| + |Q| --> moving average DC removal --> lowpass 2nd order IIR
        // Approximate envelope detection, Lyons (2011): page 756
         // E(n) = |I| + |Q| --> lowpass (here I use a 2nd order IIR instead of the 1st order IIR of the original publication)
          for(i = 0; i < FFT_length / 2; i++)
          {
                float_buffer_R[i] = abs(iFFT_buffer[FFT_length + (i * 2)]) + abs(iFFT_buffer[FFT_length + (i * 2) + 1]); 
          }
        // DC removal
        last_dc_level = fastdcblock_ff(float_buffer_R, float_buffer_L, FFT_length / 2, last_dc_level);
        arm_biquad_cascade_df1_f32 (&biquad_lowpass1, float_buffer_L, float_buffer_R, FFT_length / 2);
        arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length/2);
      }
      else 
      if(band[bands].mode == DEMOD_AM_AE2)
      {  // E(n) = |I| + |Q| --> highpass IIR 1st order for DC removal --> lowpass 2nd order IIR 
         // Approximate envelope detection, Lyons (2011): page 756
         // E(n) = |I| + |Q| --> lowpass (here I use a 2nd order IIR instead of the 1st order IIR of the original publication)
          for(i = 0; i < FFT_length / 2; i++)
          {
            audiotmp = abs(iFFT_buffer[FFT_length + (i * 2)]) + abs(iFFT_buffer[FFT_length + (i * 2) + 1]); 
            // DC removal filter ----------------------- 
            w = audiotmp + wold * 0.9999f; // yes, I want a superb bass response ;-) 
            float_buffer_L[i] = w - wold; 
            wold = w; 
          }
        arm_biquad_cascade_df1_f32 (&biquad_lowpass1, float_buffer_L, float_buffer_R, FFT_length / 2);
        arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length/2);
      }
      else 
      if(band[bands].mode == DEMOD_AM_AE3)
      {  // E(n) = |I| + |Q| --> highpass IIR 1st order for DC removal --> NO lowpass
         // Approximate envelope detection, Lyons (2011): page 756
         // E(n) = |I| + |Q| --> lowpass 1st order IIR
          for(i = 0; i < FFT_length / 2; i++)
          {
            audiotmp = abs(iFFT_buffer[FFT_length + (i * 2)]) + abs(iFFT_buffer[FFT_length + (i * 2) + 1]); 
            // DC removal filter ----------------------- 
            w = audiotmp + wold * 0.9999f; // yes, I want a superb bass response ;-) 
            float_buffer_L[i] = w - wold; 
            wold = w; 
          }
        arm_copy_f32(float_buffer_L, float_buffer_R, FFT_length/2);
      }
      else
      if(band[bands].mode == DEMOD_AM_ME1)
      {  // E(n) = alpha * max [I, Q] + beta * min [I, Q] --> moving average DC removal --> lowpass 2nd order IIR
         // Magnitude estimation Lyons (2011): page 652 / libcsdr
          for(i = 0; i < FFT_length / 2; i++)
          {
                float_buffer_R[i] = alpha_beta_mag(iFFT_buffer[FFT_length + (i * 2)], iFFT_buffer[FFT_length + (i * 2) + 1]);
          }
        // DC removal
        last_dc_level = fastdcblock_ff(float_buffer_R, float_buffer_L, FFT_length / 2, last_dc_level);
        arm_biquad_cascade_df1_f32 (&biquad_lowpass1, float_buffer_L, float_buffer_R, FFT_length / 2);
        arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length/2);
      }
      else */
      if(band[bands].mode == DEMOD_AM_ME2)
      {  // E(n) = alpha * max [I, Q] + beta * min [I, Q] --> highpass 1st order DC removal --> lowpass 2nd order IIR
         // Magnitude estimation Lyons (2011): page 652 / libcsdr
          for(i = 0; i < FFT_length / 2; i++)
          {
                audiotmp = alpha_beta_mag(iFFT_buffer[FFT_length + (i * 2)], iFFT_buffer[FFT_length + (i * 2) + 1]);
                // DC removal filter ----------------------- 
                w = audiotmp + wold * 0.9999f; // yes, I want a superb bass response ;-) 
                float_buffer_L[i] = w - wold; 
                wold = w; 
          }
        arm_biquad_cascade_df1_f32 (&biquad_lowpass1, float_buffer_L, float_buffer_R, FFT_length / 2);
        arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length/2);
      }
      else
/*      if(band[bands].mode == DEMOD_AM_ME3)
      {  // E(n) = alpha * max [I, Q] + beta * min [I, Q] --> highpass 1st order DC removal --> NO lowpass
         // Magnitude estimation Lyons (2011): page 652 / libcsdr
          for(i = 0; i < FFT_length / 2; i++)
          {
                audiotmp = alpha_beta_mag(iFFT_buffer[FFT_length + (i * 2)], iFFT_buffer[FFT_length + (i * 2) + 1]);
                // DC removal filter ----------------------- 
                w = audiotmp + wold * 0.9999f; // yes, I want a superb bass response ;-) 
                float_buffer_L[i] = w - wold; 
                wold = w; 
          }
        arm_copy_f32(float_buffer_L, float_buffer_R, FFT_length/2);
      }
      else */
      for(i = 0; i < FFT_length / 2; i++)
      {
        if(band[bands].mode == DEMOD_USB || band[bands].mode == DEMOD_LSB || band[bands].mode == DEMOD_DCF77 || band[bands].mode == DEMOD_AUTOTUNE)
        {
            float_buffer_L[i] = iFFT_buffer[FFT_length + (i * 2)]; 
            // for SSB copy real part in both outputs
            float_buffer_R[i] = float_buffer_L[i];
        }
        else if(band[bands].mode == DEMOD_STEREO_LSB || band[bands].mode == DEMOD_STEREO_USB) // creates a pseudo-stereo effect
            // could be good for copying faint CW signals
        {
            float_buffer_L[i] = iFFT_buffer[FFT_length + (i * 2)]; 
            float_buffer_R[i] = iFFT_buffer[FFT_length + (i * 2) + 1];
        }
      }
      
/**********************************************************************************
 *  INTERPOLATION
 **********************************************************************************/
// re-uses iFFT_buffer[2048] and FFT_buffer !!!

// receives 512 samples and makes 4096 samples out of it
// interpolation-by-2
// interpolation-in-place does not work
      arm_fir_interpolate_f32(&FIR_int1_I, float_buffer_L, iFFT_buffer, BUFFER_SIZE * N_BLOCKS / 8);
      arm_fir_interpolate_f32(&FIR_int1_Q, float_buffer_R, FFT_buffer, BUFFER_SIZE * N_BLOCKS / 8);

// interpolation-by-4
      arm_fir_interpolate_f32(&FIR_int2_I, iFFT_buffer, float_buffer_L, BUFFER_SIZE * N_BLOCKS / 4);
      arm_fir_interpolate_f32(&FIR_int2_Q, FFT_buffer, float_buffer_R, BUFFER_SIZE * N_BLOCKS / 4);

// scale after interpolation
      float32_t interpol_scale = 8.0;
      if(band[bands].mode == DEMOD_LSB || band[bands].mode == DEMOD_USB) interpol_scale = 16.0;
//      if(band[bands].mode == DEMOD_USB) interpol_scale = 16.0;
      arm_scale_f32(float_buffer_L, interpol_scale, float_buffer_L, BUFFER_SIZE * N_BLOCKS);
      arm_scale_f32(float_buffer_R, interpol_scale, float_buffer_R, BUFFER_SIZE * N_BLOCKS);

/**********************************************************************************
 *  CONVERT TO INTEGER AND PLAY AUDIO
 **********************************************************************************/
    for (i = 0; i < N_BLOCKS; i++)
    {
      sp_L = Q_out_L.getBuffer();
      sp_R = Q_out_R.getBuffer();
      arm_float_to_q15 (&float_buffer_L[BUFFER_SIZE * i], sp_L, BUFFER_SIZE); 
      arm_float_to_q15 (&float_buffer_R[BUFFER_SIZE * i], sp_R, BUFFER_SIZE); 
      Q_out_L.playBuffer(); // play it !
      Q_out_R.playBuffer(); // play it !
    }  

/**********************************************************************************
 *  PRINT ROUTINE FOR ELAPSED MICROSECONDS
 **********************************************************************************/
 
      sum = sum + usec;
      idx_t++;
      if (idx_t > 40) {
          tft.fillRect(260,5,60,20,ILI9341_BLACK);   
          tft.setCursor(260, 5);
          tft.setTextColor(ILI9341_GREEN);
          tft.setFont(Arial_9);
          mean = sum / idx_t;
          tft.print (mean/29.00/N_BLOCKS * SR[SAMPLE_RATE].rate / AUDIO_SAMPLE_RATE_EXACT);tft.print("%");
          Serial.print (mean);
          Serial.print (" microsec for ");
          Serial.print (N_BLOCKS);
          Serial.print ("  stereo blocks    ");
          Serial.println();
          Serial.print (" n_clear    ");
          Serial.println(n_clear);
          idx_t = 0;
          sum = 0;
          tft.setTextColor(ILI9341_WHITE);
      }
      /*
          if(zoom_display)
          {
            show_spectrum();
            zoom_display = 0;
          zoom_sample_ptr = 0;
          }
          */
          if(band[bands].mode == DEMOD_DCF77)
          { 
          }
     } // end of if(audio blocks available)
/**********************************************************************************
 *  PRINT ROUTINE FOR AUDIO LIBRARY PROCESSOR AND MEMORY USAGE
 **********************************************************************************/
          if (five_sec.check() == 1)
    {
      Serial.print("Proc = ");
      Serial.print(AudioProcessorUsage());
      Serial.print(" (");    
      Serial.print(AudioProcessorUsageMax());
      Serial.print("),  Mem = ");
      Serial.print(AudioMemoryUsage());
      Serial.print(" (");    
      Serial.print(AudioMemoryUsageMax());
      Serial.println(")");
      AudioProcessorUsageMaxReset();
      AudioMemoryUsageMaxReset();
    }
//    show_spectrum();
    if(encoder_check.check() == 1)
    {
        encoders();
        buttons();
        displayClock();
    }

    if (ms_500.check() == 1)
        wait_flag = 0;

//    if(dbm_check.check() == 1) Calculatedbm();

    if(Menu_pointer == MENU_PLAYER)
    {
        if(trackext[track] == 1)
        {
          Serial.println("MP3" );
          playFileMP3(playthis);
        }
        else if(trackext[track] == 2)
        {
          Serial.println("aac");
          playFileAAC(playthis);
        }
        if(trackchange == true)
        { //when your track is finished, go to the next one. when using buttons, it will not skip twice.
            nexttrack();
        }
    }
    
} // end loop

void IQ_phase_correction (float32_t *I_buffer, float32_t *Q_buffer, float32_t factor, uint32_t blocksize) 
{
  float32_t temp_buffer[blocksize];
  if(factor < 0.0)
  { // mix a bit of I into Q
      arm_scale_f32 (I_buffer, factor, temp_buffer, blocksize);
      arm_add_f32 (Q_buffer, temp_buffer, Q_buffer, blocksize);
  }
  else
  { // // mix a bit of Q into I
      arm_scale_f32 (Q_buffer, factor, temp_buffer, blocksize);
      arm_add_f32 (I_buffer, temp_buffer, I_buffer, blocksize);
  }
} // end IQ_phase_correction

void AGC_prep()
{
  float32_t tmp;
  float32_t sample_rate = (float32_t)SR[SAMPLE_RATE].rate / DF;
// Start variables taken from wdsp
// RXA.c !!!!
/*
    0.001,                      // tau_attack
    0.250,                      // tau_decay
    4,                        // n_tau
    10000.0,                    // max_gain
    1.5,                      // var_gain
    1000.0,                     // fixed_gain
    1.0,                      // max_input
    1.0,                      // out_target
    0.250,                      // tau_fast_backaverage
    0.005,                      // tau_fast_decay
    5.0,                      // pop_ratio
    1,                        // hang_enable
    0.500,                      // tau_hang_backmult
    0.250,                      // hangtime
    0.250,                      // hang_thresh
    0.100);                     // tau_hang_decay
 */
/*  GOOD WORKING VARIABLES
    max_gain = 1.0;                    // max_gain
    var_gain = 0.0015; // 1.5                      // var_gain
    fixed_gain = 1.0;                     // fixed_gain
    max_input = 1.0;                 // max_input
    out_target = 0.00005; //0.0001; // 1.0                // out_target

 */
    tau_attack = 0.001;               // tau_attack
    tau_decay = 0.250;                // tau_decay
    n_tau = 1;                        // n_tau
 
    max_gain = 1000.0; // 1000.0; max gain to be applied??? or is this AGC threshold = knee level?
    fixed_gain = 0.7; // if AGC == OFF
    max_input = 1.0; //
    out_targ = 0.2; // target value of audio after AGC
    var_gain = 32.0;  // slope of the AGC --> this is 10^(slope / 20) --> for 10dB slope, this is 3.0 --> doesnt work !?

    tau_fast_backaverage = 0.250;    // tau_fast_backaverage
    tau_fast_decay = 0.005;          // tau_fast_decay
    pop_ratio = 5.0;                 // pop_ratio
    hang_enable = 0;                 // hang_enable
    tau_hang_backmult = 0.500;       // tau_hang_backmult
    hangtime = 0.250;                // hangtime
    hang_thresh = 0.250;             // hang_thresh
    tau_hang_decay = 0.100;          // tau_hang_decay

  //calculate internal parameters
    switch (AGC_mode)
  {
    case 0: //agcOFF
      break;
    case 2: //agcLONG
      hangtime = 2.000;
      tau_decay = 2.000;
      break;
    case 3: //agcSLOW
      hangtime = 1.000;
      tau_decay = 0.500;
      break;
    case 4: //agcMED
      hang_thresh = 1.0;
      hangtime = 0.000;
      tau_decay = 0.250;
      break;
    case 5: //agcFAST
      hang_thresh = 1.0;
      hangtime = 0.000;
      tau_decay = 0.050;
      break;
    case 1: //agcFrank
      hang_enable = 0;
      hang_thresh = 0.100; // from which level on should hang be enabled
      hangtime = 2.000; // hang time, if enabled
      tau_hang_backmult = 0.500; // time constant exponential averager
      
      tau_decay = 1.200; // time constant decay long
      tau_fast_decay = 0.005;          // tau_fast_decay
      tau_fast_backaverage = 0.250; // time constant exponential averager
      max_gain = 1000.0; // max gain to be applied??? or is this AGC threshold = knee level?
      fixed_gain = 0.7; // if AGC == OFF
      max_input = 1.0; //
      out_targ = 0.2; // target value of audio after AGC
      var_gain = 30.0;  // slope of the AGC --> 

/*    // sehr gut!
 *     hang_thresh = 0.100;
      hangtime = 2.000;
      tau_decay = 2.000;
      tau_hang_backmult = 0.500;
      tau_fast_backaverage = 0.250;
      out_targ = 0.0004;
      var_gain = 0.001; */
      break;
    default:
      break;
  }
  attack_buffsize = (int)ceil(sample_rate * n_tau * tau_attack);
  Serial.println(attack_buffsize);
  in_index = attack_buffsize + out_index;
  attack_mult = 1.0 - expf(-1.0 / (sample_rate * tau_attack));
  Serial.print("attack_mult = ");
  Serial.println(attack_mult * 1000);
  decay_mult = 1.0 - expf(-1.0 / (sample_rate * tau_decay));
  Serial.print("decay_mult = ");
  Serial.println(decay_mult * 1000);
  fast_decay_mult = 1.0 - expf(-1.0 / (sample_rate * tau_fast_decay));
  Serial.print("fast_decay_mult = ");
  Serial.println(fast_decay_mult * 1000);
  fast_backmult = 1.0 - expf(-1.0 / (sample_rate * tau_fast_backaverage));
  Serial.print("fast_backmult = ");
  Serial.println(fast_backmult * 1000);

  onemfast_backmult = 1.0 - fast_backmult;

  out_target = out_targ * (1.0 - expf(-(float32_t)n_tau)) * 0.9999;
//  out_target = out_target * (1.0 - expf(-(float32_t)n_tau)) * 0.9999;
  Serial.print("out_target = ");
  Serial.println(out_target * 1000);
  min_volts = out_target / (var_gain * max_gain);
  inv_out_target = 1.0 / out_target;

  tmp = log10f(out_target / (max_input * var_gain * max_gain));
  if (tmp == 0.0)
    tmp = 1e-16;
  slope_constant = (out_target * (1.0 - 1.0 / var_gain)) / tmp;
  Serial.print("slope_constant = ");
  Serial.println(slope_constant * 1000);

  inv_max_input = 1.0 / max_input;

  tmp = powf (10.0, (hang_thresh - 1.0) / 0.125);
  hang_level = (max_input * tmp + (out_target / 
    (var_gain * max_gain)) * (1.0 - tmp)) * 0.637;

  hang_backmult = 1.0 - expf(-1.0 / (sample_rate * tau_hang_backmult));
  onemhang_backmult = 1.0 - hang_backmult;

  hang_decay_mult = 1.0 - expf(-1.0 / (sample_rate * tau_hang_decay));
}

   
void AGC ()
{
  int i, j, k;
  float32_t mult;

    if (AGC_mode == 0)  // AGC OFF
    {
      for (i = 0; i < FFT_length / 2; i++)
      {
        iFFT_buffer[FFT_length + 2 * i + 0] = fixed_gain * iFFT_buffer[FFT_length + 2 * i + 0];
        iFFT_buffer[FFT_length + 2 * i + 1] = fixed_gain * iFFT_buffer[FFT_length + 2 * i + 1];
      }
      return;
    }
  
    for (i = 0; i < FFT_length / 2; i++)
    {
      if (++out_index >= ring_buffsize)
        out_index -= ring_buffsize;
      if (++in_index >= ring_buffsize)
        in_index -= ring_buffsize;
  
      out_sample[0] = ring[2 * out_index + 0];
      out_sample[1] = ring[2 * out_index + 1];
      abs_out_sample = abs_ring[out_index];
      ring[2 * in_index + 0] = iFFT_buffer[FFT_length + 2 * i + 0];
      ring[2 * in_index + 1] = iFFT_buffer[FFT_length + 2 * i + 1];
      if (pmode == 0) // MAGNITUDE CALCULATION
        abs_ring[in_index] = max(fabs(ring[2 * in_index + 0]), fabs(ring[2 * in_index + 1]));
      else
        abs_ring[in_index] = sqrtf(ring[2 * in_index + 0] * ring[2 * in_index + 0] + ring[2 * in_index + 1] * ring[2 * in_index + 1]);

      fast_backaverage = fast_backmult * abs_out_sample + onemfast_backmult * fast_backaverage;
      hang_backaverage = hang_backmult * abs_out_sample + onemhang_backmult * hang_backaverage;

      if ((abs_out_sample >= ring_max) && (abs_out_sample > 0.0))
      {
        ring_max = 0.0;
        k = out_index;
        for (j = 0; j < attack_buffsize; j++)
        {
          if (++k == ring_buffsize)
            k = 0;
          if (abs_ring[k] > ring_max)
            ring_max = abs_ring[k];
        }
      }
      if (abs_ring[in_index] > ring_max)
        ring_max = abs_ring[in_index];

      if (hang_counter > 0)
        --hang_counter;
//      Serial.println(ring_max);
//      Serial.println(volts);
      
      switch (state)
      {
      case 0:
        {
          if (ring_max >= volts)
          {
            volts += (ring_max - volts) * attack_mult;
          }
          else
          {
            if (volts > pop_ratio * fast_backaverage)
            {
              state = 1;
              volts += (ring_max - volts) * fast_decay_mult;
            }
            else
            {
              if (hang_enable && (hang_backaverage > hang_level))
              {
                state = 2;
                hang_counter = (int)(hangtime * SR[SAMPLE_RATE].rate / DF);
                decay_type = 1;
              }
              else
              {
                state = 3;
                volts += (ring_max - volts) * decay_mult;
                decay_type = 0;
              }
            }
          }
          break;
        }
      case 1:
        {
          if (ring_max >= volts)
          {
            state = 0;
            volts += (ring_max - volts) * attack_mult;
          }
          else
          {
            if (volts > save_volts)
            {
              volts += (ring_max - volts) * fast_decay_mult;
            }
            else
            {
              if (hang_counter > 0)
              {
                state = 2;
              }
              else
              {
                if (decay_type == 0)
                {
                  state = 3;
                  volts += (ring_max - volts) * decay_mult;
                }
                else
                {
                  state = 4;
                  volts += (ring_max - volts) * hang_decay_mult;
                }
              }
            }
          }
          break;
        }
      case 2:
        {
          if (ring_max >= volts)
          {
            state = 0;
            save_volts = volts;
            volts += (ring_max - volts) * attack_mult;
          }
          else
          {
            if (hang_counter == 0)
            {
              state = 4;
              volts += (ring_max - volts) * hang_decay_mult;
            }
          }
          break;
        }
      case 3:
        {
          if (ring_max >= volts)
          {
            state = 0;
            save_volts = volts;
            volts += (ring_max - volts) * attack_mult;
          }
          else
          {
            volts += (ring_max - volts) * decay_mult;
          }
          break;
        }
      case 4:
        {
          if (ring_max >= volts)
          {
            state = 0;
            save_volts = volts;
            volts += (ring_max - volts) * attack_mult;
          }
          else
          {
            volts += (ring_max - volts) * hang_decay_mult;
          }
          break;
        }
      }
      if (volts < min_volts) volts = min_volts;
//      Serial.println(volts * inv_out_target);
      mult = (out_target - slope_constant * min (0.0, log10f(inv_max_input * volts))) / volts;
//    Serial.println(mult * 1000);
//      Serial.println(volts * 1000);
      iFFT_buffer[FFT_length + 2 * i + 0] = out_sample[0] * mult;
      iFFT_buffer[FFT_length + 2 * i + 1] = out_sample[1] * mult;
    }
  }

void filter_bandwidth() 
{
//    LP_F_help = (int)((float)LP_F_help * 0.3 + 0.7 * (float)LP_Fpass_old);
//    LP_F_help = (int)(LP_F_help/100 * 100);
//    LP_F_help += 100;
//    if(LP_F_help != LP_Fpass_old)
//    { //audio_flag_counter = 1000;
    AudioNoInterrupts();
    calc_FIR_coeffs (FIR_Coef, 513, (float32_t)LP_F_help, LP_Astop, 0, 0.0, SR[SAMPLE_RATE].rate / DF);
    init_filter_mask();

    // also adjust IIR AM filter
    set_IIR_coeffs ((float32_t)LP_F_help, 1.3, (float32_t)SR[SAMPLE_RATE].rate / DF, 0); // 1st stage
//    set_IIR_coeffs ((float32_t)LP_F_help, 1.3, (float32_t)SR[SAMPLE_RATE].rate / DF, 0); // 1st stage
    for(i = 0; i < 5; i++)
    {
        biquad_lowpass1_coeffs[i] = coefficient_set[i];
    }
    
    show_bandwidth (band[bands].mode, LP_F_help);
//    }      
    LP_Fpass_old = LP_F_help; 
//    delay(1);
    AudioInterrupts();
} // end filter_bandwidth

void calc_FIR_coeffs (float * coeffs, int numCoeffs, float32_t fc, float32_t Astop, int type, float dfc, float Fsamprate)
    // pointer to coefficients variable, no. of coefficients to calculate, frequency where it happens, stopband attenuation in dB, 
    // filter type, half-filter bandwidth (only for bandpass and notch) 
 {  // modified by WMXZ and DD4WH after
    // Wheatley, M. (2011): CuteSDR Technical Manual. www.metronix.com, pages 118 - 120, FIR with Kaiser-Bessel Window
    // assess required number of coefficients by
    //     numCoeffs = (Astop - 8.0) / (2.285 * TPI * normFtrans);
    // selecting high-pass, numCoeffs is forced to an even number for better frequency response

     int ii,jj;
     float32_t Beta;
     float32_t izb;
     float fcf = fc;
     int nc = numCoeffs;
     fc = fc / Fsamprate;
     dfc = dfc / Fsamprate;
     // calculate Kaiser-Bessel window shape factor beta from stop-band attenuation
     if (Astop < 20.96)
       Beta = 0.0;
     else if (Astop >= 50.0)
       Beta = 0.1102 * (Astop - 8.71);
     else
       Beta = 0.5842 * powf((Astop - 20.96), 0.4) + 0.07886 * (Astop - 20.96);

     izb = Izero (Beta);
     if(type == 0) // low pass filter
//     {  fcf = fc;
     {  fcf = fc * 2.0;
      nc =  numCoeffs;
     }
     else if(type == 1) // high-pass filter
     {  fcf = -fc;
      nc =  2*(numCoeffs/2);
     }
     else if ((type == 2) || (type==3)) // band-pass filter
     {
       fcf = dfc;
       nc =  2*(numCoeffs/2); // maybe not needed
     }
     else if (type==4)  // Hilbert transform
   {
         nc =  2*(numCoeffs/2);
       // clear coefficients
       for(ii=0; ii< 2*(nc-1); ii++) coeffs[ii]=0;
       // set real delay
       coeffs[nc]=1;

       // set imaginary Hilbert coefficients
       for(ii=1; ii< (nc+1); ii+=2)
       {
         if(2*ii==nc) continue;
       float x =(float)(2*ii - nc)/(float)nc;
       float w = Izero(Beta*sqrtf(1.0f - x*x))/izb; // Kaiser window
       coeffs[2*ii+1] = 1.0f/(PIH*(float)(ii-nc/2)) * w ;
       }
       return;
   }

     for(ii= - nc, jj=0; ii< nc; ii+=2,jj++)
     {
       float x =(float)ii/(float)nc;
       float w = Izero(Beta*sqrtf(1.0f - x*x))/izb; // Kaiser window
       coeffs[jj] = fcf * m_sinc(ii,fcf) * w;
     }

     if(type==1)
     {
       coeffs[nc/2] += 1;
     }
     else if (type==2)
     {
         for(jj=0; jj< nc+1; jj++) coeffs[jj] *= 2.0f*cosf(PIH*(2*jj-nc)*fc);
     }
     else if (type==3)
     {
         for(jj=0; jj< nc+1; jj++) coeffs[jj] *= -2.0f*cosf(PIH*(2*jj-nc)*fc);
       coeffs[nc/2] += 1;
     }

} // END calc_FIR_coeffs

float m_sinc(int m, float fc)
{  // fc is f_cut/(Fsamp/2)
  // m is between -M and M step 2
  //
  float x = m*PIH;
  if(m == 0)
    return 1.0f;
  else
    return sinf(x*fc)/(fc*x);
}
/*
void calc_FIR_lowpass_coeffs (float32_t Scale, float32_t Astop, float32_t Fpass, float32_t Fstop, float32_t Fsamprate) 
{ // Wheatley, M. (2011): CuteSDR Technical Manual. www.metronix.com, pages 118 - 120, FIR with Kaiser-Bessel Window
    int n;
    float32_t Beta;
    float32_t izb;
    // normalized F parameters
    float32_t normFpass = Fpass / Fsamprate;
    float32_t normFstop = Fstop / Fsamprate;
    float32_t normFcut = (normFstop + normFpass) / 2.0; // lowpass filter 6dB cutoff
    // calculate Kaiser-Bessel window shape factor beta from stopband attenuation
    if (Astop < 20.96)
    {
      Beta = 0.0;    
    }
    else if (Astop >= 50.0)
    {
      Beta = 0.1102 * (Astop - 8.71);
    }
    else
    {
      Beta = 0.5842 * powf((Astop - 20.96), 0.4) + 0.07886 * (Astop - 20.96);
    }
    // estimate number of taps
    m_NumTaps = (int32_t)((Astop - 8.0) / (2.285 * K_2PI * (normFstop - normFpass)) + 1.0);
    if (m_NumTaps > MAX_NUMCOEF) 
    { 
      m_NumTaps = MAX_NUMCOEF;
    }
    if (m_NumTaps < 3)
    {
      m_NumTaps = 3;
    }
    float32_t fCenter = 0.5 * (float32_t) (m_NumTaps - 1);
    izb = Izero (Beta);
    for (n = 0; n < m_NumTaps; n++)
    {
      float32_t x = (float32_t) n - fCenter;
      float32_t c;
      // create ideal Sinc() LP filter with normFcut
        if ( abs((float)n - fCenter) < 0.01)
      { // deal with odd size filter singularity where sin(0) / 0 == 1
        c = 2.0 * normFcut;
//        Serial.println("Hello, here at odd FIR filter size");
      }
      else
      {
        c = (float32_t) sinf(K_2PI * x * normFcut) / (K_PI * x);
//        c = (float32_t) sinf(K_PI * x * normFcut) / (K_PI * x);
      }
      x = ((float32_t) n - ((float32_t) m_NumTaps - 1.0) / 2.0) / (((float32_t) m_NumTaps - 1.0) / 2.0);
      FIR_Coef[n] = Scale * c * Izero (Beta * sqrt(1 - (x * x) )) / izb;
    }
} // END calc_lowpass_coeffs
*/
float32_t Izero (float32_t x) 
{
    float32_t x2 = x / 2.0;
    float32_t summe = 1.0;
    float32_t ds = 1.0;
    float32_t di = 1.0;
    float32_t errorlimit = 1e-9;
    float32_t tmp;
    do
    {
        tmp = x2 / di;
        tmp *= tmp;
        ds *= tmp;
        summe += ds;
        di += 1.0; 
    }   while (ds >= errorlimit * summe);
    return (summe);
}  // END Izero

// set samplerate code by Frank Boesing 
void setI2SFreq(int freq) {
  typedef struct {
    uint8_t mult;
    uint16_t div;
  } tmclk;

  const int numfreqs = 15;
  const int samplefreqs[numfreqs] = { 8000, 11025, 16000, 22050, 32000, 44100, (int)44117.64706 , 48000, 88200, (int)44117.64706 * 2, 96000, 100000, 176400, (int)44117.64706 * 4, 192000};

#if (F_PLL==16000000)
  const tmclk clkArr[numfreqs] = {{16, 125}, {148, 839}, {32, 125}, {145, 411}, {64, 125}, {151, 214}, {12, 17}, {96, 125}, {151, 107}, {24, 17}, {192, 125}, {1,1}, {127, 45}, {48, 17}, {255, 83} };
#elif (F_PLL==72000000)
  const tmclk clkArr[numfreqs] = {{32, 1125}, {49, 1250}, {64, 1125}, {49, 625}, {128, 1125}, {98, 625}, {8, 51}, {64, 375}, {196, 625}, {16, 51}, {128, 375}, {1,1}, {249, 397}, {32, 51}, {185, 271} };
#elif (F_PLL==96000000)
  const tmclk clkArr[numfreqs] = {{8, 375}, {73, 2483}, {16, 375}, {147, 2500}, {32, 375}, {147, 1250}, {2, 17}, {16, 125}, {147, 625}, {4, 17}, {32, 125}, {1,1}, {151, 321}, {8, 17}, {64, 125} };
#elif (F_PLL==120000000)
  const tmclk clkArr[numfreqs] = {{32, 1875}, {89, 3784}, {64, 1875}, {147, 3125}, {128, 1875}, {205, 2179}, {8, 85}, {64, 625}, {89, 473}, {16, 85}, {128, 625}, {1,1}, {178, 473}, {32, 85}, {145, 354} };
#elif (F_PLL==144000000)
  const tmclk clkArr[numfreqs] = {{16, 1125}, {49, 2500}, {32, 1125}, {49, 1250}, {64, 1125}, {49, 625}, {4, 51}, {32, 375}, {98, 625}, {8, 51}, {64, 375}, {1,1}, {196, 625}, {16, 51}, {128, 375} };
#elif (F_PLL==168000000)
  const tmclk clkArr[numfreqs] = {{32, 2625}, {21, 1250}, {64, 2625}, {21, 625}, {128, 2625}, {42, 625}, {8, 119}, {64, 875}, {84, 625}, {16, 119}, {128, 875}, {1,1}, {168, 625}, {32, 119}, {189, 646} };
#elif (F_PLL==180000000)
  const tmclk clkArr[numfreqs] = {{46, 4043}, {49, 3125}, {73, 3208}, {98, 3125}, {183, 4021}, {196, 3125}, {16, 255}, {128, 1875}, {107, 853}, {32, 255}, {219, 1604}, {224,1575}, {214, 853}, {64, 255}, {219, 802} };
#elif (F_PLL==192000000)
  const tmclk clkArr[numfreqs] = {{4, 375}, {37, 2517}, {8, 375}, {73, 2483}, {16, 375}, {147, 2500}, {1, 17}, {8, 125}, {147, 1250}, {2, 17}, {16, 125}, {1,1}, {147, 625}, {4, 17}, {32, 125} };
#elif (F_PLL==216000000)
  const tmclk clkArr[numfreqs] = {{32, 3375}, {49, 3750}, {64, 3375}, {49, 1875}, {128, 3375}, {98, 1875}, {8, 153}, {64, 1125}, {196, 1875}, {16, 153}, {128, 1125}, {1,1}, {226, 1081}, {32, 153}, {147, 646} };
#elif (F_PLL==240000000)
  const tmclk clkArr[numfreqs] = {{16, 1875}, {29, 2466}, {32, 1875}, {89, 3784}, {64, 1875}, {147, 3125}, {4, 85}, {32, 625}, {205, 2179}, {8, 85}, {64, 625}, {1,1}, {89, 473}, {16, 85}, {128, 625} };
#endif

  for (int f = 0; f < numfreqs; f++) {
    if ( freq == samplefreqs[f] ) {
      while (I2S0_MCR & I2S_MCR_DUF) ;
      I2S0_MDR = I2S_MDR_FRACT((clkArr[f].mult - 1)) | I2S_MDR_DIVIDE((clkArr[f].div - 1));
      return;
    }
  }
} // end set_I2S

void init_filter_mask() 
{
   /****************************************************************************************
 *  Calculate the FFT of the FIR filter coefficients once to produce the FIR filter mask
 ****************************************************************************************/
// the FIR has exactly m_NumTaps and a maximum of (FFT_length / 2) + 1 taps = coefficients, so we have to add (FFT_length / 2) -1 zeros before the FFT
// in order to produce a FFT_length point input buffer for the FFT 
    // copy coefficients into real values of first part of buffer, rest is zero

    for (i = 0; i < (FFT_length / 2) + 1; i++)
    {
        FIR_filter_mask[i * 2] = FIR_Coef [i];
        FIR_filter_mask[i * 2 + 1] = 0.0; 
    }
    
    for (i = FFT_length; i < FFT_length * 2; i++)
    {
        FIR_filter_mask[i] = 0.0;
    }

    
// FFT of FIR_filter_mask
// perform FFT (in-place), needs only to be done once (or every time the filter coeffs change)
    arm_cfft_f32(maskS, FIR_filter_mask, 0, 1);    
    
///////////////////////////////////////////////////////////////77
// PASS-THRU only for TESTING
/////////////////////////////////////////////////////////////77
/*
// hmmm, unclear, whether [1,1] or [1,0] or [0,1] are pass-thru filter-mask-multipliers??
// empirically, [1,0] sounds most natural = pass-thru
  for(i = 0; i < FFT_length * 2; i+=2)
  {
        FIR_filter_mask [i] = 1.0; // real
        FIR_filter_mask [i + 1] = 0.0; // imaginary
  }
*/
} // end init_filter_mask


void Zoom_FFT_prep()
{ // take value of spectrum_zoom and initialize IIR lowpass and FIR decimation filters for the right values

/*    switch (spectrum_zoom) 
    {
        case SPECTRUM_ZOOM_1:
            return;
        break;
        case SPECTRUM_ZOOM_2:
            // M = 2
            Fir_Zoom_FFT_Decimate_I.M = 2;
            Fir_Zoom_FFT_Decimate_Q.M = 2;
        break;
        case SPECTRUM_ZOOM_4:
        // at sample_rate / 4
        break;
        case SPECTRUM_ZOOM_8:
        // at sample_rate / 8
        break;
        case SPECTRUM_ZOOM_16:
            Fir_Zoom_FFT_Decimate_I.M = 2;
            Fir_Zoom_FFT_Decimate_Q.M = 2;
        break;
        case SPECTRUM_ZOOM_32:
            Fir_Zoom_FFT_Decimate_I.M = 4;
            Fir_Zoom_FFT_Decimate_Q.M = 4;
        break;
        case SPECTRUM_ZOOM_64:
            Fir_Zoom_FFT_Decimate_I.M = 8;
            Fir_Zoom_FFT_Decimate_Q.M = 8;
        break;
    }
*/

    Fir_Zoom_FFT_Decimate_I.M = (1<<spectrum_zoom);
    Fir_Zoom_FFT_Decimate_Q.M = (1<<spectrum_zoom);

    float32_t Fstop_Zoom = 0.5 * (float32_t) SR[SAMPLE_RATE].rate / (1<<spectrum_zoom); 
//    Serial.print("Fstop =  "); Serial.println(Fstop_Zoom);
    calc_FIR_coeffs (Fir_Zoom_FFT_Decimate_coeffs, 4, Fstop_Zoom, 60, 0, 0.0, (float32_t)SR[SAMPLE_RATE].rate);
    IIR_biquad_Zoom_FFT_I.pCoeffs = mag_coeffs[spectrum_zoom];
    IIR_biquad_Zoom_FFT_Q.pCoeffs = mag_coeffs[spectrum_zoom];
}


void Zoom_FFT_exe (uint32_t blockSize)
{
  /*********************************************************************************************
   * see Lyons 2011 for a general description of the ZOOM FFT
   *********************************************************************************************/
      float32_t x_buffer[4096];
      float32_t y_buffer[4096];
      int sample_no = 256;
      if(spectrum_zoom == SPECTRUM_ZOOM_32) 
      {
          sample_no = 128;
 //         blockSize = blockSize / 2;
      }
      else 
      if(spectrum_zoom == SPECTRUM_ZOOM_64)
      {
          sample_no = 64;
 //         blockSize = blockSize / 4;
      }

//      if(spectrum_zoom == SPECTRUM_ZOOM_2 || spectrum_zoom == SPECTRUM_ZOOM_16 || spectrum_zoom == SPECTRUM_ZOOM_32 || spectrum_zoom == SPECTRUM_ZOOM_64) 
      if (spectrum_zoom != SPECTRUM_ZOOM_1)
      {
           // lowpass filter
           arm_biquad_cascade_df1_f32 (&IIR_biquad_Zoom_FFT_I, float_buffer_L,x_buffer, blockSize);
           arm_biquad_cascade_df1_f32 (&IIR_biquad_Zoom_FFT_Q, float_buffer_R,y_buffer, blockSize);

            // decimation
            arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_I, x_buffer, x_buffer, blockSize);
            arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_Q, y_buffer, y_buffer, blockSize);
//            arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_I, float_buffer_L, x_buffer, blockSize);
//            arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_Q, float_buffer_R, y_buffer, blockSize);
      // put samples into buffer and apply windowing
      for(i = 0; i < sample_no; i++) 
      { // interleave real and imaginary input values [real, imag, real, imag . . .]
        // apply Hann window 
        // Nuttall window
          buffer_spec_FFT[zoom_sample_ptr] = (1<<spectrum_zoom) * (0.355768 - (0.487396*cosf((TPI*(float32_t)i)/(float32_t)512 - 1)) +
                   (0.144232*cosf((FOURPI*(float32_t)i)/(float32_t)512 - 1)) - (0.012604*cosf((SIXPI*(float32_t)i)/(float32_t)512 - 1))) * x_buffer[i];
          zoom_sample_ptr++;
          buffer_spec_FFT[zoom_sample_ptr] = (1<<spectrum_zoom) *  (0.355768 - (0.487396*cosf((TPI*(float32_t)i)/(float32_t)512 - 1)) +
                   (0.144232*cosf((FOURPI*(float32_t)i)/(float32_t)512 - 1)) - (0.012604*cosf((SIXPI*(float32_t)i)/(float32_t)512 - 1))) * y_buffer[i];
          zoom_sample_ptr++;
     }
     }
      else
      {
      
      // put samples into buffer and apply windowing
      for(i = 0; i < sample_no; i++) 
      { // interleave real and imaginary input values [real, imag, real, imag . . .]
        // apply Hann window 
        // Nuttall window
          buffer_spec_FFT[zoom_sample_ptr] = (0.355768 - (0.487396*cosf((TPI*(float32_t)i)/(float32_t)512 - 1)) +
                   (0.144232*cosf((FOURPI*(float32_t)i)/(float32_t)512 - 1)) - (0.012604*cosf((SIXPI*(float32_t)i)/(float32_t)512 - 1))) * float_buffer_L[i];
          zoom_sample_ptr++;
          buffer_spec_FFT[zoom_sample_ptr] = (0.355768 - (0.487396*cosf((TPI*(float32_t)i)/(float32_t)512 - 1)) +
                   (0.144232*cosf((FOURPI*(float32_t)i)/(float32_t)512 - 1)) - (0.012604*cosf((SIXPI*(float32_t)i)/(float32_t)512 - 1))) * float_buffer_R[i];
          zoom_sample_ptr++;
     }
      }
     if(zoom_sample_ptr >= 255)
     {
          zoom_display = 1;
          zoom_sample_ptr = 0;
//***************
      float32_t help = 0.0;
      // adjust lowpass filter coefficient, so that
      // "spectrum display smoothness" is the same across the different sample rates
      float32_t LPFcoeff = LPF_spectrum * (AUDIO_SAMPLE_RATE_EXACT / SR[SAMPLE_RATE].rate);
      if(LPFcoeff > 1.0) LPFcoeff = 1.0;

      for(i = 0; i < 256; i++)
      {
          pixelold[i] = pixelnew[i];
      }
      // perform complex FFT
      // calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
      arm_cfft_f32(spec_FFT, buffer_spec_FFT, 0, 1);
       // calculate mag = I*I + Q*Q, 
      // and simultaneously put them into the right order
      for(i = 0; i < 128; i++)
      {
          FFT_spec[i + 128] = sqrtf(buffer_spec_FFT[i * 2] * buffer_spec_FFT[i * 2] + buffer_spec_FFT[i * 2 + 1] * buffer_spec_FFT[i * 2 + 1]);
          FFT_spec[i] = sqrtf(buffer_spec_FFT[(i + 128) * 2] * buffer_spec_FFT[(i + 128)  * 2] + buffer_spec_FFT[(i + 128)  * 2 + 1] * buffer_spec_FFT[(i + 128)  * 2 + 1]);
      }
     // apply low pass filter and scale the magnitude values and convert to int for spectrum display
      for(int16_t x = 0; x < 256; x++)
      {
           help = LPFcoeff * FFT_spec[x] + (1.0 - LPFcoeff) * FFT_spec_old[x];
           FFT_spec_old[x] = help; 
              // insert display offset, AGC etc. here
           help = 10.0 * log10f(help + 1.0); 
           pixelnew[x] = (int16_t) (help * spectrum_display_scale);
      }
     }
}


void calc_256_magn() 
{
      float32_t help = 0.0;
      // adjust lowpass filter coefficient, so that
      // "spectrum display smoothness" is the same across the different sample rates
      float32_t LPFcoeff = LPF_spectrum * (AUDIO_SAMPLE_RATE_EXACT / SR[SAMPLE_RATE].rate);
      if(LPFcoeff > 1.0) LPFcoeff = 1.0;

      for(i = 0; i < 256; i++)
      {
          pixelold[i] = pixelnew[i];
      }

      // put samples into buffer and apply windowing
      for(i = 0; i < 256; i++) 
      { // interleave real and imaginary input values [real, imag, real, imag . . .]
        // apply Hann window 
        // cosf is much much faster than arm_cos_f32 !
//          buffer_spec_FFT[i * 2] = 0.5 * (float32_t)((1 - (cosf(PI*2 * (float32_t)i / (float32_t)(512-1)))) * float_buffer_L[i]);
//          buffer_spec_FFT[i * 2 + 1] = 0.5 * (float32_t)((1 - (cosf(PI*2 * (float32_t)i / (float32_t)(512-1)))) * float_buffer_R[i]);        
        // Nuttall window
          buffer_spec_FFT[i * 2] = (0.355768 - (0.487396*cosf((TPI*(float32_t)i)/(float32_t)512 - 1)) +
                   (0.144232*cosf((FOURPI*(float32_t)i)/(float32_t)512 - 1)) - (0.012604*cosf((SIXPI*(float32_t)i)/(float32_t)512 - 1))) * float_buffer_L[i];
          buffer_spec_FFT[i * 2 + 1] = (0.355768 - (0.487396*cosf((TPI*(float32_t)i)/(float32_t)512 - 1)) +
                   (0.144232*cosf((FOURPI*(float32_t)i)/(float32_t)512 - 1)) - (0.012604*cosf((SIXPI*(float32_t)i)/(float32_t)512 - 1))) * float_buffer_R[i];

      }
      
        //  Hanning 1.36
        //sc.FFT_Windat[i] = 0.5 * (float32_t)((1 - (arm_cos_f32(PI*2 * (float32_t)i / (float32_t)(FFT_IQ_BUFF_LEN2-1)))) * sc.FFT_Samples[i]);
        // Hamming 1.22
        //sc.FFT_Windat[i] = (float32_t)((0.53836 - (0.46164 * arm_cos_f32(PI*2 * (float32_t)i / (float32_t)(FFT_IQ_BUFF_LEN2-1)))) * sc.FFT_Samples[i]);
        // Blackman 1.75
        // float32_t help_sample = (0.42659 - (0.49656*arm_cos_f32((2.0*PI*(float32_t)i)/(buff_len-1.0))) + (0.076849*arm_cos_f32((4.0*PI*(float32_t)i)/(buff_len-1.0)))) * sc.FFT_Samples[i];
        // Nuttall
//             s = (0.355768 - (0.487396*arm_cos_f32((2*PI*(float32_t)i)/(float32_t)FFT_IQ_BUFF_LEN-1)) + (0.144232*arm_cos_f32((4*PI*(float32_t)i)/(float32_t)FFT_IQ_BUFF_LEN-1)) - (0.012604*arm_cos_f32((6*PI*(float32_t)i)/(float32_t)FFT_IQ_BUFF_LEN-1))) * sd.FFT_Samples[i];
     
      // perform complex FFT
      // calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
      arm_cfft_f32(spec_FFT, buffer_spec_FFT, 0, 1);
      // calculate magnitudes and put into FFT_spec
      // we do not need to calculate magnitudes with square roots, it would seem to be sufficient to
      // calculate mag = I*I + Q*Q, because we are doing a log10-transformation later anyway
      // and simultaneously put them into the right order
      // 38.50%, saves 0.05% of processor power and 1kbyte RAM ;-)
      for(i = 0; i < 128; i++)
      {
          FFT_spec[i + 128] = sqrtf(buffer_spec_FFT[i * 2] * buffer_spec_FFT[i * 2] + buffer_spec_FFT[i * 2 + 1] * buffer_spec_FFT[i * 2 + 1]);
          FFT_spec[i] = sqrtf(buffer_spec_FFT[(i + 128) * 2] * buffer_spec_FFT[(i + 128)  * 2] + buffer_spec_FFT[(i + 128)  * 2 + 1] * buffer_spec_FFT[(i + 128)  * 2 + 1]);
      }

            
      // apply low pass filter and scale the magnitude values and convert to int for spectrum display
      for(int16_t x = 0; x < 256; x++)
      {
           help = LPFcoeff * FFT_spec[x] + (1.0 - LPFcoeff) * FFT_spec_old[x];
           FFT_spec_old[x] = help; 
              // insert display offset, AGC etc. here
           help = 10.0 * log10f(help + 1.0); 
           pixelnew[x] = (int16_t) (help * spectrum_display_scale);
      }
} // end calc_256_magn

/*
 // leave this here for REFERENCE !
void calc_spectrum_mags(uint32_t zoom, float32_t LPFcoeff) {
      float32_t help, help2;
      LPFcoeff = LPFcoeff * (AUDIO_SAMPLE_RATE_EXACT / SR[SAMPLE_RATE].rate);
      if(LPFcoeff > 1.0) LPFcoeff = 1.0;
      for(i = 0; i < 256; i++)
      {
          pixelold[i] = pixelnew[i];
      }
      // this saves absolutely NO processor power at 96ksps in comparison to arm_cmplx_mag
//      for(i = 0; i < FFT_length; i++)
//      {
//          FFT_magn[i] = alpha_beta_mag(FFT_buffer[(i * 2)], FFT_buffer[(i*2)+1]);
//      }
    
      // this is slower than arm_cmplx_mag_f32 (43.3% vs. 45.5% MCU usage)
//      for(i = 0; i < FFT_length; i++)
//      {
//          FFT_magn[i] = sqrtf(FFT_buffer[(i*2)] * FFT_buffer[(i*2)] + FFT_buffer[(i*2) + 1] * FFT_buffer[(i*2) + 1]);  
//      } 

      arm_cmplx_mag_f32(FFT_buffer, FFT_magn, FFT_length);  // calculates sqrt(I*I + Q*Q) for each frequency bin of the FFT
      
      ////////////////////////////////////////////////////////////////////////
      // now calculate average of the bin values according to spectrum zoom
      // this assumes FFT_shift of 1024, ie. 5515Hz with an FFT_length of 4096
      ////////////////////////////////////////////////////////////////////////
      
      // TODO: adapt to different FFT lengths
      for(i = 0; i < FFT_length / 2; i+=zoom)
      {
              arm_mean_f32(&FFT_magn[i], zoom, &FFT_spec[i / zoom + 128]);
      }
      for(i = FFT_length / 2; i < FFT_length; i+=zoom)
      {
              arm_mean_f32(&FFT_magn[i], zoom, &FFT_spec[i / zoom - 128]);
      }

      // low pass filtering of the spectrum pixels to smooth/slow down spectrum in the time domain
      // ynew = LPCoeff * x + (1-LPCoeff) * yprevious; 
      for(int16_t x = 0; x < 256; x++)
      {
           help = LPFcoeff * FFT_spec[x] + (1.0 - LPFcoeff) * FFT_spec_old[x];
           FFT_spec_old[x] = help; 
              // insert display offset, AGC etc. here
              // a single log10 here needs another 22% of processor use on a Teensy 3.6 (96k sample rate)!
//           help = 50.0 * log10(help+1.0); 
             // sqrtf is a little bit faster than arm_sqrt_f32 ! 
             help2 = sqrtf(help); 
//           arm_sqrt_f32(help, &help2);          
           pixelnew[x] = (int16_t) (help2 * 10);
      }
      
} // end calc_spectrum_mags
*/

void show_spectrum() 
{
      int16_t y_old, y_new, y1_new, y1_old;
      int16_t y1_old_minus = 0;
      int16_t y1_new_minus = 0;

      // Draw spectrum display
      for (int16_t x = 0; x < 254; x++) 
      {
                if ((x > 1) && (x < 255)) 
                // moving window - weighted average of 5 points of the spectrum to smooth spectrum in the frequency domain
                // weights:  x: 50% , x-1/x+1: 36%, x+2/x-2: 14% 
                {    
                  y_new = pixelnew[x] * 0.5 + pixelnew[x - 1] * 0.18 + pixelnew[x + 1] * 0.18 + pixelnew[x - 2] * 0.07 + pixelnew[x + 2] * 0.07;
                  y_old = pixelold[x] * 0.5 + pixelold[x - 1] * 0.18 + pixelold[x + 1] * 0.18 + pixelold[x - 2] * 0.07 + pixelold[x + 2] * 0.07;
                }
                else 
                {
                  y_new = pixelnew[x];
                  y_old = pixelold[x];
                 }
               if(y_old > (spectrum_height - 7))
               {
                  y_old = (spectrum_height - 7);
               }

               if(y_new > (spectrum_height - 7))
               {
                  y_new = (spectrum_height - 7);
               }
               y1_old  = (spectrum_y + spectrum_height - 1) - y_old;
               y1_new  = (spectrum_y + spectrum_height - 1) - y_new; 

               if(x == 0)
               {
                   y1_old_minus = y1_old;
                   y1_new_minus = y1_new;
               }
               if(x == 254)
               {
                   y1_old_minus = y1_old;
                   y1_new_minus = y1_new;
               }

//             if(x != spectrum_pos_centre_f) 
             {
          // DELETE OLD LINE/POINT
              if(y1_old - y1_old_minus > 1) 
               { // plot line upwards
                tft.drawFastVLine(x + spectrum_x, y1_old_minus + 1, y1_old - y1_old_minus,ILI9341_BLACK);
               }
              else if (y1_old - y1_old_minus < -1) 
               { // plot line downwards
                tft.drawFastVLine(x + spectrum_x, y1_old, y1_old_minus - y1_old,ILI9341_BLACK);
               }
              else
               {
                  tft.drawPixel(x + spectrum_x, y1_old, ILI9341_BLACK); // delete old pixel
               }

          // DRAW NEW LINE/POINT
              if(y1_new - y1_new_minus > 1) 
               { // plot line upwards
                tft.drawFastVLine(x + spectrum_x, y1_new_minus + 1, y1_new - y1_new_minus,ILI9341_WHITE);
               }
              else if (y1_new - y1_new_minus < -1) 
               { // plot line downwards
                tft.drawFastVLine(x + spectrum_x, y1_new, y1_new_minus - y1_new,ILI9341_WHITE);
               }
               else
               {
                  tft.drawPixel(x + spectrum_x, y1_new, ILI9341_WHITE); // write new pixel
               }

            y1_new_minus = y1_new;
            y1_old_minus = y1_old;

        }
      } // end for loop

} // END show_spectrum


void show_bandwidth (int M, uint32_t filter_f)  
{  
//  AudioNoInterrupts();
  // M = demod_mode, FU & FL upper & lower frequency
  // this routine prints the frequency bars under the spectrum display
  // and displays the bandwidth bar indicating demodulation bandwidth
   if (spectrum_zoom != SPECTRUM_ZOOM_1) spectrum_pos_centre_f = 128;
    else spectrum_pos_centre_f = 64;
   float32_t pixel_per_khz = (1<<spectrum_zoom) * 256.0 / SR[SAMPLE_RATE].rate * 1000.0;
   float32_t leU = filter_f / 1000.0 * pixel_per_khz;
   float32_t leL = filter_f / 1000.0 * pixel_per_khz;
   char string[4];
   int pos_y = spectrum_y + spectrum_height + 2; 
   tft.drawFastHLine(spectrum_x - 1, pos_y, 258, ILI9341_BLACK); // erase old indicator
   tft.drawFastHLine(spectrum_x - 1, pos_y + 1, 258, ILI9341_BLACK); // erase old indicator 
   tft.drawFastHLine(spectrum_x - 1, pos_y + 2, 258, ILI9341_BLACK); // erase old indicator
   tft.drawFastHLine(spectrum_x - 1, pos_y + 3, 258, ILI9341_BLACK); // erase old indicator

    switch(M) {
//      case DEMOD_AM1:
      case DEMOD_AM2:
/*      case DEMOD_AM3:
      case DEMOD_AM_AE1:
      case DEMOD_AM_AE2:
      case DEMOD_AM_AE3:
      case DEMOD_AM_ME1: */
      case DEMOD_AM_ME2:
//      case DEMOD_AM_ME3:
      case DEMOD_SAM:
      case DEMOD_SAM_STEREO:
          leU = filter_f / 1000.0 * pixel_per_khz;
          leL = filter_f / 1000.0 * pixel_per_khz;
          break;
      case DEMOD_LSB:
      case DEMOD_STEREO_LSB:
      case DEMOD_SAM_LSB:
          leU = 0.0;
          leL = filter_f / 1000.0 * pixel_per_khz;
          break;
      case DEMOD_USB:
      case DEMOD_SAM_USB:
      case DEMOD_STEREO_USB:
          leU = filter_f / 1000.0 * pixel_per_khz;
          leL = 0.0;
    }
      if(leL > spectrum_pos_centre_f + 1) leL = spectrum_pos_centre_f + 2;
      if((leU + spectrum_pos_centre_f) > 255) leU = 256 - spectrum_pos_centre_f;
     
      // draw upper sideband indicator
      tft.drawFastHLine(spectrum_pos_centre_f + spectrum_x + 1, pos_y, leU, ILI9341_YELLOW);
      tft.drawFastHLine(spectrum_pos_centre_f + spectrum_x + 1, pos_y + 1, leU, ILI9341_YELLOW);
      tft.drawFastHLine(spectrum_pos_centre_f + spectrum_x + 1, pos_y + 2, leU, ILI9341_YELLOW);
      tft.drawFastHLine(spectrum_pos_centre_f + spectrum_x + 1, pos_y + 3, leU, ILI9341_YELLOW);
      // draw lower sideband indicator   
      tft.drawFastHLine(spectrum_pos_centre_f  + spectrum_x - leL + 1, pos_y, leL + 1, ILI9341_YELLOW);
      tft.drawFastHLine(spectrum_pos_centre_f  + spectrum_x - leL + 1, pos_y + 1, leL + 1, ILI9341_YELLOW);
      tft.drawFastHLine(spectrum_pos_centre_f  + spectrum_x - leL + 1, pos_y + 2, leL + 1, ILI9341_YELLOW);
      tft.drawFastHLine(spectrum_pos_centre_f  + spectrum_x - leL + 1, pos_y + 3, leL + 1, ILI9341_YELLOW);
      //print bandwidth !
          tft.fillRect(10,24,310,21,ILI9341_BLACK);   
          tft.setCursor(10, 25);
          tft.setFont(Arial_9);
          tft.setTextColor(ILI9341_WHITE);
          tft.print(DEMOD[band[bands].mode].text);
          sprintf(string,"%02.1f kHz", (float)(filter_f/1000.0));//kHz);
          tft.setCursor(70, 25);
          tft.print(string);
          tft.setCursor(140, 25);
          tft.print("   SR:  ");
          tft.print(SR[SAMPLE_RATE].text);
          show_tunestep();
          tft.setTextColor(ILI9341_WHITE); // set text color to white for other print routines not to get confused ;-)
//  AudioInterrupts();
}  // end show_bandwidth

void prepare_spectrum_display() 
{
    uint16_t base_y = spectrum_y + spectrum_height + 4;
    uint16_t b_x = spectrum_x + SR[SAMPLE_RATE].x_offset;
    float32_t x_f = SR[SAMPLE_RATE].x_factor;
    tft.fillRect(0,base_y,320,240 - base_y,ILI9341_BLACK);
    tft.drawRect(spectrum_x - 2, spectrum_y + 2, 257, spectrum_height, ILI9341_MAROON); 
    // receive freq indicator line
//    tft.drawFastVLine(spectrum_x + spectrum_pos_centre_f, spectrum_y + spectrum_height - 18, 20, ILI9341_GREEN); 
/*
    // vertical lines
    tft.drawFastVLine(b_x - 4, base_y + 1, 10, ILI9341_YELLOW);  
    tft.drawFastVLine(b_x - 3, base_y + 1, 10, ILI9341_YELLOW);  
    tft.drawFastVLine( x_f * 2.0 + b_x,  base_y + 1, 10, ILI9341_YELLOW);  
    tft.drawFastVLine( x_f * 2.0 + 1.0 + b_x,  base_y + 1, 10, ILI9341_YELLOW);  
    if(x_f * 3.0 + b_x < 256+b_x) {
    tft.drawFastVLine( x_f * 3.0 + b_x,  base_y + 1, 10, ILI9341_YELLOW);  
    tft.drawFastVLine( x_f * 3.0 + 1.0 + b_x,  base_y + 1, 10, ILI9341_YELLOW);  
    }
    if(x_f * 4.0 + b_x < 256+b_x) {
    tft.drawFastVLine( x_f * 4.0 + b_x,  base_y + 1, 10, ILI9341_YELLOW);  
    tft.drawFastVLine( x_f * 4.0 + 1.0 + b_x,  base_y + 1, 10, ILI9341_YELLOW);  
    }
    if(x_f * 5.0 + b_x < 256+b_x) {
    tft.drawFastVLine( x_f * 5.0 + b_x,  base_y + 1, 10, ILI9341_YELLOW);  
    tft.drawFastVLine( x_f * 5.0 + 1.0 + b_x,  base_y + 1, 10, ILI9341_YELLOW);  
    }
    tft.drawFastVLine( x_f * 0.5 + b_x,  base_y + 1, 6, ILI9341_YELLOW);  
    tft.drawFastVLine( x_f * 1.5 + b_x,  base_y + 1, 6, ILI9341_YELLOW);  
    tft.drawFastVLine( x_f * 2.5 + b_x,  base_y + 1, 6, ILI9341_YELLOW);  
    if(x_f * 3.5 + b_x < 256+b_x) {
    tft.drawFastVLine( x_f * 3.5 + b_x,  base_y + 1, 6, ILI9341_YELLOW);  
    }
    if(x_f * 4.5 + b_x < 256+b_x) {
        tft.drawFastVLine( x_f * 4.5 + b_x,  base_y + 1, 6, ILI9341_YELLOW);  
    }
    // text
    tft.setTextColor(ILI9341_WHITE);
    tft.setFont(Arial_8);
    int text_y_offset = 15;
    int text_x_offset = - 5;
    // zero
    tft.setCursor (b_x + text_x_offset - 1, base_y + text_y_offset);
    tft.print(SR[SAMPLE_RATE].f1);
    tft.setCursor (b_x + x_f * 2 + text_x_offset, base_y + text_y_offset);
    tft.print(SR[SAMPLE_RATE].f2);
    tft.setCursor (b_x + x_f *3 + text_x_offset, base_y + text_y_offset);
    tft.print(SR[SAMPLE_RATE].f3);
    tft.setCursor (b_x + x_f *4 + text_x_offset, base_y + text_y_offset);
    tft.print(SR[SAMPLE_RATE].f4);
//    tft.setCursor (b_x + text_x_offset + 256, base_y + text_y_offset);
    tft.print(" kHz");
    tft.setFont(Arial_10);
*/
// draw S-Meter layout
  tft.drawFastHLine (pos_x_smeter, pos_y_smeter-1, 9*s_w, ILI9341_WHITE);
  tft.drawFastHLine (pos_x_smeter, pos_y_smeter+6, 9*s_w, ILI9341_WHITE);
  tft.fillRect(pos_x_smeter, pos_y_smeter-3, 2, 2, ILI9341_WHITE);
  tft.fillRect(pos_x_smeter+8*s_w, pos_y_smeter-3, 2, 2, ILI9341_WHITE);
  tft.fillRect(pos_x_smeter+2*s_w, pos_y_smeter-3, 2, 2, ILI9341_WHITE);
  tft.fillRect(pos_x_smeter+4*s_w, pos_y_smeter-3, 2, 2, ILI9341_WHITE);
  tft.fillRect(pos_x_smeter+6*s_w, pos_y_smeter-3, 2, 2, ILI9341_WHITE);
  tft.fillRect(pos_x_smeter+7*s_w, pos_y_smeter-4, 2, 3, ILI9341_WHITE);
  tft.fillRect(pos_x_smeter+3*s_w, pos_y_smeter-4, 2, 3, ILI9341_WHITE);
  tft.fillRect(pos_x_smeter+5*s_w, pos_y_smeter-4, 2, 3, ILI9341_WHITE);
  tft.fillRect(pos_x_smeter+s_w, pos_y_smeter-4, 2, 3, ILI9341_WHITE);
  tft.fillRect(pos_x_smeter+9*s_w, pos_y_smeter-4, 2, 3, ILI9341_WHITE);
  tft.drawFastHLine (pos_x_smeter+9*s_w, pos_y_smeter-1, 3*s_w*2+2, ILI9341_GREEN);

  tft.drawFastHLine (pos_x_smeter+9*s_w, pos_y_smeter+6, 3*s_w*2+2, ILI9341_GREEN);
  tft.fillRect(pos_x_smeter+11*s_w, pos_y_smeter-4, 2, 3, ILI9341_GREEN);
  tft.fillRect(pos_x_smeter+13*s_w, pos_y_smeter-4, 2, 3, ILI9341_GREEN);
  tft.fillRect(pos_x_smeter+15*s_w, pos_y_smeter-4, 2, 3, ILI9341_GREEN);

  tft.drawFastVLine (pos_x_smeter-1, pos_y_smeter-1, 8, ILI9341_WHITE); 
  tft.drawFastVLine (pos_x_smeter+15*s_w+2, pos_y_smeter-1, 8, ILI9341_GREEN);

  tft.setCursor(pos_x_smeter - 1, pos_y_smeter - 15);
  tft.setTextColor(ILI9341_WHITE);
  tft.setFont(Arial_8);
  tft.print("S 1");
  tft.setCursor(pos_x_smeter + 28, pos_y_smeter - 15);
  tft.print("3");
  tft.setCursor(pos_x_smeter + 48, pos_y_smeter - 15);
  tft.print("5");
  tft.setCursor(pos_x_smeter + 68, pos_y_smeter - 15);
  tft.print("7");
  tft.setCursor(pos_x_smeter + 88, pos_y_smeter - 15);
  tft.print("9");
  tft.setCursor(pos_x_smeter + 120, pos_y_smeter - 15);
  tft.print("+20dB");
    FrequencyBarText();
    show_menu();
} // END prepare_spectrum_display

void FrequencyBarText()
{    // This function draws the frequency bar at the bottom of the spectrum scope, putting markers at every graticule and the full frequency
    // (rounded to the nearest kHz) in the "center".  (by KA7OEI, 20140913) modified from the mcHF source code
    float   freq_calc;
    ulong   i, clr;
    char    txt[16], *c;
    float   grat;
    int centerIdx;
    const int pos_grat_y = 20;
    grat = (float)((SR[SAMPLE_RATE].rate / 8000.0) / (1 << spectrum_zoom)); // 1, 2, 4, 8, 16

    tft.setTextColor(ILI9341_WHITE);
    tft.setFont(Arial_8);
    // clear print area for frequency text
//    tft.fillRect(0, spectrum_y + spectrum_height + pos_grat_y, 320, 8, ILI9341_BLACK);
    tft.fillRect(0,spectrum_y + spectrum_height + 5,320,240 - spectrum_y - spectrum_height - 5,ILI9341_BLACK);

    freq_calc = (float)(bands[band].freq / SI5351_FREQ_MULT);      // get current frequency in Hz

    if(spectrum_zoom == 0)         // if magnify is off, way *may* have the graticule shifted.  (If it is on, it is NEVER shifted from center.)
    {
        freq_calc += (float32_t)SR[SAMPLE_RATE].rate / 4.0;
    }

    if(spectrum_zoom < 3)
    {
        freq_calc = roundf(freq_calc/1000); // round graticule frequency to the nearest kHz
    }
    if(spectrum_zoom > 2 && spectrum_zoom < 5)
    {
        freq_calc = roundf(freq_calc/100) / 10; // round graticule frequency to the nearest 100Hz
    }
    if(spectrum_zoom == 5) // not implemented at the moment
    {
        freq_calc = roundf(freq_calc/50) / 20; // round graticule frequency to the nearest 50Hz
    }

    if(spectrum_zoom != 0)     centerIdx = 0;
      else centerIdx = -2;
    
    {
        // remainder of frequency/graticule markings
//        const static int idx2pos[2][9] = {{0,26,58,90,122,154,186,218, 242},{0,26,58,90,122,154,186,209, 229} };
        // positions for graticules: first for spectrum_zoom < 3, then for spectrum_zoom > 2
        const static int idx2pos[2][9] = {{-10,21,52,83,123,151,186,218, 252},{-10,21,50,86,123,154,178,218, 252} };
//        const static int centerIdx2pos[] = {62,94,130,160,192};
        const static int centerIdx2pos[] = {62,94,125,160,192};

/**************************************************************************************************
 * CENTER FREQUENCY PRINT 
 **************************************************************************************************/ 
        if(spectrum_zoom < 3)
        {
            snprintf(txt,16, "  %lu  ", (ulong)(freq_calc+(centerIdx*grat))); // build string for center frequency precision 1khz
        }
        else
        {
            float disp_freq = freq_calc+(centerIdx*grat);
            int bignum = (int)disp_freq;
            int smallnum = (int)roundf((disp_freq-bignum)*100);
            snprintf(txt,16, "  %u.%02u  ", bignum,smallnum); // build string for center frequency precision 100Hz/10Hz
        }

        i = centerIdx2pos[centerIdx+2] -((strlen(txt)- 4) * 4);    // calculate position of center frequency text

        tft.setCursor(spectrum_x + i, spectrum_y + spectrum_height + pos_grat_y);
        tft.print(txt);

/**************************************************************************************************
 * PRINT ALL OTHER FREQUENCIES (NON-CENTER)
 **************************************************************************************************/ 

        for (int idx = -4; idx < 5; idx++)
//        for (int idx = -3; idx < 4; idx++)
        {
            int pos_help = idx2pos[spectrum_zoom < 3? 0 : 1][idx+4];
            if (idx != centerIdx)
            {
                if(spectrum_zoom < 3)
                {
                    snprintf(txt,16, " %lu ", (ulong)(freq_calc+(idx*grat)));   // build string for middle-left frequency (1khz precision)
                    c = &txt[strlen(txt)-3];  // point at 2nd character from the end
                }
                else
                {
                    float disp_freq = freq_calc+(idx*grat);
                    int bignum = (int)disp_freq;
                    int smallnum = (int)roundf((disp_freq-bignum)*100);
                    snprintf(txt,16, " %u.%02u ", bignum, smallnum);   // build string for middle-left frequency (10Hz precision)
                    c = &txt[strlen(txt)-5];  // point at 5th character from the end
                }

            tft.setCursor(spectrum_x + pos_help, spectrum_y + spectrum_height + pos_grat_y);
            tft.print(txt);
            // insert draw vertical bar HERE:


            
            }
            if(spectrum_zoom > 2 || freq_calc > 1000)
            {
                idx++;
            }
        }
    }
    tft.setFont(Arial_10);

//**************************************************************************
    uint16_t base_y = spectrum_y + spectrum_height + 4;

    float32_t pixel_per_khz = (1<<spectrum_zoom) * 256.0 / SR[SAMPLE_RATE].rate * 1000.0;

    // center line
    if(spectrum_zoom == 0)
    {
            tft.drawFastVLine(spectrum_x + 62, base_y + 1, 10, ILI9341_RED);  
            tft.drawFastVLine(spectrum_x + 65, base_y + 1, 10, ILI9341_RED);  
            tft.drawFastVLine(spectrum_x + 63, base_y + 1, 10, ILI9341_RED);  
            tft.drawFastVLine(spectrum_x + 64, base_y + 1, 10, ILI9341_RED);  
            tft.drawFastVLine(spectrum_x + 127, base_y + 1, 10, ILI9341_YELLOW);  
    }
    else
    {
            tft.drawFastVLine(spectrum_x + 126, base_y + 1, 10, ILI9341_RED);  
            tft.drawFastVLine(spectrum_x + 129, base_y + 1, 10, ILI9341_RED);  
            tft.drawFastVLine(spectrum_x + 127, base_y + 1, 10, ILI9341_RED);  
            tft.drawFastVLine(spectrum_x + 128, base_y + 1, 10, ILI9341_RED);  
            tft.drawFastVLine(spectrum_x + 63, base_y + 1, 10, ILI9341_YELLOW);  
    }
    // vertical lines
    tft.drawFastVLine(spectrum_x, base_y + 1, 10, ILI9341_YELLOW);  
    tft.drawFastVLine(spectrum_x - 1, base_y + 1, 10, ILI9341_YELLOW);  
    tft.drawFastVLine(spectrum_x + 255, base_y + 1, 10, ILI9341_YELLOW);  
    tft.drawFastVLine(spectrum_x + 256, base_y + 1, 10, ILI9341_YELLOW);  
    tft.drawFastVLine(spectrum_x + 191, base_y + 1, 10, ILI9341_YELLOW);  

    if(spectrum_zoom < 3 && freq_calc <= 1000)
    {
        tft.drawFastVLine(spectrum_x + 31, base_y + 1, 10, ILI9341_YELLOW);  
        tft.drawFastVLine(spectrum_x + 95, base_y + 1, 10, ILI9341_YELLOW);  
        tft.drawFastVLine(spectrum_x + 159, base_y + 1, 10, ILI9341_YELLOW);  
        tft.drawFastVLine(spectrum_x + 223, base_y + 1, 10, ILI9341_YELLOW);  
    }
}

float32_t alpha_beta_mag(float32_t  inphase, float32_t  quadrature) 
// (c) András Retzler
// taken from libcsdr: https://github.com/simonyiszk/csdr
{
  // Min RMS Err      0.947543636291 0.392485425092
  // Min Peak Err     0.960433870103 0.397824734759
  // Min RMS w/ Avg=0 0.948059448969 0.392699081699
  const float32_t alpha = 0.960433870103; // 1.0; //0.947543636291;
  const float32_t beta =  0.397824734759;
   /* magnitude ~= alpha * max(|I|, |Q|) + beta * min(|I|, |Q|) */
   float32_t abs_inphase = fabs(inphase);
   float32_t abs_quadrature = fabs(quadrature);
   if (abs_inphase > abs_quadrature) {
      return alpha * abs_inphase + beta * abs_quadrature;
   } else {
      return alpha * abs_quadrature + beta * abs_inphase;
   }
}
/*
void amdemod_estimator_cf(complexf* input, float *output, int input_size, float alpha, float beta)
{ //  (c) András Retzler
  //  taken from libcsdr: https://github.com/simonyiszk/csdr
  //concept is explained here:
  //http://www.dspguru.com/dsp/tricks/magnitude-estimator

  //default: optimize for min RMS error
  if(alpha==0)
  {
    alpha=0.947543636291;
    beta=0.392485425092;
  }

  //@amdemod_estimator
  for (int i=0; i<input_size; i++)
  {
    float abs_i=iof(input,i);
    if(abs_i<0) abs_i=-abs_i;
    float abs_q=qof(input,i);
    if(abs_q<0) abs_q=-abs_q;
    float max_iq=abs_i;
    if(abs_q>max_iq) max_iq=abs_q;
    float min_iq=abs_i;
    if(abs_q<min_iq) min_iq=abs_q;

    output[i]=alpha*max_iq+beta*min_iq;
  }
}

*/

float32_t fastdcblock_ff(float32_t* input, float32_t* output, int input_size, float32_t last_dc_level)
{ //  (c) András Retzler
  //  taken from libcsdr: https://github.com/simonyiszk/csdr
  //this DC block filter does moving average block-by-block.
  //this is the most computationally efficient
  //input and output buffer is allowed to be the same
  //http://www.digitalsignallabs.com/dcblock.pdf
  float32_t avg=0.0;
  for(int i=0;i<input_size;i++) //@fastdcblock_ff: calculate block average
  {
    avg+=input[i];
  }
  avg/=input_size;

  float32_t avgdiff=avg-last_dc_level;
  //DC removal level will change lineraly from last_dc_level to avg.
  for(int i=0;i<input_size;i++) //@fastdcblock_ff: remove DC component
  {
    float32_t dc_removal_level=last_dc_level+avgdiff*((float32_t)i/input_size);
    output[i]=input[i]-dc_removal_level;
  }
  return avg;
}

void set_IIR_coeffs (float32_t f0, float32_t Q, float32_t sample_rate, uint8_t filter_type)
{      
 //     set_IIR_coeffs ((float32_t)2400.0, 1.3, (float32_t)SR[SAMPLE_RATE].rate, 0);
     /*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     * Cascaded biquad (notch, peak, lowShelf, highShelf) [DD4WH, april 2016]
     ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
    // DSP Audio-EQ-cookbook for generating the coeffs of the filters on the fly
    // www.musicdsp.org/files/Audio-EQ-Cookbook.txt  [by Robert Bristow-Johnson]
    //
    // the ARM algorithm assumes the biquad form
    // y[n] = b0 * x[n] + b1 * x[n-1] + b2 * x[n-2] + a1 * y[n-1] + a2 * y[n-2]
    //
    // However, the cookbook formulae by Robert Bristow-Johnson AND the Iowa Hills IIR Filter designer
    // use this formula:
    //
    // y[n] = b0 * x[n] + b1 * x[n-1] + b2 * x[n-2] - a1 * y[n-1] - a2 * y[n-2]
    //
    // Therefore, we have to use negated a1 and a2 for use with the ARM function
    if(f0 > sample_rate / 2.0) f0 = sample_rate / 2.0;
    float32_t w0 = f0 * (TPI / sample_rate);
    float32_t sinW0 = sinf(w0);
    float32_t alpha = sinW0 / (Q * 2.0);
    float32_t cosW0 = cosf(w0);
    float32_t scale = 1.0 / (1.0 + alpha);

    
    /* b0 */ coefficient_set[0] = ((1.0 - cosW0) / 2.0) * scale;
    /* b1 */ coefficient_set[1] = (1.0 - cosW0) * scale;
    /* b2 */ coefficient_set[2] = coefficient_set[0];
    /* a1 */ coefficient_set[3] = (2.0 * cosW0) * scale; // negated
/* a2 */     coefficient_set[4] = (-1.0 + alpha) * scale; // negated
   
}

int ExtractDigit(long int n, int k) {
        switch (k) {
              case 0: return n%10;
              case 1: return n/10%10;
              case 2: return n/100%10;
              case 3: return n/1000%10;
              case 4: return n/10000%10;
              case 5: return n/100000%10;
              case 6: return n/1000000%10;
              case 7: return n/10000000%10;
              case 8: return n/100000000%10;
              default: return 0;
        }
}



// show frequency
void show_frequency(unsigned long long freq) { 
//    tft.setTextSize(2);
    freq = freq / SI5351_FREQ_MULT;
    tft.setFont(Arial_18);
    tft.setTextColor(ILI9341_WHITE);
    uint8_t zaehler;
    uint8_t digits[10];
    zaehler = 8;

          while (zaehler--) {
              digits[zaehler] = ExtractDigit (freq, zaehler);
//              Serial.print(digits[zaehler]);
//              Serial.print(".");
// 7: 10Mhz, 6: 1Mhz, 5: 100khz, 4: 10khz, 3: 1khz, 2: 100Hz, 1: 10Hz, 0: 1Hz
        }
            //  Serial.print("xxxxxxxxxxxxx");

    zaehler = 8;
        while (zaehler--) { // counts from 7 to 0
              if (zaehler < 6) sch = 9; // (khz)
              if (zaehler < 3) sch = 18; // (Hz)
          if (digits[zaehler] != digits_old[zaehler] || !freq_flag) { // digit has changed (or frequency is displayed for the first time after power on)
              if (zaehler == 7) {
                     sch = 0;
                     tft.setCursor(pos_x_frequency + font_width * (8-zaehler) + sch,pos_y_frequency); // set print position
                     tft.fillRect(pos_x_frequency + font_width * (8-zaehler) + sch,pos_y_frequency, font_width,18,ILI9341_BLACK); // delete old digit
                     if (digits[7] != 0) tft.print(digits[zaehler]); // write new digit in white
              }
              if (zaehler == 6) {
                            sch = 0;
                            tft.setCursor(pos_x_frequency + font_width * (8-zaehler) + sch,pos_y_frequency); // set print position
                            tft.fillRect(pos_x_frequency + font_width * (8-zaehler) + sch,pos_y_frequency, font_width,18,ILI9341_BLACK); // delete old digit
                            if (digits[6]!=0 || digits[7] != 0) tft.print(digits[zaehler]); // write new digit in white
              }
               if (zaehler == 5) {
                            sch = 9;
                            tft.setCursor(pos_x_frequency + font_width * (8-zaehler) + sch,pos_y_frequency); // set print position
                            tft.fillRect(pos_x_frequency + font_width * (8-zaehler) + sch,pos_y_frequency, font_width,18,ILI9341_BLACK); // delete old digit
                            if (digits[5] != 0 || digits[6]!=0 || digits[7] != 0) tft.print(digits[zaehler]); // write new digit in white
              }
              
              if (zaehler < 5) { 
              // print the digit
              tft.setCursor(pos_x_frequency + font_width * (8-zaehler) + sch,pos_y_frequency); // set print position
              tft.fillRect(pos_x_frequency + font_width * (8-zaehler) + sch,pos_y_frequency, font_width,18,ILI9341_BLACK); // delete old digit
              tft.print(digits[zaehler]); // write new digit in white
              }
              digits_old[zaehler] = digits[zaehler]; 
        }
        }
  
      tft.setFont(Arial_10);
      if (digits[7] == 0 && digits[6] == 0)
              tft.fillRect(pos_x_frequency + font_width * 3 ,pos_y_frequency + 15, 3, 3, ILI9341_BLACK);
      else    tft.fillRect(pos_x_frequency + font_width * 3 ,pos_y_frequency + 15, 3, 3, ILI9341_YELLOW);
      tft.fillRect(pos_x_frequency + font_width * 7 - 6, pos_y_frequency + 15, 3, 3, ILI9341_YELLOW);
      if (!freq_flag) {
            tft.setCursor(pos_x_frequency + font_width * 9 + 21,pos_y_frequency + 7); // set print position
            tft.setTextColor(ILI9341_GREEN);
            tft.print("Hz");
      }
      freq_flag = 1;
      tft.setTextColor(ILI9341_WHITE);

} // END VOID SHOW-FREQUENCY

void setfreq () {
// NEVER USE AUDIONOINTERRUPTS HERE: that introduces annoying clicking noise with every frequency change
//   hilfsf = (bands[band].freq +  IF_FREQ) * 10000000 * MASTER_CLK_MULT * SI5351_FREQ_MULT;
   hilfsf = (bands[band].freq +  IF_FREQ * SI5351_FREQ_MULT) * 1000000000 * MASTER_CLK_MULT; // SI5351_FREQ_MULT is 100ULL;
   hilfsf = hilfsf / calibration_factor;
   si5351.set_freq(hilfsf, Si_5351_clock); 
   if(band[bands].mode == DEMOD_AUTOTUNE)
   {
        autotune_flag = 1;
   }
   FrequencyBarText();
}   

void buttons() {
      button1.update(); // BAND --
      button2.update(); // BAND ++
      button3.update(); // change band[bands].mode
      button4.update(); // MENU --
      button5.update(); // tune encoder button
      button6.update(); // filter encoder button
      button7.update(); // MENU ++
      button8.update(); // menu2 button
      
            if ( button1.fallingEdge()) {
              if(Menu_pointer == MENU_PLAYER)
              {
                 prevtrack();
              }
              else
              { 
                if(--band < FIRST_BAND) band=LAST_BAND; // cycle thru radio bands 
                set_band();
                set_tunestep();
              }       
            }
            if ( button2.fallingEdge()) { 
              if(Menu_pointer == MENU_PLAYER)
              {
                 pausetrack();
              }
              else  
              {
                if(++band > LAST_BAND) band=FIRST_BAND; // cycle thru radio bands
                set_band(); 
                set_tunestep();
              }
            }
            if ( button3.fallingEdge()) {  // cycle through DEMOD modes
              if(Menu_pointer == MENU_PLAYER)
              {
                 nexttrack();
              }
              else
              {
                if(++band[bands].mode > DEMOD_MAX) band[bands].mode = DEMOD_MIN; // cycle thru demod modes
                setup_mode(band[bands].mode); 
              }
            }
            if ( button4.fallingEdge()) { 
               // toggle thru menu
               if(which_menu == 1)
               {
                    if(--Menu_pointer < first_menu) Menu_pointer = last_menu;
               }
               else 
               {
                    if(--Menu2 < first_menu2) Menu2 = last_menu2;
               }
               if(Menu_pointer == (MENU_PLAYER - 1) || (Menu_pointer == last_menu && MENU_PLAYER == first_menu))
               {
                  // stop all playing
                  playMp3.stop();
                  playAac.stop();
                  delay(200);
                  setI2SFreq (SR[SAMPLE_RATE].rate);
                  delay(200); // essential ?
                  mixleft.gain(0,1.0);
                  mixright.gain(0,1.0);
                  mixleft.gain(1,0.0);
                  mixright.gain(1,0.0);
                  mixleft.gain(2,0.0);
                  mixright.gain(2,0.0);
                  prepare_spectrum_display();
                  Q_in_L.clear();
                  Q_in_R.clear();
                  Q_in_L.begin();
                  Q_in_R.begin();
               }
               show_menu();

            }
            if ( button5.fallingEdge()) { // cycle thru tune steps
                if(++tune_stepper > TUNE_STEP_MAX) tune_stepper = TUNE_STEP_MIN; 
                set_tunestep();
            }
            if (button6.fallingEdge()) { 
                autotune_flag = 1;
                Serial.println("Flag gesetzt!");
            }            
            if (button7.fallingEdge()) { 
               // toggle thru menu
               if(which_menu == 1)
               {
                    if(++Menu_pointer > last_menu) Menu_pointer = first_menu;
               }
               else 
               {
                    if(++Menu2 > last_menu2) Menu2 = first_menu2;
               }
               Serial.println("MENU BUTTON pressed");
               if(Menu_pointer == MENU_PLAYER)
               {
                  Menu2 = MENU_VOLUME;
                  setI2SFreq (AUDIO_SAMPLE_RATE_EXACT);
                  delay(200); // essential ?
//                  audio_flag = 0;
                  Q_in_L.end();
                  Q_in_R.end();
                  Q_in_L.clear();
                  Q_in_R.clear();
                  mixleft.gain(0,0.0);
                  mixright.gain(0,0.0);
                  mixleft.gain(1,0.4);
                  mixright.gain(1,0.4);
                  mixleft.gain(2,0.4);
                  mixright.gain(2,0.4);
               }
               if(Menu_pointer == (MENU_PLAYER + 1) || (Menu_pointer == 0 && MENU_PLAYER == last_menu))
               {
                  // stop all playing
                  playMp3.stop();
                  playAac.stop();
                  delay(200);
                  setI2SFreq (SR[SAMPLE_RATE].rate);
                  delay(200); // essential ?
                  mixleft.gain(0,1.0);
                  mixright.gain(0,1.0);
                  mixleft.gain(1,0.0);
                  mixright.gain(1,0.0);
                  mixleft.gain(2,0.0);
                  mixright.gain(2,0.0);
                  prepare_spectrum_display();
                  Q_in_L.clear();
                  Q_in_R.clear();
                  Q_in_L.begin();
                  Q_in_R.begin();
               }
               show_menu();
            }            
            if (button8.fallingEdge()) { 
               // toggle thru menu2
               if(++Menu2 > last_menu2) Menu2 = first_menu2;
               which_menu = 2;
               Serial.println("MENU2 BUTTON pressed");
               show_menu();
            }            
}

void show_menu()
{
    // two blue boxes show the setting for each encoder
    // menu  = filter    --> encoder under display
    // menu2 = encoder3  --> left encoder
    // define txt-string for display
    // position constant: spectrum_y
    // Menus[Menu_pointer].no
    // upper text menu
    char string[6];
    int color1 = ILI9341_WHITE;
    int color2 = ILI9341_WHITE;
    if(which_menu == 1) color1 = ILI9341_DARKGREY;
      else color2 = ILI9341_DARKGREY;
    spectrum_y +=2;
    tft.fillRect(spectrum_x + 256 + 2, spectrum_y, 320 - spectrum_x - 258, 31, ILI9341_NAVY);
    tft.drawRect(spectrum_x + 256 + 2, spectrum_y, 320 - spectrum_x - 258, 31, ILI9341_MAROON);
    tft.setCursor(spectrum_x + 256 + 5, spectrum_y + 4);
    tft.setTextColor(color2);
    tft.setFont(Arial_10);
    tft.print(Menus[Menu_pointer].text1);
    // lower text menu
    tft.setCursor(spectrum_x + 256 + 5, spectrum_y + 16);
    tft.print(Menus[Menu_pointer].text2);
    // upper text menu2
    tft.setTextColor(color1);
    tft.fillRect(spectrum_x + 256 + 2, spectrum_y + 30, 320 - spectrum_x - 258, 31, ILI9341_NAVY);
    tft.drawRect(spectrum_x + 256 + 2, spectrum_y + 30, 320 - spectrum_x - 258, 31, ILI9341_MAROON);
    tft.setCursor(spectrum_x + 256 + 5, spectrum_y + 30 + 4);
    tft.print(Menus[Menu2].text1);
    // lower text menu2
    tft.setCursor(spectrum_x + 256 + 5, spectrum_y + 30 + 16);
    tft.print(Menus[Menu2].text2);
    tft.setTextColor(ILI9341_WHITE);
    // third box: shows the value of the parameter being changed
    tft.fillRect(spectrum_x + 256 + 2, spectrum_y + 60, 320 - spectrum_x - 258, 31, ILI9341_NAVY);
    tft.drawRect(spectrum_x + 256 + 2, spectrum_y + 60, 320 - spectrum_x - 258, 31, ILI9341_MAROON);
    // print value
    tft.setFont(Arial_14);
    tft.setCursor(spectrum_x + 256 + 12, spectrum_y + 31 + 31 + 7);
    if(which_menu == 1) // print value of Menu parameter
    {
        switch(Menu_pointer)
        {
            case MENU_SPECTRUM_ZOOM:
                tft.print(1 << spectrum_zoom);
            break;
            case MENU_IQ_AMPLITUDE:
                tft.setFont(Arial_11);
                tft.setCursor(spectrum_x + 256 + 8, spectrum_y + 31 + 31 + 7);
                sprintf(string,"%01.3f", IQ_amplitude_correction_factor);
                tft.print(string);
            break;
            case MENU_IQ_PHASE:
                tft.setFont(Arial_11);
                tft.setCursor(spectrum_x + 256 + 8, spectrum_y + 31 + 31 + 7);
                sprintf(string,"%01.3f", IQ_phase_correction_factor);
                tft.print(string);
//                tft.print(IQ_phase_correction_factor);
            break;
            case MENU_CALIBRATION_FACTOR:
                tft.print(1 << spectrum_zoom);
            break;
            case MENU_CALIBRATION_CONSTANT:
                tft.print(1 << spectrum_zoom);
            break;
            case MENU_LPF_SPECTRUM:
                tft.print(1 << spectrum_zoom);
            break;
            case MENU_SAVE_EEPROM:
            break;
            case MENU_LOAD_EEPROM:
            break;
            case MENU_TIME_SET:
            break;
            case MENU_DATE_SET:
            break;            
        }
    }
    else
    { // print value of Menu2 parameter
        switch(Menu2)
        {
            case MENU_RF_GAIN:
                tft.setFont(Arial_11);
                tft.setCursor(spectrum_x + 256 + 6, spectrum_y + 31 + 31 + 7);
                sprintf(string,"%02.1fdB", (float)(bands[band].RFgain * 1.5));
                tft.print(string);
            break;
            case MENU_VOLUME:
                tft.print(audio_volume);
            break;
            case MENU_SAM_ZETA:
                tft.print(zeta);
            break;
            case MENU_TREBLE:
                sprintf(string,"%02.0f", treble * 100.0);
                tft.print(string);
            break;
            case MENU_BASS:
                sprintf(string,"%02.0f", bass * 100.0);
                tft.print(string);
            break;
            case MENU_SAM_OMEGA:
//                tft.setFont(Arial_11);
//                tft.setCursor(spectrum_x + 256 + 6, spectrum_y + 31 + 31 + 7);
                sprintf(string,"%03.0f", omegaN);
                tft.print(string);
            break;
            case MENU_SAM_CATCH_BW:
                tft.setFont(Arial_12);
                tft.setCursor(spectrum_x + 256 + 8, spectrum_y + 31 + 31 + 7);
                sprintf(string,"%04.0f", pll_fmax);
                tft.print(string);
            break;
            case MENU_AGC_MODE:
                tft.setFont(Arial_10);
                tft.setCursor(spectrum_x + 256 + 8, spectrum_y + 31 + 31 + 7);
                switch(AGC_mode)
                {
                  case 0:
                    tft.print("OFF");
                  break;
                  case 1:
                    tft.print("Long+");
                  break;
                  case 2:
                    tft.print("LONG");
                  break;
                  case 3:
                    tft.print("SLOW");
                  break;
                  case 4:
                    tft.print("MED");
                  break;
                  case 5:
                    tft.print("FAST");
                  break;
                }
            break;    
        }
    }
    
    spectrum_y -= 2;
}


void set_tunestep()
{
                if(tune_stepper == 0) 
                if(band == BAND_MW || band == BAND_LW) tunestep = 9000; else tunestep = 5000;
                else if (tune_stepper == 1) tunestep = 100;
                else if (tune_stepper == 2) tunestep = 1000;
                else if (tune_stepper == 3) tunestep = 1;
                else tunestep = 5000;
                show_tunestep(); 

}

void autotune() {
    // Lyons (2011): chapter 13.15 page 702
    // this uses the FFT_buffer DIRECTLY after the 1024 point FFT
    // and calculates the magnitudes 
    // after that, for finetuning, a quadratic interpolation is performed
    // 1.) determine bins that are inside the filterbandwidth,
    //     depending on filter bandwidth AND band[bands].mode
    // 2.) calculate magnitudes from the real & imaginary values in the FFT buffer
    //     and bring them in the right order and put them into
    //     iFFT_buffer [that is recycled for this function and filled with other values afterwards]
    // 3.) perform carrier frequency estimation
    // 4.) tune to the estimated carrier frequency with an accuracy of 0.01Hz ;-) 
    // --> in reality, we achieve about 0.2Hz accuracy, not bad
    
    const float32_t buff_len = FFT_length * 2.0;
    const float32_t bin_BW = (float32_t) (SR[SAMPLE_RATE].rate * 2.0 / DF / (buff_len));
//    const int buff_len_int = FFT_length * 2;
    float32_t bw_LSB = 0.0;
    float32_t bw_USB = 0.0;
//    float32_t help_freq = (float32_t)(bands[band].freq +  IF_FREQ * SI5351_FREQ_MULT);

    //  determine posbin (where we receive at the moment) 
    // FFT_lengh is 1024 
    // FFT_buffer is already frequency-translated !
    // so we do not need to worry about that IF stuff
    const int posbin = FFT_length / 2; //
    bw_LSB = (float32_t)bands[band].bandwidthL;
    bw_USB = (float32_t)bands[band].bandwidthU;
    // include 500Hz of the other sideband into the search bandwidth
    if(bw_LSB < 1.0) bw_LSB = 500.0;
    if(bw_USB < 1.0) bw_USB = 500.0;
    
//    Serial.print("bw_LSB = "); Serial.println(bw_LSB);
//    Serial.print("bw_USB = "); Serial.println(bw_USB);
    
    // calculate upper and lower limit for determination of maximum magnitude
     const float32_t Lbin = (float32_t)posbin - round(bw_LSB / bin_BW);
     const float32_t Ubin = (float32_t)posbin + round(bw_USB / bin_BW); // the bin on the upper sideband side

      // put into second half of iFFT_buffer
      arm_cmplx_mag_f32(FFT_buffer, &iFFT_buffer[FFT_length], FFT_length);  // calculates sqrt(I*I + Q*Q) for each frequency bin of the FFT
      
      ////////////////////////////////////////////////////////////////////////
      // now bring into right order and copy in first half of iFFT_buffer
      ////////////////////////////////////////////////////////////////////////
      
      for(i = 0; i < FFT_length / 2; i++)
      {
          iFFT_buffer[i] = iFFT_buffer[i + FFT_length + FFT_length / 2];
      }
      for(i = FFT_length / 2; i < FFT_length; i++)
      {
          iFFT_buffer[i] = iFFT_buffer[i + FFT_length / 2];
      }

    //####################################################################
    if (autotune_flag == 1)
    {
        // look for maximum value and save the bin # for frequency delta calculation
        float32_t maximum = 0.0;
        float32_t maxbin = 1.0;
        float32_t delta = 0.0;

        for (int c = (int)Lbin; c <= (int)Ubin; c++)   // search for FFT bin with highest value = carrier and save the no. of the bin in maxbin
        {
            if (maximum < iFFT_buffer[c])
            {
                maximum = iFFT_buffer[c];
                maxbin = c;
            }
        }

        // ok, we have found the maximum, now save first delta frequency
        delta = (maxbin - (float32_t)posbin) * bin_BW;
//        Serial.print("maxbin = ");
//        Serial.println(maxbin);
//        Serial.print("posbin = ");
//        Serial.println(posbin);
        
        bands[band].freq = bands[band].freq  + (long long)(delta * SI5351_FREQ_MULT);
        setfreq();
        show_frequency(bands[band].freq);
//        Serial.print("delta = ");
//        Serial.println(delta);
        autotune_flag = 2;
    }
    else
    {
        // ######################################################

        // and now: fine-tuning:
        //  get amplitude values of the three bins around the carrier

        float32_t bin1 = iFFT_buffer[posbin-1];
        float32_t bin2 = iFFT_buffer[posbin];
        float32_t bin3 = iFFT_buffer[posbin+1];

        if (bin1+bin2+bin3 == 0.0) bin1= 0.00000001; // prevent divide by 0

        // estimate frequency of carrier by three-point-interpolation of bins around maxbin
        // formula by (Jacobsen & Kootsookos 2007) equation (4) P=1.36 for Hanning window FFT function
        // but we have unwindowed data here !
        // float32_t delta = (bin_BW * (1.75 * (bin3 - bin1)) / (bin1 + bin2 + bin3));
        // maybe this is the right equation for unwindowed magnitude data ?
        // performance is not too bad ;-)
        float32_t delta = (bin_BW * ((bin3 - bin1)) / (2 * bin2 - bin1 - bin3));
        if(delta > bin_BW) delta = 0.0; // just in case something went wrong

        bands[band].freq = bands[band].freq  + (long long)(delta * SI5351_FREQ_MULT);
        setfreq();
        show_frequency(bands[band].freq);

        if(band[bands].mode == DEMOD_AUTOTUNE)
        {
          autotune_flag = 0;
        }
        else
        {
        // empirically derived: it seems good to perform the whole tuning some 5 to 10 times
        // in order to be perfect on the carrier frequency 
        if(autotune_flag < 6)
        {
            autotune_flag++;
        } 
        else 
        {
            autotune_flag = 0;
            AudioNoInterrupts();
            Q_in_L.clear();
            Q_in_R.clear();
            AudioInterrupts();
        }
        }
//        Serial.print("DELTA 2 = ");
//        Serial.println(delta);
    }  

} // end function autotune


void show_tunestep() {
          tft.fillRect(240, 25, 80, 21, ILI9341_BLACK);
          tft.setCursor(240, 25);
          tft.setFont(Arial_9);
          tft.setTextColor(ILI9341_GREEN);
          tft.print("Step: ");
          tft.print(tunestep);
}

void set_band () {
//         show_band(bands[band].name); // show new band
         setup_mode(bands[band].mode);
         sgtl5000_1.lineInLevel(bands[band].RFgain, bands[band].RFgain);
//         setup_RX(bands[band].mode, bands[band].bandwidthU, bands[band].bandwidthL);  // set up the audio chain for new mode
         setfreq();
         show_frequency(bands[band].freq);
}

/* #########################################################################
 *  
 *  void setup_mode
 *  
 *  set up radio for RX modes - USB, LSB
 * ######################################################################### 
 */

void setup_mode(int MO) {
  switch (MO)  {

    case DEMOD_LSB:
    case DEMOD_STEREO_LSB:
    case DEMOD_SAM_LSB:
        if (bands[band].bandwidthU != 0) bands[band].bandwidthL = bands[band].bandwidthU;
        bands[band].bandwidthU = 0;
    break;
    case DEMOD_USB:
    case DEMOD_STEREO_USB:
    case DEMOD_SAM_USB:
        if (bands[band].bandwidthL != 0) bands[band].bandwidthU = bands[band].bandwidthL;
        bands[band].bandwidthL = 0;
    break;
//    case DEMOD_AM1:
    case DEMOD_AM2:
/*    case DEMOD_AM3:
    case DEMOD_AM_AE1:
    case DEMOD_AM_AE2:
    case DEMOD_AM_AE3:
    case DEMOD_AM_ME1: */
    case DEMOD_AM_ME2:
//    case DEMOD_AM_ME3:
    case DEMOD_SAM:
    case DEMOD_SAM_STEREO:
    case DEMOD_AUTOTUNE:
        if (bands[band].bandwidthU >= bands[band].bandwidthL) 
            bands[band].bandwidthL = bands[band].bandwidthU;
            else bands[band].bandwidthU = bands[band].bandwidthL;
        if(band[bands].mode == DEMOD_AUTOTUNE) autotune_wait = 40;
    break;
/*    case modeDSB:
        if (bands[band].bandwidthU >= bands[band].bandwidthL) 
            bands[band].bandwidthL = bands[band].bandwidthU;
            else bands[band].bandwidthU = bands[band].bandwidthL;
    break;
    case modeStereoAM:
        if (bands[band].bandwidthU >= bands[band].bandwidthL) 
            bands[band].bandwidthL = bands[band].bandwidthU;
            else bands[band].bandwidthU = bands[band].bandwidthL;
    break;
    */
  }
    show_bandwidth(band[bands].mode, LP_F_help);
    Q_in_L.clear();
    Q_in_R.clear();

} // end void setup_mode


void encoders () {
  static long encoder_pos=0, last_encoder_pos=0;
  long encoder_change;
  static long encoder2_pos=0, last_encoder2_pos=0;
  long encoder2_change;
  static long encoder3_pos=0, last_encoder3_pos=0;
  long encoder3_change;
  unsigned long long old_freq; 
  
  // tune radio and adjust settings in menus using encoder switch  
  encoder_pos = tune.read();
  encoder2_pos = filter.read();
  encoder3_pos = encoder3.read();
   
  if (encoder_pos != last_encoder_pos){
      encoder_change=(encoder_pos-last_encoder_pos);
      last_encoder_pos=encoder_pos;
 
    if (((band == BAND_LW) || (band == BAND_MW)) && (tune_stepper == 5000))
    { 
        tunestep = 9000;
        show_tunestep();
    }
    if (((band != BAND_LW) && (band != BAND_MW)) && (tunestep == 9000))
    {
        tunestep = 5000;
        show_tunestep();
    }
    if(tunestep == 1)
    {
      if (encoder_change <= 4 && encoder_change > 0) encoder_change = 4;
      else if(encoder_change >= -4 && encoder_change < 0) encoder_change = - 4;
    } 
    long long tune_help1 = tunestep  * SI5351_FREQ_MULT * roundf((float32_t)encoder_change / 4.0);
//    long long tune_help1 = tunestep  * SI5351_FREQ_MULT * encoder_change;
    old_freq = bands[band].freq;
    bands[band].freq += (long long)tune_help1;  // tune the master vfo 
    if (bands[band].freq > F_MAX) bands[band].freq = F_MAX;
    if (bands[band].freq < F_MIN) bands[band].freq = F_MIN;
    if(bands[band].freq != old_freq) 
    {
        Q_in_L.clear();
        Q_in_R.clear();
        setfreq();
        show_frequency(bands[band].freq);
        return;
    }
    show_menu();
  }
////////////////////////////////////////////////
  if (encoder2_pos != last_encoder2_pos){
      encoder2_change=(encoder2_pos-last_encoder2_pos);
      last_encoder2_pos=encoder2_pos;
      which_menu = 1;
    if(Menu_pointer == MENU_FILTER_BANDWIDTH)
    {
        LP_F_help = LP_F_help + encoder2_change * 25.0;
        float32_t sam = (SR[SAMPLE_RATE].rate / (DF * 2.0));
        if(LP_F_help > (uint32_t)(sam)) LP_F_help = (uint32_t)(sam);
        else
        {
            if(LP_F_help < 100) LP_F_help = 100;
            else 
            {
                filter_bandwidth();
                setfreq();
                show_frequency(bands[band].freq);
            }
        }
    }
    else
    if (Menu_pointer == MENU_SPECTRUM_ZOOM) 
    {
        spectrum_zoom = spectrum_zoom + (encoder2_change / 4);
        if(spectrum_zoom > SPECTRUM_ZOOM_MAX) spectrum_zoom = SPECTRUM_ZOOM_MAX;

        if(spectrum_zoom < SPECTRUM_ZOOM_MIN) spectrum_zoom = SPECTRUM_ZOOM_MIN;
        Zoom_FFT_prep();
        Serial.print("ZOOM factor:  "); Serial.println(1<<spectrum_zoom);
        show_bandwidth (band[bands].mode, LP_F_help);
        FrequencyBarText();
    } // END Spectrum_zoom

    else
    if (Menu_pointer == MENU_IQ_AMPLITUDE) 
    {
//          K_dirty += (float32_t)encoder2_change / 1000.0;
//          Serial.print("IQ Ampl corr factor:  "); Serial.println(K_dirty * 1000);
//          Serial.print("encoder_change:  "); Serial.println(encoder2_change);
          
          IQ_amplitude_correction_factor += encoder2_change / 4000.0;
//          Serial.print("IQ Ampl corr factor:  "); Serial.println(IQ_amplitude_correction_factor * 1000000);
//          Serial.print("encoder_change:  "); Serial.println(encoder2_change);
    } // END IQadjust
    else 
    if (Menu_pointer == MENU_SAMPLE_RATE) 
    {
          if(encoder2_change < 0) SAMPLE_RATE--;
            else SAMPLE_RATE++;
            wait_flag = 1;    
          AudioNoInterrupts();
          if(SAMPLE_RATE > SAMPLE_RATE_MAX) SAMPLE_RATE = SAMPLE_RATE_MAX;
          if(SAMPLE_RATE < SAMPLE_RATE_MIN) SAMPLE_RATE = SAMPLE_RATE_MIN;
          setI2SFreq (SR[SAMPLE_RATE].rate);
          delay(500);
          IF_FREQ = SR[SAMPLE_RATE].rate / 4;
          // this sets the frequency, but without knowing the IF!
          setfreq();
          prepare_spectrum_display(); // show new frequency scale
          LP_Fpass_old = 0; // cheat the filter_bandwidth function ;-)
          // before calculating the filter, we have to assure, that the filter bandwidth is not larger than
          // sample rate / 19.0
          if(LP_F_help > SR[SAMPLE_RATE].rate / 19.0) LP_F_help = SR[SAMPLE_RATE].rate / 19.0;
          filter_bandwidth(); // calculate new FIR & IIR coefficients according to the new sample rate
          show_bandwidth(band[bands].mode, LP_F_help);
          set_SAM_PLL();
/****************************************************************************************
*  Recalculate decimation and interpolation FIR filters
****************************************************************************************/
          // Decimation filter 1, M1 = 4
          calc_FIR_coeffs (FIR_dec1_coeffs, 50, (float32_t)SR[SAMPLE_RATE].rate / 19.0, 80, 0, 0.0, SR[SAMPLE_RATE].rate);
          // Decimation filter 2, M2 = 2
          calc_FIR_coeffs (FIR_dec2_coeffs, 88, (float32_t)SR[SAMPLE_RATE].rate / 19.0, 80, 0, 0.0, SR[SAMPLE_RATE].rate / 4);
          // Interpolation filter 1, L1 = 2
          calc_FIR_coeffs (FIR_int1_coeffs, 16, (float32_t)SR[SAMPLE_RATE].rate / 19.0, 80, 0, 0.0, SR[SAMPLE_RATE].rate / 4);
          // Interpolation filter 2, L2 = 4
          calc_FIR_coeffs (FIR_int2_coeffs, 16, (float32_t)SR[SAMPLE_RATE].rate / 19.0, 80, 0, 0.0, SR[SAMPLE_RATE].rate);
          AudioInterrupts();
     } 
    else
    if (Menu_pointer == MENU_LPF_SPECTRUM) 
    {
    } // END LPFSPECTRUM
    else 
    if (Menu_pointer == MENU_IQ_PHASE) 
    {
//          P_dirty += (float32_t)encoder2_change / 1000.0;
//          Serial.print("IQ Phase corr factor:  "); Serial.println(P_dirty * 1000);
//          Serial.print("encoder_change:  "); Serial.println(encoder2_change);
          IQ_phase_correction_factor = IQ_phase_correction_factor + (float32_t)encoder2_change / 4000.0;
//          Serial.print("IQ Phase corr factor"); Serial.println(IQ_phase_correction_factor * 1000000);

    } // END IQadjust
    else if (Menu_pointer == MENU_TIME_SET) {
        helpmin = minute(); helphour = hour();
        helpmin = helpmin + encoder2_change;
        if (helpmin > 59) { 
          helpmin = 0; helphour = helphour +1;}
         if (helpmin < 0) {
          helpmin = 59; helphour = helphour -1; }
         if (helphour < 0) helphour = 23; 
        if (helphour > 23) helphour = 0;
        helpmonth = month(); helpyear = year(); helpday = day();
        setTime (helphour, helpmin, 0, helpday, helpmonth, helpyear);      
        Teensy3Clock.set(now()); // set the RTC  
      } // end TIMEADJUST
    else 
    if (Menu_pointer == MENU_DATE_SET) {
      helpyear = year(); 
      helpmonth = month();
      helpday = day();
      helpday = helpday + encoder2_change / 4;
      if (helpday < 1) {helpday=31; helpmonth=helpmonth-1;}
      if (helpday > 31) {helpmonth = helpmonth +1; helpday=1;}
      if (helpmonth < 1) {helpmonth = 12; helpyear = helpyear-1;}
      if (helpmonth > 12) {helpmonth = 1; helpyear = helpyear+1;}
      helphour=hour(); helpmin=minute(); helpsec=second(); 
      setTime (helphour, helpmin, helpsec, helpday, helpmonth, helpyear);      
      Teensy3Clock.set(now()); // set the RTC
      displayDate();
          } // end DATEADJUST

    
        show_menu();
  } // end encoder was turned

    if (encoder3_pos != last_encoder3_pos)
    {
      encoder3_change=(encoder3_pos-last_encoder3_pos);
      last_encoder3_pos=encoder3_pos;
      which_menu = 2;
    if(Menu2 == MENU_RF_GAIN)
    {
      bands[band].RFgain = bands[band].RFgain + encoder3_change / 4;
      if(bands[band].RFgain < 0) bands[band].RFgain = 0;
      if(bands[band].RFgain > 15) bands[band].RFgain = 15;
      sgtl5000_1.lineInLevel(bands[band].RFgain);
    }
    if(Menu2 == MENU_VOLUME)
    {
      audio_volume = audio_volume + encoder3_change;
      if(audio_volume < 0) audio_volume = 0;
      else if(audio_volume > 100) audio_volume = 100;
      sgtl5000_1.volume((float32_t)audio_volume / 100);
    }
    if(Menu2 == MENU_SAM_ZETA)
    {
      zeta_help = zeta_help + (float32_t)encoder3_change / 4.0;
      if(zeta_help < 15) zeta_help = 15;
      else if(zeta_help > 99) zeta_help = 99;
      set_SAM_PLL();
    }
    if(Menu2 == MENU_SAM_OMEGA)
    {
      omegaN = omegaN + (float32_t)encoder3_change * 10 / 4.0;
      if(omegaN < 20.0) omegaN = 20.0;
      else if(omegaN > 1000.0) omegaN = 1000.0;
      set_SAM_PLL();
    }
    if(Menu2 == MENU_SAM_CATCH_BW)
    {
      pll_fmax = pll_fmax + (float32_t)encoder3_change * 100.0 / 4.0;
      if(pll_fmax < 200.0) pll_fmax = 200.0;
      else if(pll_fmax > 6000.0) pll_fmax = 6000.0;
      set_SAM_PLL();
    }
    if(Menu2 == MENU_AGC_MODE)
    {
      AGC_mode = AGC_mode + (float32_t)encoder3_change / 4.0;
      if(AGC_mode > 5) AGC_mode = 5;
        else if (AGC_mode < 0) AGC_mode = 0;
      AGC_prep();  
    }
    else if(Menu2 == MENU_BASS)
    {
      bass = bass + (float32_t)encoder3_change / 80.0;
      if(bass > 1.0) bass = 1.0;
        else if (bass < -1.0) bass = 1.0;
      sgtl5000_1.eqBands (bass, treble); // (float bass, float treble) in % -100 to +100
    }
    else if(Menu2 == MENU_TREBLE)
    {
      treble = treble + (float32_t)encoder3_change / 80.0;
      if(treble > 1.0) treble = 1.0;
        else if (treble < -1.0) treble = - 1.0;
      sgtl5000_1.eqBands (bass, treble); // (float bass, float treble) in % -100 to +100
    }
    
    show_menu();
    }
}

void displayClock() {

  uint8_t hour10 = hour() / 10 % 10;
  uint8_t hour1 = hour() % 10;
  uint8_t minute10 = minute() / 10 % 10;
  uint8_t minute1 = minute() % 10;
  uint8_t second10 = second() / 10 % 10;
  uint8_t second1 = second() % 10;
  uint8_t time_pos_shift = 12; // distance between figures
  uint8_t dp = 7; // distance between ":" and figures

/*  if (mesz != mesz_old && mesz >= 0) {
    tft.setTextColor(ILI9341_ORANGE);
    tft.setFont(Arial_16);    
    tft.setCursor(pos_x_date, pos_y_date+20);
    tft.fillRect(pos_x_date, pos_y_date+20, 150-pos_x_date, 20, ILI9341_BLACK);
    tft.printf((mesz==0)?"(CET)":"(CEST)");
  }
*/
  tft.setFont(Arial_14);
  tft.setTextColor(ILI9341_WHITE);

  // set up ":" for time display
  if (!timeflag) {
    tft.setCursor(pos_x_time + 2 * time_pos_shift, pos_y_time);
    tft.print(":");
    tft.setCursor(pos_x_time + 4 * time_pos_shift + dp, pos_y_time);
    tft.print(":");
    tft.setCursor(pos_x_time + 7 * time_pos_shift + 2 * dp, pos_y_time);
    //      tft.print("UTC");
  }

  if (hour10 != hour10_old || !timeflag) {
    tft.setCursor(pos_x_time, pos_y_time);
    tft.fillRect(pos_x_time, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    if (hour10) tft.print(hour10);  // do not display, if zero
  }
  if (hour1 != hour1_old || !timeflag) {
    tft.setCursor(pos_x_time + time_pos_shift, pos_y_time);
    tft.fillRect(pos_x_time  + time_pos_shift, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    tft.print(hour1);  // always display
  }
  if (minute1 != minute1_old || !timeflag) {
    tft.setCursor(pos_x_time + 3 * time_pos_shift + dp, pos_y_time);
    tft.fillRect(pos_x_time  + 3 * time_pos_shift + dp, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    tft.print(minute1);  // always display
  }
  if (minute10 != minute10_old || !timeflag) {
    tft.setCursor(pos_x_time + 2 * time_pos_shift + dp, pos_y_time);
    tft.fillRect(pos_x_time  + 2 * time_pos_shift + dp, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    tft.print(minute10);  // always display
  }
  if (second10 != second10_old || !timeflag) {
    tft.setCursor(pos_x_time + 4 * time_pos_shift + 2 * dp, pos_y_time);
    tft.fillRect(pos_x_time  + 4 * time_pos_shift + 2 * dp, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    tft.print(second10);  // always display
  }
  if (second1 != second1_old || !timeflag) {
    tft.setCursor(pos_x_time + 5 * time_pos_shift + 2 * dp, pos_y_time);
    tft.fillRect(pos_x_time  + 5 * time_pos_shift + 2 * dp, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    tft.print(second1);  // always display
  }

  hour1_old = hour1;
  hour10_old = hour10;
  minute1_old = minute1;
  minute10_old = minute10;
  second1_old = second1;
  second10_old = second10;
  mesz_old = mesz;
  timeflag = 1;
  tft.setFont(Arial_10);
} // end function displayTime

void displayDate() {
  char string99 [20];
  tft.fillRect(pos_x_date, pos_y_date, 320 - pos_x_date, 20, ILI9341_BLACK); // erase old string
  tft.setTextColor(ILI9341_ORANGE);
  tft.setFont(Arial_12);
  tft.setCursor(pos_x_date, pos_y_date);
  //  Date: %s, %d.%d.20%d P:%d %d", Days[weekday-1], day, month, year
  sprintf(string99, "%s, %02d.%02d.%04d", Days[weekday()], day(), month(), year());
  tft.print(string99);
} // end function displayDate

void set_SAM_PLL() {
// DX adjustments: zeta = 0.15, omegaN = 100.0
// very stable, but does not lock very fast
// standard settings: zeta = 1.0, omegaN = 250.0
// maybe user can choose between slow (DX), medium, fast SAM PLL
// zeta / omegaN
// DX = 0.2, 70
// medium 0.6, 200
// fast 1.2, 500
//float32_t zeta = 0.8; // PLL step response: smaller, slower response 1.0 - 0.1
//float32_t omegaN = 250.0; // PLL bandwidth 50.0 - 1000.0

omega_min = TPI * (-pll_fmax) * DF / SR[SAMPLE_RATE].rate;
omega_max = TPI * pll_fmax * DF / SR[SAMPLE_RATE].rate;
zeta = (float32_t)zeta_help / 100.0; 
g1 = 1.0 - expf(-2.0 * omegaN * zeta * DF / SR[SAMPLE_RATE].rate);
g2 = - g1 + 2.0 * (1 - expf(- omegaN * zeta * DF / SR[SAMPLE_RATE].rate) * cosf(omegaN * DF / SR[SAMPLE_RATE].rate * sqrtf(1.0 - zeta * zeta)));

mtauR = expf(- DF / (SR[SAMPLE_RATE].rate * tauR)); 
onem_mtauR = 1.0 - mtauR;
mtauI = expf(- DF / (SR[SAMPLE_RATE].rate * tauI)); 
onem_mtauI = 1.0 - mtauI;  
}

/*
// Unfortunately, this does not work for SAM PLL phase determination
// AND it is not less computation-intense compared to atan2f
float32_t atan2_fast(float32_t Im, float32_t Re)
{
  float32_t tan_1;
//  float32_t Q_div_I;
  Im = Im + 1e-9;
  Re = Re + 1e-9;
  if(fabs(Im) - fabs(Re) < 0)
  {  
//    Q_div_I = Im / Re;
    // Eq 13-108 in Lyons 2011 
    tan_1 = (Re * Im) / (Re * Re + (0.25 + 0.03125) * Im * Im);
    int8_t sign = Re > 0;
    if(!sign) sign = -1;
    if(Re > 0)
    {
      // 1st/8th octant
      return tan_1;
    }
    else
    {
      // 4th/5th octant
      return (PI * sign + tan_1);      
    }
  }
  else 
  {
    // Eq 13-108 in Lyons 2011 
    tan_1 = (Re * Im) / (Im * Im + (0.25 + 0.03125) * Re * Re);

    if(Im > 0)
    {
      // 2nd/3rd octant
      return (PIH - tan_1);
    }
    else
    {
      // 6th/7th octant
      return (- PIH - tan_1);
    }
  }
}
*/
void playFileMP3(const char *filename)
{
  trackchange = true; //auto track change is allowed.
  // Start playing the file.  This sketch continues to
  // run while the file plays.
  printTrack();
  EEPROM.write(eeprom_adress,track); //meanwhile write the track position to eeprom address 0
  Serial.println("After EEPROM.write");
  playMp3.play(filename);
  // Simply wait for the file to finish playing.
  while (playMp3.isPlaying()) {
  show_load();
  buttons();
  encoders();
  displayClock();
  }
}

void playFileAAC(const char *filename)
{
  trackchange = true; //auto track change is allowed.
  // Start playing the file.  This sketch continues to
  // run while the file plays.
  // print track no & trackname
  printTrack ();
  EEPROM.write(eeprom_adress,track); //meanwhile write the track position to eeprom address 0
  playAac.play(filename);
  // Simply wait for the file to finish playing.
  while (playAac.isPlaying()) {
    // update controls!
  show_load();  
  buttons();
  encoders();
  displayClock();  
  }
}
void nexttrack(){
  Serial.println("Next track!");
  trackchange=false; // we are doing a track change here, so the auto trackchange will not skip another one.
  playMp3.stop();
  playAac.stop();
  track++;
  if(track >= tracknum){ // keeps in tracklist.
    track = 0;
  }  
  tracklist[track].toCharArray(playthis, sizeof(tracklist[track])); //since we have to convert String to Char will do this    
}

void prevtrack(){
  Serial.println("Previous track! ");
  trackchange=false; // we are doing a track change here, so the auto trackchange will not skip another one.
  playMp3.stop();
  playAac.stop();
  track--;
  if(track <0){ // keeps in tracklist.
    track = tracknum-1;
  }  
  tracklist[track].toCharArray(playthis, sizeof(tracklist[track])); //since we have to convert String to Char will do this    
}

void pausetrack(){
  paused = !paused;
    playMp3.pause(paused);
    playAac.pause(paused);
}


void randomtrack(){
  Serial.println("Random track!");
  trackchange=false; // we are doing a track change here, so the auto trackchange will not skip another one.
  if(trackext[track] == 1) playMp3.stop();
  if(trackext[track] == 2) playAac.stop();

  track= random(tracknum);

  tracklist[track].toCharArray(playthis, sizeof(tracklist[track])); //since we have to convert String to Char will do this    
}

void printTrack () {
  tft.fillRect(0,222,320,17,ILI9341_BLACK);
  tft.setCursor(0, 222);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextWrap(true);
  tft.setTextSize(2);
  tft.print("Track: ");
  tft.print (track); 
  tft.print (" "); tft.print (playthis);
 } //end printTrack

void show_load(){
            if (five_sec.check() == 1)
    {
      Serial.print("Proc = ");
      Serial.print(AudioProcessorUsage());
      Serial.print(" (");    
      Serial.print(AudioProcessorUsageMax());
      Serial.print("),  Mem = ");
      Serial.print(AudioMemoryUsage());
      Serial.print(" (");    
      Serial.print(AudioMemoryUsageMax());
      Serial.println(")");
      AudioProcessorUsageMaxReset();
      AudioMemoryUsageMaxReset();
    }
}

float32_t sign(float32_t x) {
  if(x < 0)
    return -1.0f;
    else
      if(x > 0)
        return 1.0f;
          else return 0.0f;
}

void Calculatedbm()
{
// calculation of the signal level inside the filter bandwidth
// taken from the spectrum display FFT
// taking into account the analog gain before the ADC
// analog gain is adjusted in steps of 1.5dB
// bands[band].RFgain = 0 --> 0dB gain
// bands[band].RFgain = 15 --> 22.5dB gain
            
            float32_t slope = 19.8; // 
            float32_t cons = -92; // 
            float32_t Lbin, Ubin;
            float32_t bw_LSB = 0.0;
            float32_t bw_USB = 0.0;
            float64_t sum_db = 0.0;
            int posbin = 0;
            float32_t bin_BW = (float32_t) (SR[SAMPLE_RATE].rate / 256.0);
            // width of a 256 tap FFT bin @ 96ksps = 375Hz
            // we have to take into account the magnify mode
            // --> recalculation of bin_BW
            bin_BW = bin_BW / (1 << spectrum_zoom); // correct bin bandwidth is determined by the Zoom FFT display setting
//            Serial.print("bin_BW = "); Serial.println(bin_BW);

            // in all magnify cases (2x up to 16x) the posbin is in the centre of the spectrum display
            if(spectrum_zoom != 0)
            {
                posbin = 128; // right in the middle!
            } 
            else 
            {
                posbin = 64;
            }

            //  determine Lbin and Ubin from ts.dmod_mode and FilterInfo.width
            //  = determine bandwith separately for lower and upper sideband

            switch(band[bands].mode)
            {
            case DEMOD_LSB:
            case DEMOD_SAM_LSB:
                bw_USB = 0.0;
                bw_LSB = (float32_t)LP_F_help;
                break;
            case DEMOD_USB:
            case DEMOD_SAM_USB:
                bw_LSB = 0.0;
                bw_USB = (float32_t)LP_F_help;
                break;
            default:
                bw_LSB = (float32_t)LP_F_help;
                bw_USB = (float32_t)LP_F_help;
            }
            // calculate upper and lower limit for determination of signal strength
            // = filter passband is between the lower bin Lbin and the upper bin Ubin
            Lbin = (float32_t)posbin - roundf(bw_LSB / bin_BW);
            Ubin = (float32_t)posbin + roundf(bw_USB / bin_BW); // the bin on the upper sideband side

            // take care of filter bandwidths that are larger than the displayed FFT bins
            if(Lbin < 0)
            {
                Lbin = 0;
            }
            if (Ubin > 255)
            {
                Ubin = 255;
            }
            //Serial.print("Lbin = "); Serial.println(Lbin);
            //Serial.print("Ubin = "); Serial.println(Ubin);
            if((int)Lbin == (int)Ubin) 
            {
                Ubin = 1.0 + Lbin;
            }
            // determine the sum of all the bin values in the passband
            for (int c = (int)Lbin; c <= (int)Ubin; c++)   // sum up all the values of all the bins in the passband
            {
//                sum_db = sum_db + FFT_spec[c];
                sum_db = sum_db + FFT_spec_old[c];
            }

            if (sum_db > 0)
            {
                dbm = slope * log10f (sum_db) + cons - (float32_t)bands[band].RFgain * 1.5;
                dbmhz = slope * log10f (sum_db) -  10 * log10f ((float32_t)(((int)Ubin-(int)Lbin) * bin_BW)) + cons;
            }
            else
            {
                dbm = -145.0;
                dbmhz = -145.0;
            }

            // lowpass IIR filter
            // Wheatley 2011: two averagers with two time constants
            // IIR filter with one element analog to 1st order RC filter
            // but uses two different time constants (ALPHA = 1 - e^(-T/Tau)) depending on
            // whether the signal is increasing (attack) or decreasing (decay)
            // m_AttackAlpha = 0.8647; //  ALPHA = 1 - e^(-T/Tau), T = 0.02s (because dbm routine is called every 20ms!)
            // Tau = 10ms = 0.01s attack time
            // m_DecayAlpha = 0.0392; // 500ms decay time
            //
            m_AttackAvedbm = (1.0 - m_AttackAlpha) * m_AttackAvedbm + m_AttackAlpha * dbm;
            m_DecayAvedbm = (1.0 - m_DecayAlpha) * m_DecayAvedbm + m_DecayAlpha * dbm;
            m_AttackAvedbmhz = (1.0 - m_AttackAlpha) * m_AttackAvedbmhz + m_AttackAlpha * dbmhz;
            m_DecayAvedbmhz = (1.0 - m_DecayAlpha) * m_DecayAvedbmhz + m_DecayAlpha * dbmhz;

            if (m_AttackAvedbm > m_DecayAvedbm)
            { // if attack average is larger then it must be an increasing signal
                m_AverageMagdbm = m_AttackAvedbm; // use attack average value for output
                m_DecayAvedbm = m_AttackAvedbm; // set decay average to attack average value for next time
            }
            else
            { // signal is decreasing, so use decay average value
                m_AverageMagdbm = m_DecayAvedbm;
            }

            if (m_AttackAvedbmhz > m_DecayAvedbmhz)
            { // if attack average is larger then it must be an increasing signal
                m_AverageMagdbmhz = m_AttackAvedbmhz; // use attack average value for output
                m_DecayAvedbmhz = m_AttackAvedbmhz; // set decay average to attack average value for next time
            }
            else
            { // signal is decreasing, so use decay average value
                m_AverageMagdbmhz = m_DecayAvedbmhz;
            }

            dbm = m_AverageMagdbm; // write average into variable for S-meter display
            dbmhz = m_AverageMagdbmhz; // write average into variable for S-meter display
 
}

void Display_dbm()
{
    static int dbm_old = (int)dbm;
    bool display_something = false;
        long val;
        const char* unit_label;
        float32_t dbuv;

        switch(display_dbm)
        {
        case DISPLAY_S_METER_DBM:
            display_something = true;
            val = dbm;
            unit_label = "dBm   ";
            break;
        case DISPLAY_S_METER_DBMHZ:
            display_something = true;
            val = dbmhz;
            unit_label = "dBm/Hz";
            break;
        }
        if((int) dbm == dbm_old) display_something = false;
        
        if (display_something == true) 
        {        
            tft.fillRect(pos_x_dbm, pos_y_dbm, 100, 16, ILI9341_BLACK);
            char txt[12];
            snprintf(txt,12,"%4ld      ", val);
            tft.setFont(Arial_14);
            tft.setCursor(pos_x_dbm, pos_y_dbm);
            tft.setTextColor(ILI9341_WHITE);
            tft.print(txt);
            tft.setFont(Arial_9);
            tft.setCursor(pos_x_dbm + 42, pos_y_dbm + 5);
            tft.setTextColor(ILI9341_GREEN);
            tft.print(unit_label);
            dbm_old = (int)dbm;
            
            float32_t s = 9.0 + ((dbm + 73.0) / 6.0);
            if (s <0.0) s=0.0;
            if ( s > 9.0)
            {
              dbuv = dbm + 73.0;
              s = 9.0;
            }
            else dbuv = 0.0;
           uint8_t pos_sxs_w = pos_x_smeter + s * s_w + 2;
           uint8_t sxs_w = s * s_w + 2;
           uint8_t nine_sw_minus = (9*s_w + 1) - s * s_w; 
//            tft.drawFastHLine(pos_x_smeter, pos_y_smeter, s*s_w+1, ILI9341_BLUE);
//            tft.drawFastHLine(pos_x_smeter+s*s_w+1, pos_y_smeter, nine_sw_minus, ILI9341_BLACK);

            tft.drawFastHLine(pos_x_smeter + 1, pos_y_smeter + 1, sxs_w, ILI9341_GREENYELLOW);
            tft.drawFastHLine(pos_sxs_w, pos_y_smeter+1, nine_sw_minus, ILI9341_BLACK);
            tft.drawFastHLine(pos_x_smeter + 1, pos_y_smeter + 2, sxs_w, ILI9341_GREENYELLOW);
            tft.drawFastHLine(pos_sxs_w, pos_y_smeter+2, nine_sw_minus, ILI9341_BLACK);
            tft.drawFastHLine(pos_x_smeter + 1, pos_y_smeter + 3, sxs_w, ILI9341_GREENYELLOW);
            tft.drawFastHLine(pos_sxs_w, pos_y_smeter+3, nine_sw_minus, ILI9341_BLACK);
            tft.drawFastHLine(pos_x_smeter + 1, pos_y_smeter + 4, sxs_w, ILI9341_GREENYELLOW);
            tft.drawFastHLine(pos_sxs_w, pos_y_smeter+4, nine_sw_minus, ILI9341_BLACK);
//            tft.drawFastHLine(pos_x_smeter, pos_y_smeter+5, sxs_w, ILI9341_BLUE);
//            tft.drawFastHLine(pos_x_smeter+s*s_w+1, pos_y_smeter+5, nine_sw_minus, ILI9341_BLACK);
      
            if(dbuv>30) dbuv=30;
            if(dbuv > 0)
            {
                uint8_t dbuv_div_x = (dbuv/5)*s_w+1;
                uint8_t six_sw_minus = (6*s_w+1)-(dbuv/5)*s_w;
                uint8_t nine_sw_plus_x = pos_x_smeter + 9*s_w + (dbuv/5) * s_w + 1;
                uint8_t nine_sw_plus = pos_x_smeter + 9*s_w + 1;
                tft.drawFastHLine(nine_sw_plus, pos_y_smeter, dbuv_div_x, ILI9341_RED);
                tft.drawFastHLine(nine_sw_plus_x, pos_y_smeter, six_sw_minus, ILI9341_BLACK);
                tft.drawFastHLine(nine_sw_plus, pos_y_smeter + 1, dbuv_div_x, ILI9341_RED);
                tft.drawFastHLine(nine_sw_plus_x, pos_y_smeter + 1, six_sw_minus, ILI9341_BLACK);
                tft.drawFastHLine(nine_sw_plus, pos_y_smeter + 2, dbuv_div_x, ILI9341_RED);
                tft.drawFastHLine(nine_sw_plus_x, pos_y_smeter + 2, six_sw_minus, ILI9341_BLACK);
                tft.drawFastHLine(nine_sw_plus, pos_y_smeter + 3, dbuv_div_x, ILI9341_RED);
                tft.drawFastHLine(nine_sw_plus_x, pos_y_smeter + 3, six_sw_minus, ILI9341_BLACK);
                tft.drawFastHLine(nine_sw_plus, pos_y_smeter + 4, dbuv_div_x, ILI9341_RED);
                tft.drawFastHLine(nine_sw_plus_x, pos_y_smeter + 4, six_sw_minus, ILI9341_BLACK);
                tft.drawFastHLine(nine_sw_plus, pos_y_smeter + 5, dbuv_div_x, ILI9341_RED);
                tft.drawFastHLine(nine_sw_plus_x, pos_y_smeter + 5, six_sw_minus, ILI9341_BLACK);
            }
            }
    
}

void EEPROMLOAD() {

   struct config_t {
       unsigned long long calibration_factor;
       long calibration_constant;
       unsigned long long freq[NUM_BANDS];
       int mode[NUM_BANDS];
       int bwu[NUM_BANDS];
       int bwl[NUM_BANDS];
       int rfg[NUM_BANDS];
       int band;
//       float I_ampl;
//       float Q_in_I;
//       float I_in_Q;
//       int Window_FFT;
       float32_t LPFcoeff;
       int audio_volume;
       int8_t AGC_mode;
       float32_t pll_fmax;
       float32_t omegaN;
       int zeta_help;
 } E; 

    eeprom_read_block(&E,0,sizeof(E));
    calibration_factor = E.calibration_factor;
    calibration_constant = E.calibration_constant;
    for (int i=0; i< (NUM_BANDS); i++) 
        bands[i].freq = E.freq[i];
    for (int i=0; i< (NUM_BANDS); i++)
        bands[i].mode = E.mode[i];
    for (int i=0; i< (NUM_BANDS); i++)
        bands[i].bandwidthU = E.bwu[i];
    for (int i=0; i< (NUM_BANDS); i++)
        bands[i].bandwidthL = E.bwl[i];
    for (int i=0; i< (NUM_BANDS); i++)
        bands[i].RFgain = E.rfg[i];
    band = E.band;
    //I_help = E.I_ampl;
    //Q_in_I_help = E.Q_in_I;
    //I_in_Q_help = E.I_in_Q;
    //Window_FFT = E.Window_FFT;
     LPF_spectrum = E.LPFcoeff;
     audio_volume = E.audio_volume;
     AGC_mode = E.AGC_mode;
     pll_fmax = E.pll_fmax;
     omegaN = E.omegaN;
     zeta_help = E.zeta_help;
} // end void eeProm LOAD 

void EEPROMSAVE() {

  struct config_t {
       unsigned long long calibration_factor;
       long calibration_constant;
       unsigned long long freq[NUM_BANDS];
       int mode[NUM_BANDS];
       int bwu[NUM_BANDS];
       int bwl[NUM_BANDS];
       int rfg[NUM_BANDS];
       int band;
//       float I_ampl;
//       float Q_in_I;
//       float I_in_Q;
//       int Window_FFT;
       float32_t LPFcoeff;
       int audio_volume;
       int8_t AGC_mode;
       float32_t pll_fmax;
       float32_t omegaN;
       int zeta_help;
 } E; 

      E.calibration_factor = calibration_factor;
      E.band = band;
      E.calibration_constant = calibration_constant;
      for (int i=0; i< (NUM_BANDS); i++) 
        E.freq[i] = bands[i].freq;
      for (int i=0; i< (NUM_BANDS); i++)
        E.mode[i] = bands[i].mode;
      for (int i=0; i< (NUM_BANDS); i++)
        E.bwu[i] = bands[i].bandwidthU;
      for (int i=0; i< (NUM_BANDS); i++)
        E.bwl[i] = bands[i].bandwidthL;
      for (int i=0; i< (NUM_BANDS); i++)
        E.rfg[i] = bands[i].RFgain;
//      E.I_ampl = I_help;
//      E.Q_in_I = Q_in_I_help;
//      E.I_in_Q = I_in_Q_help;
//      E.Window_FFT = Window_FFT;
//      E.LPFcoeff = LPFcoeff;
     E.LPFcoeff = LPF_spectrum;
     E.audio_volume = audio_volume;
     E.AGC_mode = AGC_mode;
     E.pll_fmax = pll_fmax;
     E.omegaN = omegaN;
     E.zeta_help = zeta_help;
      eeprom_write_block (&E,0,sizeof(E));
} // end void eeProm SAVE 



