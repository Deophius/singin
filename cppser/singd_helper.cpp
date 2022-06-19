#include "singd.h"
#include <boost/asio.hpp>
#include <exception>
#include <regex>

namespace Spirit {
    std::vector<LessonInfo> near_exits(Connection& conn) {
        std::vector<LessonInfo> ans;
        const std::string sql = "select 考勤结束时间, ID from 课程信息 where "
            "考勤结束时间 > datetime('now', 'localtime') and "
            "考勤结束时间 < datetime('now', 'localtime', '3 minutes')";
        Statement stmt(conn, sql);
        while (true) {
            auto row = stmt.next();
            if (!row)
                break;
            ans.push_back({
                Clock::str2time(row->get<std::string>(0).substr(11)),
                row->get<std::string>(1)
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
        {
            asio::streambuf req;
            std::ostream req_stream(&req);
            req_stream << "POST " << url << " HTTP/1.1\r\n"
                << "Content-Type: application/json\r\n"
                << "Host: " << host << "\r\n"
                << "Content-Length: " << req_body.size() << "\r\n"
                << "Connection: close\r\n\r\n"
                << req_body;
            logfile << "About to connect\n";
            asio::connect(socket, resolver.resolve(host, "http"));
            logfile << "Connected\nAbout to write.\n";
            asio::write(socket, req);
            logfile << "Written.\n";
        }
        // Start the receive section
        asio::streambuf response;
        asio::read_until(socket, response, "\r\n");
        std::istream resp_stream(&response);
        {
            // Status code line
            std::string http_version, status_msg;
            int status_code;
            resp_stream >> http_version >> status_code;
            std::getline(resp_stream, status_msg);
            if (!resp_stream || http_version.substr(0, 5) != "HTTP/")
                throw NetworkError("Invalid response from server, no HTTP version.");
            if (status_code != 200 && status_code != 302)
                throw NetworkError("Status code: " + std::to_string(status_code) + " " + status_msg);
        }
        logfile << "Received status code line\n";
        // Directly ignore all headers
        try {
            asio::read_until(socket, response, "\r\n\r\n");
            std::string header;
            while (std::getline(resp_stream, header) && header != "\r")
                ;
        } catch (const boost::system::system_error& ex) {
            throw NetworkError("While reading header, "s + ex.what());
        }
        logfile << "Headers ignored!\n";
        // Finally, read the last line of response, which is the body.
        try {
            asio::read_until(socket, response, "\r\n");
        } catch (const boost::system::system_error& ex) {
            throw NetworkError("While reading body, "s + ex.what());
        }
        std::string body;
        // It seems that there are no whitespaces in the server's reply.
        resp_stream >> body;
        logfile << "Body is:\n" << body << '\n';
        return nlohmann::json::parse(body);
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
            return nlohmann::json();
    }
}