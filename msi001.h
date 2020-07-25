
//Define RF filter switches (BPF Board)
#define LPF      B00000001
#define BPF1     B00001011
#define BPF2     B00001110
#define BPF3     B00000100

#define BPF4     B00010000
#define BPF5     B10110000
#define BPF6     B11100000
#define HPF      B01000000


//Define RF Range switches (Main Board)
#define Range0   B01111000          //0-12
#define Range1   B01001000          //12-30
#define Range2   B00110000          //30-60
#define Range3   B01100010          //60-120
#define Range4   B01100100          //120-250
#define Range5   B01100110          //250-1000
#define Range6   B01100000          //>1000


/*** Register 0: IC Mode / Power Control ***/

/* reg0: 4:8 (AM_MODE, VHF_MODE, B3_MODE, B45_MODE, BL_MODE) */
#define MIRISDR_MODE_AM                                 0x01
#define MIRISDR_MODE_VHF                                0x02
#define MIRISDR_MODE_B3                                 0x04
#define MIRISDR_MODE_B45                                0x08
#define MIRISDR_MODE_BL                                 0x10

/* reg0: 9 (AM_MODE2) */
#define MIRISDR_UPCONVERT_MIXER_OFF                     0
#define MIRISDR_UPCONVERT_MIXER_ON                      1

/* reg0: 10 (RF_SYNTH) */
#define MIRISDR_RF_SYNTHESIZER_OFF                      0
#define MIRISDR_RF_SYNTHESIZER_ON                       1

/* reg0: 11 (AM_PORT_SEL) */
#define MIRISDR_AM_PORT1                                0
#define MIRISDR_AM_PORT2                                1

/* reg0: 12:13 (FIL_MODE_SEL0, FIL_MODE_SEL1) */
#define MIRISDR_IF_MODE_2048KHZ                         0
#define MIRISDR_IF_MODE_1620KHZ                         1
#define MIRISDR_IF_MODE_450KHZ                          2
#define MIRISDR_IF_MODE_ZERO                            3

/* reg0: 14:16 (FIL_BW_SEL0 - FIL_BW_SEL2) */

/* reg0: 17:19 (XTAL_SEL0 - XTAL_SEL2) */

/* reg0: 20:22 (IF_LPMODE0 - IF_LPMODE2) */
#define MIRISDR_IF_LPMODE_NORMAL                        0
#define MIRISDR_IF_LPMODE_ONLY_Q                        1
#define MIRISDR_IF_LPMODE_ONLY_I                        2
#define MIRISDR_IF_LPMODE_LOW_POWER                     4

/* reg0: 23 (VCO_LPMODE) */
#define MIRISDR_VCO_LPMODE_NORMAL                       0
#define MIRISDR_VCO_LPMODE_LOW_POWER                    1

/*** Register 2: Synthesizer Programming ***/

/* reg2: 4:15 (FRAC0 - FRAC11) */

/* reg2: 16:21 (INT0 - INT5) */

/* reg2: 22 (LNACAL_EN) */
#define MIRISDR_LBAND_LNA_CALIBRATION_OFF               0
#define MIRISDR_LBAND_LNA_CALIBRATION_ON                1

/*** Register 6: RF Synthesizer Configuration ***/

/* reg5: 4:15 (THRESH0 - THRESH11) */

/* reg5: 16:21 (reserved) */
#define MIRISDR_RF_SYNTHESIZER_RESERVED_PROGRAMMING     0x28


/*** Register 1: Receiver Gain Control ***/

/* reg1: 4:9 (BBGAIN0 - BBGAIN5) */
#define MIRISDR_BASEBAND_GAIN_REDUCTION_MIN             0
#define MIRISDR_BASEBAND_GAIN_REDUCTION_MAX             0x3B

/* reg1: 10:11 (MIXBU0, MIXBU1) - AM port 1 */
#define MIRISDR_AM_PORT1_BLOCKUP_CONVERT_GAIN_REDUCTION_0DB  0
#define MIRISDR_AM_PORT1_BLOCKUP_CONVERT_GAIN_REDUCTION_6DB  1
#define MIRISDR_AM_PORT1_BLOCKUP_CONVERT_GAIN_REDUCTION_12DB 2
#define MIRISDR_AM_PORT1_BLOCKUP_CONVERT_GAIN_REDUCTION_18DB 3

/* reg1: 10:11 (MIXBU0, MIXBU1) - AM port 2 */
#define MIRISDR_AM_PORT2_BLOCKUP_CONVERT_GAIN_REDUCTION_0DB  0
#define MIRISDR_AM_PORT2_BLOCKUP_CONVERT_GAIN_REDUCTION_24DB 3

/* reg1: 12 (MIXL) */
#define MIRISDR_LNA_GAIN_REDUCTION_OFF                  0
#define MIRISDR_LNA_GAIN_REDUCTION_ON                   1

/* reg1: 13 (LNAGR) */
#define MIRISDR_MIXER_GAIN_REDUCTION_OFF                0
#define MIRISDR_MIXER_GAIN_REDUCTION_ON                 1

/* reg1: 14:16 (DCCAL0 - DCCAL2) */
#define MIRISDR_DC_OFFSET_CALIBRATION_STATIC            0
#define MIRISDR_DC_OFFSET_CALIBRATION_PERIODIC1         1
#define MIRISDR_DC_OFFSET_CALIBRATION_PERIODIC2         2
#define MIRISDR_DC_OFFSET_CALIBRATION_PERIODIC3         3
#define MIRISDR_DC_OFFSET_CALIBRATION_ONE_SHOT          4
#define MIRISDR_DC_OFFSET_CALIBRATION_CONTINUOUS        5

/* reg1: 17 (DCCAL_SPEEDUP) */
#define MIRISDR_DC_OFFSET_CALIBRATION_SPEEDUP_OFF       0
#define MIRISDR_DC_OFFSET_CALIBRATION_SPEEDUP_ON        1

/*** Register 6: DC Offset Calibration setup ***/

/* reg6: 4:7 (DCTRK_TIM0 - DCTRK_TIM3) */

/* reg6: 8:21 (DCRATE_TIM0 - DCRATE_TIM11) */
