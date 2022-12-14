#include <iostream>
#include <gxrio.hpp>

int main(int argc, char * const argv[])
{
	gxrio::ifstream in(argv[1]);

	if (not in.is_open())
	{
		std::cerr << "Could not open file" << std::endl;
		exit(1);
	}

	std::string line;
	while (getline(in, line))
		std::cout << line << std::endl;

	return 0;
}
