#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <sstream>
#include <boost/program_options.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include <boost/date_time.hpp>
#include <boost/regex.hpp>

#define IO_TIMEOUT 60
#define COMMAND 25

using namespace std;
namespace a = boost::asio;
namespace ai = a::ip;
namespace it = boost::archive::iterators;
namespace po = boost::program_options;
namespace pt = boost::posix_time;

void printHelp(const po::options_description &desc)
{
	cout << endl << "C.S.C. (Command Swann Cam)" << endl;
	cout << "\tSend a scan command to your swann cam every delta_sec seconds." << endl;
	cout << endl << "csc [options] [help | [host [port [username [password [delta_secs]]]]]]" << endl;
	cout << endl << desc << endl;
	cout << "Examples:" << endl;
	cout << "\tcsc.exe mySubdomain.myDomain.com 80 myUsername myPassword 60" << endl;
	cout << "\tcsc.exe -t 60 -o 90 -u myUsername -h mySubdomain.myDomain.com -p myPassword" << endl;
}

void printTimeStamp()
{
	pt::ptime now = pt::second_clock::local_time();
	cout << static_cast<int>(now.date().month()) << "/";
	cout << now.date().day() << "/";
	cout << now.date().year() << " ";
	cout << now.time_of_day() << " - ";
}

int main(int argc, char * argv[])
{
	try
	{
		string host, port, username, password;
		unsigned int secs;

		// Declare the supported options.
		po::options_description desc("Options");
		desc.add_options()
			("help", "Print this help message.")
			("host,h", po::value<string>(&host), "The host to connect to.")
			("port,o", po::value<string>(&port), "The port to use.")
			("username,u", po::value<string>(&username), "Service username.")
			("password,p", po::value<string>(&password), "Service password.")
			("delta_secs,t", po::value<unsigned int>(&secs), "Seconds between commands.")
			;

		po::positional_options_description p;
		p.add("host", 1);
		p.add("port", 1);
		p.add("username", 1);
		p.add("password", 1);
		p.add("delta_secs", 1);

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
		po::notify(vm);

		string hlpMsg("csc --help to see available options.");
		bool hostNotSet = !vm.count("host");
		bool portNotSet = !vm.count("port");
		bool userNotSet = !vm.count("username");
		bool passNotSet = !vm.count("password");
		bool secsNotSet = !vm.count("delta_secs");

		if (vm.count("help") || (hostNotSet && portNotSet && userNotSet && passNotSet && secsNotSet))
		{
			printHelp(desc);
			return 0;
		}
		if (hostNotSet)
		{
			cout << "Error: No host name provided." << endl;
		}
		if (portNotSet)
		{
			cout << "Error: No port provided." << endl;
		}
		if (userNotSet)
		{
			cout << "Error: No username provided." << endl;
		}
		if (passNotSet)
		{
			cout << "Error: No password provided." << endl;
		}
		if (secsNotSet)
		{
			cout << "Error: No delta_mins provided." << endl;
		}
		if (hostNotSet || portNotSet || userNotSet || passNotSet || secsNotSet)
		{
			cout << hlpMsg << endl;
			return -1;
		}

		// Filter hostname
		boost::regex expression(R"((?:\w*:\/\/)(?:w{0,3}\.)?(\w+\.\w+\.?\w*):?(\d+)?\/?)");
		boost::match_results<string::const_iterator> matches;
		boost::regex_search(host, matches, expression);
		host = matches[1];

		string auth(username + ":" + password);
		stringstream os;
		typedef
			it::base64_from_binary <    // convert binary values ot base64 characters
				it::transform_width <   // retrieve 6 bit integers from a sequence of 8 bit bytes
					const char *,
					6,
					8
				>
			>
			base64_text; // compose all the above operations in to a new iterator
		std::copy(base64_text(auth.c_str()), base64_text(auth.c_str() + auth.length()), ostream_iterator<char>(os));

		ai::tcp::iostream s;
		a::io_service io;
		a::deadline_timer timer(io);
		while (true)
		{
			// Each request or error will have a timestamp.
			printTimeStamp();

			// Time between requests.
			timer.expires_from_now(boost::posix_time::seconds(secs));

			// The entire sequence of I/O operations must complete within 60 seconds.
			// If an expiry occurs, the socket is automatically closed and the stream
			// becomes bad.
			s.expires_from_now(boost::posix_time::seconds(IO_TIMEOUT));

			// Establish a connection to the server.
			s.connect(host, port);
			if (s)
			{
				// Send the request. We specify the "Connection: close" header so that the
				// server will close the socket after transmitting the response. This will
				// allow us to treat all data up until the EOF as the content.
				s << "GET /decoder_control.cgi?command=" + to_string(COMMAND) + " HTTP/1.1\r\n";
				s << "Host: " + host + ":" + port + "\r\n";
				s << "Accept: */*\r\n";
				s << "Connection: close\r\n";
				s << "Authorization: Basic " + os.str() + "=\r\n\r\n";

				// By default, the stream is tied with itself. This means that the stream
				// automatically flush the buffered output before attempting a read. It is
				// not necessary not explicitly flush the stream at this point.

				// Check that response is OK.
				string http_version;
				s >> http_version;
				if (s && http_version.substr(0, 5) == "HTTP/")
				{
					unsigned int status_code;
					s >> status_code;
					if (status_code == 200)
					{
						string status_message;
						getline(s, status_message);

						// Process the response headers, which are terminated by a blank line.
						string header;
						while (getline(s, header) && header != "\r") {}

						cout << "Success. Sent command: " << COMMAND << ". ";
						cout << "Server response: " << s.rdbuf();
					}
					else
					{
						cerr << "Response returned with status code " << status_code << endl;
					}
				}
				else
				{
					cerr << "Invalid response: " << http_version << endl;
				}
			}
			else
			{
				cerr << "Unable to connect: " << s.error().message() << endl;
			}

			timer.wait();
		}
	}
	catch (exception& e)
	{
		cerr << "Error: " << e.what() << endl;
		return -1;
	}

	return 0;
}