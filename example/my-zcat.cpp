#include <iostream>
#include <cstdlib>
#include <gxrio.hpp>

int main(int argc, char * const argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: my-zcat <file>" << '\n';
		exit(1);
	}

	gxrio::ifstream in(argv[1]);

	if (not in.is_open())
	{
		std::cerr << "Could not open file" << '\n';
		exit(1);
	}

	std::string line;
	while (getline(in, line))
		std::cout << line << '\n';

	return 0;
}
