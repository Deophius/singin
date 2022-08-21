#ifndef SPIRIT_LOGGER_H
#define SPIRIT_LOGGER_H
#include <fstream>
#include <ctime>
#include <string>

namespace Spirit {
	class Logfile {
	public:
		// Constructs a log file named filename
		// It will be truncated and opened.
		explicit Logfile(const std::string& name, std::ios::openmode mode = std::ios::out);

		template <typename T>
		friend std::ofstream& operator << (Logfile& file, const T& t);

		// Flushes the underlying stream. Use LogSection for this.
		void flush();

		// Destructor, closes the stream. Actually this is not noexcept,
		// but we are going to ignore that possibility.
		virtual ~Logfile() noexcept = default;

		// Copy is prohibited.
		Logfile(const Logfile&) = delete;
		Logfile& operator = (const Logfile&) = delete;

		// Move is defaulted.
		Logfile(Logfile&&) = default;
		Logfile& operator = (Logfile&&) = default;
	private:
		std::ofstream mFile;
	};

	// Template, impl must be in header
	template <typename T>
	std::ofstream& operator << (Logfile& file, const T& t) {
		const auto ct = []{
			auto t = std::time(nullptr);
			return std::localtime(&t);
		}();
		std::string ts = std::asctime(ct);
		ts.back() = ' ';
		file.mFile << ts << t;
		return file.mFile;
	}

	// RAII type for a section after which the log should be flushed.
	class LogSection {
	public:
		// Initializes the log section with the log file.
		LogSection(Logfile& file);

		// Owner, no copy
		LogSection(const LogSection&) = delete;
		LogSection& operator = (const LogSection&) = delete;

		// After move, this object won't be responsible for flushing.
		LogSection(LogSection&& src);
		LogSection& operator = (LogSection&& src);

		// Destructor flushes the stream
		virtual ~LogSection() noexcept;
	private:
		// Non-owning pointer to the logfile
		Logfile* mFile;
	};
}

#endif