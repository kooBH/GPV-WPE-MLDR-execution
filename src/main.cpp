#include <string>
#include <filesystem>

#include "STFT.h"
#include "WAV.h"
#include "mel.h"
#include "GPV.h"
#include "WPE.h"
#include "MLDR.h"
#include "jsonConfig.h"

/* Set Parameter of Input */
constexpr int samplerate = 16000;
constexpr int frame = 512;
constexpr int shift = 128;
constexpr int n_mels = 40;


int main() {

  bool is_first_frame = true;
  bool is_it_processed = false;

  /* Define Algorithm Class here */
  int length;
  int n_unit = -1;

  /* Config */
  const std::string path_config = "../config.json";
  jsonConfig config_GPV(path_config,"GPV");
  jsonConfig config_WPE(path_config,"WPE");
  jsonConfig config_MLDR(path_config,"MLDR");

  double gpv_threshold= config_GPV["threshold"];
  double wpe_post_gamma = config_WPE["post_gamma"];
  
  int wpe_tap_delay = static_cast<int>(config_WPE["tap_delay"]);
  int wpe_tap_num = static_cast<int>(config_WPE["tap_num"]);

  bool mldr_on = static_cast<bool>(config_MLDR["on"]);


  STFT stft_vad(1, frame, shift);
  mel mel_filter(samplerate, frame, n_mels);
  GPV vad("GPV.pt", n_mels);

  WAV* output;
  STFT* stft;
  WPE* wpe;
  MLDR* mldr;

  int nhfft = int(frame / 2 + 1);

  short* buf_in;
  buf_in = new short[shift];

  double* spec;
  spec = new double[frame + 2];
  memset(spec, 0, sizeof(double) * (frame + 2));

  double* mag;
  mag = new double[frame / 2 + 1];
  memset(mag, 0, sizeof(double) * (frame / 2 + 1));

  double* mel;
  mel = new double[n_mels];
  memset(mel, 0, sizeof(double) * (n_mels));

  double** vad_data;
  double* prob;

  double** data;
  short* buf_out = new short[shift];

  for (auto path : std::filesystem::directory_iterator{ "../input" }) {
    int cnt_frame;

    std::string target(path.path().string());
    printf("processing : %s\n", target.c_str());
    WAV input;
    input.OpenFile(target.c_str());

    // Get param
    int ch = input.GetChannels();
    int n_sample = input.GetNumOfSamples();
    int rate = input.GetSampleRate();

    if (rate != 16000) {
      printf("ERROR::only support 16kHz.\n");
      exit(16);
    }

    int n_unit = int(n_sample / shift + (n_sample % shift ? 1 : 0));
    printf("n_unit : %d\n", n_unit);

    // buffer alloc
    short* buf_tmp = new short[ch * shift];
    printf("channels : %d\n", ch);

    vad_data = new double* [n_unit];
    for (int i = 0; i < n_unit; i++) {
      vad_data[i] = new double[n_mels];
      memset(vad_data[i], 0, sizeof(double) * n_mels);
    }

    prob = new double[n_unit];
    memset(prob, 0, sizeof(double) * (n_unit));

    int cnt_crop = 0;
    cnt_frame = 0;

    /* Create vad_data Buffer */
    while (!input.IsEOF()) {
      length = input.ReadUnit(buf_tmp, shift * ch);

      // extract first channel only
      for (int i = 0; i < shift; i++)
        buf_in[i] = buf_tmp[ch * i];

      // stft
      stft_vad.stft(buf_in, spec);

      // mag
      for (int i = 0; i < nhfft; i++)
        mag[i] = std::sqrt(spec[2 * i] * spec[2 * i] + spec[2 * i + 1] * spec[2 * i + 1]);

      // mel
      mel_filter.filter(mag, mel);

      // accumlate 
      memcpy(vad_data[cnt_frame], mel, sizeof(double) * n_mels);
      cnt_frame++;
    }
    printf("INFO::feature extraction done. cnt_frame : %d\n", cnt_frame);

    vad.process(vad_data, prob, n_unit);
    /* Post Processing for vad output */

    /* Process output */
    input.Rewind();


    cnt_frame = 0;
    while (!input.IsEOF()) {
      length = input.ReadUnit(buf_tmp, shift * ch);
     // printf("prob[%d] : %lf\n", cnt_frame, prob[cnt_frame]);

      // non speech
      if (prob[cnt_frame++] < gpv_threshold) {
        // speech end
        if (is_it_processed) {
          is_it_processed = false;
          is_first_frame = true;
          cnt_crop++;

          output->Finish();
          delete output;
          delete stft;
          delete wpe;
          delete mldr;

          for (int i = 0; i < ch; i++)
            delete[] data[i];
          delete[] data;
        }
        continue;
      }

      // speech start
      if (is_first_frame) {
        is_first_frame = false;
        is_it_processed = true;

        output = new WAV(1, rate);
        //output path
        std::string path_o = "../output/" + target.substr(9, target.length() - 9);
        path_o = path_o.substr(0, path_o.length() - 4);
        path_o = path_o + "_" +std::to_string(cnt_crop) + ".wav";
        std::cout << "prob : "<<prob[cnt_frame]<<" | "<< path_o << std::endl;
        output->NewFile(path_o);

        stft = new STFT(ch, frame, shift);
        wpe = new WPE(samplerate, frame, shift, ch,
         wpe_post_gamma,
         wpe_tap_delay,
         wpe_tap_num
        );
        mldr = new MLDR(frame, ch);

        data = new double* [ch];
        for (int i = 0; i < ch; i++) {
          data[i] = new double[frame + 2];
          memset(data[i], 0, sizeof(double) * (frame + 2));
        }

      }
      stft->stft(buf_tmp, length, data);
      wpe->Process(data);
      if (mldr_on)
        mldr->Process(data);

      stft->istft(data[0], buf_out);

      output->Append(buf_out, shift);
    }
    input.Finish();

    for (int i = 0; i < n_unit; i++)
      delete[] vad_data[i];
    delete[] vad_data;
    delete[] prob;
    delete[] buf_tmp;

  }
  printf("Done\n");

  delete[] buf_in;
  delete buf_out;
  delete[] spec;
  delete[] mag;
  delete[] mel;

  return 0;
}
