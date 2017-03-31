The sample app to decode an audio file by OpenSL ES
============================

Now, I think that OpenSL ES's decoders have some bugs.

This app is it to check those problems.


First problem
---------

When an MP3 decoder starts to decode using seeking interface, the decoder outputs noise before decoded data.<br>
And, when an AAC decoder starts to decode using seeking interface, the decoder outputs silence before decoded data.<br>
When not using seeking interface, a problem doesn't occur.<br>
This is problem of only API 25 (OS 7.1.1).<br>
It's checked by Nexus 5X (7.1.1) and x86 emulator(7.1.1).

### How to check a problem of mp3 codec

1. Select a codec type of MP3.

2. Check a seek checkbox.

3. Tap the "Decode" button and wait for completion. Then an wav file is written in an indicated pass.

4. You can check the wav file with "Play" button.


### How to check a problem of mp3 codec

1. Select a codec type of AAC.

2. Check a seek checkbox.

3. Tap the "Decode" button and wait for completion. Then an wav file is written in an indicated pass.

4. Check the waveform of an output wave file by suitable software. You can check the silence.


Second problem
---------

An Ogg decoder outputs broken data stream.
This is problem of API 24 or more (OS 7.0 ~).
It's checked by Nexus 5X (7.1.1) and x86 emulator(7.0).

### How to check a problem of mp3 codec

1. Select a codec type of OGG.

2. Tap the "Decode" button and wait for completion. Then an wav file is written in an indicated pass.

3. You can check the wav file with "Play" button.


Output data
---------

The waveform of original

![The waveform of original](https://drive.google.com/file/d/0B3P1-RHbWDQRU0JUZFhmT3BhZkE/view?usp=sharing)


The MP3 decoder makes the noise

![The MP3 decoder makes the noise](https://drive.google.com/file/d/0B3P1-RHbWDQRcmlrcG1DanZGdkk/view?usp=sharing)


The AAC decoder makes the silence

![The AAC decoder makes the silence](https://drive.google.com/file/d/0B3P1-RHbWDQRRy03TGtLQ0ZoUnc/view?usp=sharing)


The OGG decoder makes the noise (1)

![The OGG decoder makes the noise (1)](https://drive.google.com/file/d/0B3P1-RHbWDQRbXpYVDVZT0RIaTA/view?usp=sharing)


The OGG decoder makes the noise (2)

![The OGG decoder makes the noise (2)](https://drive.google.com/file/d/0B3P1-RHbWDQRbXpYVDVZT0RIaTA/view?usp=sharing)

