#include "singd.h"
#include <boost/asio.hpp>
#include <exception>
#include <regex>

namespace Spirit {
    std::vector<LessonInfo> near_exits(Connection& conn) {
        std::vector<LessonInfo> ans;
        const std::string sql = "select 考勤结束时间, ID, 安排ID from 课程信息 where "
            "考勤结束时间 > datetime('now', 'localtime') and "
            "考勤结束时间 < datetime('now', 'localtime', '3 minutes')";
        Statement stmt(conn, sql);
        while (true) {
            auto row = stmt.next();
            if (!row)
                break;
            ans.push_back({
                Clock::str2time(row->get<std::string>(0).substr(11)),
                row->get<std::string>(1),
                row->get<int>(2)
            });
        }
        return ans;
    }

    void parse_url(const Configuration& config, std::string& host, std::string& url) {
        // First, we will get the URL.
        url = config["url_leave_info"];
        // The host is matched with this raw regex
        std::regex re(R"(\d+\.\d+\.\d+\.\d+)");
        std::smatch matched;
        if (!std::regex_search(url, matched, re))
            throw std::logic_error("No host can be separated!");
        host = matched[0];
    }

    nlohmann::json execute_request(
        const Configuration& config,
        const std::vector<Student>& absent,
        const LessonInfo& lesson,
        Logfile& logfile
    ) {
        namespace asio = boost::asio;
        std::string url, host;
        parse_url(config, host, url);
        logfile << "host is " << host << "\nurl is " << url << std::endl;
        // The request body
        const std::string req_body = [&]{
            nlohmann::json j;
            j["courseid"] = lesson.anpai;
            j["coursedate"] = DayBeginClock()();
            j["studentids"] = nlohmann::json::array();
            for (const auto& student : absent)
                j["studentids"].push_back(student.id);
            return j.dump();
        }();
        logfile << "req_body:\n" << req_body << std::endl;
        // See example, sync HTTP client
        asio::io_context ioc;
        asio::ip::tcp::resolver resolver(ioc);
        asio::ip::tcp::socket socket(ioc);
        // RAII object to ensure log is flushed when error occurs.
        LogSection log_section(logfile);
        // Write the request.
        asio::streambuf req;
        std::ostream req_stream(&req);
        req_stream << "POST " << url << " HTTP/1.1\r\n"
            << "Content-Type: application/json\r\n"
            << "Host: " << host << "\r\n"
            << "Content-Length: " << req_body.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << req_body;
        logfile << "About to connect\n";
        try {
            asio::connect(socket, resolver.resolve(host, "http"));
        } catch (const boost::system::system_error& ex) {
            throw NetworkError(ex.what());
        }
        logfile << "Connected\nAbout to write.\n";
        asio::write(socket, req);
        logfile << "Written.\n";
        // Start the receive section
        asio::streambuf response;
        boost::system::error_code ec;
        asio::read(socket, response, ec);
        if (ec && ec != asio::error::eof)
            throw NetworkError(boost::system::system_error(ec).what());
        std::istream resp_stream(&response);
        // Now start the first line
        int status_code = 0;
        std::string status_msg, http_version;
        resp_stream >> http_version >> status_code >> status_msg;
        if (status_code != 200 && status_code != 302)
            throw NetworkError("Fail status code: "s + std::to_string(status_code));
        std::string lastline, currline;
        while (resp_stream) {
            std::getline(resp_stream, currline);
            if (currline.back() == '\r')
                currline.pop_back();
            if (currline.size())
                lastline = currline;
        }
        logfile << lastline << '\n';
        return nlohmann::json::parse(lastline);
    }

    nlohmann::json get_leave_info(
        const Configuration& config,
        const std::vector<Student>& absent,
        const LessonInfo& lesson,
        Logfile& logfile,
        int timeout
    ) {
        std::promise<nlohmann::json> prom;
        auto fut = prom.get_future();
        std::thread([&]{
            try {
                prom.set_value(execute_request(config, absent, lesson, logfile));
            } catch (...) {
                try {
                    prom.set_exception(std::current_exception());
                } catch (...) {
                    logfile << "Exception occurred in execute_request, but failed to set exception!\n" << std::flush;
                }
            }
        }).detach();
        auto fut_status = fut.wait_for(std::chrono::seconds(timeout));
        if (fut_status == std::future_status::ready)
            return fut.get();
        else
            throw NetworkError("execute_request timed out.");
    }
}