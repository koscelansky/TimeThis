#include <boost/program_options.hpp>
#include <boost/process.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <chrono>

namespace po = boost::program_options;
namespace bp = boost::process;

struct Parameters
{
    bool PrintHelp = false;
    int Timeout = -1; // unlimited
    std::string Executable;
    std::vector<std::string> Params;
};

class ArgumentParser
{
public:
    ArgumentParser()
        : desc_("Allowed options")
    {
        desc_.add_options()
            ("help,H", "produce help message")
            ("timeout", po::value<int>()->default_value(-1), "set process timeout");

        p_.add("executable", 1);
        p_.add("parameters", -1);
    }

    Parameters Parse(int argc, char** argv) const
    {
        po::variables_map vm;

        std::vector<std::string> params;

        po::options_description hidden;
        hidden.add_options()
            ("executable", po::value<std::string>())
            ("parameters", po::value(&params));

        po::options_description cmdline_options;
        cmdline_options.add(desc_).add(hidden);

        po::store(po::command_line_parser(argc, argv).
            options(cmdline_options).positional(p_).run(), vm);
        po::notify(vm);

        if (vm.count("help") != 0)
            return Parameters{ true };

        if (vm.count("executable") == 0)
            throw std::runtime_error("Path to executable is required.");

        return Parameters{
            false, // help
            vm["timeout"].as<int>(),
            vm["executable"].as<std::string>(),
            params,
        };
    }

private:
    po::options_description desc_;
    po::positional_options_description p_;

    friend std::ostream& operator<<(std::ostream& lhs, const ArgumentParser& rhs);
};

std::ostream& operator<<(std::ostream& lhs, const ArgumentParser& rhs)
{
    lhs << "Usage: TimeThis [options] executable param1 ... paramN\n";
    lhs << rhs.desc_;
    return lhs;
}

int main(int argc, char** argv)
{
    ArgumentParser ap;
    try
    {
        Parameters params = ap.Parse(argc, argv);

        if (params.PrintHelp)
        {
            std::cout << ap;
            return 0;
        }

        auto start = std::chrono::high_resolution_clock::now();

        bp::child process(params.Executable, bp::args(params.Params), bp::std_out > bp::null, bp::std_err > bp::null);

        if (params.Timeout < 0)
        {
            process.wait(); // infinite wait
        }
        else
        {
            auto timeout = std::chrono::seconds(params.Timeout);
            if (!process.wait_for(timeout))
            {
                process.terminate();
                std::cout << "Timeout!\n";
                return 0;
            }
        }

        auto duration = std::chrono::high_resolution_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        std::cout << "Time " << ms.count()  <<"ms\n";
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
        std::cout << ap;
        return 1;
    }
}