# rPPG Deepfake Detector

> **This project is a work in progress.** Right now the build setup, dependencies, and a basic video-reading program are working. The rest is planned below.

## What I'm building

A C++ command-line tool that takes a video of someone's face and tries to tell whether it's real or a deepfake by looking at their **heartbeat**.

When your heart beats, a little more blood flows into the skin of your face, and your skin gets very slightly darker in green light. It's way too small to see, but a camera picks it up. This is called **remote photoplethysmography (rPPG)**. In a real video, the pulse should look the same across your whole face: same rate, same timing on your forehead and both cheeks. Deepfakes often get this wrong, so that's what I want to detect.

## How I plan to do it

1. **Read the video** with OpenCV and keep track of the real frame rate (a wrong frame rate gives a wrong heart rate).
2. **Find the face** in each frame with OpenCV's YuNet face detector, and use its landmarks (eyes, nose, mouth) to pick three skin regions: forehead, left cheek, right cheek.
3. **Average the color** of each region in every frame, which gives a red, green, and blue signal over time.
4. **Turn the colors into a pulse signal** using the POS algorithm (Wang et al., 2017), which combines R, G, and B so that lighting changes and head movement mostly cancel out.
5. **Clean the signal** by removing slow drift and filtering to 0.7–4 Hz (42–240 BPM, the range of human heart rates).
6. **Estimate the heart rate** with an FFT: the strongest frequency in that range is the pulse.
7. **Check that it actually works** by comparing my heart rates to real pulse-sensor readings from the UBFC-rPPG dataset (see [References](#references)). I want this working before I touch any deepfakes.
8. **Detect deepfakes** by comparing the pulse between face regions and training a classifier on real and fake videos from FaceForensics++. If the signal is too weak to trust, the tool should say "inconclusive" instead of guessing.
9. **Measure and optimize speed**, then package it with Docker and set up CI.

I also want to test how much video compression (like YouTube or WhatsApp) hurts accuracy, and whether it works equally well across skin tones, since rPPG is known to be less accurate on darker skin.

## Tech stack

C++17, OpenCV, Eigen, CMake, Conan, GoogleTest

## Building

Needs a C++17 compiler, CMake, and Conan 2. The first `conan install` compiles OpenCV from source, so it takes a while.

```bash
conan install . --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
ctest --test-dir build/Release --output-on-failure
./build/Release/rppg_detect path/to/video.mp4
```

## References

**UBFC-rPPG dataset:**
S. Bobbia, R. Macwan, Y. Benezeth, A. Mansouri, J. Dubois, "Unsupervised skin tissue segmentation for remote photoplethysmography", *Pattern Recognition Letters*, 2017.
