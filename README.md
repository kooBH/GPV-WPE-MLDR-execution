# GPV-WPE-MLDR-execution    
  
AV-TR 시물레이션 테스트를 위한 전처리 프로그램.    
   
+ 기능    
GPV - WPE - MLDR(optional)     
     
+ 입력    
```input``` 폴더에 있는 다채널  ```<wav-id>.wav```     
     
+ 출력      
```output```폴더에 발화별로 crop 된 상태로 단채널  ```<wav-id>_<crop-id>.wav```로 추출         
  
+ 설정  
GPV : 임계값   
MLDR : on/off     

---  
  
# Installing Dependency  
  
libtorch-1.10.2-CPU : https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-1.10.2%2Bcpu.zip        
를 ```lib/libtorch``` 로 구성되도록 압축을 푼다.  


