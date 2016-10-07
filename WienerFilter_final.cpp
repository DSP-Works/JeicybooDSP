/*
Wiener Filter based Noise Cancellation
���� ���� : http://dsp-book.narod.ru/299.pdf
This program is made by jongcheol boo.

���� �̸��� �밡���� ǥ�����������.
http://jinsemin119.tistory.com/61 , https://en.wikipedia.org/wiki/Hungarian_notation , http://web.mst.edu/~cpp/common/hungarian.html

We are targetting format of 16kHz SamplingRate, mono channel, 16bit per sample.

���� �� �������.

1) ���������� Noise Spectrum���� ���� save �� ��.

2) PROCESSING_LEN�� BLOCK_LEN���� 2���� ������ �����ϰ�, double Buffering�� ����Ͽ� ����.

3) Overlap and Saved Method ����Ͽ� ����.

4) WienerFilter�� Transfer Function��  W(f) = Ps(f) / (Ps(f) + Pn(f)) �̴�.(Ps�� Power of singnal Spectrum, Pn�� Power of noise Spectrum)
�� ���Ŀ��� ���� �и� Ps(f)�� ������, W(f) = SNR(f) / (SNR(f) + 1)�� �ȴ�.

5) Fixed point�� ��ȯ �� Qformat�� ��� ������ ������ �� ��. Q15.16���� �ϸ�,, ���������� overflow�� �� ����. �׷���, value�� ������ ����Ͽ� Q format���� ��.

*/
#include<stdio.h>
#include<string.h>
#include<fftw3.h>
#include<math.h>
//#include"WaveHeader.h"


#define THRESHOLD_OF_ENERGY 700.0
#define THRESHOLD_OF_ZCR 200.0
//#define DURATION 10.08 // 10 sec.
#define SAMPLING_RATE 16000 // 16000 sampling per 1 sec.
#define CHANNEL 1 // mono channel
#define BIT_RATE 16 // BIT PER SAMPLE 

#define FALSE 0
#define TRUE 1
#define PI 3.141592
#define KEEP_LEN 512
#define BLOCK_LEN 512 // 1000*(512/16000)= 32ms.
#define FFT_PROCESSING_SIZE 1024// 2�� n�°��� �Ǿ�� �Ѵ�.
#define NOISE_ESTIMATION_FRAMECOUNT 10.0 

//void FillingHeaderInform(WAVE_HEADER *header, double dDuration);
bool VoiceActivityDetection(short *rgsInputBuffer, int iFrameCount);
void EstimateNoiseSpectrum(short *rgsTempBuffer, int iNumOfIteration, short *psInputBuffer, double *pdEstimatedNoiseSpec, int iFrameCount);
bool WienerFiltering(short *psInputBuffer, double *pdEstimatedNoiseSpec, short *psOutputBuffer, int iFrameCount);

void main(int argc, char** argv) {

	// fRead�� input, fWrite�� processing���� write �� output file pointer.
	FILE *fpRead;
	FILE *fpWrite;
	char rgcHeader[44] = { '\0', }; // header�� ������ �迭.
	short rgsInputBuffer[BLOCK_LEN] = { 0, };
	short rgsTempBuffer[BLOCK_LEN] = { 0, };
	double rgdEstimatedNS[FFT_PROCESSING_SIZE] = { 0, };
	short rgsOutputBuffer[BLOCK_LEN] = { 0, };
	int iNumOfIteration = 0, iFileSize = 0;
	double dDuration = 0;
	//WAVE_HEADER headerWrite;
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
	//fread(rgcHeader, 1, 44, fpRead);

	//fseek(fpRead, 0, SEEK_END);
	//iFileSize = ftell(fpRead);
	//dDuration = ((iFileSize - 44) / 2.0) / 16000.0;  // 44�� WAV header�� size, FrameSize�� 1 channel, 16 bit per sample�̹Ƿ� 2
	//fseek(fpRead, 44, SEEK_SET);
	//dDuration = 9.865;
	//FillingHeaderInform(&headerWrite, dDuration);
	//fwrite(&headerWrite, sizeof(headerWrite), 1, fpWrite);

	while (true)
	{
		if ((fread(rgsInputBuffer, sizeof(short), BLOCK_LEN, fpRead)) == 0) {
			printf("Break! The buffer is insufficient.\n");
			break;
		}
		if (!VoiceActivityDetection(rgsInputBuffer, BLOCK_LEN)) {
			iNumOfIteration++;
			if (iNumOfIteration == 1) {
				memcpy(rgsTempBuffer, rgsInputBuffer, sizeof(rgsInputBuffer));
			}
			else if (iNumOfIteration > 1) {
				EstimateNoiseSpectrum(rgsTempBuffer, iNumOfIteration, rgsInputBuffer, rgdEstimatedNS, BLOCK_LEN);
			}
		}
		else {
			iNumOfIteration = 0;
		}

		if (WienerFiltering(rgsInputBuffer, rgdEstimatedNS, rgsOutputBuffer, BLOCK_LEN)) // TRUE�� return�Ǿ��� ��쿡�� file�� ����.
			fwrite(rgsOutputBuffer, sizeof(short), BLOCK_LEN, fpWrite);
	}
	printf("Processing End iteration %d \n", iNumOfIteration);
	fclose(fpRead);
	fclose(fpWrite);
	getchar();
	return;
}

void EstimateNoiseSpectrum(short *rgsTempBuffer, int iNumOfIteration, short *psInputBuffer, double *pdEstimatedNoiseSpec, int iFrameCount) {

	static double rgsdAveragedNS[FFT_PROCESSING_SIZE] = { 0, };
	fftw_complex fcInputBefFFT[FFT_PROCESSING_SIZE] = { 0, }, fcInputAftFFT[FFT_PROCESSING_SIZE] = { 0, };
	fftw_plan fpInput_p;
	static short rgssKeepBuffer[KEEP_LEN] = { 0, };
	if (iNumOfIteration == 2) {
		memcpy(rgssKeepBuffer, rgsTempBuffer, sizeof(rgssKeepBuffer));
	}
	for (int i = 0; i < KEEP_LEN; i++) {
		fcInputBefFFT[i][0] = rgssKeepBuffer[i]; // copy keepBuffer to processing buffer.
	}
	for (int i = 0; i < iFrameCount; i++) {
		fcInputBefFFT[KEEP_LEN + i][0] = psInputBuffer[i]; // copy inputBuffer to processing buffer.
	}

	for (int i = 0; i < FFT_PROCESSING_SIZE; i++) {
		fcInputBefFFT[i][0] *= (0.54 - 0.46 * cos(2 * PI * i / (FFT_PROCESSING_SIZE - 1))); // Windowing
	}

	fpInput_p = fftw_plan_dft_1d(FFT_PROCESSING_SIZE, fcInputBefFFT, fcInputAftFFT, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_execute(fpInput_p);

	for (int i = 0; i < FFT_PROCESSING_SIZE; i++) {
		rgsdAveragedNS[i] += sqrt(fcInputAftFFT[i][0] * fcInputAftFFT[i][0] + fcInputAftFFT[i][1] * fcInputAftFFT[i][1]); // FFT��ȯ �� �� magnitude���� ����.
		if (iNumOfIteration >= 3) { // 3��° ���ۺ��� 2���� �ǹǷ�, �̶����� ������ 2 �� ��.
			rgsdAveragedNS[i] /= 2.0;
		}
	}

	if (iNumOfIteration == NOISE_ESTIMATION_FRAMECOUNT) {
		for (int i = 0; i < FFT_PROCESSING_SIZE; i++) {
			pdEstimatedNoiseSpec[i] = rgsdAveragedNS[i]; // / (NOISE_ESTIMATION_FRAMECOUNT - 1); // output Buffer���� copy
		}
	}
	//printf("pdEstimatedNoiseSpec[0] %f, pdEstimatedNoiseSpec[1] %f pdEstimatedNoiseSpec[2] %f \n", pdEstimatedNoiseSpec[0], pdEstimatedNoiseSpec[1], pdEstimatedNoiseSpec[2]);
	memcpy(rgssKeepBuffer, &psInputBuffer[iFrameCount - KEEP_LEN], sizeof(rgssKeepBuffer));
	fftw_destroy_plan(fpInput_p);
	return;
}


bool WienerFiltering(short *psInputBuffer, double *pdEstimatedNoiseSpec, short *psOutputBuffer, int iFrameCount) {
	static int iNumOfIteration = 0;
	double rgdSubtractedAmp[FFT_PROCESSING_SIZE] = { 0, };
	double rgdSaveAngle[FFT_PROCESSING_SIZE] = { 0, };
	fftw_complex fcInputBefFFT[FFT_PROCESSING_SIZE] = { 0, }, fcInputAftFFT[FFT_PROCESSING_SIZE] = { 0, };
	fftw_complex fcOutputBefFFT[FFT_PROCESSING_SIZE] = { 0, }, fcOutputAftFFT[FFT_PROCESSING_SIZE] = { 0, };
	fftw_plan fpInput_p, fpOutput_p;
	static short rgssKeepBuffer[KEEP_LEN] = { 0, };
	static double rgsdOveraped[FFT_PROCESSING_SIZE] = { 0, }; // Overlap and Saved method
	double dTempPowerOfInput = 0, dTempValue = 0;

	iNumOfIteration++;
	if (iNumOfIteration == 1) {
		memcpy(rgssKeepBuffer, &psInputBuffer[iFrameCount - KEEP_LEN], sizeof(rgssKeepBuffer));
		//fftw_destroy_plan(fpInput_p);
		//fftw_destroy_plan(fpOutput_p);
		return FALSE; // ù��° frame�� 2���� ���۰� �غ� �ȵƱ� ������ �������� ����.
	}

	for (int i = 0; i < KEEP_LEN; i++) {
		fcInputBefFFT[i][0] = rgssKeepBuffer[i];
	}
	for (int i = 0; i < iFrameCount; i++) {
		fcInputBefFFT[KEEP_LEN + i][0] = psInputBuffer[i];
	}

	for (int i = 0; i < FFT_PROCESSING_SIZE; i++) {
		fcInputBefFFT[i][0] *= (0.54 - 0.46 * cos(2 * PI * i / (FFT_PROCESSING_SIZE - 1)));
	}

	fpInput_p = fftw_plan_dft_1d(FFT_PROCESSING_SIZE, fcInputBefFFT, fcInputAftFFT, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_execute(fpInput_p);

	// Save phase information.
	for (int i = 0; i < FFT_PROCESSING_SIZE; i++) {
		rgdSaveAngle[i] = atan2(fcInputAftFFT[i][1], fcInputAftFFT[i][0]); // ���Ҽ��� angle ���� ����.
	}

	for (int i = 0; i < FFT_PROCESSING_SIZE; i++) {
		dTempPowerOfInput = fcInputAftFFT[i][0] * fcInputAftFFT[i][0] + fcInputAftFFT[i][1] * fcInputAftFFT[i][1];
		//rgdSubtractedAmp[i] = sqrt(dTempPowerOfInput) * (dTempPowerOfInput / (dTempPowerOfInput + pow(pdEstimatedNoiseSpec[i], 2)));
		//printf("(pow(pdEstimatedNoiseSpec[i], 2.0) / dTempPowerOfInput) %f \n", (pow(pdEstimatedNoiseSpec[i], 2.0) / dTempPowerOfInput));
		dTempValue = (pow(pdEstimatedNoiseSpec[i], 2.0) / dTempPowerOfInput);
		if (dTempValue >= 1.0) {
			dTempValue = 1.0;
		}
		rgdSubtractedAmp[i] = abs(sqrt(dTempPowerOfInput)) * (1.0 - dTempValue);

		// Ȥ�� ������ ������� �ʾҴ��� Ȯ��.
		fcOutputAftFFT[i][0] = rgdSubtractedAmp[i] * cos(rgdSaveAngle[i]); // ������ angle������ ���� ���Ҽ� ����.
		fcOutputAftFFT[i][1] = rgdSubtractedAmp[i] * sin(rgdSaveAngle[i]);
	}

	fpOutput_p = fftw_plan_dft_1d(FFT_PROCESSING_SIZE, fcOutputAftFFT, fcOutputBefFFT, FFTW_BACKWARD, FFTW_ESTIMATE);
	fftw_execute(fpOutput_p);

	for (int i = 0; i < FFT_PROCESSING_SIZE; i++) {
		rgsdOveraped[i] += 1. / FFT_PROCESSING_SIZE * fcOutputBefFFT[i][0]; // FFTW lib.�� IFFT ������ 1/N������ �����Ǿ��� ������ iFFT �� �� ����� ��. 
	}

	for (int i = 0; i < iFrameCount; i++) {
		psOutputBuffer[i] = rgsdOveraped[i];
	}
	printf("psOutputBuffer[0] %d, psOutputBuffer[1] %d psOutputBuffer[2] %d \n", psOutputBuffer[0], psOutputBuffer[1], psOutputBuffer[2]);
	memcpy(rgsdOveraped, rgsdOveraped + KEEP_LEN, sizeof(rgsdOveraped) / 2); // Overlap ���ۿ��� ���� ���۸� ������ ��� ��.
	memset(rgsdOveraped + KEEP_LEN, 0, sizeof(rgsdOveraped) / 2); // Overlap ���ۿ��� ������ ���.
	memcpy(rgssKeepBuffer, &psInputBuffer[iFrameCount - KEEP_LEN], sizeof(rgssKeepBuffer));
	fftw_destroy_plan(fpInput_p);
	fftw_destroy_plan(fpOutput_p);
	if (iNumOfIteration >= 3)
		return TRUE;
	else
		return FALSE;
}
#if 0
void FillingHeaderInform(WAVE_HEADER *header, double dDuration) {

	// "RIFF"
	memcpy(header->Riff.ChunkID, "RIFF", sizeof(char) * 4);
	header->Riff.ChunkSize = dDuration * SAMPLING_RATE * CHANNEL * BIT_RATE / 8 + 36; // CHUNK_ID�� CHUNK_SIZE �� 8BYTE�� ������ ��ü ũ��.
	memcpy(header->Riff.Format, "WAVE", sizeof(char) * 4);

	// "FMT"
	memcpy(header->Fmt.ChunkID, "fmt ", 4);
	header->Fmt.ChunkSize = 0x10; // FMT���� CHUNK�� ������(8BYte�� ������) FMT�� SIZE
	header->Fmt.AudioFormat = WAVE_FORMAT_PCM; // 2 Byte
	header->Fmt.NumChannels = CHANNEL; // 2 Byte
	header->Fmt.SampleRate = SAMPLING_RATE;
	header->Fmt.AvgByteRate = SAMPLING_RATE * CHANNEL * BIT_RATE / 8;
	header->Fmt.BlockAlign = CHANNEL * BIT_RATE / 8; // 2 Byte
	header->Fmt.BitPerSample = BIT_RATE; // 2 Byte

	// "DATA"
	memcpy(header->Data.ChunkID, "data", 4);
	header->Data.ChunkSize = dDuration * SAMPLING_RATE * CHANNEL * BIT_RATE / 8;

}
#endif

bool VoiceActivityDetection(short *rgsInputBuffer, int iFrameCount) {

	static short rgssKeepBuffer[KEEP_LEN] = { 0, };
	short rgsProcessingBuffer[FFT_PROCESSING_SIZE] = { 0, };
	double dEnergy = 0.0;
	int dZCR = 0;
	memcpy(rgsProcessingBuffer, rgssKeepBuffer, sizeof(rgssKeepBuffer));
	memcpy(rgsProcessingBuffer + KEEP_LEN, rgsInputBuffer, sizeof(short) * iFrameCount);

	for (int i = 0; i < FFT_PROCESSING_SIZE; i++) {
		rgsProcessingBuffer[i] *= (0.54 - 0.46 * cos(2 * PI * i / (FFT_PROCESSING_SIZE - 1))); // Windowing 
		// ���� ���� �� �κ�, short�� �Ǿ, �Ҽ��� ©��.

		//Calc Energy
		dEnergy += pow(rgsProcessingBuffer[i], 2.0);

		//Calc Zero Crossing Rate
		if (i != FFT_PROCESSING_SIZE) {
			if (rgsProcessingBuffer[i] * rgsProcessingBuffer[i + 1] < 0)
				dZCR++;
		}
	}
	dEnergy /= FFT_PROCESSING_SIZE;
	//dZCR /= (FFT_PROCESSING_SIZE - 1);

	printf(" dEnergy %f , dZCR %d \n", dEnergy, dZCR);
	if (dEnergy > THRESHOLD_OF_ENERGY || dZCR < THRESHOLD_OF_ZCR) {
		return TRUE;
	}
	else {
		return FALSE;
	}

	memcpy(rgssKeepBuffer, &rgsInputBuffer[iFrameCount - KEEP_LEN], sizeof(rgssKeepBuffer));

}