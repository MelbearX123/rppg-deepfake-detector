# rPPG Deepfake Detector

> **This project is a work in progress.** Right now it reads a video with correct per-frame timing, checks that the video is usable (length, compression, face size), finds and tracks the face with 5 landmarks in every frame, and measures the color of five skin regions (glabella, forehead, upper nasal dorsum, both malars) in every frame, skipping hair with a per-person skin model. Pulse extraction and the rest are planned below.

## What I'm building

A C++ command-line tool that takes a video of someone's face and tries to tell whether it's real or a deepfake by looking at their **heartbeat**.

When your heart beats, a little more blood flows into the skin of your face, and your skin gets very slightly darker in green light. It's way too small to see, but a camera picks it up. This is called **remote photoplethysmography (rPPG)**. In a real video, the pulse should look the same across your whole face: same rate, same timing on your forehead and both cheeks. Deepfakes often get this wrong, so that's what I want to detect.

## How I plan to do it

1. **Read the video** with OpenCV and keep track of the real frame rate (a wrong frame rate gives a wrong heart rate).
2. **Find the face** in each frame with OpenCV's YuNet face detector, and use its landmarks to place five skin regions: glabella, forehead, upper nasal dorsum, and both malars (cheekbones), the regions that performed best in Li et al. (2024) (see [References](#references)).
3. **Average the color** of each region in every frame, which gives a red, green, and blue signal over time. A skin model learned from each person's own face skips hair and shadow, and a trimmed mean drops glare.
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

To check the face tracking, save annotated frames and the landmark positions, then plot them:

```bash
./build/Release/rppg_detect path/to/video.mp4 --debug-dir debug --landmarks-csv results/track.csv
python3 -m venv .venv && .venv/bin/pip install -r scripts/requirements.txt
.venv/bin/python scripts/plot_landmarks.py results/track.csv
```

To check the skin regions, save the per-region colors and plot the green channel:

```bash
./build/Release/rppg_detect path/to/video.mp4 --signals-csv results/signals.csv
.venv/bin/python scripts/plot_signals.py results/signals.csv
```

## References

**UBFC-rPPG dataset:**
S. Bobbia, R. Macwan, Y. Benezeth, A. Mansouri, J. Dubois, "Unsupervised skin tissue segmentation for remote photoplethysmography", *Pattern Recognition Letters*, 2017.

**Face regions:**
S. Li, M. Elgendi, C. Menon, "Optimal facial regions for remote heart rate measurement during physical and cognitive activities", *npj Cardiovascular Health*, vol. 1, art. 33, 2024. https://doi.org/10.1038/s44325-024-00033-7
