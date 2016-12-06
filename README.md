# Teensy-ConvolutionSDR

Software Defined Radio with the Teensy 3.6, the Teensy Audio board and any IQ-Quadrature Sampling Detector

This radio uses the Fast convolution approach to filter and process the radio signals with the floating point microprocessor Teensy 3.6

[![Teensy Convolution SDR video](http://img.youtube.com/vi/VdJXrZoBHjU/0.jpg)](http://www.youtube.com/watch?v=VdJXrZoBHjU)

Hardware needed:
- Teensy 3.6
- Teensy Audio board
- QSD = quadrature sampling detector and a local oscillator that can be tuned by I2C (eg. Si5351, Si570, Si514 . . .)
[should run with Softrock, Elektor SDR (https://www.elektor.de/elektor-sdr-reloaded-150515-91), Fifi SDR and many others]
- one encoder for frequency tuning
- one pot for filter bandwidth
- five pushbuttons
- TFT display ILI9341-based
- antenna for longwave/mediumwave/shortwave
- 5V power supply

Software: 
- Install Arduino
- Install Teensyduino
- Install a newer version of the ARM CMSIS DSP library and set it to use floating point (explained HERE: https://forum.pjrc.com/threads/40551-How-to-be-sure-I-m-using-the-FPU-in-T3-5-3-6?p=126193&viewfull=1#post126193)
- Listen to the radio ;-)

Specifications at the moment:
* Teensy 3.6 with audio board working in floating point
* reception from about 12kHz to 30Mhz
* FFT and inverse FFT with 4096 points using the new CMSIS DSP lib complex FFT function
* filtering with 2049 tap FIR filter --> comparably steep filters are normally available in commercial receivers from 2000$ upwards ;-)
* filter coefficients are calculated by the Teensy itself and the filter bandwidth can be chosen arbitrarily by the user
* demodulation of AM, LSB, USB, pseudo-Stereo-USB, pseuso-Stereo-LSB (more to come . . .)
* display up to 96kHz of frequency spectrum
* Zoom into the spectrum for greater frequency resolution detail: built in with Frank Bösings code for sample rate change on-the-fly
* superb audio quality (of course not like FM quality, but very good compared to other commercially available shortwave receivers)
* needs about 50% of the processor ressources at 96ksps sample rate


