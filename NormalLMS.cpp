/*
LMS(Least Mean Square) Filter. (Adaptive Filter)
This program is made by jongcheol boo.

���� �̸��� �밡���� ǥ�����������.
http://jinsemin119.tistory.com/61 , https://en.wikipedia.org/wiki/Hungarian_notation , http://web.mst.edu/~cpp/common/hungarian.html

We are targetting format of 16kHz SamplingRate, mono channel, 16bit per sample.

�������� ����.

Reference Signal�� Estimation�� Signal�� ����(Least Mean Square error)�� �ּҰ� �ǵ��� 
Estimation System�� FilterCoefficient�� �ǽð� update�ϴ� filter�̴�.
���� ���������� ���� �⺻���� LMS version�̴�. 
(�� Sample ���� coefficient�� update, 8kHz�� ��� 1�ʿ� 8000�� update) 
Haykin�� Adaptive Filter Theory å ���� �� ��.

Tunning Value�� MU value������ �� �ؼ�, Filter�� ����� �߻����� �ʵ���, ��.

*/
#include<stdio.h>
#include<string.h>
#include<fftw3.h>
#include<math.h>

#define FALSE 0
#define TRUE 1
#define PI 3.141592
#define BLOCK_LEN 1024 // 1000*(512/16000)= 32ms.
#define FILTER_LEN 256
#define KEEP_LEN 255
#define MU 0.0001
#define COMPENSATION 0.0001

bool LMSFilter(short *rgsInputBuffer, short *rgsRefBuffer, short *rgdLMSFilteringBuffer, short *rgsError);

void main(int argc, char** argv) {

	// fRead�� input, fWrite�� processing���� write �� output file pointer.
	FILE *fpReadInput, *fpReadRef, *fpWriteEst, *fpWriteErr;
	char rgcHeader[44] = { '\0', }; // header�� ������ �迭.
	short rgsInputBuffer[BLOCK_LEN] = { 0, }, rgsRefBuffer[BLOCK_LEN] = { 0, }, rgdLMSFilteringBuffer[BLOCK_LEN] = { 0, }, rgsError[BLOCK_LEN] = { 0, };

	if (argc != 5) {
		printf("path�� 4�� �Է��ؾ� �մϴ�.\n"); // input path, output path
		return;
	} else {
		for (int i = 1; i < 5; i++)
			printf("%d-th path %s \n", i, argv[i]);
	}

	if ((fpReadInput = fopen(argv[1], "rb")) == NULL)
		printf("Read Input File Open Error\n");

	if ((fpReadRef = fopen(argv[2], "rb")) == NULL)
		printf("Read Ref File Open Error\n");

	if ((fpWriteEst = fopen(argv[3], "wb")) == NULL)
		printf("Write File Open Error\n");

	if ((fpWriteErr = fopen(argv[4], "wb")) == NULL)
		printf("Err Write File Open Error\n");

	// Read�� Wav ���� �� ���� Header 44Byte ��ŭ Write�� Wav���Ͽ� write.
	fread(rgcHeader, 1, 44, fpReadInput);
	//fread(rgcHeader, 1, 44, fpReadRef);
	//fwrite(rgcHeader, 1, 44, fpWrite);

	while (true)
	{
		if ((fread(rgsInputBuffer, sizeof(short), BLOCK_LEN, fpReadInput)) == 0) {
			printf("Break! The buffer is insufficient.\n");
			break;
		}

		if ((fread(rgsRefBuffer, sizeof(short), BLOCK_LEN, fpReadRef)) == 0) {
			printf("Break! The buffer is insufficient.\n");
			break;
		}

		if (LMSFilter(rgsInputBuffer, rgsRefBuffer, rgdLMSFilteringBuffer, rgsError)) { // TRUE�� return�Ǿ��� ��쿡�� file�� ����.

			fwrite(rgdLMSFilteringBuffer, sizeof(short), BLOCK_LEN, fpWriteEst);
			fwrite(rgsError, sizeof(short), BLOCK_LEN, fpWriteErr);
		}
	}
	printf("Processing End\n");
	fclose(fpReadInput);
	fclose(fpReadRef);
	fclose(fpWriteEst);
	fclose(fpWriteErr);
	getchar();
	return;
}

bool LMSFilter(short *rgsInputBuffer, short *rgsRefBuffer, short *rgdLMSFilteringBuffer, short *rgsError) {

	static short rgssKeepInput[KEEP_LEN] = { 0, };
	static double rgsdCoefficient[FILTER_LEN] = { 0, };
	short rgsProcessingInput[BLOCK_LEN + KEEP_LEN] = { 0, };
	double dNormalizingFactor = 0; // using Normalizing Factor.
	static int iNumOfIteration = 0;
	double dTempCalc = 0.0; // short�� �ٷ� ��⺸�ٴ� ����� �� �ϰ�, �� �������� short ���ڿ� �ִ´�.

	iNumOfIteration++;
	memcpy(rgsProcessingInput, rgssKeepInput, KEEP_LEN * sizeof(short));
	memcpy(rgsProcessingInput + KEEP_LEN, rgsInputBuffer, BLOCK_LEN * sizeof(short));

	for (int i = 0; i < BLOCK_LEN; i++) {
		// Estimation output
		dTempCalc = 0;
		for (int j = 0; j < FILTER_LEN; j++) {
			dTempCalc += rgsdCoefficient[FILTER_LEN - 1 - j] * rgsProcessingInput[j + i];
		}
		rgdLMSFilteringBuffer[i] = dTempCalc; //dTempCalc �� double���ڿ� �������� �ְ�, �������� short�� output buffer�� �ִ´�.
		rgsError[i] = rgsRefBuffer[i] - rgdLMSFilteringBuffer[i];
		// Calc Normalizing Factor
		dNormalizingFactor = 0;
		for (int j = 0; j < FILTER_LEN; j++) {
			dNormalizingFactor += pow((double)rgsProcessingInput[j + i], 2.0);
		}
		// Estimation Coefficient

		for (int j = 0; j < FILTER_LEN; j++) {
			rgsdCoefficient[j] += 2.0 * (double)rgsProcessingInput[j + i] * MU * (rgsRefBuffer[i] - rgdLMSFilteringBuffer[i]) / (dNormalizingFactor + COMPENSATION);
		}
	}
	printf("rgsdCoefficient[0] %f, rgsdCoefficient[1] %f, rgsdCoefficient[2] %f \n", rgsdCoefficient[0], rgsdCoefficient[1], rgsdCoefficient[2]);
	//Keeping Buffer
	memcpy(rgssKeepInput, &rgsInputBuffer[BLOCK_LEN - KEEP_LEN], sizeof(short) * KEEP_LEN);

	if (iNumOfIteration > 1)
		return TRUE;
	else
		return FALSE;
}