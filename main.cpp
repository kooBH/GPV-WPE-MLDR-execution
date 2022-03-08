#include "STFT.h"
#include "WAV.h"
#include <string>
#include <filesystem>
/* Inlcude Algorithm source here */

/* Set Parameter of Input */
constexpr int ch = 6;
constexpr int rate = 16000;
constexpr int frame = 512;
constexpr int shift = 128;

int main() {
  /* Define Algorithm Class here */

  int length;
  WAV input;
  WAV output(ch, rate);
  STFT process(ch, frame, shift);

  short buf_in[ch * shift];
  double** data;
  short buf_out[ch * shift];

  data = new double* [ch];
  for (int i = 0; i < ch; i++) {
    data[i] = new double[frame + 2];
    memset(data[i], 0, sizeof(double) * (frame + 2));
  }

  for (auto path : std::filesystem::directory_iterator{"../input"}) {
    std::string i(path.path().string());
    printf("processing : %s\n",i.c_str());
    input.OpenFile(i.c_str());
    i = "../output/"+ i.substr(9, i.length() - 9);
    output.NewFile((i).c_str());
    while (!input.IsEOF()) {
      length = input.ReadUnit(buf_in, shift * ch);
      process.stft(buf_in, length, data);

      /* Run Process here */

      process.istft(data, buf_out);
      output.Append(buf_out, shift * ch);
    }
    output.Finish();
    input.Finish();
  }

  for (int i = 0; i < ch; i++)
    delete[] data[i];
  delete[] data;

  return 0;
}
