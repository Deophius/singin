#ifndef SPIRIT_LOGGER_H
#define SPIRIT_LOGGER_H
#include <fstream>
#include <ctime>
#include <string>
#include <filesystem>

namespace Spirit {
	class Logfile {
	public:
		// Constructs a log file named filename
		// The openmode can be manually chosen.
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

	// Chooses the logfile with the least recently written log file.
	// The log files' names have the format <base><num>.log.
	// lognum is the number of log files to maintain.
	// If some log files are not present, creates them on the fly.
	//
	// For example, if there are a1.log (12:45), a2.log (8:43),
	// and the user calls select_logfile("a", 4) at 15:00,
	// a3.log and a4.log will be created. Because they're freshly created
	// we will return a2.log.
	//
	// Throws std::logic_error if lognum is 0.
	std::string select_logfile(const std::string& base, std::size_t lognum);
}

#endif