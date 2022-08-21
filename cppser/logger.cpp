#include "logger.h"

namespace Spirit {
	Logfile::Logfile(const std::string& file, std::ios::openmode mode) : mFile(file, mode)
	{}

	void Logfile::flush() {
		mFile.flush();
	}

	LogSection::LogSection(Logfile& file) : mFile(&file)
	{}

	LogSection::LogSection(LogSection&& src) : mFile(src.mFile) {
		src.mFile = nullptr;
	}

	LogSection& LogSection::operator = (LogSection&& src) {
		mFile = src.mFile;
		src.mFile = nullptr;
		return *this;
	}

	LogSection::~LogSection() noexcept {
		mFile->flush();
	}
}