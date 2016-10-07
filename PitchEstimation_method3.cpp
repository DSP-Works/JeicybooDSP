/*
Pitch Estimation
This program is made by jongcheol boo.

���� �̸��� �밡���� ǥ�����������.
http://jinsemin119.tistory.com/61 , https://en.wikipedia.org/wiki/Hungarian_notation , http://web.mst.edu/~cpp/common/hungarian.html

������������ PCM�� time scale�� �÷��� ����, �ֱ������� �ݺ��Ǵ� �������� ���� �� �� �ִ�.
�� �ݺ��Ǵ� �ð��� �ֱ��̰�, �� �ֱ��� ������ �ٷ� ��Ҹ��� �⺻���ļ� pitch�� �ȴ�.

Pitch Estimation�� ������ ����� ����.
1) Method 1 : Using relation of (Autocorrelation --->(FFT) PowerSpectralDensity)
http://ktword.co.kr/abbr_view.php?m_temp1=3726&m_search=S

2) Method 2 : Using AMDF(Average Magnitude Difference Function)
http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.590.5684&rep=rep1&type=pdf

3) Method 3 : Using AutoCorrelation
http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.590.5684&rep=rep1&type=pdf

*/
#include<stdio.h>
#include<string.h>
#include<math.h>

#define BLOCK_LEN 512 // 1000*(512/16000)= 32ms.
#define ROCESSING_LEN 1024// 2�� n�°��� �Ǿ�� �Ѵ�.
#define KEEP_LEN 512
#define DEFAULT_SAMPLINGRATE 16000.0

void CalcPitch(short *psInputBuffer, int iFrameCount);

void main(int argc, char** argv) {

	// fRead�� input, fWrite�� processing���� write �� output file pointer.
	FILE *fpRead;
	char rgcHeader[44] = { '\0', }; // header�� ������ �迭.
	short rgsInputBuffer[BLOCK_LEN] = { 0, };

	if (argc != 2) {
		printf("path�� 1�� �Է��ؾ� �մϴ�.\n"); // input path, output path
		return;
	}
	else {
		for (int i = 1; i < 2; i++)
			printf("%d-th path %s \n", i, argv[i]);
	}

	if ((fpRead = fopen(argv[1], "rb")) == NULL)
		printf("Read File Open Error\n");

	// Read�� Wav ���� �� ���� Header 44Byte ��ŭ Write�� Wav���Ͽ� write.
	fread(rgcHeader, 1, 44, fpRead);

	while (true)
	{
		if ((fread(rgsInputBuffer, sizeof(short), BLOCK_LEN, fpRead)) == 0) {
			printf("Break! The buffer is insufficient.\n");
			break;
		}
		CalcPitch(rgsInputBuffer, BLOCK_LEN);
	}
	printf("Processing End\n");
	fclose(fpRead);
	getchar();
	return;
}

void CalcPitch(short *psInputBuffer, int iFrameCount) {

	static short rgssKeepBuffer[KEEP_LEN] = { 0, };
	double dAutoCorrelation[BLOCK_LEN] = { 0, };
	short rgsProcessingBuffer[ROCESSING_LEN] = { 0, };
	double dMax = 0;
	int iArg = 0;
	memcpy(rgsProcessingBuffer, rgssKeepBuffer, sizeof(rgssKeepBuffer));
	memcpy(rgsProcessingBuffer + KEEP_LEN, psInputBuffer, sizeof(short) * iFrameCount);

	for (int k = 0; k < BLOCK_LEN; k++) {
		for (int i = 0; i < ROCESSING_LEN - k; i++) {
			dAutoCorrelation[k] += rgsProcessingBuffer[i] * rgsProcessingBuffer[i + k];
		}
		dAutoCorrelation[k] /= (double)(ROCESSING_LEN - k);
	}

	//Calculate pitch : find min arg of AutoCorrelation
	dMax = dAutoCorrelation[iFrameCount - 1];
	printf(" dMin %f \n", dMax);
	for (int i = iFrameCount - 1; i > 100; i--) {

		if (dAutoCorrelation[i] >= dMax) {
			iArg = i;
			dMax = dAutoCorrelation[i];
		}
	}
	printf("Estimation arg %d , dMin %f pitch %f \n", iArg, dMax, (DEFAULT_SAMPLINGRATE / (double)iArg));

	// ������ �� ������ �κ��� KEEP_LENGTH ��ŭ Keep��. 
	memcpy(rgssKeepBuffer, &psInputBuffer[iFrameCount - KEEP_LEN], sizeof(rgssKeepBuffer));
	return;
}
