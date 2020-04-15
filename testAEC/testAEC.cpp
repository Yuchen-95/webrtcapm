// testAEC.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>

#include "modules/audio_processing/include/audio_processing.h"
#include "api\audio\echo_canceller3_factory.h"
#include "api\audio\audio_frame.h"

#pragma comment(lib, "webrtcapm.lib")

using namespace webrtc;

class APMEcho {
public:
	APMEcho();
	int Init(int nearSampleRate,
		int nearChannels,
		int farSampleRate,
		int farChannels);

	int FarFrame(int16_t* data, int frames, int16_t* outData, int& outFrames);
	int NearFrame(int16_t* data, int frames, int16_t* outData, int& outFrames);
protected:
private:
	std::unique_ptr<webrtc::AudioProcessing> apm_;
	StreamConfig near_in_config_;
	StreamConfig far_in_config_;
	std::unique_ptr<int16_t> near_buf_;
	std::unique_ptr<int16_t> far_buf_;
	int near_buf_cache_len_;
	int far_buf_cache_len_;
	AudioFrame nearFrame_;
	AudioFrame farFrame_;
	int nearSampleRate_;
	int nearChannels_;
	int farSampleRate_;
	int farChannels_;
	int nearFrameNums_;
	int farFrameNums_;
};

APMEcho::APMEcho() {}

int APMEcho::Init(int nearSampleRate,
	int nearChannels,
	int farSampleRate,
	int farChannels) {
	std::unique_ptr<webrtc::AudioProcessingBuilder> ap_builder_ =
		std::make_unique<webrtc::AudioProcessingBuilder>();
	Config config;
	AudioProcessing::Config apm_config;
	std::unique_ptr<EchoControlFactory> echo_control_factory;
	apm_config.echo_canceller.enabled = true;
	apm_config.echo_canceller.mobile_mode = false;
	{
		EchoCanceller3Config cfg;
		echo_control_factory.reset(new EchoCanceller3Factory(cfg));
	}
#if 1
	config.Set<ExtendedFilter>(new ExtendedFilter(true));
	config.Set<DelayAgnostic>(new DelayAgnostic(true));
	config.Set<ExperimentalAgc>(new ExperimentalAgc(true));
	config.Set<ExperimentalNs>(new ExperimentalNs(true));
	apm_.reset((*ap_builder_)
		.SetEchoControlFactory(std::move(echo_control_factory))
		.Create(config));
#else
	apm_.reset((*ap_builder_).Create());
#endif
	if (!apm_)
	{
		return -1;
	}
	apm_->ApplyConfig(apm_config);

	near_in_config_ = StreamConfig(nearSampleRate, nearChannels);
	far_in_config_ = StreamConfig(farSampleRate, farChannels);

	near_buf_.reset(new int16_t[near_in_config_.num_samples()]);
	far_buf_.reset(new int16_t[far_in_config_.num_samples()]);
	near_buf_cache_len_ = 0;
	far_buf_cache_len_ = 0;
	nearSampleRate_ = nearSampleRate;
	farSampleRate_ = farSampleRate;
	nearFrameNums_ = AudioProcessing::kChunkSizeMs * nearSampleRate / 1000;
	farFrameNums_ = AudioProcessing::kChunkSizeMs * farSampleRate / 1000;

	farChannels_ = farChannels;
	nearChannels_ = nearChannels;
	return 0;
}

int APMEcho::FarFrame(int16_t* data,
	int frames,
	int16_t* outData,
	int& outFrames)
{
	int i = 0, iRet = 0;
	outFrames = 0;
	// 缓存的数据
	if (far_buf_cache_len_ > 0)
	{
		if (far_buf_cache_len_ + frames < farFrameNums_)
		{
			memcpy(far_buf_.get() + far_buf_cache_len_, data, frames * sizeof(int16_t));
			far_buf_cache_len_ += frames;
			return 0;
		}
		int needLen = farFrameNums_ - far_buf_cache_len_;
		memcpy(far_buf_.get() + far_buf_cache_len_, data, needLen * sizeof(int16_t));
		farFrame_.UpdateFrame(0, far_buf_.get(), farFrameNums_, farSampleRate_,
			AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown,
			farChannels_);
		iRet = apm_->ProcessReverseStream(&farFrame_);
		if (iRet != 0)
		{
			return iRet;
		}
		far_buf_cache_len_ = 0;
		i += needLen;

		if (outData != 0)
		{
			memcpy(outData + outFrames, farFrame_.data(), farFrame_.samples_per_channel_ * sizeof(int16_t));
		}
		outFrames += farFrame_.samples_per_channel_;
	}

	for (; i + farFrameNums_ <= frames; i += farFrameNums_)
	{
		farFrame_.UpdateFrame(0, data + i, farFrameNums_, farSampleRate_,
			AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown,
			farChannels_);
		apm_->ProcessReverseStream(&farFrame_);

		if (outData != 0)
		{
			memcpy(outData + outFrames, farFrame_.data(), farFrame_.samples_per_channel_ * sizeof(int16_t));
		}
		outFrames += farFrame_.samples_per_channel_;
	}
	if (i < frames)
	{
		far_buf_cache_len_ = frames - i;
		memcpy(far_buf_.get(), data + i, far_buf_cache_len_ * sizeof(int16_t));
	}
	return 0;
}

int APMEcho::NearFrame(int16_t* data,
	int frames,
	int16_t* outData,
	int& outFrames) {
	int i = 0, iRet = 0;
	outFrames = 0;
	// 缓存的数据
	if (near_buf_cache_len_ > 0)
	{
		if (near_buf_cache_len_ + frames < nearFrameNums_)
		{
			memcpy(near_buf_.get() + near_buf_cache_len_, data, frames * sizeof(int16_t));
			near_buf_cache_len_ += frames;
			return 0;
		}
		int needLen = nearFrameNums_ - near_buf_cache_len_;
		memcpy(near_buf_.get() + near_buf_cache_len_, data, needLen * sizeof(int16_t));
		nearFrame_.UpdateFrame(0, near_buf_.get(), nearFrameNums_, nearSampleRate_,
			AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown,
			nearChannels_);
		iRet = apm_->ProcessStream(&nearFrame_);
		if (iRet != 0)
		{
			return iRet;
		}
		near_buf_cache_len_ = 0;
		i += needLen;

		if (outData != 0)
		{
			memcpy(outData + outFrames, nearFrame_.data(), nearFrame_.samples_per_channel_ * sizeof(int16_t));
		}
		outFrames += nearFrame_.samples_per_channel_;
	}

	for (; i + nearFrameNums_ <= frames; i += nearFrameNums_) {
		nearFrame_.UpdateFrame(0, data + i, nearFrameNums_, nearSampleRate_,
			AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown,
			nearChannels_);
		apm_->ProcessStream(&nearFrame_);

		if (outData != 0)
		{
			memcpy(outData + outFrames, nearFrame_.data(), nearFrame_.samples_per_channel_ * sizeof(int16_t));
		}
		outFrames += nearFrame_.samples_per_channel_;
	}
	if (i < frames)
	{
		near_buf_cache_len_ = frames - i;
		memcpy(near_buf_.get(), data + i, near_buf_cache_len_ * sizeof(int16_t));
	}
	return 0;
}



int WebRtcAecTest() {

	int farSampleRate = 8000;
	int farSampleLen = farSampleRate / 1000 * 10;
	int nearSampleRate = 8000;
	int nearSampleLen = nearSampleRate / 1000 * 10;
	short* far_frame = new short[farSampleLen * 2];
	short* near_frame = new short[nearSampleLen * 2];
	short* out_frame = new short[nearSampleLen * 2];
	short* out_frame_far = new short[farSampleLen * 2];
	int outLen, iRet = 0;
	FILE* fp_far = fopen("speaker_8K.pcm", "rb");
	FILE* fp_near = fopen("micin_8K.pcm", "rb");
	FILE* fp_out = fopen("out.pcm", "wb");
	FILE* fp_out_far = fopen("out_far.pcm", "wb");

	APMEcho apm;
	apm.Init(nearSampleRate, 1, farSampleRate, 1);

	do {
		if (!fp_far || !fp_near || !fp_out) {
			printf("WebRtcAecTest open file err \n");
			break;
		}

		while (1) {
			if (farSampleLen ==
				(int)
				fread(far_frame, sizeof(short), (size_t)farSampleLen, fp_far)) {
				fread(near_frame, sizeof(short), (size_t)nearSampleLen, fp_near);
				apm.FarFrame(far_frame, farSampleLen, out_frame_far, iRet);
				apm.NearFrame(near_frame, nearSampleLen, out_frame, outLen);

				fwrite(out_frame_far, sizeof(short), iRet, fp_out_far);
				fwrite(out_frame, sizeof(short), outLen, fp_out);



			}
			else {
				break;
			}
		}
	} while (0);

	fclose(fp_far);
	fclose(fp_near);
	fclose(fp_out);
	fclose(fp_out_far);

	delete[] far_frame;
	delete[] near_frame;
	delete[] out_frame;
	delete[] out_frame_far;
	return 0;
}

int main()
{
	WebRtcAecTest();
    std::cout << "Hello World!\n"; 
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门提示: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
