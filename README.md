# Teensy-ConvolutionSDR

Software Defined Radio with the Teensy 3.6, the Teensy Audio board and any IQ-Quadrature Sampling Detector

This radio uses the Fast convolution approach to filter and process the radio signals with the floating point microprocessor Teensy 3.6

Reception of longwave, medium wave and shortwave signals from about 12kHz to 30Mhz [the newest software version additionally supports undersampling reception of strong FM broadcast signals 88 - 108MHz in HiFi quality and STEREO]

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

Software: 
- Install Arduino
- Install Teensyduino
- Install the si5351 library by NT7S (if your QSD has the Si5351 as the local oscillator): https://github.com/etherkit/Si5351Arduino/tree/master/src
- Install a newer version of the ARM CMSIS DSP library and set it to use floating point (explained HERE: https://forum.pjrc.com/threads/40590-Teensy-Convolution-SDR-(Software-Defined-Radio)?p=129081&viewfull=1#post129081
- Listen to the radio ;-)

Specifications at the moment:
* Teensy 3.6 with audio board working in floating point
* reception from about 12kHz to 30Mhz
* FFT and inverse FFT with 512 points using the new CMSIS DSP lib complex FFT function
* filtering with 257 tap FIR filter (equivalent to 2056 tap filtering because of decimation-by-8) --> comparably steep filters are normally available in commercial receivers from 2000$ upwards ;-)
* filter coefficients are calculated by the Teensy itself and the filter bandwidth can be chosen arbitrarily by the user in 100Hz-steps (100Hz to 11kHz audio bandwidth)
* demodulation of AM (nine different algorithms implemented), SAM (real synchronous AM !) with user-selected sidebands and Stereo-SAM, LSB, USB, pseudo-Stereo-USB, pseuso-Stereo-LSB (more to come . . .)
* display up to 192kHz of frequency spectrum
* sophisticated automatic gain control (AGC) adapted from the wdsp lib by Warren Pratt
* Zoom into the spectrum for greater frequency resolution detail: uses a Zoom FFT approach (Lyons 2011)
* superb audio quality (of course not like FM quality, but very good compared to other commercially available shortwave receivers)
* also plays MP3 and M4A files from the SD card in best HiFi stereo quality (with the excellent lib by Frank Bösing)
* needs about 80% of the processor ressources at 192ksps sample rate and SAM demodulation (much lower with USB/LSB and AM)
* automatic IQ amplitude and phase imbalance correction: this corrects imbalances that are caused by the hardware and results in > 55dBc mirror rejection
* supports 100ksps sample rate which could potentially eliminate spurs present with other sample rates
* dynamic frequency indicator figures and graticules on spectrum display x-axis
* menu system to adjust many variables
* save and load settings to/from EEPROM
* demodulation of wide FM broadcast signals: possible at 192ksps sample rate in order to pass through the WFM signal, which has 180kHz bandwidth for MONO and 246kHz for STEREO. The audio quality is superb at 192ksps and still acceptable (but no longer HiFi due to narrow-bandwidth-related distortion) at 96ksps sample rate. The reception of VHF signals is possible by undersampling reception with the QSD at 5th harmonic (receive 95.4MHz at 19.08MHz with LO running at 4 x 19.08MHz = 76.32MHz). Undersampling reception at the 5th harmonic involves an attenuation of the signals by 14dB, so for better sensitivity a preamp is recommended for undersampling reception in the range of 88 - 150MHz. But even without a preamp the reception is quite good if you are near enough to strong FM broadcast transmitters. Is STEREO reception possible??? --> EDIT: yes, implemented!
* automatic test for the "twinpeak syndrome" - a fault in the mirror rejection that occurs sometimes at startup. This is now reliably detected by the automatic IQ phase correction algorithm and the codec restarted to cure the problem
* AGC now has an optical indicator for the AGC threshold and many more AGC parameters can be adjusted
* Codec gain has now its own automatic AGC
* Spectrum display now also has an "AGC" that prevents empty screen for small signals
* now spectrum display also working in wide FM reception (well, kind of . . . )
* spectrum display zoom now allows zoom factors up to 4096! --> this allows to differentiate between different carrier frequencies which is helpful in identifying AM medium wave stations (which pretend to transmit on the same frequency, but in fact have differing frequencies, sometimes several Hz, sometimes only several milli-Hz). The frequency resolution is smaller than 0.1 Hz with this zoom factor ;-) Disadvantage is the very long time delay for refreshing the display (frequency resolution is always inversely related to time resolution in DSP . . .)
* STEREO FM wideband reception is now implemented (undersampling mode has been changed to 3 times undersampling instead of 5 times undersampling, which means a 6dB higher signal strength)
* Automatic notch filter and
* Noise reduction implemented as variable-leak LMS algorithms, both taken from the excellent WDSP library by Warren Pratt, thank you Warren! 
* complex FIR filter coefficients implemented --> 
* RX filters now available as freely adjustable bandpasses: passband tuning available!
* supports newest Arduino and Teensyduino versions 1.8.5 and 1.40

 


