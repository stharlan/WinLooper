# WinLooper
Live audio looper (2nd try)

So, this is a second try at a audio looper program. It uses Core Audio 
on Windows 10 to record a sound. It will then loop that sound indefinitely.
Additional loops can be recorded and mixed live.

As of 1/11/2020, it is a work-in-progress. It will record a single
sound and play it on the next loop. The loop is currently hard-coded at
four seconds.

Capture and playback device must share the same settings (sample rate, buffer size, etc).
