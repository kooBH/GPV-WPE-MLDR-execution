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
  int n_frame = -1;

  /* Config */
  const std::string path_config = "../config.json";
  jsonConfig config_GPV(path_config,"GPV");
  jsonConfig config_WPE(path_config,"WPE");
  jsonConfig config_MLDR(path_config,"MLDR");
  jsonConfig config_VAD_post(path_config,"VAD_post");

  config_GPV.Print();
  config_VAD_post.Print();
  config_WPE.Print();
  config_MLDR.Print();

  // VAD
  double gpv_threshold= config_GPV["threshold"];
  int len_bridge = static_cast<int>(config_VAD_post["len_bridge"]);
  int min_frame = static_cast<int>(config_VAD_post["min_frame"]);
  int pad_pre = static_cast<int>(config_VAD_post["pad_pre"]);
  int pad_post = static_cast<int>(config_VAD_post["pad_post"]);

  // WPE
  bool wpe_on = static_cast<bool>(config_WPE["on"]);
  double wpe_post_gamma = config_WPE["post_gamma"];
  int wpe_tap_delay = static_cast<int>(config_WPE["tap_delay"]);
  int wpe_tap_num = static_cast<int>(config_WPE["tap_num"]);

  // MLDR
  bool mldr_on = static_cast<bool>(config_MLDR["on"]);


  STFT stft_vad(1, frame, shift);
  mel mel_filter(samplerate, frame, n_mels);
  GPV vad("GPV.pt", n_mels);

  WAV* output_seg;
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
  double* vad_prob;
  bool* vad_label;

  double** data;
  short* buf_out = new short[shift];
  short* buf_zero = new short[shift];
  memset(buf_zero, 0, sizeof(short) * shift);


  double frame2msec = 1 / (double)samplerate * shift;

  //TODO search .wav file only
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

    int n_frame = int(n_sample / shift + (n_sample % shift ? 1 : 0));
    printf("n_frame : %d\n", n_frame);

    // buffer alloc
    short* buf_tmp = new short[ch * shift];
    printf("channels : %d\n", ch);

    vad_data = new double* [n_frame];
    for (int i = 0; i < n_frame; i++) {
      vad_data[i] = new double[n_mels];
      memset(vad_data[i], 0, sizeof(double) * n_mels);
    }

    vad_prob = new double[n_frame];
    memset(vad_prob, 0, sizeof(double) * (n_frame));

    vad_label = new bool[n_frame];

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

    vad.process(vad_data, vad_prob, n_frame);


    /* Post Processing for vad output */
    {
      // init label
      memset(vad_label, 0, sizeof(bool) * n_frame);

      // connect sparse utterances
      int vad_cnt_down = len_bridge;
      for (int i = 0; i < n_frame; i++) {
        // speech
        if (vad_prob[i] >= gpv_threshold) {
          vad_label[i] = true;

          // connect
          if (vad_cnt_down < len_bridge) {
            // printf("connect : %d ~ %d\n",i - len_bridge + vad_cnt_down, i);
            for (int j = 0; j < len_bridge - vad_cnt_down + 1; j++)
              vad_label[i - j] = true;
          }
          vad_cnt_down = len_bridge;

        }
        // non-speech
        else {
          vad_cnt_down--;
          if (vad_cnt_down < 0)
            vad_cnt_down = 0;
        }
      }

      // elim too short utterances
      bool prev_label = false;
      int idx_start;
      for (int i = 0; i < n_frame; i++) {
        prev_label = i > 0 ? vad_label[i - 1] : false;
        // rising
        if (!prev_label && vad_label[i]) {
          idx_start = i;
          // printf("VAD::start idx : %d\n",idx_start);
        }
        // falling
        else if (prev_label && !vad_label[i]) {
          //printf("VAD::end idx : %d | %d %d\n",i,idx_start,min_frame);
          // too short
          if (i - idx_start < min_frame) {
            for (int j = i; j > idx_start - 1; j--) {
              vad_label[j] = false;
            }
          }

        }
      }

      /* fixed size padding */
      prev_label = false;
      for (int i = 0; i < n_frame; i++) {
        prev_label = i > 0 ? vad_label[i - 1] : false;
        // rising
        if (!prev_label && vad_label[i]) {
          for (int j = 0; j < pad_pre && i - j > 0; j++)
            vad_label[i - j] = true;
        }
        // falling
        else if (prev_label && !vad_label[i]) {
          for (int j = 0; j < pad_post && i + j < n_frame; j++)
            vad_label[i + j] = true;
        }

      }
    }

    //unsegmented output
    WAV output_unseg(1, samplerate);
    std::string path_unseg = "../output_unseg/" + target.substr(9, target.length() -9);
    path_unseg = path_unseg.substr(0, path_unseg.length() - 4);
    path_unseg = path_unseg + "_output.wav";
    //std::cout << "WAV_unseg : " << path_unseg << std::endl;
    output_unseg.NewFile(path_unseg);

    /* Process output_seg */
    input.Rewind();

    cnt_frame = 0;
    while (!input.IsEOF()) {
      length = input.ReadUnit(buf_tmp, shift * ch);
     // printf("vad_prob[%d] : %lf\n", cnt_frame, vad_prob[cnt_frame]);

      // non speech
      if (!vad_label[cnt_frame++]) {
        // speech end
        if (is_it_processed) {
          is_it_processed = false;
          is_first_frame = true;
          cnt_crop++;
         
          printf("seg\n");
          output_seg->Normalize();
          output_seg->Finish();
          delete output_seg;
          delete stft;
          delete wpe;
          delete mldr;

          for (int i = 0; i < ch; i++)
            delete[] data[i];
          delete[] data;
        }
        output_unseg.Append(buf_zero, shift);
        continue;
      }

      // speech start
      if (is_first_frame) {
        is_first_frame = false;
        is_it_processed = true;

        output_seg = new WAV(1, rate);
        //output_seg path
        std::string path_o = "../output_seg/" + target.substr(9, target.length() -9);
        path_o = path_o.substr(0, path_o.length() - 4);
        path_o = path_o + "_" +std::to_string(int(frame2msec*10*cnt_frame)) + ".wav";
        //std::cout << "vad_prob : "<<vad_prob[cnt_frame]<<" | "<< path_o << std::endl;
        output_seg->NewFile(path_o);

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

      //processing speech
      stft->stft(buf_tmp, length, data);

      if(wpe_on)wpe->Process(data);
      if(mldr_on)mldr->Process(data);

      stft->istft(data[0], buf_out);

      output_seg->Append(buf_out, shift);
      output_unseg.Append(buf_out, shift);
    }

    // For the case input ended with speech
    if (is_it_processed) {
      is_it_processed = false;
      is_first_frame = true;
      cnt_crop++;

      printf("seg\n");
      output_seg->Normalize();
      output_seg->Finish();
      delete output_seg;
      delete stft;
      delete wpe;
      delete mldr;

      for (int i = 0; i < ch; i++)
        delete[] data[i];
      delete[] data;
    }


    input.Finish();
    output_unseg.Normalize();
    output_unseg.Finish();

    for (int i = 0; i < n_frame; i++)
      delete[] vad_data[i];
    delete[] vad_data;
    delete[] vad_prob;
    delete[] buf_tmp;

  }
  printf("Done\n");

  delete[] buf_in;
  delete[] buf_out;
  delete[] buf_zero;
  delete[] spec;
  delete[] mag;
  delete[] mel;

  return 0;
}
