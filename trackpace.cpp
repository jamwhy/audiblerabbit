// trackpace.cpp : Defines the entry point for the console application.
//

//#include "stdafx.h"

#ifdef _WIN32
#pragma warning(disable : 4996)
#endif

#include <string>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
static const std::string kPath = "wavs\\";
#else
static const std::string kPath = "wavs/";
#endif

static const int kDataSizeOffset = 40;
static const int kDataOffset = 44;
static const int kHeaderSize = 36;
static const int kRIFFSize = 8;
static const int kMetersPerLap = 400;
static const double kMetersPerMile = 1609.344;
static const int kSamplesPerSecond = 16000;
const int kSizeOfSampleInBytes = 2;

// Globals
int s_iDistancePerMarker = 50;// 25;// 50;
double s_fDistancePerMarker = (double)s_iDistancePerMarker;
double s_fPaceDistanceUnits = kMetersPerMile;

#ifdef _WIN32
typedef unsigned long SizeType;
#else
typedef unsigned int SizeType;
#endif

SizeType sTotalDataSize = 0;
SizeType sTotalLabelSize = 0;

double sTotalSeconds = 0;

size_t GetFileSize(FILE* fp)
{
	fseek(fp, 0L, SEEK_END);
	size_t sz = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	return sz;
}

bool ReadFile(FILE* fp, char*&  buffer, size_t& fileSize)
{
	fileSize = GetFileSize(fp);
	buffer = new char[fileSize];
	bool res = true;
	if (buffer)
	{
		if (fread(buffer, 1, fileSize, fp) != fileSize) {
			res = false;
		}
	}

	return true;
}

struct FileInfo
{
	FileInfo() { mFp = NULL; mBuffer = NULL; mDataSize = 0; mFileSize = 0; mIsGo = false; mIsCountdown = false; }
	FileInfo(FILE* fp, char* buffer, SizeType dataSize, size_t fileSize, bool isGo = false)
	{
		mFp = fp;
		mBuffer = buffer;
		mDataSize = dataSize;
		mFileSize = fileSize;
		mIsGo = isGo;
		mIsCountdown = false;
	}

	FILE* mFp;
	char* mBuffer;
	std::string mPath;
	SizeType mDataSize;
	size_t mFileSize;
	bool mIsGo;
	bool mIsCountdown;
};

struct IntervalInfo
{
	IntervalInfo(){
		mTimes = 0;
		mMinutes = 0;
		mSeconds = 0;
		mDistance = 0.0;
		mPaceMinutes = 0;
		mPaceSeconds = 0;
	}

	int mTimes;
	int mMinutes;
	int mSeconds;
	double mDistance;
	int mPaceMinutes;
	int mPaceSeconds;
};

void LoadDistanceFile(FileInfo& info, std::string fileName)
{
	std::string thePath = kPath + fileName + std::string(".wav");
	FILE* fp = fopen(thePath.c_str(), "rb");
	if (!fp) {
		return;
	}

	char* buffer = NULL;
	size_t fileSize = 0;
	if (!ReadFile(fp, buffer, fileSize)) {
		// TODO: Error
	}
	fclose(fp);

	info.mFp = fp,
		info.mBuffer = buffer;
	info.mDataSize = *(SizeType*)&buffer[kDataSizeOffset];
	info.mFileSize = fileSize;
}

void LoadDistanceFileMinutes(FileInfo& info, int label)
{
	char str[12];
	sprintf(str, "%d", label);
	std::string distance(str);
	LoadDistanceFile(info, distance);
}

void LoadDistanceFileSeconds(FileInfo& info, int label)
{
	char str[12];
	sprintf(str, "%d", label);
	std::string distance(label ? str : "flat");

	if (label >= 1 && label <= 9) {
		distance = "0" + distance;
	}

	LoadDistanceFile(info, distance);
}

void AddCountdownFiles(std::vector<FileInfo>& fileVector)
{
	for (char i = '4'; i >= '1'; i--)
	{
		std::string count;
		count += i;
		std::string thePath = kPath + count + std::string(".wav");


		FILE* fp = fopen(thePath.c_str(), "rb");
		char* buffer = NULL;
		size_t fileSize = 0;
		if (!ReadFile(fp, buffer, fileSize)) {
			// TODO: Error
		}
		fclose(fp);
		FileInfo info(fp, buffer, *(SizeType*)&buffer[kDataSizeOffset], fileSize);
		info.mIsCountdown = true;

		fileVector.push_back(info);
	}
}

int totalSamples = 0;
void WriteData(char* data, size_t size, FILE* outFile)
{
	if (fwrite(data, 1, size, outFile) != size)
	{
		// TODO: Error
		int shit = 1;
	}

	if (totalSamples < 2879990 && (totalSamples + (size / kSizeOfSampleInBytes)) >= 2879990) {
		int shit = 1;
	}
	totalSamples += size / kSizeOfSampleInBytes;

	sTotalDataSize += size;
	fflush(outFile);

	sTotalSeconds += (double)sTotalDataSize / (double)kSamplesPerSecond / (double)kSizeOfSampleInBytes;
}


void AddSilence(FILE* outFile, size_t size)
{
	if (size == 0) {
		return;
	}

	char* fillData = new char[size];
	memset(fillData, 0, size);
	WriteData(fillData, size, outFile);
	delete[] fillData;
}

int SamplesPer50ThisLap(const IntervalInfo& interval)
{
	int secondsThisLap = interval.mMinutes * 60 + interval.mSeconds;
	double secsPer50ThisLap = secondsThisLap / (interval.mDistance / s_fDistancePerMarker);
	int samplesPer50ThisLap = (int)((double)secsPer50ThisLap * (double)kSamplesPerSecond);

	return samplesPer50ThisLap;
}

void BuildIntervals(FILE* outFile,
	const std::vector<IntervalInfo>& intervals,
	int overallRepeats,
	int initialFrontOffsetInSamples)
{

	std::map<std::string, FileInfo> distances;
	//	int frontOffset = initialFrontOffset;
	bool didInitial = false;
	bool needToAddLabelOnNextPass = false;
	FileInfo labelFileMinutes;
	FileInfo labelFileSeconds;
	int totalLabelSizeInSamples = 0;
	int distanceLabelOffset = 0;
	int total_d = 0;
	double startingSilentDistance = 0.0;

	for (int r = 0; r<overallRepeats; r++)
	{
		for (int n = 0; n<(int)intervals.size(); n++)
		{
			if (!didInitial) {
				totalSamples = initialFrontOffsetInSamples;
			}

			IntervalInfo theInterval = intervals[n];
		    for (int t = 0; t<theInterval.mTimes; t++)
			{
				int samplesThisInterval = ((theInterval.mMinutes * 60) + theInterval.mSeconds) * kSamplesPerSecond;
				int samplesPer50ThisLap = SamplesPer50ThisLap(theInterval);
				int sampleWrittenThisInterval = 0;
				double usedDistance = theInterval.mDistance - startingSilentDistance;

				for (int d = 0; (double)d<usedDistance; d += s_iDistancePerMarker, total_d += s_iDistancePerMarker)
				{
					int label = total_d - distanceLabelOffset;
					if (label >= kMetersPerLap) {
						distanceLabelOffset += kMetersPerLap;
						label = total_d - distanceLabelOffset; // recalulate
					}

					// NOTE: Hack for having one long incrementing lap count.
					static int totalLapCount = 1;
					if (label == 0) {
						label = totalLapCount++;
					}

					int frontOffsetInSamples = 0;

					char str[12];
					sprintf(str, "%d", label);
					std::string distance(str);
					if (!didInitial)  {
						didInitial = true;
						frontOffsetInSamples = initialFrontOffsetInSamples;
					}
					else
					{
						if (distances[distance].mFp == NULL)
						{
							LoadDistanceFile(distances[distance], distance);
						}

						const FileInfo& file = distances[distance];

						// Distance label
						char* data = &file.mBuffer[kDataOffset];
						WriteData(data, file.mDataSize, outFile);
						frontOffsetInSamples = file.mDataSize / kSizeOfSampleInBytes;

						if (samplesPer50ThisLap <= frontOffsetInSamples) {
							int shit = 1;
						}
					}

					int remainingSamples = samplesPer50ThisLap - frontOffsetInSamples;


					int padLabelSamples = kSamplesPerSecond / 2;

					if (needToAddLabelOnNextPass) {
						needToAddLabelOnNextPass = false;

						AddSilence(outFile, padLabelSamples*kSizeOfSampleInBytes);

						char* data = &labelFileMinutes.mBuffer[kDataOffset];
						WriteData(data, labelFileMinutes.mDataSize, outFile);

						data = &labelFileSeconds.mBuffer[kDataOffset];
						WriteData(data, labelFileSeconds.mDataSize, outFile);

						remainingSamples -= totalLabelSizeInSamples + padLabelSamples;
						if (remainingSamples < 0)  {
							remainingSamples = 0;
						}
						totalLabelSizeInSamples = 0;

						if (samplesPer50ThisLap <= totalLabelSizeInSamples) {
							int shit = 1;
						}
					}

					if (usedDistance - (double)d < s_fDistancePerMarker)
					{
						double percentageRemaining = (usedDistance - (double)d) / s_fDistancePerMarker;
						int leftOverSamples = (int)((double)samplesPer50ThisLap * percentageRemaining);

						// Check if there is a next interval
						if (n + 1 < (int)intervals.size())
						{

							startingSilentDistance = s_fDistancePerMarker - (usedDistance - (double)d);
							if (startingSilentDistance < 0.0){
								int shit = 1;
							}

							int samplesPer50NextLap = SamplesPer50ThisLap(intervals[n + 1]);

							int nextSilenceSamples = (int)((double)samplesPer50NextLap * (1.0 - percentageRemaining));

							LoadDistanceFileMinutes(labelFileMinutes, intervals[n + 1].mPaceMinutes);
							LoadDistanceFileSeconds(labelFileSeconds, intervals[n + 1].mPaceSeconds);
							totalLabelSizeInSamples = (labelFileMinutes.mDataSize + labelFileSeconds.mDataSize) / kSizeOfSampleInBytes;

							if (samplesPer50ThisLap <= totalLabelSizeInSamples) {
								int shit = 1;
							}

							if (leftOverSamples <= frontOffsetInSamples) // stopped in the middle of the marker
							{
								int over = totalSamples - samplesThisInterval;
								totalSamples = frontOffsetInSamples - leftOverSamples;
								int drift = over - totalSamples;

								AddSilence(outFile, padLabelSamples*kSizeOfSampleInBytes);

								char* data = &labelFileMinutes.mBuffer[kDataOffset];
								WriteData(data, labelFileMinutes.mDataSize, outFile);

								data = &labelFileSeconds.mBuffer[kDataOffset];
								WriteData(data, labelFileSeconds.mDataSize, outFile);

								int totalSamplesHere = totalLabelSizeInSamples + padLabelSamples + (frontOffsetInSamples - leftOverSamples);

								remainingSamples = nextSilenceSamples - totalSamplesHere;
								remainingSamples -= drift;
								if (remainingSamples < 0)  {
									remainingSamples = 0;
								}

								totalLabelSizeInSamples = 0;
							}
							else if ((totalLabelSizeInSamples + padLabelSamples) > nextSilenceSamples)
							{
								int totalSamplesHere = leftOverSamples - frontOffsetInSamples;
								AddSilence(outFile, totalSamplesHere*kSizeOfSampleInBytes);
								//	AddSilence(outFile, (totalSamplesHere - 1)*kSizeOfSampleInBytes);
								//char  thing[2];
								//	thing[0] = 0xff, thing[1] = 0x7f;
								//	WriteData(thing, 2, outFile);

								int drift = totalSamples - samplesThisInterval;
								totalSamples = 0;

								remainingSamples = nextSilenceSamples;
								remainingSamples -= drift;
								if (remainingSamples < 0)  {
									remainingSamples = 0;
								}
								// Defer until next time
								needToAddLabelOnNextPass = true;
							}
							else // write the label here
							{
								leftOverSamples -= frontOffsetInSamples; // marker was written

								AddSilence(outFile, leftOverSamples*kSizeOfSampleInBytes);
								//	AddSilence(outFile, (leftOverSamples-1)*kSizeOfSampleInBytes);
								//	char  thing[2];
								//	thing[0] = 0xff, thing[1] = 0x7f;
								//	WriteData(thing, 2, outFile);

								int drift = totalSamples - samplesThisInterval;
								totalSamples = 0;

								char* data = &labelFileMinutes.mBuffer[kDataOffset];
								WriteData(data, labelFileMinutes.mDataSize, outFile);

								data = &labelFileSeconds.mBuffer[kDataOffset];
								WriteData(data, labelFileSeconds.mDataSize, outFile);

								remainingSamples = nextSilenceSamples - totalLabelSizeInSamples;
								remainingSamples -= drift;
								if (remainingSamples < 0)  {
									remainingSamples = 0;
								}
							}
						}
						else {
							if (leftOverSamples <= frontOffsetInSamples) // stopped in the middle of the marker
							{
								remainingSamples = 0;
							}
							else
							{
								remainingSamples = leftOverSamples - frontOffsetInSamples;
								int drift = (totalSamples + remainingSamples) - samplesThisInterval;
								remainingSamples -= drift;
							}
						}
					}

					if (remainingSamples < 0)  {
						remainingSamples = 0;
					}
					AddSilence(outFile, remainingSamples*kSizeOfSampleInBytes);
				}
			}
		}
	}

	std::map<std::string, FileInfo>::iterator fileIter = distances.begin();
	for (; fileIter != distances.end(); fileIter++)
	{
		delete[] fileIter->second.mBuffer;
	}
}

int main(int argc, char* argv[])
{
	std::vector<FileInfo> theStartFiles;
	std::vector<FileInfo> theLabelFiles;
	std::map<std::string, FileInfo> theLabelMap;
	std::vector<IntervalInfo> theIntervals;
	int overallRepeats = 1;

	//	::MessageBox(NULL, L"Awaiting debugger connection before continuing", L"Debug Trap", MB_OK);
	//	TCHAR pwd[MAX_PATH];
	//	GetCurrentDirectory(MAX_PATH, pwd);
	//	MessageBox(NULL, pwd, pwd, 0);

	// Read Go File
	std::string thePath = kPath + std::string("go.wav");
	FILE* go_fp = fopen(thePath.c_str(), "rb");
	char* goBuffer = NULL;
	size_t fileSize = 0;
	if (!ReadFile(go_fp, goBuffer, fileSize)) {
		// TODO: Error
	}
	fclose(go_fp);
	FileInfo goInfo(go_fp, goBuffer, *(SizeType*)&goBuffer[kDataSizeOffset], fileSize, true);

	std::string outFileName("TEST.wav");

	// Read args
	bool hasCountdown = false;
	bool labelIsFirst = false;
	bool isPacedBased = false;// true; // HACK!!
	bool labelIsOff = false; // HACK!!
	for (int i = 1; i<argc; i++)
	{
		std::string input(argv[i]);

		if (input == "-countdown")
		{
			hasCountdown = true;
		}
		else if (input == "-i")
		{
			i++;
			outFileName = argv[i];
		}
		else if (input == "-label_first")
		{
			labelIsFirst = true;
		}
		else if (input == "-kilometers")
		{
			s_fPaceDistanceUnits = 1000.0;
		}
		else if (argv[i][0] == '-' && argv[i][1] == 'r')
		{
			size_t timesPos = input.find('x');
			std::string timesStr = input.substr(timesPos + 1);
			int repeats = atoi(timesStr.c_str());

			if (repeats > 1) {
				overallRepeats = repeats;
			}
		}
		else
		{
			// 1x1:15@350

			const std::string& interval = input;
			IntervalInfo intervalInfo;

			int times = 1;
			int minutes = 1;
			int seconds = 1;
			double distance = 400.0;

			if (isPacedBased)
			{
				size_t paceMinutePos = interval.find(':');
				std::string paceMinuteStr = interval.substr(0, paceMinutePos);
				int paceMinutes = atoi(paceMinuteStr.c_str());

				size_t paceSecondPos = interval.find('@');
				std::string paceSecondStr = interval.substr(paceMinutePos + 1, paceSecondPos - (paceMinutePos + 1));
				int paceSeconds = atoi(paceSecondStr.c_str());

				size_t timeMinutePos = interval.find(':', paceSecondPos);
				std::string timeMinuteStr = interval.substr(paceSecondPos + 1, paceSecondPos - (timeMinutePos + 1));
				minutes = atoi(timeMinuteStr.c_str());

				std::string timeSecondStr = interval.substr(timeMinutePos + 1);
				seconds = atoi(timeSecondStr.c_str());

				int paceInSeconds = (60 * paceMinutes) + paceSeconds;
				double metersPerSecond = s_fPaceDistanceUnits / (double)paceInSeconds;
				distance = metersPerSecond * ((60 * minutes) + seconds);

				intervalInfo.mPaceMinutes = paceMinutes;
				intervalInfo.mPaceSeconds = paceSeconds;
				intervalInfo.mTimes = 1;
				intervalInfo.mMinutes = minutes;
				intervalInfo.mSeconds = seconds;
				intervalInfo.mDistance = distance;

				if (!theLabelFiles.size()) // only the first pace
				{
					// Labal string stuff
					char str[12];
					if (paceMinutes)
					{
						sprintf(str, "%d", paceMinutes);
						std::string minutesStr(str);
						LoadDistanceFile(theLabelMap[minutesStr], minutesStr);
						sTotalLabelSize += theLabelMap[minutesStr].mDataSize;
						theLabelFiles.push_back(theLabelMap[minutesStr]);
					}

					// seconds
					sprintf(str, "%d", paceSeconds);
					std::string secondsStr(paceSeconds ? str : "flat"); // go flat for 0
					//	LoadDistanceFile(theLabelMap[secondsStr], secondsStr);
					LoadDistanceFileSeconds(theLabelMap[secondsStr], paceSeconds);
					sTotalLabelSize += theLabelMap[secondsStr].mDataSize;
					theLabelFiles.push_back(theLabelMap[secondsStr]);
				}

			}
			else
			{
				size_t timesPos = interval.find('x');
				std::string timesStr = interval.substr(0, timesPos);
				times = atoi(timesStr.c_str());

				size_t minutePos = interval.find(':');
				std::string minuteStr = interval.substr(timesPos + 1, minutePos - (timesPos + 1));
				minutes = atoi(minuteStr.c_str());

				size_t secondPos = interval.find('@');
				std::string secondStr = interval.substr(minutePos + 1, secondPos - (minutePos + 1));
				seconds = atoi(secondStr.c_str());

				std::string distanceStr = interval.substr(secondPos + 1);
				distance = (double)atoi(distanceStr.c_str());

				intervalInfo.mTimes = times;
				intervalInfo.mMinutes = minutes;
				intervalInfo.mSeconds = seconds;
				intervalInfo.mDistance = distance;


				// Labal string stuff
				char str[12];
				if (minutes)
				{
					sprintf(str, "%d", minutes);
					std::string minutesStr(str);
					LoadDistanceFile(theLabelMap[minutesStr], minutesStr);
					sTotalLabelSize += theLabelMap[minutesStr].mDataSize;
					theLabelFiles.push_back(theLabelMap[minutesStr]);
				}

				// seconds
				sprintf(str, "%d", seconds);
				std::string secondsStr(str);
				LoadDistanceFile(theLabelMap[secondsStr], secondsStr);
				sTotalLabelSize += theLabelMap[secondsStr].mDataSize;
				theLabelFiles.push_back(theLabelMap[secondsStr]);
			}


			// Validate distance
			if (intervalInfo.mDistance == 0.0) {
				intervalInfo.mDistance = 400.0;
			}

			// Validate time
			if (!minutes && seconds < 5) {
				intervalInfo.mSeconds = 5; // minimum time
			}

			theIntervals.push_back(intervalInfo);
		}
	}

	// TOTAL LABEL WILL FIT??
	/*	if (!labelIsFirst && !labelIsOff)
	{
	int totalFirst50Size = SamplesPer50ThisLap(theIntervals[0]) * kSizeOfSampleInBytes;
	if(totalFirst50Size < (int)(sTotalLabelSize + goInfo.mDataSize)) {
	labelIsFirst = true;
	}
	}*/

	// Add the first sounds
	if (labelIsFirst && !labelIsOff)
	{
		for (int i = 0; i<(int)theLabelFiles.size(); i++)
		{
			theStartFiles.push_back(theLabelFiles[i]);
		}
	}

	if (hasCountdown)
	{
		AddCountdownFiles(theStartFiles);
	}

	// Add Go
	theStartFiles.push_back(goInfo);

	if (!labelIsFirst && !labelIsOff)
	{
		for (int i = 0; i<(int)theLabelFiles.size(); i++)
		{
			theStartFiles.push_back(theLabelFiles[i]);
		}
	}

	// Open the out file and start writing
	FILE* outFile = fopen(outFileName.c_str(), "wb");
	if (outFile)
	{
		SizeType initialFrontOffset = 0;

		// First sounds
		bool foundGo = false;
		bool sawFirstCountdown = false;
		int goSampleCount = 0;
		for (int i = 0; i<(int)theStartFiles.size(); i++)
		{
			if (labelIsFirst && !labelIsOff)
			{
				if (!sawFirstCountdown && theStartFiles[i].mIsCountdown){
					sawFirstCountdown = true;
					int padLabelSamples = kSamplesPerSecond / 2;
					AddSilence(outFile, padLabelSamples*kSizeOfSampleInBytes);
				}
			}

			size_t size = (size_t)theStartFiles[i].mDataSize;
			if (i == 0)
			{
				size = theStartFiles[i].mFileSize; // write the whole file on the very first file
				if (fwrite(theStartFiles[i].mBuffer, 1, size, outFile) != size)
				{
					// TODO: Error
					int shit = 1;
				}
				sTotalDataSize += theStartFiles[i].mDataSize;
				fflush(outFile);
			}
			else
			{
				char* data = &theStartFiles[i].mBuffer[kDataOffset];
				if (fwrite(data, 1, size, outFile) != size)
				{
					// TODO: Error
					int shit = 1;
				}
				sTotalDataSize += size;
				fflush(outFile);
			}

			//if (theStartFiles[i].mIsGo) {
			//	foundGo = true;
				goSampleCount += theStartFiles[i].mDataSize / kSizeOfSampleInBytes;
			//}

			if (theStartFiles[i].mIsCountdown) {
				AddSilence(outFile, (kSamplesPerSecond*kSizeOfSampleInBytes) - theStartFiles[i].mDataSize);
				fflush(outFile);
			}

			//	if(foundGo) {
			//		initialFrontOffset += size;
			//	}
		}

		// Cheater miles
		// 3x1:15@300 1x0:45@100 1x0:52@300

		// 40-30s
		// 1x0:40@200 1x0:30@200 -rx2

		BuildIntervals(outFile, theIntervals, overallRepeats, goSampleCount);



		// done
		FileInfo done;
		LoadDistanceFile(done, "done");
		char* doneData = &done.mBuffer[kDataOffset];
		WriteData(doneData, done.mDataSize, outFile);

		// Set Sizes
		fseek(outFile, 4, SEEK_SET);
		SizeType totalFileSize = sTotalDataSize + kHeaderSize;
		fwrite((char*)&totalFileSize, 1, sizeof(SizeType), outFile);

		fseek(outFile, kDataSizeOffset, SEEK_SET);
		fwrite((char*)&sTotalDataSize, 1, sizeof(SizeType), outFile);
		fclose(outFile);
	} // else TODO: Error

	for (int i = 0; i<(int)theStartFiles.size(); i++)
	{
		delete[] theStartFiles[i].mBuffer;
	}

	return 0;
}

