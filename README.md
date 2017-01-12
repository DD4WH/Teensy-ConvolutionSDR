# Teensy-ConvolutionSDR

Software Defined Radio with the Teensy 3.6, the Teensy Audio board and any IQ-Quadrature Sampling Detector

This radio uses the Fast convolution approach to filter and process the radio signals with the floating point microprocessor Teensy 3.6

Reception of longwave, medium wave and shortwave signals from about 12kHz to 30Mhz

[![Teensy Convolution SDR video](http://img.youtube.com/vi/VdJXrZoBHjU/0.jpg)](http://www.youtube.com/watch?v=VdJXrZoBHjU)

Hardware needed:
- Teensy 3.6
- Teensy Audio board
- QSD = quadrature sampling detector and a local oscillator that can be tuned by I2C (eg. Si5351, Si570, Si514 . . .)
[should run with Softrock, Elektor SDR (https://www.elektor.de/elektor-sdr-reloaded-150515-91), Fifi SDR and many others]
- one encoder for frequency tuning
- one encoder for filter bandwidth
- six pushbuttons (two of them could be encoder pushbuttons)
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
* FFT and inverse FFT with 1024 points using the new CMSIS DSP lib complex FFT function
* filtering with 513 tap FIR filter (equivalent to 4104 tap filtering because of decimation-by-8) --> comparably steep filters are normally available in commercial receivers from 2000$ upwards ;-)
* filter coefficients are calculated by the Teensy itself and the filter bandwidth can be chosen arbitrarily by the user in 100Hz-steps (100Hz to 11kHz audio bandwidth)
* demodulation of AM (nine different algorithms implemented), SAM (real synchronous AM !) with user-selected sidebands and Stereo-SAM, LSB, USB, pseudo-Stereo-USB, pseuso-Stereo-LSB (more to come . . .)
* display up to 192kHz of frequency spectrum
* sophisticated automatic gain control (AGC) adapted from the wdsp lib by Warren Pratt
* Zoom into the spectrum for greater frequency resolution detail: uses a Zoom FFT approach (Lyons 2011)
* superb audio quality (of course not like FM quality, but very good compared to other commercially available shortwave receivers)
* needs about 80% of the processor ressources at 192ksps sample rate and SAM demodulation (much lower with USB/LSB and AM)


