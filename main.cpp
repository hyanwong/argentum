#include "InputReader.h"
#include "Tree.h"
#include "TreeController.h"
#include <sstream>
#include <getopt.h>
using namespace std;

int atoi_min(char const *value, int min, char const *parameter, char const *name)
{
    std::istringstream iss(value);
    int i;
    char c;
    if (!(iss >> i) || iss.get(c))
    {
        cerr << "error: argument of " << parameter << " must be of type <int>, and greater than or equal to " << min << endl
             << "Check " << name << " --help' for more information." << endl;
        std::exit(1);
    }

    if (i < min)
    {
        cerr << "error: argument of " << parameter << " must be greater than or equal to " << min << endl
             << "Check `" << name << " --help' for more information." << endl;
        std::exit(1);
    }
    return i;
}

int main(int argc, char ** argv)
{
    string inputfile, dotfile;
    InputReader::input_format_t inputformat = InputReader::input_unset;
    unsigned nrows = ~0u;
    bool verbose = false;
    bool debug = false;
    
    static struct option long_options[] =
        {
            {"input",     required_argument, 0, 'i'},
            {"nrows",     required_argument, 0, 'n'},
            {"plaintext", no_argument,       0, 's'},
            {"dot",       required_argument, 0, 'd'},
            {"verbose",   no_argument,       0, 'v'},
            {"debug",     no_argument,       0, 'D'},
            {0, 0, 0, 0}
        };
    int option_index = 0;
    int c;
    while ((c = getopt_long(argc, argv, "i:n:sd:vD",
                            long_options, &option_index)) != -1) 
    {
        switch(c) 
        {
        case 'i':
            inputfile = string(optarg); break;
        case 'n':
            nrows = atoi_min(optarg, 1, "-n,--nrows", argv[0]); break;
        case 's': 
            inputformat = InputReader::input_plaintext; break;
        case 'd':
            dotfile = string(optarg); break;
        case 'v':
            verbose = true; break;
        case 'D':
            debug = true; break;
        default:
            cerr << "error: see -h for usage!" << endl;
            return 1;
        }
    }

    unsigned step = 0;
    InputColumn ic;
    InputReader *inputr = InputReader::build(inputformat, inputfile);
    if (!inputr->good())
    {
        cerr << "error: could not read input file '" << inputfile << "'" << endl;
        return 1;
    }

    /**
     * Main loop over input data
     */
    inputr->next(ic);
    PointerTree tree(ic);
    TreeController tc(tree, debug);
    do
    {
        if (debug)
        {
            cout << "step " << step << ": ";
            for (InputColumn::const_iterator it = ic.begin(); it!= ic.end(); ++it)
                cout << (int)*it;
            cout << endl;
        }

        tc.process(ic, step);

        if (!dotfile.empty())
            tree.outputDOT(dotfile, step);
        if (debug)
            tree.validate();
        ++step;
    } while (inputr->next(ic) && step < nrows);

    if (verbose)
        cerr << "All done. Finished after " << step << " steps of input." << endl;
    
    // Clean up
    delete inputr; inputr = 0;
    return 0;
}
