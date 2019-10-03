#include <iostream>
#include <sstream>
#include "MBR.hpp"
#include "FAT32.hpp"

using namespace std;



int main(int argc,char** argv) {

	if (argc < 2) {
		cerr << "Usage:\tFAT32_editor vhd_file" << endl;
		return -1;
	}

	string filename(argv[1]);

	FAT32* fs = nullptr;
	MBR* vhd = nullptr;

	do {

		try {
			vhd = new MBR(filename);
			//MBR vhd(string(argv[1]));

			for (auto i = 0; i < 4; i++) {
				if (fs = vhd->partition(i))
					break;
			}
		}
		catch (exception& e) {
			cerr << e.what() << endl;
		}
		if (!fs) {
			cerr << "No FAT32 partition found" << endl;
			break;
		}

		/*

		put inner_path outer_path


		*/

		while (true) {
			cout << "FAT32 > ";
			string str;
			getline(cin, str);
			if (!cin.good())
				break;

			stringstream ss(str);

			ss >> str;

			try {
				if (str == "list") {
					auto it = fs->list();

					while (it.step()) {
						string name = it.fullname();
						if (it.is_folder())
							name += '\\';
						cout << name << '\t' << it.size() << endl;
					}


					continue;
				}

				if (str == "put") {
					string inner_name;
					ss >> inner_name;
					ss >> str;	//outer_path

					ifstream file(str, ios::in | ios::binary);
					if (!file.is_open()) {
						cerr << "cannot open file\t" << str << endl;
						continue;
					}

					if (!fs->put(inner_name, file)) {
						cerr << "error in FAT32::put\t" << inner_name << endl;
					}
					continue;
				}
				cerr << "unknown command" << endl;
			}
			catch (exception& e) {
				cerr << e.what() << endl;
			}

		}
	} while (false);

	delete vhd;

	return 0;
}