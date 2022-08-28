#include "singd.h"
#include <array>
#include <algorithm>
#include <boost/asio.hpp>
#include <iterator>
#include <memory>
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
        url = config["url_stu_new"];
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
            j["ID"] = lesson.anpai;
            j["date"] = APITimeClock()(lesson.endtime);
            j["isloadfacedata"] = false;
            j["faceversion"] = 2;
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
        try {
            asio::connect(socket, resolver.resolve(host, "http"));
        } catch (const boost::system::system_error& ex) {
            throw NetworkError(ex.what());
        }
        logfile << "Connected to the school server.\n";
        try {
            asio::write(socket, req);
        } catch (const boost::system::system_error& ex) {
            throw NetworkError(ex.what());
        }
        logfile << "Request written.\n";
        // Start the receive section
        asio::streambuf response;
        boost::system::error_code ec;
        asio::read(socket, response, ec);
        if (ec && ec != asio::error::eof)
            throw NetworkError(boost::system::system_error(ec).what());
        logfile << "Received response.\n";
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
        logfile << "Done processing.\n";
        logfile << "Last line length: " << lastline.size() << '\n';
        return nlohmann::json::parse(lastline);
    }

    nlohmann::json get_stu_new(
        const Configuration& config,
        const std::vector<Student>& absent,
        const LessonInfo& lesson,
        Logfile& logfile,
        int timeout
    ) {
        using PromType = std::promise<nlohmann::json>;
        std::shared_ptr<PromType> prom(new PromType());
        auto fut = prom->get_future();
        // For thread safety concerns, copy all the data except for the logfile one.
        // This should copy-assign the shared pointer, so prom will still be valid if
        // get_stu_new returns a timeout error.
        // logfile should always be valid.
        std::thread([&logfile, &config]
            (std::vector<Student> absent, LessonInfo lesson, std::shared_ptr<std::promise<nlohmann::json>> prom) {
            try {
                prom->set_value(execute_request(config, absent, lesson, logfile));
            } catch (...) {
                try {
                    prom->set_exception(std::current_exception());
                } catch (...) {
                    logfile << "Exception occurred in execute_request, but failed to set exception!\n" << std::flush;
                }
            }
        }, absent, lesson, prom).detach();
        auto fut_status = fut.wait_for(std::chrono::seconds(timeout));
        if (fut_status == std::future_status::ready)
            return fut.get();
        else
            throw NetworkError("execute_request timed out.");
    }

    static void send_to_gs_impl(int gs_port, std::string msg, std::shared_ptr<std::promise<std::string>> prom) {
        namespace asio = boost::asio;
        asio::io_context ioc;
        asio::ip::udp::socket socket(ioc);
        socket.open(asio::ip::udp::v4());
        asio::ip::udp::endpoint addr(asio::ip::address::from_string("127.0.0.1"), gs_port);
        std::exception_ptr exptr;
        try {
            socket.send_to(asio::buffer(std::move(msg)), addr);
            std::array<char, 128> buff;
            asio::ip::udp::endpoint endpoint;
            auto n = socket.receive_from(asio::buffer(buff), endpoint);
            prom->set_value(std::string(buff.begin(), n));
            return;
        } catch (const boost::system::system_error& ex) {
            try {
                if (ex.code().value() == asio::error::connection_reset)
                    throw NetworkError("Connection was reset, maybe GS not up?");
                else
                    throw NetworkError("Network error " + std::to_string(ex.code().value()));
            } catch (const NetworkError& ex2) {
                // Set it to the promise
                try {
                    prom->set_exception(std::current_exception());
                } catch (...) {
                    // Nothing to do.
                }
            }
        }
    }

    void send_to_gs(const Configuration& config, Logfile& log, const std::string& msg) {
        log << "Sending message to GS: " << msg << '\n';
        using Promise = std::promise<std::string>;
        std::shared_ptr<Promise> prom(new Promise());
        auto fut = prom->get_future();
        // Launch the detached thread
        std::thread(&send_to_gs_impl, config["gs_port"].get<int>(), msg, prom).detach();
        auto stat = fut.wait_for(std::chrono::seconds(2));
        if (stat == std::future_status::ready) {
            // This line might throw NetworkError
            std::string_view line = std::move(fut.get());
            if (line.substr(0, 7) != "success")
                throw GSError(line.data());
            // Success, return
            return;
        } else
            throw NetworkError("Sending to GS timed out");
    }
}
