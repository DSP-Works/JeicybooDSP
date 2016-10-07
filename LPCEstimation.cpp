/*
Linear Prediction Coding Estimation.
This program is made by jongcheol boo.

���� �̸��� �밡���� ǥ�����������.
http://jinsemin119.tistory.com/61 , https://en.wikipedia.org/wiki/Hungarian_notation , http://web.mst.edu/~cpp/common/hungarian.html

We are targetting format of 16kHz SamplingRate, mono channel, 16bit per sample.

�������� ����.

�ڱ��ڽ��� Estimation�ϴ� Filter�� Coefficient�� ���ϸ� �װ��� LPC�� �ȴ�.
���� �ǹ̴� ���������� Vocal tract(����)�� �ش��ϴ� �����̴�.
���°���� FFT�ϸ�, Speech�� FFT �������� Evelope�� ���� ����� ������,
�� Evelope�� peak�� �Ǵ� Frequency�� Formant�� �ȴ�.

Yule-Walker Equation�� Ǯ��� �ϱ� ������ Matrix inverse������ �ʿ��ϴ�.
Matrix ������ �ؾߵǾ, Eigen lib�� �̸� ��ġ�� �� ��.(http://eigen.tuxfamily.org/index.php?title=Main_Page
http://blog.daum.net/pg365/156 ����)

LPC�� �����ϴµ� http://slideplayer.com/slide/2390999/ �ڷ� ������ ��.

*/
#include<stdio.h>
#include<string.h>
#include<fftw3.h>
#include<math.h>
#include<Eigen/Dense>
using Eigen::MatrixXd;

#define LPC_LEN 12
#define FALSE 0
#define TRUE 1
#define PI 3.141592
#define BLOCK_LEN 256 // 1000*(1024/44100)= 23ms.

bool LPCEstimation(short *rgsInputBuffer, double *dLPCFeature);

void main(int argc, char** argv) {

	// fRead�� input, fWrite�� processing���� write �� output file pointer.
	FILE *fpRead;
	FILE *fpWrite;
	char rgcHeader[44] = { '\0', }; // header�� ������ �迭.
	short rgsInputBuffer[BLOCK_LEN] = { 0, };
	double dLPCFeature[LPC_LEN] = { 0, };

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
		if ((fread(rgsInputBuffer, sizeof(short), BLOCK_LEN, fpRead)) == 0) {
			printf("Break! The buffer is insufficient.\n");
			break;
		}

		if (LPCEstimation(rgsInputBuffer, dLPCFeature)) { // TRUE�� return�Ǿ��� ��쿡�� file�� ����.
			printf("dLPCFeature[0] %f, dLPCFeature[1] %f, dLPCFeature[2] %f \n", dLPCFeature[0], dLPCFeature[1], dLPCFeature[2]);

			fwrite(dLPCFeature, sizeof(double), LPC_LEN, fpWrite);
		}
	}
	printf("Processing End\n");
	fclose(fpRead);
	fclose(fpWrite);
	getchar();
	return;
}

bool LPCEstimation(short *rgsInputBuffer, double *dLPCFeature) {

	MatrixXd mxToeplitzMatrix = MatrixXd(LPC_LEN, LPC_LEN);
	MatrixXd mxToeplitzVector = MatrixXd(LPC_LEN, 1);
	MatrixXd LPCFeature = MatrixXd(LPC_LEN, 1);
	static int iNumOfIteration = 0;
	static short rgssKeepBuffer[BLOCK_LEN] = { 0, };
	short rgsProcessingBuffer[BLOCK_LEN * 2] = { 0, };
	double rgdAftWindow[BLOCK_LEN * 2] = { 0, };
	double rgdResult[LPC_LEN + 1] = { 0, };

	iNumOfIteration++;
	// copy to processingbuffer (keepBuffer + InputBuffer)
	memcpy(rgsProcessingBuffer, rgssKeepBuffer, sizeof(short) * BLOCK_LEN);
	memcpy(rgsProcessingBuffer + BLOCK_LEN, rgsInputBuffer, sizeof(short) * BLOCK_LEN);

	// multiply window.
	for (int i = 0; i < BLOCK_LEN * 2; i++) {
		rgdAftWindow[i] = rgsProcessingBuffer[i] * (double)(0.54 - 0.46 * cos(2 * PI * i / (double)(2 * BLOCK_LEN - 1)));
	}

	for (int i = 0; i < LPC_LEN + 1; i++) {
		for (int j = 0; j < BLOCK_LEN * 2 - i; j++) {
			rgdResult[i] += rgdAftWindow[j] * rgdAftWindow[j + i];
		}
		rgdResult[i] /= BLOCK_LEN * 2 - i;
	}
	// Calc mxToeplitzMatrix
	for (int i = 0; i < LPC_LEN; i++) {
		for (int j = 0; j < LPC_LEN; j++) {
			mxToeplitzMatrix(i, j) = rgdResult[abs(i - j)];
		}
	}
	// Calc mxToeplitzVector
	for (int i = 0; i < LPC_LEN; i++) {
		mxToeplitzVector(i, 0) = -rgdResult[i + 1];
	}

	// Calc LPC Feature
	LPCFeature = mxToeplitzMatrix.inverse() * mxToeplitzVector;
	// Copy to output buffer.
	for (int i = 0; i < LPC_LEN; i++) {
		dLPCFeature[i] = LPCFeature(i);
	}
	// Save KeepBuffer
	memcpy(rgssKeepBuffer, rgsInputBuffer, sizeof(short) * BLOCK_LEN);
	if (iNumOfIteration > 1)
		return TRUE;
	else
		return FALSE;
}