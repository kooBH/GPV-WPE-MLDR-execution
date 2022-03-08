# preprocessing_testbed
Testbed for audio signal preprocessing

# Submodules

```
git clone --recursive
```
or  
```
git submodule init
git submodule update
```

+ [WAV](https://github.com/kooBH/WAV)  
c++ WAV class for my processing modules  
+ [STFT](https://github.com/kooBH/STFT)  
STFT(Short Time Fourier Transform), ISTFT(Inverse - Short Time Fourier Transform)  for wav,mic input and signal processing modules

# Requirements
C++17 or higher (std::filesystem)  

# Usage
1. mkdir ```build```, ```input```, ```output``` at root directory of the project
2. goto ```build``` directory run ```cmake ..``` or use cmake GUI 
2. add your algorithm code file
3. add routines for your algorithm into ```CMakeLists.txt```,```src/main.cpp```  (as instructed in the code)
4. the main code read every file in the ```input``` directory as WAV format
5. processed outputs will be saved into the ```output``` directory
