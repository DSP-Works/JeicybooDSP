/*
VAD based Noise Spectrum Estimation.
Apply to Spectral Subtraction/Wiener Filter.
This program is made by jongcheol boo.

���� �̸��� �밡���� ǥ�����������.
http://jinsemin119.tistory.com/61 , https://en.wikipedia.org/wiki/Hungarian_notation , http://web.mst.edu/~cpp/common/hungarian.html

We are targetting format of 16kHz SamplingRate, mono channel, 16bit per sample.

���� �� �������.

���� Study���� ����ߴ� �����. �ʱ��� ���� ms������ Noise��� �����ϰ�, Estimation�Ͽ���.
�׷���, recording�� ���� �� ���Ŀ� Noise update������ �߻� �� �� ������, ���� �ʹݿ� ������ ��� �� �� �ֱ� ������
Voice Activity Detection�� �ʿ��ϴ�.

VAD�� ���������� ���Ǿ� �Դ� Ư¡�� ZCR�� Energy�� �̿��Ͽ� ��������/������ ������ �к��ϰ�,
������ ������ ��쿡�� Estimation�ϴ� Noise Spectrum�� update��Ų��.
https://www.asee.org/documents/zones/zone1/2008/student/ASEE12008_0044_paper.pdf


1) Zero Crossing Rate
: Time domain���� Amplitude 0���� �������� �󸶳� ���� �����Ǵ°��� ������ feature�̴�.
�����ϼ��� �������� ���� ū ���� ��Ÿ����, ������ ��� �������� ���� ���� ������ �ִ�.
2) Energy
: WAV������ Envelope�� ����ϴ�.

ZCR�� Energy�ΰ��� ���� ������ Threadhold����� Decision�� �̷����� �ϴµ�,
���������� �������̶� �Ǵ��ϴ� ����, ������������ �����̶� �Ǵ��ϴ� �ͺ��� risk�� �� ũ��.
�ֳ��ϸ�, ���������� �������̶�� �Ǵ��ϸ�, ���� ������ ����Ư���� ����, ������ ����� �ְ�ǰ� �Ǳ� �����̴�.
����������� �˻��� ����, Energy���� �������� ���ϴ� ���������� �ְ�, ZCR���� �������� ���ϴ� ���������� �ִ�.
����, �������� ������ ������ ���� or ������ �����ϱ�� �Ѵ�.
���� : (ZCR < Threadhold(ZCR) || Energy > Threadhold(Energy))

����� �� �������Ŵ� �����ν��� �ƴ� Voice Recorder�� ����Ǵ� ��츦 ����Ѵٸ�
���������̵� �����������̵� �׻� ����� �����ؾ��Ѵ�.

�Ʒ� �� Ư¡ ��� SNR�� ���� ȯ�濡���� Detection������ ��������.
���� GMM Study�� ������, Voice / UnVoiced �� GMM Ȯ���𵨸��� ���� Likelihood Ratio Test�� ���� Decision�ϴ� ����� ���� �غ���.

*/
#include<stdio.h>
#include<string.h>
#include<fftw3.h>
#include<math.h>


#define THRESHOLD_OF_ENERGY 700.0
#define THRESHOLD_OF_ZCR 200.0
#define FALSE 0
#define TRUE 1
#define PI 3.141592
#define KEEP_LEN 512
#define BLOCK_LEN 512 // 1000*(512/16000)= 32ms.
#define FFT_PROCESSING_SIZE 1024// 2�� n�°��� �Ǿ�� �Ѵ�.
#define NOISE_ESTIMATION_FRAMECOUNT 10.0 

bool VoiceActivityDetection(short *rgsInputBuffer, int iFrameCount);
void EstimateNoiseSpectrum(short *rgsTempBuffer, int iNumOfIteration, short *psInputBuffer, double *pdEstimatedNoiseSpec, int iFrameCount);
bool SpectralSubtraction(short *psInputBuffer, double *pdEstimatedNoiseSpec, short *psOutputBuffer, int iFrameCount);

void main(int argc, char** argv) {

	// fRead�� input, fWrite�� processing���� write �� output file pointer.
	FILE *fpRead;
	FILE *fpWrite;
	char rgcHeader[44] = { '\0', }; // header�� ������ �迭.
	short rgsInputBuffer[BLOCK_LEN] = { 0, };
	short rgsTempBuffer[BLOCK_LEN] = { 0, };
	double rgdEstimatedNS[FFT_PROCESSING_SIZE] = { 0, };
	short rgsOutputBuffer[BLOCK_LEN] = { 0, };
	int iNumOfIteration = 0;
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
	//fwrite(rgcHeader, 1, 44, fpWrite);

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

		if (SpectralSubtraction(rgsInputBuffer, rgdEstimatedNS, rgsOutputBuffer, BLOCK_LEN)) // TRUE�� return�Ǿ��� ��쿡�� file�� ����.
			fwrite(rgsOutputBuffer, sizeof(short), BLOCK_LEN, fpWrite);
	}
	printf("Processing End\n");
	fclose(fpRead);
	fclose(fpWrite);
	getchar();
	return;
}

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


bool SpectralSubtraction(short *psInputBuffer, double *pdEstimatedNoiseSpec, short *psOutputBuffer, int iFrameCount) {
	static int iNumOfIteration = 0;
	double rgdSubtractedAmp[FFT_PROCESSING_SIZE] = { 0, };
	double rgdSaveAngle[FFT_PROCESSING_SIZE] = { 0, };
	fftw_complex fcInputBefFFT[FFT_PROCESSING_SIZE] = { 0, }, fcInputAftFFT[FFT_PROCESSING_SIZE] = { 0, };
	fftw_complex fcOutputBefFFT[FFT_PROCESSING_SIZE] = { 0, }, fcOutputAftFFT[FFT_PROCESSING_SIZE] = { 0, };
	fftw_plan fpInput_p, fpOutput_p;
	static short rgssKeepBuffer[KEEP_LEN] = { 0, };
	static double rgsdOveraped[FFT_PROCESSING_SIZE] = { 0, }; // Overlap and Saved method
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
		rgdSubtractedAmp[i] = sqrt(fcInputAftFFT[i][0] * fcInputAftFFT[i][0] + fcInputAftFFT[i][1] * fcInputAftFFT[i][1]) - pdEstimatedNoiseSpec[i]; // Noise�� Spectrum Magnitude�� ��ȣ���� �����Ѵ�.
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