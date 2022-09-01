#include "logger.h"
#include <filesystem>

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

	std::string select_logfile(const std::string& base, std::size_t lognum) {
		namespace stdfs = std::filesystem;
		if (lognum == 0)
			throw std::logic_error("Lognum should be greater than zero!");
		std::string ans = base + "0.log";
		for (std::size_t i = 0; i < lognum; i++) {
			std::string currfile = base + std::to_string(i) + ".log";
			if (!stdfs::exists(currfile)) {
				// This file does not exist, it is indeed new enough.
				return currfile;
			}
			if (stdfs::last_write_time(ans) > stdfs::last_write_time(currfile))
				ans = currfile;
		}
		return ans;
	}
}