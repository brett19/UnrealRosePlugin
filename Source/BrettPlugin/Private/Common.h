#pragma once

#include "BrettPluginPrivatePCH.h"

class ReadHelper {
public:
	ReadHelper() : pos(0) {
	}

	template<typename T> const T& read() {
		pos += sizeof(T);
		return *(T*)&data[pos - sizeof(T)];
	}

	const char* read(int size) {
		auto out = (char*)&data[pos];
		pos += size;
		return out;
	}

	const char* readStr() {
		auto out = (char*)&data[pos];
		pos += strlen(out) + 1;
		return out;
	}

	FString readStr(int32 len) {
		char TempBuffer[256];
		memcpy(TempBuffer, &data[pos], len);
		TempBuffer[len] = 0;
		pos += len;
		return TempBuffer;
	}

	FString readByteStr() {
		return readStr(read<uint8>());
	}

	FLinearColor readColor3() {
		auto out = (float*)read(sizeof(float) * 3);
		return FLinearColor(out[0], out[1], out[2]);
	}

	FQuat readBadQuat() {
		struct FBadQuat { float W, X, Y, Z; };
		auto q = read<FBadQuat>();
		return FQuat(q.X, q.Y, q.Z, q.W);
	}

	int tell() {
		return pos;
	}

	void seek(int _pos) {
		pos = _pos;
	}

	void skip(int num) {
		pos += num;
	}

	int pos;
	TArray<uint8> data;
};

FVector rtuPosition(const FVector& v) {
	return FVector(v.X, -v.Y, v.Z);
};

FQuat rtuRotation(const FQuat& q) {
	return FQuat(-q.X, q.Y, -q.Z, q.W);
};

FVector rtuScale(const FVector& v) {
	return v;
}
