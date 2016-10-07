/*
AnalysisAWGN
This program is made by jongcheol boo.

���� �̸��� �밡���� ǥ�����������.
http://jinsemin119.tistory.com/61 , https://en.wikipedia.org/wiki/Hungarian_notation , http://web.mst.edu/~cpp/common/hungarian.html

We are targetting format of 16kHz SamplingRate, mono channel, 16bit per sample.

C++11���� �����ϴ� API�� �̿��Ͽ� normali distribution�� noise�� generation�Ͽ���,
�̸� ���� clean ��ȣ�� ���������ν�, noisy��ȣ�� ���� �� �ִ�.

White Noise�� sample������ ������ normal distribution�� �����µ� �ڼ��� ������ �Ʒ�����Ʈ ����ٶ��ϴ�.
https://en.wikipedia.org/wiki/White_noise

AutoCorrelation�� PowerSpectralDensity�� Fourier Transform���谡 �ְ�,
White Noise�� �ٸ��ð��� dependency�� ���� ������, Rxx(0) = variance�̰�, Rxx(m) = 0 (��, m�� 0�� �ƴ�)
Digital ��ȣ���� 1 0 0 0 0 ... �� �Ǵ� ��ȣ�� impulse�̰� �̸� FT.�ϸ� ������� �ȴ�.
�ᱹ PowerSpectralDensity�� ����� �ǰ�, �� �뿪�� ������ �������� ���� �ȴ�.
https://cnx.org/contents/cillqc8i@5/Autocorrelation-of-Random-Proc
���� White noise�� Spectrogram�� ���� �� �뿪�� �������� ���� �����ϰ� �����Ѵ�.

*/
#include<stdio.h>
#include<string.h>
#include<fftw3.h>
#include<math.h>
#include<stdlib.h> // for using rand() function.
#include<Windows.h>
#include<random>
#include<chrono>

#define BLOCK_SIZE 512 // 1000*(512/16000)= 32ms.
#define FFT_PROCESSING_SIZE 1024// 2�� n�°��� �Ǿ�� �Ѵ�.
#define KEEP_LENGTH 512
#define DEFAULT_SAMPLINGRATE 16000.0

void GaussianRandom(short* rgdNoise, double dAverage, double dStddev);
void AnalysisAdditiveWhiteGaussianNoise(short *psNoiseBuffer, int iFrameCount);
void AddAdditiveWhiteGaussianNoise(short *psInputBuffer, short *psOutputBuffer, int iFrameCount);

void main(int argc, char** argv) {

	// fRead�� input, fWrite�� processing���� write �� output file pointer.
	FILE *fpRead;
	FILE *fpWrite;
	char rgcHeader[44] = { '\0', }; // header�� ������ �迭.
	short rgsInputBuffer[BLOCK_SIZE] = { 0, };
	short rgsOutputBuffer[BLOCK_SIZE] = { 0, };

	if (argc != 3) {
		printf("path�� 2�� �Է��ؾ� �մϴ�.\n"); // input path, output path
		return;
	}
	else {
		for (int i = 1; i < 3; i++)
			printf("%d-th path %s \n", i, argv[i]);
	}

	if ((fpRead = fopen(argv[1], "rb")) == NULL)
		printf("Read File Open Error\n");

	if ((fpWrite = fopen(argv[2], "wb")) == NULL)
		printf("Write File Open Error\n");

	// Read�� Wav ���� �� ���� Header 44Byte ��ŭ Write�� Wav���Ͽ� write.
	fread(rgcHeader, 1, 44, fpRead);
	//fwrite(rgcHeader, 1, 44, fpWrite);

	while (true)
	{
		if ((fread(rgsInputBuffer, sizeof(short), BLOCK_SIZE, fpRead)) != BLOCK_SIZE) {
			printf("Break! The buffer is insufficient.\n");
			break;
		}
		AddAdditiveWhiteGaussianNoise(rgsInputBuffer, rgsOutputBuffer, BLOCK_SIZE);
		fwrite(rgsOutputBuffer, sizeof(short), BLOCK_SIZE, fpWrite);
	}
	printf("Processing End\n");
	fclose(fpRead);
	fclose(fpWrite);
	getchar();
	return;
}

void GaussianRandom(short* rgdNoise, double dAverage, double dStddev) {
	// construct a trivial random generator engine from a time-based seed:
	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);

	std::normal_distribution<double> distribution(dAverage, dStddev);

	for (int i = 0; i < BLOCK_SIZE; i++) {
		rgdNoise[i] = distribution(generator);
	}
}

void AnalysisAdditiveWhiteGaussianNoise(short *psNoiseBuffer, int iFrameCount) {

	fftw_complex fcInputBefFFT[FFT_PROCESSING_SIZE] = { 0, }, fcInputAftFFT[FFT_PROCESSING_SIZE] = { 0, };
	fftw_complex fcOutputBefFFT[FFT_PROCESSING_SIZE] = { 0, }, fcOutputAftFFT[FFT_PROCESSING_SIZE] = { 0, };
	fftw_plan fpInput_p, fpOutput_p;
	static short rgssKeepBuffer[KEEP_LENGTH] = { 0, };
	double dAutoCorrelation[BLOCK_SIZE] = { 0, };

	for (int i = 0; i < KEEP_LENGTH; i++) {
		fcInputBefFFT[i][0] = rgssKeepBuffer[i];
	}
	for (int i = 0; i < iFrameCount; i++) {
		fcInputBefFFT[KEEP_LENGTH + i][0] = psNoiseBuffer[i];
	}
	fpInput_p = fftw_plan_dft_1d(FFT_PROCESSING_SIZE, fcInputBefFFT, fcInputAftFFT, FFTW_FORWARD, FFTW_ESTIMATE);

	fpOutput_p = fftw_plan_dft_1d(FFT_PROCESSING_SIZE, fcOutputAftFFT, fcOutputBefFFT, FFTW_BACKWARD, FFTW_ESTIMATE);
	fftw_execute(fpInput_p);

	for (int i = 0; i < FFT_PROCESSING_SIZE; i++) {
		fcOutputAftFFT[i][0] = fcInputAftFFT[i][0] * fcInputAftFFT[i][0] + fcInputAftFFT[i][1] * fcInputAftFFT[i][1];
		fcOutputAftFFT[i][1] = 0;
	}
	fftw_execute(fpOutput_p);
	for (int i = 0; i < iFrameCount; i++) {
		dAutoCorrelation[i] = fcOutputBefFFT[i][0] * 1. / FFT_PROCESSING_SIZE;
	}
	//Check AutoCorrelation Vector


	// ������ �� ������ �κ��� KEEP_LENGTH ��ŭ Keep��.
	memcpy(rgssKeepBuffer, &psNoiseBuffer[iFrameCount - KEEP_LENGTH], sizeof(rgssKeepBuffer));
	fftw_destroy_plan(fpInput_p);
	fftw_destroy_plan(fpOutput_p);
	return;
}

void AddAdditiveWhiteGaussianNoise(short *psInputBuffer, short *psOutputBuffer, int iFrameCount) {
	short rgdNoise[BLOCK_SIZE] = { 0, };
	double dTargetMean = 0.0, dTargetStd = 10.0;
	srand(GetTickCount());
	GaussianRandom(rgdNoise, dTargetMean, dTargetStd);
	for (int i = 0; i < iFrameCount; i++) {
		psOutputBuffer[i] = rgdNoise[i] + psInputBuffer[i];
	}
	AnalysisAdditiveWhiteGaussianNoise(rgdNoise, iFrameCount);
}