# GPV-WPE-MLDR-execution    
  
AV-TR 시물레이션 테스트를 위한 전처리 프로그램.    
   
## 기능    
GPV - WPE(optional) - MLDR(optional)     
     
## 입력    
```input``` 폴더에 있는 다채널  ```<wav-id>.wav```     
     
## 출력      
```output_seg```폴더에 발화별로 crop 된 상태로 단채널  ```<wav-id>_<0.1초 단위의 시작시간>.wav```로 추출  
```output_unseg```에 speech구간만 담아서 ```<wav-id>.wav```로 추출
  
## 설정 ```config.json``` 

### NOTE  
samplerate가 16kHz일 경우 125 프레임이 1초이다. (shift==128)  


### GPV
+ ```threshold``` : (실수)VAD 출력이 이 값 이상일 때 발화로 판정한다.   

### VAD_post
+ ```len_bridge``` : (프레임 단위의 자연수)이 값 내에서 인접한 발화를 하나로 묶는다.
+ ```min_frame```  : (프레임 단위의 자연수)이 값 보다 짧은 발화를 제거한다.  

### WPE
+ ```on``` : (true 또는 false)

### MLDR
+ ```on``` : (true 또는 false)

---  
  
# Installing Dependency  
  
libtorch-1.10.2-CPU : https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-1.10.2%2Bcpu.zip        
를 ```lib/libtorch``` 로 구성되도록 압축을 푼다.  


