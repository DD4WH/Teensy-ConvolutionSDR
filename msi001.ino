#include "msi001.h"
#include <SPI.h>

#define MIRISDR_DEBUG 0
//#define Fif1  120000000UL               //1st IF Frequency (used to calculate backwords the frequency => FLO = FC +FIF MHz for non AM modes and FLO = FC +FIF + FIF1 MHz for AM modes.) (Xtal = 24MHz)
#define Fif1  122880000UL               //1st IF Frequency (used to calculate backwords the frequency => FLO = FC +FIF MHz for non AM modes and FLO = FC +FIF + FIF1 MHz for AM modes.) (Xtal = 24.576MHz) 


char buffer[100];

const int msi001_cs_pin = 1;
const int msi001_dat_pin = 2;
const int msi001_clk_pin = 3;

const int CS_SR_595 = 31;     //chip select shift register 595
const int CS_SR_595_2 = 33;     //chip select shift register 595 on the external board

#define clamp(val, lo, hi) min((typeof(val))max(val, lo), hi)

typedef enum
{
  MIRISDR_HW_DEFAULT,
  MIRISDR_HW_SDRPLAY,
} mirisdr_hw_flavour_t;

typedef enum
{
  MIRISDR_BAND_AM1,
  MIRISDR_BAND_AM2,
  MIRISDR_BAND_VHF,
  MIRISDR_BAND_3,
  MIRISDR_BAND_45,
  MIRISDR_BAND_L,
} mirisdr_band_t;


typedef  enum {
  MIRISDR_BW_200KHZ = 0,
  MIRISDR_BW_300KHZ,
  MIRISDR_BW_600KHZ,
  MIRISDR_BW_1536KHZ,
  MIRISDR_BW_5MHZ,
  MIRISDR_BW_6MHZ,
  MIRISDR_BW_7MHZ,
  MIRISDR_BW_8MHZ
} bandwidth_enum;
typedef  enum {
  MIRISDR_IF_ZERO = 0,
  MIRISDR_IF_450KHZ,
  MIRISDR_IF_1620KHZ,
  MIRISDR_IF_2048KHZ
} if_freq_enum;
typedef  enum {
  MIRISDR_XTAL_19_2M = 0,
  MIRISDR_XTAL_22M,
  MIRISDR_XTAL_24M,
  MIRISDR_XTAL_24_576M,
  MIRISDR_XTAL_26M,
  MIRISDR_XTAL_38_4M
} xtal_enum;

struct mirisdr_dev {

  /* parametry */
  uint32_t            index;
  uint32_t            freq;
  uint32_t            rate;
  int                 gain;
  int                 gain_reduction_lna;
  int                 gain_reduction_mixbuffer;
  int                 gain_reduction_mixer;
  int                 gain_reduction_baseband;
  mirisdr_hw_flavour_t hw_flavour;
  mirisdr_band_t      band;
  bandwidth_enum      bandwidth;
  if_freq_enum        if_freq;
  xtal_enum           xtal;

};

typedef struct
{
  uint32_t low_cut;
  int mode;
  int upconvert_mixer_on;
  int am_port;
  int lo_div;
  uint32_t band_select_word;
} hw_switch_freq_plan_t;

hw_switch_freq_plan_t hw_switch_freq_plan_default[] = {
  {0,    MIRISDR_MODE_AM,  MIRISDR_UPCONVERT_MIXER_ON, MIRISDR_AM_PORT2, 16, 0xf780},
  {12,   MIRISDR_MODE_AM,  MIRISDR_UPCONVERT_MIXER_ON, MIRISDR_AM_PORT2, 16, 0xff80},
  {30,   MIRISDR_MODE_AM,  MIRISDR_UPCONVERT_MIXER_ON, MIRISDR_AM_PORT2, 16, 0xf280},
  {50,   MIRISDR_MODE_VHF, 0, 0, 32, 0xf380},
  {108,  MIRISDR_MODE_B3,  0, 0, 16, 0xfa80},
  {250,  MIRISDR_MODE_B3,  0, 0, 16, 0xf680},
  {330,  MIRISDR_MODE_B45, 0, 0, 4,  0xf380},
  {960,  MIRISDR_MODE_BL,  0, 0, 2,  0xfa80},
  {2400, -1, 0, 0, 0, 0x0000},
};

hw_switch_freq_plan_t hw_switch_freq_plan_sdrplay[] = {
  {0,    MIRISDR_MODE_AM,  MIRISDR_UPCONVERT_MIXER_ON, MIRISDR_AM_PORT2, 16, 0xf780},
  {12,   MIRISDR_MODE_AM,  MIRISDR_UPCONVERT_MIXER_ON, MIRISDR_AM_PORT2, 16, 0xff80},
  {30,   MIRISDR_MODE_AM,  MIRISDR_UPCONVERT_MIXER_ON, MIRISDR_AM_PORT2, 16, 0xf280},
  {50,   MIRISDR_MODE_VHF, 0, 0, 32, 0xf380},
  {120,  MIRISDR_MODE_B3,  0, 0, 16, 0xfa80},
  {250,  MIRISDR_MODE_B3,  0, 0, 16, 0xf680},
  //        {250,  MIRISDR_MODE_AM,  MIRISDR_UPCONVERT_MIXER_OFF, MIRISDR_AM_PORT2, 16}, // Really ???
  {380,  MIRISDR_MODE_B45, 0, 0, 4,  0xf380},
  {1000, MIRISDR_MODE_BL,  0, 0, 2,  0xfa80},
  {2400, -1, 0, 0, 0, 0x0000},
};

hw_switch_freq_plan_t *hw_switch_freq_plan[2] = {
  hw_switch_freq_plan_default,
  hw_switch_freq_plan_sdrplay
};

mirisdr_dev p;

void spi_transfer(byte working) {

  ; // function to actually bit shift the data byte out


  for (int i = 1; i <= 8; i++) { // setup a loop of 8 iterations, one for each bit

    if (working > 127) { // test the most significant bit

      digitalWrite (msi001_dat_pin, HIGH); // if it is a 1 (ie. B1XXXXXXX), set the master out pin high
    }

    else {

      digitalWrite (msi001_dat_pin, LOW); // if it is not 1 (ie. B0XXXXXXX), set the master out pin low
    }

    digitalWrite (msi001_clk_pin, HIGH); // set clock high, the pot IC will read the bit into its register
    //digitalWrite(msi001_clk_pin, LOW); // set clock low, the pot IC will stop reading and prepare for the next iteration (next significant bit
    //delayMicroseconds(1);
    
    working = working << 1;

    digitalWrite(msi001_clk_pin, LOW); // set clock low, the pot IC will stop reading and prepare for the next iteration (next significant bit
    //digitalWrite (msi001_clk_pin, HIGH); // set clock high, the pot IC will read the bit into its register
    //delayMicroseconds(1);
  }
}


void mirisdr_write_reg (uint32_t val) {

  uint8_t a = (val & 0xFF); //extract first byte
  uint8_t b = ((val >> 8) & 0xFF); //extract second byte
  uint8_t c = ((val >> 16) & 0xFF); //extract third byte

  //  Serial.print("SPI transfer: ");
  //  Serial.print(val, BIN);
  //  Serial.print("     ");
  //  Serial.print(c, BIN);
  //  Serial.print(":");
  //  Serial.print(b, BIN);
  //  Serial.print(":");
  //  Serial.println(a, BIN);

  digitalWrite(msi001_cs_pin, LOW);

  spi_transfer(c);
  spi_transfer(b);
  spi_transfer(a);

  digitalWrite(msi001_cs_pin, HIGH);

}

int mirisdr_set_soft()
{
  uint32_t XtalFreq;
  uint32_t reg0 = 0, reg2 = 2, reg3 = 3, reg5 = 5;
  uint64_t n, thresh, frac, lo_div = 0, fvco = 0, offset = 0, a, b, c;
  int i;

  /*** registr0 - parameters zone ***/

  /* zone */

  i = 0;

  while (p.freq >= 1000000 * hw_switch_freq_plan[(int) p.hw_flavour][i].low_cut)
  {
    if (hw_switch_freq_plan[(int) p.hw_flavour][i].mode < 0) {
      break;
    }

    i++;
  }

  hw_switch_freq_plan_t switch_plan = hw_switch_freq_plan[(int) p.hw_flavour][i - 1];


  //  uint32_t low_cut;
  //  int mode;
  //  int upconvert_mixer_on;
  //  int am_port;
  //  int lo_div;
  //  uint32_t band_select_word;

#if MIRISDR_DEBUG >= 1
  Serial.print("Frequency: ");
  Serial.println(p.freq);
  sprintf(buffer, "mirisdr_set_soft: i:%d flavour:%d flow:%u mode:%d up:%d port:%d lo:%d band_select_word:%x\n",
          i - 1,
          (int) p.hw_flavour,
          switch_plan.low_cut,
          switch_plan.mode,
          switch_plan.upconvert_mixer_on,
          switch_plan.am_port,
          switch_plan.lo_div,
          switch_plan.band_select_word);
  Serial.println(buffer);
#endif

  if (switch_plan.mode == MIRISDR_MODE_AM)
  {
    reg0 |= MIRISDR_MODE_AM << 4;
    reg0 |= switch_plan.upconvert_mixer_on << 9;
    reg0 |= switch_plan.am_port << 11;

    if (switch_plan.upconvert_mixer_on)
    {
      offset += Fif1;
    }

    lo_div = 16;

    if (switch_plan.am_port == 0) {
      p.band = MIRISDR_BAND_AM1;
    } else {
      p.band = MIRISDR_BAND_AM2;
    }
  }
  else
  {
    reg0 |= switch_plan.mode << 4;
    lo_div = switch_plan.lo_div;

    if (switch_plan.mode == MIRISDR_MODE_VHF) {
      p.band = MIRISDR_BAND_VHF;
    } else if (switch_plan.mode == MIRISDR_MODE_B3) {
      p.band = MIRISDR_BAND_3;
    } else if (switch_plan.mode == MIRISDR_MODE_B45) {
      p.band = MIRISDR_BAND_45;
    } else if (switch_plan.mode == MIRISDR_MODE_BL) {
      p.band = MIRISDR_BAND_L;
    }
  }

  /* RF synthesizer is always active */
  reg0 |= MIRISDR_RF_SYNTHESIZER_ON << 10;

  /* IF filter mode - has not worked? */
  switch (p.if_freq)
  {
    case MIRISDR_IF_ZERO:
      reg0 |= MIRISDR_IF_MODE_ZERO << 12;
      break;
    case MIRISDR_IF_450KHZ:
      reg0 |= MIRISDR_IF_MODE_450KHZ << 12;
      break;
    case MIRISDR_IF_1620KHZ:
      reg0 |= MIRISDR_IF_MODE_1620KHZ << 12;
      break;
    case MIRISDR_IF_2048KHZ:
      reg0 |= MIRISDR_IF_MODE_2048KHZ << 12;
      break;
  }

  /* Bandwidth - 8 MHz, the highest possible */
  switch (p.bandwidth)
  {
    case MIRISDR_BW_200KHZ:
      reg0 |= 0x00 << 14;
      break;
    case MIRISDR_BW_300KHZ:
      reg0 |= 0x01 << 14;
      break;
    case MIRISDR_BW_600KHZ:
      reg0 |= 0x02 << 14;
      break;
    case MIRISDR_BW_1536KHZ:
      reg0 |= 0x03 << 14;
      break;
    case MIRISDR_BW_5MHZ:
      reg0 |= 0x04 << 14;
      break;
    case MIRISDR_BW_6MHZ:
      reg0 |= 0x05 << 14;
      break;
    case MIRISDR_BW_7MHZ:
      reg0 |= 0x06 << 14;
      break;
    case MIRISDR_BW_8MHZ:
      reg0 |= 0x07 << 14;
      break;
  }


/* xtal frequency - we do not support change */
    switch (p.xtal)
    {
       case MIRISDR_XTAL_19_2M:
        XtalFreq = 19200000;
      break;
      case MIRISDR_XTAL_22M:
        XtalFreq = 22000000;
      break;
      case MIRISDR_XTAL_24M:
        XtalFreq = 24000000;
      break;
      case MIRISDR_XTAL_24_576M:
        XtalFreq = 24576000;
      break;
      case MIRISDR_XTAL_26M:
        XtalFreq = 26000000;
      break;
      case MIRISDR_XTAL_38_4M:
        XtalFreq = 38400000;
       break;
    }
  

  //  /* xtal frequency - we do not support change */
  //  switch (p.xtal)
  //  {
  //    case MIRISDR_XTAL_19_2M:
  //      reg0 |= 0x00 << 17;
  //      break;
  //    case MIRISDR_XTAL_22M:
  //      reg0 |= 0x01 << 17;
  //      break;
  //    case MIRISDR_XTAL_24M:
  //    case MIRISDR_XTAL_24_576M:
  //      reg0 |= 0x02 << 17;
  //      break;
  //    case MIRISDR_XTAL_26M:
  //      reg0 |= 0x03 << 17;
  //      break;
  //    case MIRISDR_XTAL_38_4M:
  //      reg0 |= 0x04 << 17;
  //      break;
  //  }

  reg0 |= 0x02 << 17; // problema con el XTAL no sé por qué

  /* 4 bits for power saving modes */
  reg0 |= MIRISDR_IF_LPMODE_NORMAL << 20;
  reg0 |= MIRISDR_VCO_LPMODE_NORMAL << 23;

  /* VCO frequency is better to use a 64-bit range */
  fvco = (p.freq + offset) * lo_div;
  
  /* shift the main frequency */
  //n = fvco / 88000000UL;
  //n = fvco / 96000000UL;                      //96000000UL - 4 times the clock (24Mhz)
  n = fvco / 98304000UL;                      //98304000UL - 4 times the clock (24.576Mhz)

  /* major registry, coarse tuning */
  //thresh = 88000000UL / lo_div;
  //thresh = 96000000UL / lo_div;                //96000000UL - 4 times the clock (24Mhz)
  thresh = 98304000UL / lo_div;                //98304000UL - 4 times the clock (24.576Mhz)

  /* side register, fine tuning */
  //frac = (fvco % 88000000UL) / lo_div;
  //frac = (fvco % 96000000UL) / lo_div;        //96000000UL - 4 times the clock  (24Mhz)
  frac = (fvco % 98304000UL) / lo_div;        //98304000‬UL - 4 times the clock  (24.576Mhz)


  /* We find the greatest common divisor for thresh and frac */
   for (a = thresh, b = frac; a != 0;)
  {
    c = a;
    a = b % a;
    b = c;
  }

  /* divided */
  thresh /= b;
  frac /= b;
  
  /* In this section we reduce the resolution to the maximum extent registry */
  a = (thresh + 4094) / 4095;
  thresh = (thresh + (a / 2)) / a;
  frac = (frac + (a / 2)) / a;

  //Calculate the actual Flo basd on the upper determined coeficients ("frac" and "thresh") (calculate without the fine tuning value - AFC)
  uint64_t Mult = (4*XtalFreq)/lo_div;
  uint64_t Flo = Mult*n + (Mult*frac)/thresh;

  if (switch_plan.upconvert_mixer_on)   //FLO = FC +FIF MHz for non AM modes and FLO = FC +FIF + FIF1 MHz for AM modes
    {
      Flo -= Fif1;
    }

  int DeltaF = p.freq - Flo;           //Calculate the diference between the "set" and the "result"(without fine correction) frequency 

  if (DeltaF<0)                       //If the difference is lower than 0 => the result frequency is higher than the "set" (no able to reduce with the fine tuning AFC) 
    {                                 //reduce frac with 1 to be able to add fine tuning (AFC) 
    frac--;
    //Recalculate the Flo again
    Mult = (4*XtalFreq)/lo_div;
    Flo = Mult*n + (Mult*frac)/thresh;
    if (switch_plan.upconvert_mixer_on)   //FLO = FC +FIF MHz for non AM modes and FLO = FC +FIF + FIF1 MHz for AM modes
      {
      Flo -= Fif1;
      }
    DeltaF = p.freq - Flo;            //Recalculate the delta again
    }

  uint64_t Fstep = (4*XtalFreq)/(lo_div*thresh);    //calculate the Fstep

  uint32_t AFC = (4096*DeltaF)/Fstep;               //Calculate the needed fine tune freq. to match the set frequency

#if MIRISDR_DEBUG >= 1
  Serial.print("N:");                               //Print the n = INT "INT parameter is a 6 bit word in register 2 and controls the integer division of the synthesizer"
  Serial.println((0x3F & uint32_t(n)));
  
  Serial.print("LoDiv: ");                          //Print LO division ratio
  Serial.println(uint32_t(lo_div));
  
  Serial.print("Flo: ");                              //!!!Print the Flo value without the fine tuning correction!!!!
  Serial.println(uint32_t(Flo));
  Serial.print("Fstep: ");                            //Print the Fstep value (used  to calculate the fine tuning value)
  Serial.println(uint32_t(Fstep));                    
  Serial.print("DeltaF: ");                           //Print the difference between the "Set value" with the "real value" without the finetuning value
  Serial.println(DeltaF);
   Serial.print("AFC: ");                             //Print fine tuning value 
  Serial.println(AFC);
#endif

  reg5 |= (0xFFF & thresh) << 4;
  /* Reserved, must be 0x28 */
  reg5 |= MIRISDR_RF_SYNTHESIZER_RESERVED_PROGRAMMING << 16;

  reg2 |= (0xFFF & frac) << 4;
  reg2 |= (0x3F & n) << 16;
  reg2 |= MIRISDR_LBAND_LNA_CALIBRATION_OFF << 22;

  
  reg3 |= (0xFFF & AFC) << 4;           //Set the fine tuning word in register 3
  
  
  /* kernel driver adjusts to changing frequencies  */

  mirisdr_write_reg( 0xf380);
  mirisdr_write_reg( 0x6280);
  mirisdr_write_reg( switch_plan.band_select_word);

  mirisdr_write_reg( 0x0e); //OK
  mirisdr_write_reg( 0x03); //OK

  mirisdr_write_reg( reg0);
  mirisdr_write_reg( reg3);
  mirisdr_write_reg( reg5);
  mirisdr_write_reg( reg2);

  return 0;
}

int mirisdr_set_center_freq(uint32_t freq)
{
  p.freq = freq;
  mirisdr_set_soft();
  mirisdr_set_gain(); // restore gain
  return 0;
}



int mirisdr_set_if_freq(uint32_t freq)
{
  switch (freq)
  {
    case 0:
      p.if_freq = MIRISDR_IF_ZERO;
      break;
    case 450000:
      p.if_freq = MIRISDR_IF_450KHZ;
      break;
    case 1620000:
      p.if_freq = MIRISDR_IF_1620KHZ;
      break;
    case 2048000:
      p.if_freq = MIRISDR_IF_2048KHZ;
      break;
    default:
      break;
  }

  mirisdr_set_soft();
  mirisdr_set_gain(); // restore gain
  return 0;
}

/* not supported yet */
int mirisdr_set_xtal_freq(uint32_t freq)
{
  (void) p;
  (void) freq;
  return -1;
}

uint32_t mirisdr_set_bandwidth(uint32_t bw)
{
  switch (bw)
  {
    case 200000:
      p.bandwidth = MIRISDR_BW_200KHZ;
      break;
    case 300000:
      p.bandwidth = MIRISDR_BW_300KHZ;
      break;
    case 600000:
      p.bandwidth = MIRISDR_BW_600KHZ;
      break;
    case 1536000:
      p.bandwidth = MIRISDR_BW_1536KHZ;
      break;
    case 5000000:
      p.bandwidth = MIRISDR_BW_5MHZ;
      break;
    case 6000000:
      p.bandwidth = MIRISDR_BW_6MHZ;
      break;
    case 7000000:
      p.bandwidth = MIRISDR_BW_7MHZ;
      break;
    case 8000000:
      p.bandwidth = MIRISDR_BW_8MHZ;
      break;
    default:
      break;
  }

  mirisdr_set_soft();
  mirisdr_set_gain(); // restore gain
  return 0;
}


int mirisdr_set_offset_tuning(int on)
{

  if (on)
  {
    p.if_freq = MIRISDR_IF_450KHZ;
  }
  else
  {
    p.if_freq = MIRISDR_IF_ZERO;
  }

  return mirisdr_set_soft();

}

int mirisdr_set_gain()
{
  uint32_t reg1 = 1, reg6 = 6;

#if MIRISDR_DEBUG >= 1
  sprintf(buffer, "gain reduction baseband: %d, lna: %d, mixer: %d\n",
          p.gain_reduction_baseband, p.gain_reduction_lna, p.gain_reduction_mixer);
  Serial.println(buffer);
#endif

  //  unsigned int reg;
  //
  //  reg = 1 << 0;
  //  reg |= (59 - p.gain_reduction_baseband) << 4;
  //  reg |= 0 << 10;
  //  reg |= (1 - p.gain_reduction_mixer) << 12;
  //  reg |= (1 - p.gain_reduction_lna) << 13;
  //  reg |= 4 << 14;
  //  reg |= 0 << 17;
  //  mirisdr_write_reg( reg);
  //  reg = 6 << 0;
  //  reg |= 63 << 4;
  //  reg |= 4095 << 10;
  //  mirisdr_write_reg( reg);
  //  return 0;

  /* Receiver Gain Control */
  /* 0-3 => registr */
  /* 4-9 => baseband, 0 - 59, 60-63 je stejné jako 59 */
  /* 10-11 => mixer gain reduction pouze pro AM režim */
  /* 12 => mixer gain reduction -19dB */
  /* 13 => lna gain reduction -24dB */
  /* 14-16 => DC kalibrace */
  /* 17 => zrychlená DC kalibrace */
  
  reg1 |= p.gain_reduction_baseband << 4;

  // Mixbuffer is on AM1 and AM2 inputs only
  if (p.band == MIRISDR_BAND_AM1)
  {
    reg1 |= (p.gain_reduction_mixbuffer & 0x03) << 10;
  }
  else if (p.band == MIRISDR_BAND_AM2)
  {
    reg1 |= (p.gain_reduction_mixbuffer == 0 ? 0x0 : 0x03) << 10;
  }
  else
  {
    reg1 |= 0x0 << 10;
  }

  reg1 |= p.gain_reduction_mixer << 12;

  // LNA is not on AM1 nor AM2 inputs
  if ((p.band == MIRISDR_BAND_AM1) || (p.band == MIRISDR_BAND_AM2))
  {
    reg1 |= 0x0 << 13;
  }
  else
  {
    reg1 |= p.gain_reduction_lna << 13;
  }

  reg1 |= MIRISDR_DC_OFFSET_CALIBRATION_PERIODIC2 << 14;
  reg1 |= MIRISDR_DC_OFFSET_CALIBRATION_SPEEDUP_OFF << 17;

  mirisdr_write_reg( reg1);

  /* DC Offset Calibration setup */
  reg6 |= 0x1F << 4;
  reg6 |= 0x800 << 10;
  mirisdr_write_reg( reg6);

  return 0;
}


int mirisdr_set_tuner_gain(int gain)
{
  p.gain = gain;    //used in the sence of attenuation (0 -> highest gain)
                    //used in the sence of attenuation (107 -> lowest gain) (59(baseband attenuation)+24(Front-End Gain Reduction)+24(IQmixer))
  
  /*
     For VHF mode LNA is turned on to + 24 db, mixer to + 19 dB and baseband
     can be adjusted continuously from 0 to 59 db, of which the maximum gain of 102 db
  

      Always the highest sensitivity without reducing the IQ mixer and LNA 
  */
  if ((p.gain >= 0) && (p.gain <= 59))
    {
      p.gain_reduction_lna = 0;       //LNA (for all ranges exept AM)  
      p.gain_reduction_mixbuffer = 0; // LNA equivalent for AM inputs
      p.gain_reduction_mixer = 0;     //IQ mixer reduction - off
      p.gain_reduction_baseband = p.gain;
    }
  else if ((p.gain >= 60) && (p.gain <= 83))                        //In the best case have to rework it because now is asuming that AM_MODE2 = 1 is allways true
   {
    p.gain_reduction_lna = 1;       //LNA (for all ranges exept AM)
    p.gain_reduction_mixbuffer = 3; // LNA equivalent for AM inputs (AM1: 18dB / AM2: 24 dB)
    p.gain_reduction_mixer = 0;     //IQ mixer reduction - off
    p.gain_reduction_baseband = p.gain - 24;
   }
  else                              
    {                                //Max attenuation case 
    p.gain_reduction_lna = 1;       //LNA (for all ranges exept AM)
    p.gain_reduction_mixbuffer = 3; // LNA equivalent for AM inputs (AM1: 18dB / AM2: 24 dB)
    p.gain_reduction_mixer = 1;     //IQ mixer reduction - on (24dB)  
    p.gain_reduction_baseband = 59;  //Max attenuation 
    } 
    
    return mirisdr_set_gain();
}


/*
   Gain reduction is an index that depends on the AM mode (only applies to AM inputs)
            AM1     AM2
   0x00    0 dB    0 dB
   0x01    6 dB   24 dB
   0x10   12 dB   24 dB
   0x11   18 dB   24 dB
*/
int mirisdr_set_mixer_gain( int gain)
{
  p.gain_reduction_mixer = gain;

  return mirisdr_set_gain();
}

int mirisdr_set_mixbuffer_gain( int gain)
{
  p.gain_reduction_mixbuffer = gain & 0x03;

  return mirisdr_set_gain();
}

int mirisdr_set_lna_gain( int gain)
{
  p.gain_reduction_lna = gain;

  return mirisdr_set_gain();
}

int mirisdr_set_baseband_gain( int gain)
{
  p.gain_reduction_baseband = 59 - gain;

  return mirisdr_set_gain();
}


#define DEFAULT_RATE            2000000
#define DEFAULT_FREQ            7150000
#define DEFAULT_GAIN            43

void mirisdr_init() {

  pinMode (msi001_cs_pin, OUTPUT);
  pinMode (msi001_dat_pin, OUTPUT);
  pinMode (msi001_clk_pin, OUTPUT);

  pinMode (CS_SR_595, OUTPUT);        //chip select for the shift register 595
  digitalWrite(CS_SR_595, HIGH);

   pinMode (CS_SR_595_2, OUTPUT);        //chip select for the shift register 595
  digitalWrite(CS_SR_595_2, HIGH);

  p.freq = DEFAULT_FREQ;
  p.rate = DEFAULT_RATE;
  p.gain = DEFAULT_GAIN;
  p.band = MIRISDR_BAND_AM2;

  p.gain_reduction_lna = 0;
  p.gain_reduction_mixer = 0;
  p.gain_reduction_baseband = 43;     //43
  p.if_freq = MIRISDR_IF_ZERO;
  p.bandwidth = MIRISDR_BW_200KHZ;
  //p.bandwidth = MIRISDR_BW_1536KHZ;
  //p.xtal = MIRISDR_XTAL_22M;
  //p.xtal = MIRISDR_XTAL_24M;
  p.xtal = MIRISDR_XTAL_24_576M;

  mirisdr_set_center_freq(DEFAULT_FREQ);

  mirisdr_hw_flavour_t hw_flavour = MIRISDR_HW_SDRPLAY;
  p.hw_flavour = hw_flavour;

  mirisdr_set_tuner_gain(RF_attenuation);
  upd_OnBoardSR595();
  
  
}


void upd_OnBoardSR595()
{
  uint16_t Data_595=0;

  switch (ActiveRange)                        //set the RF switches data in the register
    {
      case 0: Data_595 = Range0;  break;

      case 1: Data_595 = Range1;  break;

      case 2: Data_595 = Range2;  break;

      case 3: Data_595 = Range3;  break;

      case 4: Data_595 = Range4;  break;

      case 5: Data_595 = Range5;  break;

      case 6: Data_595 = Range6;  break;
    }
  
  
  Data_595 = Data_595 + (RF_Amp_EN<<7) + Ext_Speacker_Enable;   //set the RF gain flag and the speaker en. flag (data is ready)

  digitalWrite(CS_SR_595, LOW);
  spi_transfer(Data_595);
  digitalWrite(CS_SR_595, HIGH);
}

//update the 2 595 shift registers on the external BPF board
void upd_ExtBoardSR595s()
{
  char Data0_595=0;
  char Data1_595=0;

  switch (ActiveBPF) 
    {
      case 0: 
        Data0_595 = LPF;
        Data1_595 = 1<<3;             //Set the group 1 (LF) 
      break;
      
      case 1: 
        Data0_595 = BPF1; 
        Data1_595 = 1<<3;             //Set the group 1 (LF)
      break;
      
      case 2: 
        Data0_595 = BPF2;
        Data1_595 = 1<<3;             //Set the group 1 (LF)
      break;
      
      case 3: 
        Data0_595 = BPF3;
        Data1_595 = 1<<3;             //Set the group 1 (LF)
      break;
      
      case 4: 
        Data0_595 = BPF4;
        Data1_595 = 1<<2;             //Set the group 2 (HF) 
      break;
      
      case 5: 
        Data0_595 = BPF5;
        Data1_595 = 1<<2;             //Set the group 2 (HF) 
      break;
      
      case 6: 
        Data0_595 = BPF6;
        Data1_595 = 1<<2;             //Set the group 2 (HF) 
      break;
      
      case 7: 
        Data0_595 = HPF;
        Data1_595 = 1<<2;             //Set the group 2 (HF) 
      break;
    }
    if (ANT_BIAST == 1)             
       Data1_595 = Data1_595 + 0x3;     //Set the RF Switch to BIAS-T input and turn on the power 

    digitalWrite(CS_SR_595_2, LOW);
    spi_transfer(Data0_595);
    spi_transfer(Data1_595);
    digitalWrite(CS_SR_595_2, HIGH);
}
