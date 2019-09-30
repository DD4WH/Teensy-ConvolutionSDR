# Teensy-ConvolutionSDR

Software Defined Radio with the Teensy 3.6, the Teensy Audio board and any IQ-Quadrature Sampling Detector

This radio uses the Fast convolution approach to filter and process the radio signals with the floating point microprocessor Teensy 3.6

Reception of longwave, medium wave and shortwave signals from about 12kHz to 30Mhz, additionally undersampling reception of FM broadcast signals 88 - 108MHz in HiFi quality and STEREO

![Teensy Convolution SDR](https://user-images.githubusercontent.com/14326464/55977162-65877b80-5c8e-11e9-97f7-866eaebdb601.jpg)
Teensy Convolution SDR software running on the hardware designed by Dante DO7JBH


[![Teensy Convolution SDR video](http://img.youtube.com/vi/VdJXrZoBHjU/0.jpg)](http://www.youtube.com/watch?v=VdJXrZoBHjU)

[![Teensy Convolution SDR video](http://img.youtube.com/vi/hCFvDHAo2mg/0.jpg)](https://www.youtube.com/watch?v=hCFvDHAo2mg)

[![Teensy Convolution SDR video](http://img.youtube.com/vi/qXAM5OmVnHE/0.jpg)](https://www.youtube.com/watch?v=qXAM5OmVnHE)

Hardware needed:
- Teensy 3.6
- Teensy Audio board
- QSD = quadrature sampling detector and a local oscillator that can be tuned by I2C (eg. Si5351, Si570, Si514 . . .)
[should run with Softrock, Elektor SDR (https://www.elektor.de/elektor-sdr-reloaded-150515-91), Fifi SDR and many others]
- one encoder for frequency tuning
- one encoder for filter bandwidth
- one encoder for volume and analog gain
- eight pushbuttons (three of them could be encoder pushbuttons)
- TFT display ILI9341-based
- antenna for longwave/mediumwave/shortwave
- 5V power supply

Software setup: 
- Install Arduino
- Install Teensyduino
- Unzip and copy the following libraries into your Arduino folder (/Program Files/Arduino/hardware/teensy/avr/libraries):
Si5351: https://github.com/etherkit/Si5351Arduino
Arduino-Teensy-Codec-lib: https://github.com/FrankBoesing/Arduino-Teensy-Codec-lib
- Install a newer version of the ARM CMSIS DSP library and set it to use floating point (explained HERE: https://forum.pjrc.com/threads/40590-Teensy-Convolution-SDR-(Software-Defined-Radio)?p=129081&viewfull=1#post129081
- download Teensy_Convolution_SDR.ino from this github page and compile it
- Listen to the radio ;-)

Specifications at the moment:
* Teensy 3.6 with audio board working in floating point
* reception from about 12kHz to 30Mhz plus undersampling 3x mode for wideband FM receive
* FFT and inverse FFT with 512 points using the new CMSIS DSP lib complex FFT function
* filtering with 257 tap FIR filter (equivalent to 2056 tap filtering because of decimation-by-8) --> comparably steep filters are normally available in commercial receivers from 2000$ upwards ;-)
* filter coefficients are calculated by the Teensy itself and the filter bandwidth can be chosen arbitrarily by the user in 100Hz-steps (100Hz to 11kHz audio bandwidth)
* demodulation of AM (nine different algorithms implemented), SAM (real synchronous AM !) with user-selected sidebands and Stereo-SAM, LSB, USB, pseudo-Stereo-USB, pseuso-Stereo-LSB (more to come . . .)
* display up to 192kHz (281kHz experimental) of frequency spectrum
* sophisticated automatic gain control (AGC) adapted from the wdsp lib by Warren Pratt
* Zoom into the spectrum for greater frequency resolution detail (up to 2048x = 0.2Hz resolution per pixel): uses a Zoom FFT approach (Lyons 2011). This allows to differentiate between different carrier frequencies which is helpful in identifying AM medium wave stations (which pretend to transmit on the same frequency, but in fact have differing frequencies, sometimes several Hz, sometimes only several milli-Hz).
* superb audio quality (of course not like FM quality, but very good compared to other commercially available shortwave receivers)
* also plays MP3 and M4A files from the SD card in best HiFi stereo quality (with the excellent lib by Frank Bösing)
* automatic IQ amplitude and phase imbalance correction: this corrects imbalances that are caused by the hardware and results in > 55dBc mirror rejection
* supports many different sample rates up to 281ksps
* dynamic frequency indicator figures and graticules on spectrum display x-axis
* menu system to adjust many variables
* save and load settings to/from EEPROM
* demodulation of wide FM broadcast signals: possible at 234ksps sample rate in order to pass through the WFM signal, which has 180kHz bandwidth for MONO and 246kHz for STEREO. The audio quality is superb at 234ksps and still acceptable (but no longer HiFi due to narrow-bandwidth-related distortion) at 96ksps sample rate. The reception of VHF signals is possible by undersampling reception with the QSD at 5th harmonic (receive 95.4MHz at 19.08MHz with LO running at 4 x 19.08MHz = 76.32MHz). Undersampling reception at the 3rd harmonic involves an attenuation of the signals by 9dB, so for better sensitivity a preamp is recommended for undersampling reception in the range of 88 - 150MHz. But even without a preamp the reception is quite good if you are near enough to strong FM broadcast transmitters. 
* automatic test for the "twinpeak syndrome" - a fault in the mirror rejection that occurs sometimes at startup. This is now in most cases detected by the automatic IQ phase correction algorithm and the codec restarted to cure the problem. Bob Larkin has an alternative (better!) approach with cross-correlation which needs a simple hardware addition
* AGC now has an optical indicator for the AGC threshold and many more AGC parameters can be adjusted
* Codec gain has now its own automatic AGC
* Spectrum display now also has an "AGC" that prevents empty screen for small signals
* Automatic notch filter with LMS algorithm
* Spectral noise reduction using state-of-the-art DSP algorithms in the frequency domain (Ephraim-Malah variant). This noise reduction is able to pull weak speech signals out of the noise
* complex FIR filter coefficients implemented --> independent passband tuning with freely adjustable bandwidth now possible for all modes
* supports newest Arduino and Teensyduino versions 1.8.5 and 1.40
* state-of-the-art noise blanker by Michael DL2FW integrated
* added support for SDR hardware by Dante DO7JBH
* CW (Morse) decoder from UHSDR implemented
* RTTY decoder from UHSDR implemented
* alternative RTTY decoder by Martin Ossmann implemented
* EFR (European Teleswitching) decoding by Martin Ossmann implemented (can be used to automatically set the RTC to the standard atomic clock time)


 


